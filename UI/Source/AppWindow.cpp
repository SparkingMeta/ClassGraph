#pragma execution_character_set("utf-8")

#include "AppWindow.h"
#include "MainWindow.h"

#include <QApplication>
#include <QStyleFactory>

int ui::Application::Run(int argc, char* argv[])
{

	QApplication app(argc, argv);
	QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

	MainWindow window;
	window.showMaximized();

	const int code = app.exec();
	return code;
}
