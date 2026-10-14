#include "MainWindow.h"
#include "MainWindowInternal.h"
#include "GraphView.h"
#include "Theme.h"
#include "VDtoJson.h"

#include <QJsonObject>
#include <QTreeWidget>
#include <QTreeWidgetItem>

using namespace MainWindowUtil;

void MainWindow::RegisterType()
{
	handlers_.Subscribe(Action::GetPalette, Handler<Message>(std::bind(&MainWindow::GetPalette, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::GenerateCode, Handler<Message>(std::bind(&MainWindow::GenerateCode, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::CompileType, Handler<Message>(std::bind(&MainWindow::CompileType, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::LoadType, Handler<Message>(std::bind(&MainWindow::LoadType, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::UnloadType, Handler<Message>(std::bind(&MainWindow::UnloadType, this, std::placeholders::_1)));
}

void MainWindow::GetPalette(const Message& msg)
{
	Palette dto;
	if (ParsePalette(MessageText(msg), dto))
		ApplyPaletteDto(dto);
}

void MainWindow::GenerateCode(const Message&)
{
	Dispatch(Action::GetBlueprint);
	Dispatch(Action::GetPalette);
}

void MainWindow::CompileType(const Message&)
{
	Dispatch(Action::GetPalette);
}

void MainWindow::LoadType(const Message&)
{
	Dispatch(Action::GetPalette);
}

void MainWindow::UnloadType(const Message&)
{
	Dispatch(Action::GetPalette);
}

