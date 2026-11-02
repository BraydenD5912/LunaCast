#include "SourceTabBar.hpp"

#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>

SourceTabBar::SourceTabBar(QWidget *parent) : QTabBar(parent)
{
	setAcceptDrops(true);
}

static bool isSourceDrag(const QMimeData *mime)
{
	return mime && mime->hasFormat(QStringLiteral("application/x-qabstractitemmodeldatalist"));
}

void SourceTabBar::dragEnterEvent(QDragEnterEvent *event)
{
	if (isSourceDrag(event->mimeData()))
		event->acceptProposedAction();
	else
		QTabBar::dragEnterEvent(event);
}

void SourceTabBar::dragMoveEvent(QDragMoveEvent *event)
{
	if (isSourceDrag(event->mimeData()))
		event->acceptProposedAction();
	else
		QTabBar::dragMoveEvent(event);
}

void SourceTabBar::dropEvent(QDropEvent *event)
{
	if (!isSourceDrag(event->mimeData())) {
		QTabBar::dropEvent(event);
		return;
	}

	const int index = tabAt(event->position().toPoint());
	if (index < 0) {
		event->ignore();
		return;
	}

	/* The drag comes from the source list's internal move. Accepting it as a
	 * copy keeps Qt from removing the dragged rows afterwards. */
	event->setDropAction(Qt::CopyAction);
	event->accept();
	emit sourceDropped(index);
}
