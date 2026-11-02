#pragma once

#include <QDialog>

class QLabel;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;
struct obs_source;

class LumaCastAssetManager : public QDialog {
	Q_OBJECT

public:
	explicit LumaCastAssetManager(QWidget *parent = nullptr);

signals:
	void assetsChanged();

private:
	void Scan();
	void ScanSource(obs_source *source);
	void AddFileAsset(const QString &uuid, const QString &sourceName, const QString &type,
			  const QString &path, const QString &key, int index, int filter);
	void AddFontAsset(const QString &uuid, const QString &sourceName, const QString &type,
			  const QString &face);
	void SetStatus(QTreeWidgetItem *item, const QString &text, bool ok);
	void UpdateSummary();
	void UpdateActionState();
	void LocateAsset();
	void ChangeFont();
	void RevealAsset();
	QTreeWidgetItem *SelectedItem() const;
	QString UuidOf(QTreeWidgetItem *item) const;

	QTreeWidget *tree = nullptr;
	QLabel *summary = nullptr;
	QPushButton *locateButton = nullptr;
	QPushButton *revealButton = nullptr;
	QPushButton *refreshButton = nullptr;

	int assetCount = 0;
	int missingCount = 0;
};
