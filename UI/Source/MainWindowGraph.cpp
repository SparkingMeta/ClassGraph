#include "MainWindow.h"
#include "MainWindowInternal.h"
#include "GraphView.h"
#include "Theme.h"
#include "VDtoJson.h"

#include <QJsonObject>
#include <QTreeWidget>
#include <QTreeWidgetItem>

using namespace MainWindowUtil;

void MainWindow::RegisterGraph()
{
	handlers_.Subscribe(Action::GetDetails, Handler<Message>(std::bind(&MainWindow::GetDetails, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::AddNode, Handler<Message>(std::bind(&MainWindow::AddNode, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::DelNode, Handler<Message>(std::bind(&MainWindow::DelNode, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::AddExitNode, Handler<Message>(std::bind(&MainWindow::AddExit, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::ConnectPin, Handler<Message>(std::bind(&MainWindow::ConnectPin, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::DisconnectPin, Handler<Message>(std::bind(&MainWindow::DisconnectPin, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::DisconnectLine, Handler<Message>(std::bind(&MainWindow::DisconnectLine, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::SetPinValue, Handler<Message>(std::bind(&MainWindow::SetPinValue, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::SelectNode, Handler<Message>(std::bind(&MainWindow::SelectNode, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::AddParam, Handler<Message>(std::bind(&MainWindow::AddParam, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::DelParam, Handler<Message>(std::bind(&MainWindow::DelParam, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::RenameParam, Handler<Message>(std::bind(&MainWindow::RenameParam, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::SetParamType, Handler<Message>(std::bind(&MainWindow::SetParamType, this, std::placeholders::_1)));
}

void MainWindow::GetDetails(const Message& msg)
{
	ApplyDetails(QString::fromStdString(MessageText(msg)));
}

void MainWindow::AddNode(const Message& msg)
{
	Node node;
	if (!ParseNodeDto(MessageText(msg), node))
		return;
	const QString flow = MsgJson(msg).value(QStringLiteral("flow_guid")).toString();
	GraphView* view = GraphViewOf(flow);
	if (!view)
		view = CurrentGraphView();
	if (view)
		view->UpsertNode(node);
	Dispatch(Action::GetDetails);
}

void MainWindow::DelNode(const Message& msg)
{
	const QString guid = MsgJson(msg).value(QStringLiteral("guid")).toString();
	for (auto& [flow, view] : graph_views_) {
		if (view)
			view->RemoveNode(guid);
	}
	Dispatch(Action::GetDetails);
}

void MainWindow::AddExit(const Message& msg)
{
	AddNode(msg);
}

void MainWindow::ConnectPin(const Message& msg)
{
	Line line;
	if (!ParseLineDto(MessageText(msg), line))
		return;
	const QString flow = MsgJson(msg).value(QStringLiteral("flow_guid")).toString();
	GraphView* view = GraphViewOf(flow);
	if (!view)
		view = CurrentGraphView();
	if (view)
		view->UpsertLine(line);
}

void MainWindow::DisconnectPin(const Message& msg)
{
	const QString guid = MsgJson(msg).value(QStringLiteral("guid")).toString();
	for (auto& [flow, view] : graph_views_) {
		if (view)
			view->RemoveLinesOfPin(guid);
	}
}

void MainWindow::DisconnectLine(const Message& msg)
{
	const QString guid = MsgJson(msg).value(QStringLiteral("guid")).toString();
	for (auto& [flow, view] : graph_views_) {
		if (view)
			view->RemoveLine(guid);
	}
}

void MainWindow::SetPinValue(const Message&)
{
	Dispatch(Action::GetDetails);
}

void MainWindow::SelectNode(const Message&)
{
	Dispatch(Action::GetDetails);
}

void MainWindow::AddParam(const Message&)
{
	Dispatch(Action::GetGraph);
	Dispatch(Action::GetDetails);
}

void MainWindow::DelParam(const Message&)
{
	Dispatch(Action::GetGraph);
	Dispatch(Action::GetDetails);
}

void MainWindow::RenameParam(const Message&)
{
	Dispatch(Action::GetGraph);
	Dispatch(Action::GetDetails);
}

void MainWindow::SetParamType(const Message&)
{
	Dispatch(Action::GetGraph);
	Dispatch(Action::GetDetails);
}

