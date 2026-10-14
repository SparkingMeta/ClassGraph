#pragma execution_character_set("utf-8")

#include "Meta.h"
#include "Engine.h"
#include "queue.h"
#include "event.h"
#include "bus.h"
#include "worker.h"

#include "MainWindow.h"
#include <QApplication>
#include <QDir>
#include <QStyleFactory>

int main(int argc, char* argv[])
{
	QApplication app(argc, argv);
	QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

	cg::model::Model model;
	cg::engine::Engine engine;
	engine.SetRoot(QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("Assets")).toStdString());
	engine.SetTypeDir(QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("Types")).toStdString());

	auto bus = CreateBus<Event>();

	auto sink_queue = CreateQueue<Event>();
	auto rise_queue = CreateQueue<Event>();

	sink_queue->Start();
	rise_queue->Start();

	auto sink_worker = CreateWorker(
		[&bus, &sink_queue] {
			auto event = sink_queue->WaitPop();
			if (!event) return false;
			bus->Publish(*event);
			return true;
		},
		[&] { sink_queue->Stop(); });

	auto rise_worker = CreateWorker(
		[&bus, &rise_queue] {
			auto event = rise_queue->WaitPop();
			if (!event) return false;
			bus->Publish(*event);
			return true;
		},
		[&] { rise_queue->Stop(); });

	sink_worker->Start();
	rise_worker->Start();

	MainWindow window;

	bus->Subscribe(EventId::EI_Sink, Handler<Message>{[&engine](const Message& msg) {
		engine.Apply(msg);
	}});
	bus->Subscribe(EventId::EI_Rise, Handler<Message>{[&model](const Message& msg) {
		model.Reply(msg);
	}});

	model.SetRise([&](const Message& msg) {
		window.Notify(msg);
	});

	engine.SetRise([&model](const Message& msg) {
		model.Reply(msg);
	});
	engine.LoadTypes();
	engine.SetRise([&rise_queue](const Message& msg) {
		rise_queue->Push(Event{ EventId::EI_Rise, msg });
	});

	window.SetSink([&](const Message& msg) {
		model.Submit(msg);
		sink_queue->Push(Event{ EventId::EI_Sink, msg });
	});

	window.showMaximized();

	const int code = app.exec();

	sink_worker->Stop();
	rise_worker->Stop();
	model.SetRise(nullptr);
	return code;
}
