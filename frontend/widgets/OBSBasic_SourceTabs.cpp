#include "OBSBasic.hpp"

#include <components/SourceTabBar.hpp>

#include <dialogs/NameDialog.hpp>

#include <qt-wrappers.hpp>

#include <QAbstractItemModel>
#include <QAction>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QMenu>
#include <QSignalBlocker>
#include <QTabBar>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

using namespace std;

void OBSBasic::SetupSourceTabs()
{
	auto *strip = new QWidget(ui->sourcesFrame);
	auto *stripLayout = new QHBoxLayout(strip);
	stripLayout->setContentsMargins(0, 0, 0, 0);
	stripLayout->setSpacing(0);

	sourceTabBar = new SourceTabBar(strip);
	sourceTabBar->setDocumentMode(true);
	sourceTabBar->setDrawBase(false);
	sourceTabBar->setExpanding(false);
	sourceTabBar->setElideMode(Qt::ElideRight);
	sourceTabBar->setFocusPolicy(Qt::NoFocus);
	sourceTabBar->setContextMenuPolicy(Qt::CustomContextMenu);
	sourceTabBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

	auto *addButton = new QToolButton(strip);
	addButton->setText(QStringLiteral("+"));
	addButton->setToolTip(QTStr("Basic.Main.SourceTab.Create"));
	addButton->setAutoRaise(true);
	addButton->setFocusPolicy(Qt::NoFocus);

	stripLayout->addWidget(sourceTabBar, 1);
	stripLayout->addWidget(addButton, 0);

	if (auto *sourcesLayout = qobject_cast<QVBoxLayout *>(ui->sourcesFrame->layout()))
		sourcesLayout->insertWidget(0, strip);

	connect(addButton, &QToolButton::clicked, this, [this]() { CreateSourceTabInteractive(); });

	connect(sourceTabBar, &QTabBar::currentChanged, this, [this](int index) {
		if (!sourceTabBar || index < 0)
			return;

		const QString tab = sourceTabBar->tabData(index).toString();
		if (tab == activeSourceTab)
			return;

		activeSourceTab = tab;
		ApplySourceTabFilter(true);
		SaveProject();
	});

	connect(sourceTabBar, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
		if (!sourceTabBar)
			return;

		const int index = sourceTabBar->tabAt(pos);
		const QString tab = index >= 0 ? sourceTabBar->tabData(index).toString() : QString();

		QMenu menu(this);
		if (!tab.isEmpty()) {
			menu.addAction(QTStr("Rename"), this,
				       [this, tab]() { RenameSourceTabInteractive(tab); });
			menu.addAction(QTStr("Basic.Main.SourceTab.Remove"), this,
				       [this, tab]() { RemoveSourceTab(tab); });
			menu.addSeparator();
		}
		menu.addAction(QTStr("Basic.Main.SourceTab.Create") + "...", this,
			       [this]() { CreateSourceTabInteractive(); });
		menu.exec(sourceTabBar->mapToGlobal(pos));
	});

	connect(sourceTabBar, &SourceTabBar::sourceDropped, this, [this](int index) {
		if (!sourceTabBar || index < 0)
			return;

		const QString tab = sourceTabBar->tabData(index).toString();
		const QModelIndexList selected = ui->sources->selectionModel()->selectedRows();
		for (const QModelIndex &modelIndex : selected)
			AssignSourceToTab(SourceNameAtRow(modelIndex.row()), tab);
	});

	QAbstractItemModel *model = ui->sources->model();
	connect(model, &QAbstractItemModel::rowsInserted, this, &OBSBasic::ScheduleSourceTabFilter);
	connect(model, &QAbstractItemModel::rowsRemoved, this, &OBSBasic::ScheduleSourceTabFilter);
	connect(model, &QAbstractItemModel::rowsMoved, this, &OBSBasic::ScheduleSourceTabFilter);
	connect(model, &QAbstractItemModel::modelReset, this, &OBSBasic::ScheduleSourceTabFilter);
	connect(model, &QAbstractItemModel::layoutChanged, this, &OBSBasic::ScheduleSourceTabFilter);

	RefreshSourceTabBar();
}

void OBSBasic::ScheduleSourceTabFilter()
{
	if (sourceTabFilterScheduled)
		return;

	sourceTabFilterScheduled = true;
	QTimer::singleShot(0, this, [this]() {
		sourceTabFilterScheduled = false;
		ApplySourceTabFilter();
	});
}

