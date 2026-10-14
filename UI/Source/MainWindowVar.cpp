#include "MainWindow.h"
#include "MainWindowInternal.h"
#include "GraphView.h"
#include "Theme.h"
#include "VDtoJson.h"

#include <QJsonObject>
#include <QTreeWidget>
#include <QTreeWidgetItem>

#include <algorithm>

using namespace MainWindowUtil;

void MainWindow::RegisterVar()
{
	handlers_.Subscribe(Action::AddVar, Handler<Message>(std::bind(&MainWindow::AddVar, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::DelVar, Handler<Message>(std::bind(&MainWindow::DelVar, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::RenameVar, Handler<Message>(std::bind(&MainWindow::RenameVar, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::SetVarType, Handler<Message>(std::bind(&MainWindow::SetVarType, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::SetVarValue, Handler<Message>(std::bind(&MainWindow::SetVarValue, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::SelectVar, Handler<Message>(std::bind(&MainWindow::SelectVar, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::PlaceVarGet, Handler<Message>(std::bind(&MainWindow::PlaceGet, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::PlaceVarSet, Handler<Message>(std::bind(&MainWindow::PlaceSet, this, std::placeholders::_1)));
}

void MainWindow::AddVar(const Message& msg)
{
	const auto obj = MsgJson(msg);
	const QString guid = obj.value(QStringLiteral("guid")).toString();
	const QString name = obj.value(QStringLiteral("name")).toString();
	const QString type = obj.value(QStringLiteral("type")).toString();
	if (guid.isEmpty() || !var_tree_)
		return;
	if (FindTreeItem(var_tree_, guid))
		return;
	VarItem item;
	item.guid = guid.toStdString();
	item.name = name.toStdString();
	item.type_name = type.toStdString();
	blueprint_dto_.vars.push_back(item);
	auto* row = new QTreeWidgetItem(QStringList() << name);
	row->setFlags(row->flags() | Qt::ItemIsEditable);
	row->setData(0, Qt::UserRole, guid);
	row->setData(0, Qt::UserRole + 1, name);
	row->setForeground(0, QBrush(PinColor(type, PinKind::Data)));
	row->setToolTip(0, QStringLiteral("变量  %1 : %2\nF2 重命名，Delete 删除").arg(name, type));
	var_tree_->addTopLevelItem(row);
	var_tree_->setCurrentItem(row);
	if (pending_edit_var_) {
		pending_edit_var_ = false;
		EditNewItem(var_tree_, guid);
	}
}

void MainWindow::DelVar(const Message& msg)
{
	const QString guid = MsgJson(msg).value(QStringLiteral("guid")).toString();
	RemoveTreeItem(var_tree_, guid);
	blueprint_dto_.vars.erase(
		std::remove_if(blueprint_dto_.vars.begin(), blueprint_dto_.vars.end(),
			[&](const VarItem& v) { return v.guid == guid.toStdString(); }),
		blueprint_dto_.vars.end());
	Dispatch(Action::GetDetails);
}

void MainWindow::RenameVar(const Message& msg)
{
	const auto obj = MsgJson(msg);
	const QString guid = obj.value(QStringLiteral("guid")).toString();
	const QString name = obj.value(QStringLiteral("name")).toString();
	RenameTreeItem(var_tree_, guid, name);
	if (auto* item = FindTreeItem(var_tree_, guid)) {
		const QString type_name = [&]() {
			for (const auto& v : blueprint_dto_.vars) {
				if (v.guid == guid.toStdString())
					return QString::fromStdString(v.type_name);
			}
			return QString();
		}();
		item->setToolTip(0, QStringLiteral("变量  %1 : %2\nF2 重命名，Delete 删除").arg(name, type_name));
	}
	for (auto& v : blueprint_dto_.vars) {
		if (v.guid == guid.toStdString())
			v.name = name.toStdString();
	}
	Dispatch(Action::GetGraph);
}

void MainWindow::SetVarType(const Message& msg)
{
	const auto obj = MsgJson(msg);
	const QString guid = obj.value(QStringLiteral("guid")).toString();
	const QString type = obj.value(QStringLiteral("type")).toString();
	for (auto& v : blueprint_dto_.vars) {
		if (v.guid != guid.toStdString())
			continue;
		v.type_name = type.toStdString();
		if (auto* item = FindTreeItem(var_tree_, guid)) {
			item->setForeground(0, QBrush(PinColor(type, PinKind::Data)));
			item->setToolTip(0, QStringLiteral("变量  %1 : %2\nF2 重命名，Delete 删除")
				.arg(QString::fromStdString(v.name), type));
		}
		break;
	}
	Dispatch(Action::GetGraph);
	Dispatch(Action::GetDetails);
}

void MainWindow::SetVarValue(const Message&)
{
	Dispatch(Action::GetDetails);
}

void MainWindow::SelectVar(const Message&)
{
	Dispatch(Action::GetDetails);
}

void MainWindow::PlaceGet(const Message& msg)
{
	AddNode(msg);
}

void MainWindow::PlaceSet(const Message& msg)
{
	AddNode(msg);
}

