#include "MainWindow.h"
#include "MainWindowInternal.h"
#include "GraphView.h"
#include "Theme.h"
#include "VDtoJson.h"

#include <QJsonObject>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QBrush>
#include <QColor>

#include <algorithm>

using namespace MainWindowUtil;

void MainWindow::RegisterFlow()
{
	handlers_.Subscribe(Action::GetGraph, Handler<Message>(std::bind(&MainWindow::GetGraph, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::AddFlow, Handler<Message>(std::bind(&MainWindow::AddFlow, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::DelFlow, Handler<Message>(std::bind(&MainWindow::DelFlow, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::RenameFlow, Handler<Message>(std::bind(&MainWindow::RenameFlow, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::SelectFlow, Handler<Message>(std::bind(&MainWindow::SelectFlow, this, std::placeholders::_1)));
}

void MainWindow::GetGraph(const Message& msg)
{
	Graph dto;
	if (ParseGraph(MessageText(msg), dto))
		ApplyGraphDto(dto);
}

void MainWindow::AddFlow(const Message& msg)
{
	const auto obj = MsgJson(msg);
	const QString guid = obj.value(QStringLiteral("guid")).toString();
	const QString name = obj.value(QStringLiteral("name")).toString();
	if (guid.isEmpty() || !function_tree_)
		return;
	if (!FindTreeItem(function_tree_, guid)) {
		FlowItem item;
		item.guid = guid.toStdString();
		item.name = name.toStdString();
		blueprint_dto_.flows.push_back(item);
		auto* row = new QTreeWidgetItem(QStringList() << name);
		row->setFlags(row->flags() | Qt::ItemIsEditable);
		row->setData(0, Qt::UserRole, guid);
		row->setData(0, Qt::UserRole + 1, name);
		row->setForeground(0, QBrush(QColor(140, 210, 140)));
		row->setToolTip(0, QStringLiteral("函数  %1\n拖到图表可放置调用节点\nF2 重命名，Delete 删除").arg(name));
		function_tree_->addTopLevelItem(row);
		function_tree_->setCurrentItem(row);
	}
	blueprint_dto_.current_flow_guid = guid.toStdString();
	blueprint_dto_.current_flow_name = name.toStdString();
	RefreshGraphTabs();
	UpdateBreadcrumb();
	if (pending_edit_flow_) {
		pending_edit_flow_ = false;
		EditNewItem(function_tree_, guid);
	}
	Dispatch(Action::GetPalette);
	ShowFlowView(guid, true);
	Dispatch(Action::GetDetails);
}

void MainWindow::DelFlow(const Message& msg)
{
	const auto obj = MsgJson(msg);
	const QString guid = obj.value(QStringLiteral("guid")).toString();
	RemoveTreeItem(function_tree_, guid);
	blueprint_dto_.flows.erase(
		std::remove_if(blueprint_dto_.flows.begin(), blueprint_dto_.flows.end(),
			[&](const FlowItem& f) { return f.guid == guid.toStdString(); }),
		blueprint_dto_.flows.end());
	const QString current = obj.value(QStringLiteral("current")).toString();
	if (!current.isEmpty()) {
		blueprint_dto_.current_flow_guid = current.toStdString();
		for (const auto& f : blueprint_dto_.flows) {
			if (f.guid == current.toStdString()) {
				blueprint_dto_.current_flow_name = f.name;
				break;
			}
		}
	}
	RefreshGraphTabs();
	UpdateBreadcrumb();
	Dispatch(Action::GetPalette);
	ShowFlowView(QString::fromStdString(blueprint_dto_.current_flow_guid), true);
	Dispatch(Action::GetDetails);
}

void MainWindow::RenameFlow(const Message& msg)
{
	const auto obj = MsgJson(msg);
	const QString guid = obj.value(QStringLiteral("guid")).toString();
	const QString name = obj.value(QStringLiteral("name")).toString();
	RenameTreeItem(function_tree_, guid, name);
	if (auto* item = FindTreeItem(function_tree_, guid))
		item->setToolTip(0, QStringLiteral("函数  %1\n拖到图表可放置调用节点\nF2 重命名，Delete 删除").arg(name));
	for (auto& f : blueprint_dto_.flows) {
		if (f.guid != guid.toStdString())
			continue;
		f.name = name.toStdString();
		if (blueprint_dto_.current_flow_guid == f.guid)
			blueprint_dto_.current_flow_name = f.name;
		break;
	}
	RefreshGraphTabs();
	UpdateBreadcrumb();
	Dispatch(Action::GetPalette);
}

void MainWindow::SelectFlow(const Message& msg)
{
	const auto obj = MsgJson(msg);
	const QString guid = obj.value(QStringLiteral("guid")).toString();
	const QString name = obj.value(QStringLiteral("name")).toString();
	blueprint_dto_.current_flow_guid = guid.toStdString();
	if (!name.isEmpty())
		blueprint_dto_.current_flow_name = name.toStdString();
	RefreshGraphTabs();
	UpdateBreadcrumb();
	ShowFlowView(guid, true);
	Dispatch(Action::GetDetails);
}