void OBSBasic::RefreshSourceTabBar()
{
	if (!sourceTabBar)
		return;

	QSignalBlocker blocker(sourceTabBar);
	while (sourceTabBar->count())
		sourceTabBar->removeTab(0);

	sourceTabBar->addTab(QTStr("Basic.Main.SourceTab.All"));
	sourceTabBar->setTabData(0, QString());

	int current = 0;
	for (const QString &tab : sourceTabs) {
		const int index = sourceTabBar->addTab(tab);
		sourceTabBar->setTabData(index, tab);
		if (tab == activeSourceTab)
			current = index;
	}

	if (activeSourceTab.isEmpty() || !sourceTabs.contains(activeSourceTab))
		activeSourceTab.clear();

	sourceTabBar->setCurrentIndex(current);
}

QString OBSBasic::SourceNameAtRow(int row)
{
	QAbstractItemModel *model = ui->sources->model();
	if (!model || row < 0 || row >= model->rowCount())
		return QString();

	OBSSceneItem item = ui->sources->Get(row);
	if (!item)
		return QString();

	obs_source_t *source = obs_sceneitem_get_source(item);
	return source ? QT_UTF8(obs_source_get_name(source)) : QString();
}

void OBSBasic::ApplySourceTabFilter(bool clearHiddenSelection)
{
	if (!ui || !ui->sources)
		return;

	const bool filter = !activeSourceTab.isEmpty();
	QAbstractItemModel *model = ui->sources->model();
	const int rows = model ? model->rowCount() : 0;

	for (int i = 0; i < rows; i++) {
		const QString name = SourceNameAtRow(i);
		const bool hide = filter && sourceTabMembership.value(name) != activeSourceTab;
		ui->sources->setRowHidden(i, hide);
	}

	if (clearHiddenSelection) {
		const int row = ui->sources->currentIndex().row();
		if (row >= 0 && ui->sources->isRowHidden(row))
			ui->sources->setCurrentIndex(QModelIndex());
	}
}

QString OBSBasic::CreateSourceTabInteractive(const QString &assignSource)
{
	QString defaultName = QTStr("Basic.Main.SourceTab.DefaultName");
	QString placeholder = defaultName;
	int suffix = 2;
	while (sourceTabs.contains(placeholder))
		placeholder = QString("%1 %2").arg(defaultName).arg(suffix++);

	for (;;) {
		string name = placeholder.toStdString();
		bool accepted = NameDialog::AskForName(this, QTStr("Basic.Main.SourceTab.Add.Title"),
						       QTStr("Basic.Main.SourceTab.Add.Text"), name, placeholder);
		if (!accepted)
			return QString();

		QString tabName = QString::fromStdString(name).trimmed();
		if (tabName.isEmpty()) {
			OBSMessageBox::warning(this, QTStr("NoNameEntered.Title"), QTStr("NoNameEntered.Text"));
			continue;
		}
		if (sourceTabs.contains(tabName)) {
			OBSMessageBox::warning(this, QTStr("NameExists.Title"), QTStr("NameExists.Text"));
			continue;
		}

		sourceTabs.append(tabName);
		activeSourceTab = tabName;
		if (!assignSource.isEmpty())
			sourceTabMembership.insert(assignSource, tabName);

		RefreshSourceTabBar();
		ApplySourceTabFilter();
		SaveProject();
		return tabName;
	}
}

void OBSBasic::RenameSourceTabInteractive(const QString &name)
{
	if (!sourceTabs.contains(name))
		return;

	for (;;) {
		string newName = name.toStdString();
		bool accepted = NameDialog::AskForName(this, QTStr("Basic.Main.SourceTab.Rename.Title"),
						       QTStr("Basic.Main.SourceTab.Rename.Text"), newName, name);
		if (!accepted)
			return;

		QString tabName = QString::fromStdString(newName).trimmed();
		if (tabName.isEmpty()) {
			OBSMessageBox::warning(this, QTStr("NoNameEntered.Title"), QTStr("NoNameEntered.Text"));
			continue;
		}
		if (tabName == name)
			return;
		if (sourceTabs.contains(tabName)) {
			OBSMessageBox::warning(this, QTStr("NameExists.Title"), QTStr("NameExists.Text"));
			continue;
		}

		sourceTabs[sourceTabs.indexOf(name)] = tabName;
		if (activeSourceTab == name)
			activeSourceTab = tabName;
		for (auto it = sourceTabMembership.begin(); it != sourceTabMembership.end(); ++it) {
			if (it.value() == name)
				it.value() = tabName;
		}
		break;
	}

	RefreshSourceTabBar();
	ApplySourceTabFilter();
	SaveProject();
}

