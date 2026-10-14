#include "MainWindow.h"
#include "MainWindowInternal.h"
#include "GraphView.h"
#include "Theme.h"
#include "VDtoJson.h"

#include <QJsonObject>
#include <QTreeWidget>
#include <QTreeWidgetItem>

using namespace MainWindowUtil;

void MainWindow::RegisterAsset()
{
	handlers_.Subscribe(Action::GetBlueprint, Handler<Message>(std::bind(&MainWindow::GetBlueprint, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::GetAssets, Handler<Message>(std::bind(&MainWindow::GetAssets, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::LoadAssets, Handler<Message>(std::bind(&MainWindow::LoadAssets, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::OpenBP, Handler<Message>(std::bind(&MainWindow::OpenBP, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::CloseBP, Handler<Message>(std::bind(&MainWindow::CloseBP, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::RenameBP, Handler<Message>(std::bind(&MainWindow::RenameBP, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::SetSpace, Handler<Message>(std::bind(&MainWindow::SetSpace, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::CreateBP, Handler<Message>(std::bind(&MainWindow::CreateBP, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::DeleteBP, Handler<Message>(std::bind(&MainWindow::DeleteBP, this, std::placeholders::_1)));
}

void MainWindow::GetBlueprint(const Message& msg)
{
	Blueprint dto;
	if (ParseBlueprint(MessageText(msg), dto))
		ApplyBlueprintDto(dto);
}

void MainWindow::GetAssets(const Message& msg)
{
	AssetList dto;
	if (ParseAssets(MessageText(msg), dto))
		ApplyAssetListDto(dto);
}

void MainWindow::CreateBP(const Message& msg)
{
	const auto obj = MsgJson(msg);
	const QString guid = obj.value(QStringLiteral("guid")).toString();
	const QString name = obj.value(QStringLiteral("name")).toString();
	const QString space = obj.value(QStringLiteral("space")).toString();
	if (guid.isEmpty() || !asset_tree_)
		return;
	if (FindTreeItem(asset_tree_, guid))
		return;
	auto* row = new QTreeWidgetItem(QStringList() << name);
	row->setFlags(row->flags() & ~Qt::ItemIsEditable);
	row->setData(0, Qt::UserRole, guid);
	if (auto* folder = EnsureAssetFolder(asset_tree_, space))
		folder->addChild(row);
	else
		asset_tree_->addTopLevelItem(row);
}

void MainWindow::DeleteBP(const Message& msg)
{
	const QString guid = MsgJson(msg).value(QStringLiteral("guid")).toString();
	auto* item = FindTreeItem(asset_tree_, guid);
	if (!item)
		return;
	auto* folder = item->parent();
	delete item;
	if (folder && folder->childCount() == 0 && folder->data(0, Qt::UserRole).toString().isEmpty())
		delete folder;
	if (guid == current_asset_guid_)
		CloseBP(msg);
}

void MainWindow::LoadAssets(const Message& msg)
{
	GetAssets(msg);
}

void MainWindow::OpenBP(const Message& msg)
{
	const auto obj = MsgJson(msg);
	current_asset_guid_ = obj.value(QStringLiteral("guid")).toString();
	Dispatch(Action::GetBlueprint);
	Dispatch(Action::GetPalette);
	Dispatch(Action::GetGraph);
	Dispatch(Action::GetDetails);
}

void MainWindow::CloseBP(const Message&)
{
	current_asset_guid_.clear();
	has_blueprint_ = false;
	blueprint_dto_ = {};
	graph_dto_.reset();
	last_node_details_.reset();
	selected_var_guid_.clear();
	if (function_tree_)
		function_tree_->clear();
	if (var_tree_)
		var_tree_->clear();
	ClearGraphViews();
	if (graph_tabs_) {
		const bool blocked = graph_tabs_->blockSignals(true);
		while (graph_tabs_->count() > 0)
			graph_tabs_->removeTab(0);
		graph_tabs_->blockSignals(blocked);
	}
	SetEditorVisible(false);
	UpdateBreadcrumb();
}

void MainWindow::RenameBP(const Message& msg)
{
	SetSpace(msg);
}

void MainWindow::SetSpace(const Message& msg)
{
	const auto obj = MsgJson(msg);
	const QString guid = obj.value(QStringLiteral("guid")).toString();
	const QString name = obj.value(QStringLiteral("name")).toString();
	const QString space = obj.value(QStringLiteral("space")).toString();
	if (!name.isEmpty())
		blueprint_dto_.name = name.toStdString();
	if (!space.isEmpty())
		blueprint_dto_.space = space.toStdString();
	UpdateBreadcrumb();
	if (!guid.isEmpty() && guid != current_asset_guid_) {
		if (auto* item = FindTreeItem(asset_tree_, current_asset_guid_)) {
			auto* folder = item->parent();
			delete item;
			if (folder && folder->childCount() == 0 && folder->data(0, Qt::UserRole).toString().isEmpty())
				delete folder;
		}
		current_asset_guid_ = guid;
		if (asset_tree_ && !FindTreeItem(asset_tree_, guid)) {
			const QString row_name = name.isEmpty() ? QString::fromStdString(blueprint_dto_.name) : name;
			const QString row_space = space.isEmpty() ? QString::fromStdString(blueprint_dto_.space) : space;
			auto* row = new QTreeWidgetItem(QStringList() << row_name);
			row->setFlags(row->flags() & ~Qt::ItemIsEditable);
			row->setData(0, Qt::UserRole, guid);
			if (auto* folder = EnsureAssetFolder(asset_tree_, row_space))
				folder->addChild(row);
			else
				asset_tree_->addTopLevelItem(row);
		}
	}
	Dispatch(Action::GetBlueprint);
}

