#include "LumaCastAssetManager.hpp"

#include <OBSApp.hpp>
#include <obs.hpp>
#include <qt-wrappers.hpp>

#include <QColor>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFontDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QUrl>

#include <QProcess>

#include <cstring>

namespace {

enum Column { ColStatus = 0, ColSource, ColType, ColLocation, ColCount };

enum Role {
	UuidRole = Qt::UserRole,
	KeyRole,
	IndexRole,
	KindRole,
	FilterRole,
	ValueRole,
};

enum class AssetKind { File, Font };

enum FilterKind { FilterAll = 0, FilterImages, FilterMedia, FilterPages };

const char *SourceIdOf(obs_source_t *source)
{
	const char *id = obs_source_get_id(source);
	return id ? id : "";
}

QString TypeLabel(const char *id)
{
	if (strcmp(id, "ffmpeg_source") == 0)
		return QStringLiteral("Media");
	if (strcmp(id, "vlc_source") == 0)
		return QStringLiteral("VLC Playlist");
	if (strcmp(id, "image_source") == 0)
		return QStringLiteral("Image");
	if (strncmp(id, "slideshow", 9) == 0)
		return QStringLiteral("Slideshow");
	if (strcmp(id, "browser_source") == 0)
		return QStringLiteral("Browser");
	if (strncmp(id, "text_gdiplus", 12) == 0 || strncmp(id, "text_ft2", 8) == 0)
		return QTStr("Basic.AssetManager.Type.Font");
	return QString::fromUtf8(id);
}

bool IsRemotePath(const QString &path)
{
	return path.contains(QStringLiteral("://"));
}

bool FontInstalled(const QString &face)
{
	return QFontDatabase::families().contains(face, Qt::CaseInsensitive);
}

} // namespace

LumaCastAssetManager::LumaCastAssetManager(QWidget *parent) : QDialog(parent)
{
	setWindowTitle(QTStr("Basic.AssetManager.Title"));
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	setMinimumSize(720, 380);
	resize(880, 500);

	auto *layout = new QVBoxLayout(this);

	summary = new QLabel(this);
	summary->setWordWrap(true);
	layout->addWidget(summary);

	tree = new QTreeWidget(this);
	tree->setColumnCount(ColCount);
	tree->setRootIsDecorated(false);
	tree->setUniformRowHeights(true);
	tree->setAlternatingRowColors(true);
	tree->setSelectionMode(QAbstractItemView::SingleSelection);
	tree->setSelectionBehavior(QAbstractItemView::SelectRows);
	tree->setHeaderLabels({QTStr("Basic.AssetManager.Column.Status"),
			       QTStr("Basic.AssetManager.Column.Source"),
			       QTStr("Basic.AssetManager.Column.Type"),
			       QTStr("Basic.AssetManager.Column.Location")});
	tree->header()->setStretchLastSection(true);
	tree->header()->setSectionResizeMode(ColStatus, QHeaderView::Fixed);
	tree->header()->resizeSection(ColStatus, 90);
	tree->header()->resizeSection(ColSource, 200);
	tree->header()->resizeSection(ColType, 110);
	layout->addWidget(tree, 1);

	auto *buttons = new QHBoxLayout();
	refreshButton = new QPushButton(QTStr("Basic.AssetManager.Refresh"), this);
	revealButton = new QPushButton(QTStr("Basic.AssetManager.Reveal"), this);
	locateButton = new QPushButton(QTStr("Basic.AssetManager.Replace"), this);
	auto *closeButton = new QPushButton(QTStr("Close"), this);

	buttons->addWidget(refreshButton);
	buttons->addStretch(1);
	buttons->addWidget(revealButton);
	buttons->addWidget(locateButton);
	buttons->addWidget(closeButton);
	layout->addLayout(buttons);

	connect(refreshButton, &QPushButton::clicked, this, &LumaCastAssetManager::Scan);
	connect(revealButton, &QPushButton::clicked, this, &LumaCastAssetManager::RevealAsset);
	connect(locateButton, &QPushButton::clicked, this, &LumaCastAssetManager::LocateAsset);
	connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
	connect(tree, &QTreeWidget::itemSelectionChanged, this, &LumaCastAssetManager::UpdateActionState);
	connect(tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *, int) {
		LocateAsset();
	});

	Scan();
}

void LumaCastAssetManager::Scan()
{
	tree->clear();
	assetCount = 0;
	missingCount = 0;

	obs_enum_sources(
		[](void *param, obs_source_t *source) {
			static_cast<LumaCastAssetManager *>(param)->ScanSource(source);
			return true;
		},
		this);

	UpdateSummary();
	UpdateActionState();
}

