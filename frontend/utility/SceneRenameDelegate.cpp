/******************************************************************************
    Copyright (C) 2023 by Lain Bailey <lain@obsproject.com>
                          Zachary Lund <admin@computerquip.com>
                          Philippe Groarke <philippe.groarke@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include "SceneRenameDelegate.hpp"

#include <QApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QStyle>

#include <components/SceneTree.hpp>

#include "moc_SceneRenameDelegate.cpp"

SceneRenameDelegate::SceneRenameDelegate(QObject *parent) : QStyledItemDelegate(parent) {}

void SceneRenameDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
				const QModelIndex &index) const
{
	const QString folder = index.data(SceneTree::FolderMembershipRole).toString();
	auto *view = qobject_cast<QListWidget *>(parent());

	if (folder.isEmpty() || !view || view->viewMode() != QListView::ListMode) {
		QStyledItemDelegate::paint(painter, option, index);
		return;
	}

	QStyleOptionViewItem opt = option;
	initStyleOption(&opt, index);

	QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
	style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);

	painter->save();
	QColor line = opt.palette.color(opt.state & QStyle::State_Enabled ? QPalette::Normal
									 : QPalette::Disabled,
					QPalette::Mid);
	line.setAlpha(140);
	painter->setPen(QPen(line, 1));
	const int x = option.rect.left() + 9;
	painter->drawLine(x, option.rect.top() + 2, x, option.rect.bottom() - 2);
	painter->restore();

	QStyleOptionViewItem content = opt;
	content.rect = option.rect.adjusted(18, 0, 0, 0);
	QStyledItemDelegate::paint(painter, content, index);
}

void SceneRenameDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
	QStyledItemDelegate::setEditorData(editor, index);
	QLineEdit *lineEdit = qobject_cast<QLineEdit *>(editor);
	if (lineEdit) {
		lineEdit->selectAll();
	}
}

bool SceneRenameDelegate::eventFilter(QObject *editor, QEvent *event)
{
	if (event->type() == QEvent::KeyPress) {
		QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
		switch (keyEvent->key()) {
		case Qt::Key_Escape: {
			QLineEdit *lineEdit = qobject_cast<QLineEdit *>(editor);
			if (lineEdit) {
				lineEdit->undo();
			}
			break;
		}
		case Qt::Key_Tab:
		case Qt::Key_Backtab:
			return false;
		}
	}

	return QStyledItemDelegate::eventFilter(editor, event);
}