void OBSBasic::RemoveSourceTab(const QString &name)
{
	if (!sourceTabs.contains(name))
		return;

	sourceTabs.removeAll(name);
	if (activeSourceTab == name)
		activeSourceTab.clear();
	for (auto it = sourceTabMembership.begin(); it != sourceTabMembership.end();) {
		if (it.value() == name)
			it = sourceTabMembership.erase(it);
		else
			++it;
	}

	RefreshSourceTabBar();
	ApplySourceTabFilter();
	SaveProject();
}

void OBSBasic::AssignSourceToTab(const QString &sourceName, const QString &tab)
{
	if (sourceName.isEmpty())
		return;

	if (tab.isEmpty()) {
		sourceTabMembership.remove(sourceName);
	} else if (sourceTabs.contains(tab)) {
		sourceTabMembership.insert(sourceName, tab);
	} else {
		return;
	}

	ApplySourceTabFilter();
	SaveProject();
}

obs_data_t *OBSBasic::SaveSourceTabs()
{
	obs_data_t *data = obs_data_create();

	OBSDataArrayAutoRelease tabs = obs_data_array_create();
	for (const QString &tab : sourceTabs) {
		OBSDataAutoRelease entry = obs_data_create();
		obs_data_set_string(entry, "name", QT_TO_UTF8(tab));
		obs_data_array_push_back(tabs, entry);
	}
	obs_data_set_array(data, "tabs", tabs);
	obs_data_set_string(data, "active", QT_TO_UTF8(activeSourceTab));

	OBSDataArrayAutoRelease membership = obs_data_array_create();
	for (auto it = sourceTabMembership.cbegin(); it != sourceTabMembership.cend(); ++it) {
		OBSDataAutoRelease entry = obs_data_create();
		obs_data_set_string(entry, "source", QT_TO_UTF8(it.key()));
		obs_data_set_string(entry, "tab", QT_TO_UTF8(it.value()));
		obs_data_array_push_back(membership, entry);
	}
	obs_data_set_array(data, "membership", membership);

	return data;
}

void OBSBasic::LoadSourceTabs(obs_data_t *data)
{
	sourceTabs.clear();
	sourceTabMembership.clear();
	activeSourceTab.clear();

	if (!data) {
		RefreshSourceTabBar();
		return;
	}

	OBSDataArrayAutoRelease tabs = obs_data_get_array(data, "tabs");
	if (tabs) {
		const size_t count = obs_data_array_count(tabs);
		for (size_t i = 0; i < count; i++) {
			OBSDataAutoRelease entry = obs_data_array_item(tabs, i);
			const char *name = obs_data_get_string(entry, "name");
			if (!name || !*name)
				continue;

			const QString tab = QT_UTF8(name).trimmed();
			if (tab.isEmpty() || sourceTabs.contains(tab))
				continue;
			sourceTabs.append(tab);
		}
	}

	const char *active = obs_data_get_string(data, "active");
	activeSourceTab = active ? QT_UTF8(active) : QString();
	if (!sourceTabs.contains(activeSourceTab))
		activeSourceTab.clear();

	OBSDataArrayAutoRelease membership = obs_data_get_array(data, "membership");
	if (membership) {
		const size_t count = obs_data_array_count(membership);
		for (size_t i = 0; i < count; i++) {
			OBSDataAutoRelease entry = obs_data_array_item(membership, i);
			const char *source = obs_data_get_string(entry, "source");
			const char *tab = obs_data_get_string(entry, "tab");
			if (!source || !*source || !tab || !*tab)
				continue;
			if (!sourceTabs.contains(QT_UTF8(tab)))
				continue;
			sourceTabMembership.insert(QT_UTF8(source), QT_UTF8(tab));
		}
	}

	RefreshSourceTabBar();
	ApplySourceTabFilter();
}
