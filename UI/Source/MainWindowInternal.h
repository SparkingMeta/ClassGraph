#pragma once
#pragma execution_character_set("utf-8")

#include "MainWindow.h"
#include "Theme.h"
#include "IRouter.h"
#include "VDtoJson.h"

#include <QBrush>
#include <QColor>
#include <QComboBox>
#include <QDockWidget>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMainWindow>
#include <QString>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QWidget>

#include <functional>

using namespace CG::Ui;

namespace MainWindowUtil {

	inline QTreeWidgetItem* FindTreeItem(QTreeWidget* tree, const QString& guid)
	{
		if (!tree || guid.isEmpty())
			return nullptr;
		std::function<QTreeWidgetItem*(QTreeWidgetItem*)> walk = [&](QTreeWidgetItem* item) -> QTreeWidgetItem* {
			if (!item)
				return nullptr;
			if (item->data(0, Qt::UserRole).toString() == guid)
				return item;
			for (int i = 0; i < item->childCount(); ++i) {
				if (auto* found = walk(item->child(i)))
					return found;
			}
			return nullptr;
		};
		for (int i = 0; i < tree->topLevelItemCount(); ++i) {
			if (auto* found = walk(tree->topLevelItem(i)))
				return found;
		}
		return nullptr;
	}

	inline QTreeWidgetItem* EnsureAssetFolder(QTreeWidget* tree, const QString& space)
	{
		if (!tree || space.isEmpty())
			return nullptr;
		for (int i = 0; i < tree->topLevelItemCount(); ++i) {
			auto* folder = tree->topLevelItem(i);
			if (folder && folder->data(0, Qt::UserRole).toString().isEmpty() && folder->text(0) == space)
				return folder;
		}
		auto* folder = new QTreeWidgetItem(QStringList() << space);
		folder->setFlags(folder->flags() & ~Qt::ItemIsEditable);
		folder->setForeground(0, TextMuted());
		tree->addTopLevelItem(folder);
		folder->setExpanded(true);
		return folder;
	}

	inline void RemoveTreeItem(QTreeWidget* tree, const QString& guid)
	{
		if (auto* item = FindTreeItem(tree, guid))
			delete item;
	}

	inline void RenameTreeItem(QTreeWidget* tree, const QString& guid, const QString& name)
	{
		auto* item = FindTreeItem(tree, guid);
		if (!item)
			return;
		item->setText(0, name);
		item->setData(0, Qt::UserRole + 1, name);
	}

	inline QJsonObject MsgJson(const Message& msg)
	{
		return QJsonDocument::fromJson(QString::fromStdString(MessageText(msg)).toUtf8()).object();
	}

}