void LumaCastAssetManager::ScanSource(obs_source *source)
{
	if (obs_source_get_type(source) == OBS_SOURCE_TYPE_SCENE)
		return;

	const char *id = SourceIdOf(source);
	const QString uuid = QT_UTF8(obs_source_get_uuid(source));
	const QString name = QT_UTF8(obs_source_get_name(source));
	const QString type = TypeLabel(id);

	OBSDataAutoRelease settings = obs_source_get_settings(source);

	if (strcmp(id, "ffmpeg_source") == 0 || strcmp(id, "browser_source") == 0) {
		const char *path = obs_data_get_string(settings, "local_file");
		if (path && *path)
			AddFileAsset(uuid, name, type, QT_UTF8(path), QStringLiteral("local_file"), -1,
				     strcmp(id, "browser_source") == 0 ? FilterPages : FilterMedia);
	} else if (strcmp(id, "image_source") == 0) {
		const char *path = obs_data_get_string(settings, "file");
		if (path && *path)
			AddFileAsset(uuid, name, type, QT_UTF8(path), QStringLiteral("file"), -1,
				     FilterImages);
	} else if (strncmp(id, "slideshow", 9) == 0) {
		OBSDataArrayAutoRelease files = obs_data_get_array(settings, "files");
		const size_t count = files ? obs_data_array_count(files) : 0;
		for (size_t i = 0; i < count; i++) {
			OBSDataAutoRelease entry = obs_data_array_item(files, i);
			const char *path = obs_data_get_string(entry, "value");
			if (path && *path)
				AddFileAsset(uuid, name, type, QT_UTF8(path), QStringLiteral("files"),
					     int(i), FilterImages);
		}
	} else if (strcmp(id, "vlc_source") == 0) {
		OBSDataArrayAutoRelease playlist = obs_data_get_array(settings, "playlist");
		const size_t count = playlist ? obs_data_array_count(playlist) : 0;
		for (size_t i = 0; i < count; i++) {
			OBSDataAutoRelease entry = obs_data_array_item(playlist, i);
			const char *path = obs_data_get_string(entry, "value");
			if (path && *path && !IsRemotePath(QT_UTF8(path)))
				AddFileAsset(uuid, name, type, QT_UTF8(path), QStringLiteral("playlist"),
					     int(i), FilterMedia);
		}
	} else if (strcmp(id, "text_gdiplus") == 0 || strncmp(id, "text_gdiplus", 12) == 0 ||
		   strncmp(id, "text_ft2", 8) == 0) {
		OBSDataAutoRelease font = obs_data_get_obj(settings, "font");
		const char *face = font ? obs_data_get_string(font, "face") : nullptr;
		if (face && *face)
			AddFontAsset(uuid, name, type, QT_UTF8(face));
	}
}

void LumaCastAssetManager::AddFileAsset(const QString &uuid, const QString &sourceName,
					const QString &type, const QString &path, const QString &key,
					int index, int filter)
{
	auto *item = new QTreeWidgetItem(tree);
	item->setText(ColSource, sourceName);
	item->setText(ColType, type);
	item->setText(ColLocation, QDir::toNativeSeparators(path));
	item->setData(ColSource, UuidRole, uuid);
	item->setData(ColSource, KeyRole, key);
	item->setData(ColSource, IndexRole, index);
	item->setData(ColSource, KindRole, int(AssetKind::File));
	item->setData(ColSource, FilterRole, filter);
	item->setData(ColSource, ValueRole, path);
	item->setToolTip(ColLocation, QDir::toNativeSeparators(path));

	const bool remote = IsRemotePath(path);
	const bool present = !remote && QFileInfo(path).isFile();
	if (remote)
		SetStatus(item, QTStr("Basic.AssetManager.Status.Remote"), true);
	else
		SetStatus(item, present ? QTStr("Basic.AssetManager.Status.Ok")
					: QTStr("Basic.AssetManager.Status.Missing"),
			  present);
}

void LumaCastAssetManager::AddFontAsset(const QString &uuid, const QString &sourceName,
					const QString &type, const QString &face)
{
	auto *item = new QTreeWidgetItem(tree);
	item->setText(ColSource, sourceName);
	item->setText(ColType, type);
	item->setText(ColLocation, face);
	item->setData(ColSource, UuidRole, uuid);
	item->setData(ColSource, KeyRole, QStringLiteral("font"));
	item->setData(ColSource, IndexRole, -1);
	item->setData(ColSource, KindRole, int(AssetKind::Font));
	item->setData(ColSource, FilterRole, FilterAll);
	item->setData(ColSource, ValueRole, face);
	item->setToolTip(ColLocation, QTStr("Basic.AssetManager.FontTip"));

	const bool installed = FontInstalled(face);
	SetStatus(item, installed ? QTStr("Basic.AssetManager.Status.Ok")
				  : QTStr("Basic.AssetManager.Status.FontMissing"),
		  installed);
}

void LumaCastAssetManager::SetStatus(QTreeWidgetItem *item, const QString &text, bool ok)
{
	item->setText(ColStatus, text);
	item->setForeground(ColStatus, ok ? palette().color(QPalette::Text) : QColor("#ff5f5f"));

	assetCount++;
	if (!ok)
		missingCount++;
}

void LumaCastAssetManager::UpdateSummary()
{
	if (assetCount == 0) {
		summary->setText(QTStr("Basic.AssetManager.NoAssets"));
		return;
	}

	summary->setText(QTStr("Basic.AssetManager.Summary")
				 .arg(QString::number(assetCount), QString::number(missingCount)));
}

