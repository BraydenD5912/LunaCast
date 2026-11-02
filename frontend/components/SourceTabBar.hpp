#pragma once

#include <QTabBar>

class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;

class SourceTabBar : public QTabBar {
	Q_OBJECT

public:
	explicit SourceTabBar(QWidget *parent = nullptr);

signals:
	void sourceDropped(int index);

protected:
	virtual void dragEnterEvent(QDragEnterEvent *event) override;
	virtual void dragMoveEvent(QDragMoveEvent *event) override;
	virtual void dropEvent(QDropEvent *event) override;
};