QTreeWidgetItem *LumaCastAssetManager::SelectedItem() const
{
	const QList<QTreeWidgetItem *> selected = tree->selectedItems();
	return selected.isEmpty() ? nullptr : selected.first();
}

QString LumaCastAssetManager::UuidOf(QTreeWidgetItem *item) const
{
	return item ? item->data(ColSource, UuidRole).toString() : QString();
}

void LumaCastAssetManager::UpdateActionState()
{
	QTreeWidgetItem *item = SelectedItem();
	revealButton->setEnabled(false);
	locateButton->setEnabled(false);
	locateButton->setText(QTStr("Basic.AssetManager.Replace"));

	if (!item)
		return;

	const bool isFont = item->data(ColSource, KindRole).toInt() == int(AssetKind::Font);
	locateButton->setEnabled(true);
	locateButton->setText(isFont ? QTStr("Basic.AssetManager.ChangeFont")
				     : QTStr("Basic.AssetManager.Replace"));

	const QString path = item->data(ColSource, ValueRole).toString();
	revealButton->setEnabled(!isFont && !IsRemotePath(path) && QFileInfo(path).isFile());
}

void LumaCastAssetManager::LocateAsset()
{
	QTreeWidgetItem *item = SelectedItem();
	if (!item)
		return;

	if (item->data(ColSource, KindRole).toInt() == int(AssetKind::Font)) {
		ChangeFont();
		return;
	}

	const QString uuid = UuidOf(item);
	const QString key = item->data(ColSource, KeyRole).toString();
	const int index = item->data(ColSource, IndexRole).toInt();
	const QString oldPath = item->data(ColSource, ValueRole).toString();

	QString filter;
	switch (item->data(ColSource, FilterRole).toInt()) {
	case FilterImages:
		filter = QTStr("Basic.AssetManager.Filter.Images");
		break;
	case FilterMedia:
		filter = QTStr("Basic.AssetManager.Filter.Media");
		break;
	case FilterPages:
		filter = QTStr("Basic.AssetManager.Filter.Pages");
		break;
	default:
		filter = QTStr("Basic.AssetManager.Filter.All");
		break;
	}

	const QString dir = QFileInfo(oldPath).absolutePath();
	const QString path = QFileDialog::getOpenFileName(
		this, QTStr("Basic.AssetManager.Locate.Title"), dir, filter);
	if (path.isEmpty())
		return;

	obs_source_t *source = obs_get_source_by_uuid(QT_TO_UTF8(uuid));
	if (!source)
		return;

	OBSDataAutoRelease settings = obs_source_get_settings(source);
	const QByteArray keyBytes = key.toUtf8();

	if (index >= 0) {
		OBSDataArrayAutoRelease array = obs_data_get_array(settings, keyBytes.constData());
		if (array && size_t(index) < obs_data_array_count(array)) {
			OBSDataAutoRelease entry = obs_data_array_item(array, index);
			obs_data_set_string(entry, "value", QT_TO_UTF8(path));
			obs_data_array_erase(array, index);
			obs_data_array_insert(array, index, entry);
			obs_data_set_array(settings, keyBytes.constData(), array);
		}
	} else {
		obs_data_set_string(settings, keyBytes.constData(), QT_TO_UTF8(path));
	}

	obs_source_update(source, settings);
	obs_source_release(source);

	emit assetsChanged();
	Scan();
}

void LumaCastAssetManager::ChangeFont()
{
	QTreeWidgetItem *item = SelectedItem();
	if (!item || item->data(ColSource, KindRole).toInt() != int(AssetKind::Font))
		return;

	const QString oldFace = item->data(ColSource, ValueRole).toString();
	bool ok = false;
	const QFont font = QFontDialog::getFont(&ok, QFont(oldFace), this,
						QTStr("Basic.AssetManager.ChangeFont"));
	if (!ok)
		return;

	const QString face = font.family();
	if (face.isEmpty() || face == oldFace)
		return;

	obs_source_t *source = obs_get_source_by_uuid(QT_TO_UTF8(UuidOf(item)));
	if (!source)
		return;

	OBSDataAutoRelease settings = obs_source_get_settings(source);
	OBSDataAutoRelease fontObj = obs_data_get_obj(settings, "font");
	if (fontObj) {
		obs_data_set_string(fontObj, "face", QT_TO_UTF8(face));
		obs_data_set_obj(settings, "font", fontObj);
	}
	obs_source_update(source, settings);
	obs_source_release(source);

	emit assetsChanged();
	Scan();
}

void LumaCastAssetManager::RevealAsset()
{
	QTreeWidgetItem *item = SelectedItem();
	if (!item)
		return;

	const QString path = item->data(ColSource, ValueRole).toString();
	if (path.isEmpty() || IsRemotePath(path))
		return;

#ifdef Q_OS_WIN
	QProcess::startDetached(QStringLiteral("explorer.exe"),
				{QStringLiteral("/select,") + QDir::toNativeSeparators(path)});
#else
	QProcess::startDetached(QStringLiteral("xdg-open"), {QFileInfo(path).absolutePath()});
#endif
}
