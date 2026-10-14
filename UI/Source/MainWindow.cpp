#pragma execution_character_set("utf-8")

#include "MainWindow.h"
#include "MainWindowInternal.h"
#include "GraphView.h"
#include "Theme.h"

#include "IRouter.h"
#include "VDto.h"
#include "VDtoJson.h"

#include <QAction>
#include <QAbstractItemView>
#include <QApplication>
#include <QBrush>
#include <QCheckBox>
#include <QComboBox>
#include <QCursor>
#include <QDrag>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QEvent>
#include <QEventLoop>
#include <QFontMetrics>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMimeData>
#include <QPainter>
#include <QPixmap>
#include <QPointer>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTabBar>
#include <QTextEdit>
#include <QThread>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QUuid>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace {

	QDockWidget* MakeDock(const QString& title, QWidget* widget, QMainWindow* parent)
	{
		auto* dock = new QDockWidget(title, parent);
		dock->setWidget(widget);
		dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
		dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea);
		return dock;
	}

	void FillTypeCombo(QComboBox* combo, const std::vector<TypeItem>& types, const QString& current)
	{
		if (!combo) return;
		const QSignalBlocker blocker(combo);
		combo->clear();
		for (const auto& t : types) {
			if (!t.name.empty())
				combo->addItem(QString::fromStdString(t.name));
		}
		if (combo->count() == 0)
			combo->addItem(QStringLiteral("int"));
		const int idx = combo->findText(current);
		if (idx >= 0)
			combo->setCurrentIndex(idx);
		else if (!current.isEmpty()) {
			combo->addItem(current);
			combo->setCurrentIndex(combo->count() - 1);
		}
	}

	void FillTypeCombo(QComboBox* combo, const std::vector<std::string>& types, const QString& current)
	{
		if (!combo) return;
		const QSignalBlocker blocker(combo);
		combo->clear();
		for (const auto& t : types) {
			if (!t.empty())
				combo->addItem(QString::fromStdString(t));
		}
		if (combo->count() == 0)
			combo->addItem(QStringLiteral("int"));
		const int idx = combo->findText(current);
		if (idx >= 0)
			combo->setCurrentIndex(idx);
		else if (!current.isEmpty()) {
			combo->addItem(current);
			combo->setCurrentIndex(combo->count() - 1);
		}
	}

	QWidget* MakeParamEditRow(QWidget* parent, const QString& name_text, const QString& placeholder,
		QLineEdit** name_out, QComboBox** type_out, QToolButton** btn_out, const QString& btn_text)
	{
		auto* row = new QWidget(parent);
		row->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
		auto* layout = new QHBoxLayout(row);
		layout->setContentsMargins(0, 0, 0, 0);
		layout->setSpacing(4);

		auto* name = new QLineEdit(name_text, row);
		name->setPlaceholderText(placeholder);
		name->setMinimumWidth(48);
		name->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);

		auto* type = new QComboBox(row);
		type->setObjectName(QStringLiteral("ParamTypeCombo"));
		type->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
		type->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
		type->setMinimumContentsLength(4);
		type->setFixedWidth(96);

		auto* btn = new QToolButton(row);
		btn->setObjectName(QStringLiteral("PlusButton"));
		btn->setText(btn_text);
		btn->setAutoRaise(false);
		btn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
		btn->setFixedSize(22, 22);

		layout->addWidget(name, 1);
		layout->addWidget(type, 0);
		layout->addWidget(btn, 0, Qt::AlignVCenter);

		if (name_out) *name_out = name;
		if (type_out) *type_out = type;
		if (btn_out) *btn_out = btn;
		return row;
	}

	QTreeWidget* MakeTree()
	{
		auto* tree = new QTreeWidget;
		tree->setHeaderHidden(true);
		tree->setIndentation(12);
		tree->setRootIsDecorated(false);
		tree->setAnimated(false);
		tree->setEditTriggers(QAbstractItemView::EditKeyPressed);
		tree->setContextMenuPolicy(Qt::CustomContextMenu);
		tree->setSelectionMode(QAbstractItemView::SingleSelection);
		return tree;
	}

	QWidget* MakeAddSection(const QString& title, QTreeWidget* tree,
		QToolButton** plus_out, QComboBox** type_out)
	{
		auto* root = new QWidget;
		auto* layout = new QVBoxLayout(root);
		layout->setContentsMargins(0, 0, 0, 0);
		layout->setSpacing(0);

		auto* header = new QWidget;
		header->setObjectName(QStringLiteral("SectionHeader"));
		auto* h = new QHBoxLayout(header);
		h->setContentsMargins(0, 2, 2, 2);
		h->setSpacing(4);

		auto* label = new QLabel(title);
		label->setObjectName(QStringLiteral("SectionTitle"));
		h->addWidget(label);
		h->addStretch();

		if (type_out) {
			auto* combo = new QComboBox;
			combo->setObjectName(QStringLiteral("VarTypeCombo"));
			combo->setToolTip(QStringLiteral("新变量类型"));
			*type_out = combo;
			h->addWidget(combo);
		}

		auto* plus = new QToolButton;
		plus->setObjectName(QStringLiteral("PlusButton"));
		plus->setText(QStringLiteral("+"));
		plus->setToolTip(QStringLiteral("添加%1").arg(title));
		plus->setAutoRaise(false);
		plus->setFixedSize(22, 22);
		*plus_out = plus;
		h->addWidget(plus, 0, Qt::AlignVCenter);

		layout->addWidget(header);
		layout->addWidget(tree, 1);
		return root;
	}

	class FunctionTreeWidget : public QTreeWidget {
	public:
		explicit FunctionTreeWidget(QWidget* parent = nullptr) : QTreeWidget(parent)
		{
			setHeaderHidden(true);
			setIndentation(12);
			setRootIsDecorated(false);
			setAnimated(false);
			setEditTriggers(QAbstractItemView::EditKeyPressed);
			setContextMenuPolicy(Qt::CustomContextMenu);
			setSelectionMode(QAbstractItemView::SingleSelection);
			setDragEnabled(true);
			setAcceptDrops(false);
			setDragDropMode(QAbstractItemView::DragOnly);
			setDefaultDropAction(Qt::CopyAction);
		}

	protected:
		QMimeData* mimeData(const QList<QTreeWidgetItem*> items) const override
		{
			auto* data = new QMimeData;
			if (!items.isEmpty()) {
				data->setData(kClassGraphFlowMime, items.front()->data(0, Qt::UserRole).toString().toUtf8());
				data->setText(items.front()->text(0));
			}
			return data;
		}
	};

	class VarTreeWidget : public QTreeWidget {
	public:
		explicit VarTreeWidget(QWidget* parent = nullptr) : QTreeWidget(parent)
		{
			setHeaderHidden(true);
			setIndentation(12);
			setRootIsDecorated(false);
			setAnimated(false);
			setEditTriggers(QAbstractItemView::EditKeyPressed);
			setContextMenuPolicy(Qt::CustomContextMenu);
			setSelectionMode(QAbstractItemView::SingleSelection);
			setDragEnabled(true);
			setAcceptDrops(false);
			setDragDropMode(QAbstractItemView::DragOnly);
			setDefaultDropAction(Qt::CopyAction);
		}

	protected:
		QMimeData* mimeData(const QList<QTreeWidgetItem*> items) const override
		{
			auto* data = new QMimeData;
			if (!items.isEmpty()) {
				data->setData(kClassGraphVarMime, items.front()->data(0, Qt::UserRole).toString().toUtf8());
				data->setText(items.front()->text(0));
			}
			return data;
		}

		void startDrag(Qt::DropActions supportedActions) override
		{
			const auto items = selectedItems();
			if (items.isEmpty())
				return;

			QMimeData* mime = mimeData(items);
			if (!mime)
				return;

			const QString text = items.front()->text(0);
			const QFontMetrics fm(font());
			constexpr int pad_x = 8;
			constexpr int pad_y = 3;
			const int w = qMax(48, fm.horizontalAdvance(text) + pad_x * 2);
			const int h = qMax(20, fm.height() + pad_y * 2);

			const qreal dpr = devicePixelRatioF();
			QPixmap pixmap(QSize(qRound(w * dpr), qRound(h * dpr)));
			pixmap.setDevicePixelRatio(dpr);
			pixmap.fill(Qt::transparent);

			QPainter painter(&pixmap);
			painter.setRenderHint(QPainter::Antialiasing);
			painter.setBrush(QColor(43, 43, 43, 235));
			painter.setPen(QPen(QColor(20, 20, 20), 1));
			painter.drawRoundedRect(QRectF(0.5, 0.5, w - 1.0, h - 1.0), 3, 3);
			painter.setPen(QColor(224, 224, 224));
			painter.setFont(font());
			painter.drawText(QRect(pad_x, 0, w - pad_x * 2, h), Qt::AlignVCenter | Qt::AlignLeft, text);
			painter.end();

			QDrag drag(this);
			drag.setMimeData(mime);
			drag.setPixmap(pixmap);
			drag.setHotSpot(QPoint(pad_x, h / 2));
			drag.exec(supportedActions, defaultDropAction());
		}
	};

} // namespace

using namespace CG::Ui;
using namespace MainWindowUtil;

MainWindow::MainWindow(QWidget* parent)
	: QMainWindow(parent)
{
	setStyleSheet(StyleSheet());
	BuildUi();
	RegisterAction();
	resize(1440, 900);
	setWindowTitle(QStringLiteral("ClassGraph"));
}

MainWindow::~MainWindow() = default;

void MainWindow::Dispatch(Action action, const QJsonObject& body)
{
	if (!sink_)
		return;
	const QString token = QUuid::createUuid().toString(QUuid::WithoutBraces);
	const QByteArray json = QJsonDocument(body).toJson(QJsonDocument::Compact);
	sink_(MakeMessage(action, token.toStdString(), json.toStdString()));
}

void MainWindow::SetSink(std::function<void(const Message&)> sink)
{
	sink_ = std::move(sink);
	for (auto& [guid, view] : graph_views_) {
		if (view)
			view->SetSink(sink_);
	}
	Dispatch(Action::LoadAssets);
}

void MainWindow::Notify(const Message& msg)
{
	if (QThread::currentThread() != thread()) {
		QTimer::singleShot(0, this, [this, msg]() { Notify(msg); });
		return;
	}

	handlers_.Publish({static_cast<Action>(msg.act), msg});
}

void MainWindow::RegisterAction()
{
	handlers_.Subscribe(Action::Print, Handler<Message>(std::bind(&MainWindow::Print, this, std::placeholders::_1)));
	RegisterAsset();
	RegisterFlow();
	RegisterVar();
	RegisterGraph();
	RegisterType();
}

void MainWindow::Print(const Message& msg)
{
	AppendLog(QString::fromStdString(MessageText(msg)));
}

void MainWindow::BuildUi()
{
	setDockOptions(AllowTabbedDocks | AllowNestedDocks | AnimatedDocks);
	setTabPosition(Qt::AllDockWidgetAreas, QTabWidget::North);

	auto* file_menu = menuBar()->addMenu(QStringLiteral("File"));
	file_menu->addAction(QStringLiteral("New Blueprint"), this, &MainWindow::OnNewBlueprint, QKeySequence::New);
	auto* edit_menu = menuBar()->addMenu(QStringLiteral("Edit"));
	edit_menu->addAction(QStringLiteral("Add Return Node"), this, &MainWindow::OnAddExit);
	auto* window_menu = menuBar()->addMenu(QStringLiteral("Window"));

	auto* toolbar = addToolBar(QStringLiteral("Blueprint"));
	toolbar->setMovable(false);
	toolbar->setIconSize(QSize(16, 16));

	compile_btn_ = new QToolButton;
	compile_btn_->setObjectName(QStringLiteral("CompileButton"));
	compile_btn_->setText(QStringLiteral("Compile"));
	compile_btn_->setToolTip(QStringLiteral("Generate C++ from this Blueprint"));
	connect(compile_btn_, &QToolButton::clicked, this, &MainWindow::OnGenerateCode);
	toolbar->addWidget(compile_btn_);

	play_btn_ = new QToolButton;
	play_btn_->setObjectName(QStringLiteral("PlayButton"));
	play_btn_->setText(QStringLiteral("Play"));
	play_btn_->setToolTip(QStringLiteral("Run the current graph"));
	connect(play_btn_, &QToolButton::clicked, this, &MainWindow::OnRunFlow);
	toolbar->addWidget(play_btn_);
	toolbar->addSeparator();

	breadcrumb_ = new QLabel(QStringLiteral("Blueprints"));
	breadcrumb_->setObjectName(QStringLiteral("Breadcrumb"));
	toolbar->addWidget(breadcrumb_);

	auto* center = new QWidget(this);
	auto* center_layout = new QVBoxLayout(center);
	center_layout->setContentsMargins(0, 0, 0, 0);
	center_layout->setSpacing(0);

	graph_tab_frame_ = new QFrame;
	graph_tab_frame_->setObjectName(QStringLiteral("GraphTabBarFrame"));
	auto* tab_layout = new QVBoxLayout(graph_tab_frame_);
	tab_layout->setContentsMargins(6, 0, 0, 0);
	tab_layout->setSpacing(0);
	graph_tabs_ = new QTabBar(graph_tab_frame_);
	graph_tabs_->setExpanding(false);
	graph_tabs_->setDrawBase(false);
	graph_tabs_->setMovable(false);
	tab_layout->addWidget(graph_tabs_);
	center_layout->addWidget(graph_tab_frame_);

	graph_stack_ = new QStackedWidget(center);
	center_layout->addWidget(graph_stack_, 1);

	empty_hint_ = new QLabel(QStringLiteral("从底部 Content Browser 打开蓝图"));
	empty_hint_->setObjectName(QStringLiteral("PanelHint"));
	empty_hint_->setAlignment(Qt::AlignCenter);
	center_layout->addWidget(empty_hint_, 1);
	setCentralWidget(center);

	QToolButton* fn_plus = nullptr;
	QToolButton* var_plus = nullptr;
	function_tree_ = new FunctionTreeWidget(this);
	var_tree_ = new VarTreeWidget(this);
	auto* fn_section = MakeAddSection(QStringLiteral("函数"), function_tree_, &fn_plus, nullptr);
	auto* var_section = MakeAddSection(QStringLiteral("变量"), var_tree_, &var_plus, &var_type_combo_);

	auto* splitter = new QSplitter(Qt::Vertical);
	splitter->addWidget(fn_section);
	splitter->addWidget(var_section);
	splitter->setChildrenCollapsible(false);
	splitter->setStretchFactor(0, 3);
	splitter->setStretchFactor(1, 2);

	auto* bp_root = new QWidget;
	auto* bp_layout = new QVBoxLayout(bp_root);
	bp_layout->setContentsMargins(0, 0, 0, 0);
	bp_layout->setSpacing(0);
	auto* bp_header = new QWidget;
	bp_header->setObjectName(QStringLiteral("SectionHeader"));
	auto* bp_row = new QHBoxLayout(bp_header);
	bp_row->setContentsMargins(8, 4, 6, 4);
	bp_row->setSpacing(6);
	auto* bp_label = new QLabel(QStringLiteral("Blueprint"));
	bp_label->setObjectName(QStringLiteral("SectionTitle"));
	bp_name_edit_ = new QLineEdit;
	bp_name_edit_->setObjectName(QStringLiteral("BlueprintNameEdit"));
	bp_name_edit_->setToolTip(QStringLiteral("蓝图名称（C++ 类名）"));
	connect(bp_name_edit_, &QLineEdit::editingFinished, this, &MainWindow::OnBlueprintRenamed);
	bp_row->addWidget(bp_label);
	bp_row->addWidget(bp_name_edit_, 1);
	bp_layout->addWidget(bp_header);
	bp_layout->addWidget(splitter, 1);

	bp_dock_ = MakeDock(QStringLiteral("My Blueprint"), bp_root, this);
	auto* bp_title = new QWidget(bp_dock_);
	bp_title->setObjectName(QStringLiteral("DockTitleBar"));
	auto* bp_title_row = new QHBoxLayout(bp_title);
	bp_title_row->setContentsMargins(10, 3, 4, 3);
	bp_title_row->setSpacing(4);
	auto* bp_title_label = new QLabel(QStringLiteral("My Blueprint"), bp_title);
	bp_title_label->setObjectName(QStringLiteral("DockTitle"));
	bp_title_row->addWidget(bp_title_label);
	bp_title_row->addStretch(1);
	auto* bp_plus = new QToolButton(bp_title);
	bp_plus->setObjectName(QStringLiteral("PlusButton"));
	bp_plus->setText(QStringLiteral("+"));
	bp_plus->setToolTip(QStringLiteral("新建蓝图"));
	bp_plus->setAutoRaise(false);
	bp_plus->setFixedSize(22, 22);
	bp_title_row->addWidget(bp_plus, 0, Qt::AlignVCenter);
	bp_dock_->setTitleBarWidget(bp_title);
	connect(bp_plus, &QToolButton::clicked, this, &MainWindow::OnNewBlueprint);
	addDockWidget(Qt::LeftDockWidgetArea, bp_dock_);
	bp_dock_->setMinimumWidth(260);

	auto* details_scroll = new QScrollArea;
	details_scroll->setWidgetResizable(true);
	details_scroll->setFrameShape(QFrame::NoFrame);
	details_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	details_host_ = new QWidget;
	details_host_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
	details_form_ = new QFormLayout(details_host_);
	details_form_->setContentsMargins(10, 10, 10, 10);
	details_form_->setHorizontalSpacing(8);
	details_form_->setVerticalSpacing(8);
	details_form_->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	details_form_->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
	details_form_->setRowWrapPolicy(QFormLayout::DontWrapRows);
	details_form_->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
	details_scroll->setWidget(details_host_);
	detail_dock_ = MakeDock(QStringLiteral("Details"), details_scroll, this);
	addDockWidget(Qt::RightDockWidgetArea, detail_dock_);
	detail_dock_->setMinimumWidth(280);

	palette_tree_ = MakeTree();
	palette_dock_ = MakeDock(QStringLiteral("Palette"), palette_tree_, this);
	addDockWidget(Qt::RightDockWidgetArea, palette_dock_);
	tabifyDockWidget(detail_dock_, palette_dock_);
	detail_dock_->raise();

	asset_tree_ = MakeTree();
	asset_tree_->setRootIsDecorated(true);
	asset_tree_->setContextMenuPolicy(Qt::CustomContextMenu);
	auto* asset_dock = MakeDock(QStringLiteral("Content Browser"), asset_tree_, this);
	auto* asset_title = new QWidget(asset_dock);
	asset_title->setObjectName(QStringLiteral("DockTitleBar"));
	auto* asset_title_row = new QHBoxLayout(asset_title);
	asset_title_row->setContentsMargins(10, 3, 4, 3);
	asset_title_row->setSpacing(4);
	auto* asset_title_label = new QLabel(QStringLiteral("Content Browser"), asset_title);
	asset_title_label->setObjectName(QStringLiteral("DockTitle"));
	asset_title_row->addWidget(asset_title_label);
	asset_title_row->addStretch(1);
	auto* asset_plus = new QToolButton(asset_title);
	asset_plus->setObjectName(QStringLiteral("PlusButton"));
	asset_plus->setText(QStringLiteral("+"));
	asset_plus->setToolTip(QStringLiteral("新建蓝图"));
	asset_plus->setAutoRaise(false);
	asset_plus->setFixedSize(22, 22);
	asset_title_row->addWidget(asset_plus, 0, Qt::AlignVCenter);
	asset_dock->setTitleBarWidget(asset_title);
	connect(asset_plus, &QToolButton::clicked, this, &MainWindow::OnNewBlueprint);
	addDockWidget(Qt::BottomDockWidgetArea, asset_dock);

	log_ = new QTextEdit;
	log_->setReadOnly(true);
	log_->setContextMenuPolicy(Qt::CustomContextMenu);
	auto* log_dock = MakeDock(QStringLiteral("Output Log"), log_, this);
	auto* log_title = new QWidget(log_dock);
	log_title->setObjectName(QStringLiteral("DockTitleBar"));
	auto* log_title_row = new QHBoxLayout(log_title);
	log_title_row->setContentsMargins(10, 3, 4, 3);
	log_title_row->setSpacing(4);
	auto* log_title_label = new QLabel(QStringLiteral("Output Log"), log_title);
	log_title_label->setObjectName(QStringLiteral("DockTitle"));
	log_title_row->addWidget(log_title_label);
	log_title_row->addStretch(1);
	auto* log_clear = new QToolButton(log_title);
	log_clear->setObjectName(QStringLiteral("ClearButton"));
	log_clear->setText(QStringLiteral("Clear"));
	log_clear->setToolTip(QStringLiteral("清空日志"));
	log_clear->setAutoRaise(false);
	log_clear->setFixedHeight(22);
	log_title_row->addWidget(log_clear, 0, Qt::AlignVCenter);
	log_dock->setTitleBarWidget(log_title);
	connect(log_clear, &QToolButton::clicked, this, &MainWindow::ClearLog);
	connect(log_, &QTextEdit::customContextMenuRequested, this, [this](const QPoint& pos) {
		if (!log_)
			return;
		auto* menu = log_->createStandardContextMenu();
		menu->addSeparator();
		auto* clear = menu->addAction(QStringLiteral("Clear"));
		clear->setEnabled(!log_->document()->isEmpty());
		connect(clear, &QAction::triggered, this, &MainWindow::ClearLog);
		menu->exec(log_->mapToGlobal(pos));
		delete menu;
	});
	addDockWidget(Qt::BottomDockWidgetArea, log_dock);
	tabifyDockWidget(asset_dock, log_dock);
	asset_dock->raise();

	window_menu->addAction(bp_dock_->toggleViewAction());
	window_menu->addAction(detail_dock_->toggleViewAction());
	window_menu->addAction(palette_dock_->toggleViewAction());
	window_menu->addAction(asset_dock->toggleViewAction());
	window_menu->addAction(log_dock->toggleViewAction());

	statusBar()->showMessage(QStringLiteral("Ready"));
	SetEditorVisible(false);

	connect(fn_plus, &QToolButton::clicked, this, &MainWindow::OnAddFlow);
	connect(var_plus, &QToolButton::clicked, this, &MainWindow::OnAddVar);
	connect(function_tree_, &QTreeWidget::itemClicked, this, &MainWindow::OnFlowClicked);
	connect(function_tree_, &QTreeWidget::itemDoubleClicked, this, &MainWindow::OnFlowClicked);
	connect(function_tree_, &QTreeWidget::customContextMenuRequested, this, &MainWindow::OnFunctionMenu);
	connect(function_tree_, &QTreeWidget::itemChanged, this, &MainWindow::OnFunctionRenamed);
	connect(var_tree_, &QTreeWidget::itemClicked, this, &MainWindow::OnVarClicked);
	connect(var_tree_, &QTreeWidget::customContextMenuRequested, this, &MainWindow::OnVariableMenu);
	connect(var_tree_, &QTreeWidget::itemChanged, this, &MainWindow::OnVariableRenamed);
	if (var_type_combo_) {
		if (auto* view = var_type_combo_->view())
			view->viewport()->setAttribute(Qt::WA_NoMouseReplay, true);
		connect(var_type_combo_, QOverload<int>::of(&QComboBox::activated),
			this, &MainWindow::OnVarTypeChosen);
	}
	connect(palette_tree_, &QTreeWidget::itemDoubleClicked, this, &MainWindow::OnPaletteDoubleClicked);
	connect(graph_tabs_, &QTabBar::currentChanged, this, &MainWindow::OnGraphTabChanged);
	connect(asset_tree_, &QTreeWidget::itemDoubleClicked, this, &MainWindow::OnAssetDoubleClicked);
	connect(asset_tree_, &QTreeWidget::customContextMenuRequested, this, &MainWindow::OnAssetMenu);

	function_tree_->installEventFilter(this);
	var_tree_->installEventFilter(this);
	asset_tree_->installEventFilter(this);
}

void MainWindow::AppendLog(const QString& text)
{
	if (log_)
		log_->append(text);
	statusBar()->showMessage(text, 4000);
}

void MainWindow::ClearLog()
{
	if (log_)
		log_->clear();
}

void MainWindow::OnLog(const QString& text)
{
	AppendLog(text);
}

void MainWindow::UpdateBreadcrumb()
{
	if (!has_blueprint_) {
		breadcrumb_->setText(QStringLiteral("Blueprints"));
		setWindowTitle(QStringLiteral("ClassGraph"));
		return;
	}
	const QString space = QString::fromStdString(blueprint_dto_.space);
	const QString name = QString::fromStdString(blueprint_dto_.name);
	const QString flow = QString::fromStdString(blueprint_dto_.current_flow_name);
	breadcrumb_->setText(QStringLiteral("Blueprints / %1 / %2    %3").arg(space, name, flow));
	setWindowTitle(QStringLiteral("%1 - ClassGraph").arg(name));
	if (bp_name_edit_ && bp_name_edit_->text() != name) {
		const QSignalBlocker blocker(bp_name_edit_);
		bp_name_edit_->setText(name);
	}
}

void MainWindow::RefreshTypeCombo()
{
	if (!var_type_combo_) return;
	const QString current = var_type_combo_->currentText();
	const QString self = QString::fromStdString(blueprint_dto_.self_type_name);
	std::vector<TypeItem> types;
	for (const auto& t : blueprint_dto_.types) {
		if (!self.isEmpty() && QString::fromStdString(t.name) == self)
			continue;
		types.push_back(t);
	}
	FillTypeCombo(var_type_combo_, types, current);
}

void MainWindow::RefreshGraphTabs()
{
	if (!graph_tabs_) return;
	const bool blocked = graph_tabs_->blockSignals(true);
	while (graph_tabs_->count() > 0)
		graph_tabs_->removeTab(0);
	int current = 0;
	int i = 0;
	std::unordered_set<std::string> keep;
	for (const auto& flow : blueprint_dto_.flows) {
		keep.insert(flow.guid);
		EnsureGraphView(QString::fromStdString(flow.guid));
		const int idx = graph_tabs_->addTab(QString::fromStdString(flow.name));
		graph_tabs_->setTabData(idx, QString::fromStdString(flow.guid));
		if (flow.guid == blueprint_dto_.current_flow_guid)
			current = i;
		++i;
	}
	std::vector<QString> drop;
	for (const auto& [guid, view] : graph_views_) {
		if (!keep.count(guid))
			drop.push_back(QString::fromStdString(guid));
	}
	for (const auto& guid : drop)
		RemoveGraphView(guid);
	if (graph_tabs_->count() > 0)
		graph_tabs_->setCurrentIndex(current);
	graph_tabs_->blockSignals(blocked);
	ShowFlowView(QString::fromStdString(blueprint_dto_.current_flow_guid), false);
}

void MainWindow::SetEditorVisible(bool visible)
{
	if (graph_tab_frame_)
		graph_tab_frame_->setVisible(visible);
	if (graph_stack_)
		graph_stack_->setVisible(visible);
	if (empty_hint_)
		empty_hint_->setVisible(!visible);
	if (bp_dock_)
		bp_dock_->setVisible(visible);
	if (detail_dock_)
		detail_dock_->setVisible(visible);
	if (palette_dock_)
		palette_dock_->setVisible(visible);
	if (compile_btn_)
		compile_btn_->setVisible(visible);
	if (play_btn_)
		play_btn_->setVisible(visible);
}

GraphView* MainWindow::CurrentGraphView() const
{
	if (!graph_stack_)
		return nullptr;
	return qobject_cast<GraphView*>(graph_stack_->currentWidget());
}

GraphView* MainWindow::GraphViewOf(const QString& flow_guid) const
{
	if (flow_guid.isEmpty())
		return nullptr;
	auto it = graph_views_.find(flow_guid.toStdString());
	if (it == graph_views_.end())
		return nullptr;
	return it->second;
}

GraphView* MainWindow::EnsureGraphView(const QString& flow_guid)
{
	if (flow_guid.isEmpty() || !graph_stack_)
		return nullptr;
	if (auto* existing = GraphViewOf(flow_guid))
		return existing;
	auto* view = new GraphView(graph_stack_);
	view->SetFlowGuid(flow_guid);
	if (sink_)
		view->SetSink(sink_);
	view->SetPalette(palette_items_);
	BindGraphView(view);
	graph_stack_->addWidget(view);
	graph_views_[flow_guid.toStdString()] = view;
	return view;
}

void MainWindow::BindGraphView(GraphView* view)
{
	if (!view)
		return;
	connect(view, &GraphView::NodeSelected, this, &MainWindow::OnNodeSelected);
	connect(view, &GraphView::LogMessage, this, &MainWindow::OnLog);
	connect(view, &GraphView::VariableDropped, this, &MainWindow::OnVariableDropped);
	connect(view, &GraphView::FunctionDropped, this, &MainWindow::OnFunctionDropped);
	connect(view, &GraphView::NodeContextMenu, this, &MainWindow::OnNodeContextMenu);
}

void MainWindow::RemoveGraphView(const QString& flow_guid)
{
	auto it = graph_views_.find(flow_guid.toStdString());
	if (it == graph_views_.end())
		return;
	auto* view = it->second;
	graph_views_.erase(it);
	if (graph_stack_ && view)
		graph_stack_->removeWidget(view);
	delete view;
}

void MainWindow::ClearGraphViews()
{
	for (auto& [guid, view] : graph_views_) {
		if (graph_stack_ && view)
			graph_stack_->removeWidget(view);
		delete view;
	}
	graph_views_.clear();
	graph_dto_.reset();
}

void MainWindow::ShowFlowView(const QString& flow_guid, bool load_if_empty)
{
	auto* view = EnsureGraphView(flow_guid);
	if (!view || !graph_stack_)
		return;
	graph_stack_->setCurrentWidget(view);
	if (load_if_empty && !view->GraphLoaded())
		Dispatch(Action::GetGraph);
}

void MainWindow::ApplyBlueprintDto(const Blueprint& dto)
{
	blueprint_dto_ = dto;
	has_blueprint_ = !dto.guid.empty() || !dto.name.empty();

	if (!function_tree_ || !var_tree_) return;
	const bool blocked_fn = function_tree_->blockSignals(true);
	const bool blocked_var = var_tree_->blockSignals(true);
	function_tree_->clear();
	var_tree_->clear();

	for (const auto& flow : dto.flows) {
		const QString name = QString::fromStdString(flow.name);
		const QString guid = QString::fromStdString(flow.guid);
		auto* item = new QTreeWidgetItem(QStringList() << name);
		item->setFlags(item->flags() | Qt::ItemIsEditable);
		item->setData(0, Qt::UserRole, guid);
		item->setData(0, Qt::UserRole + 1, name);
		item->setForeground(0, QBrush(QColor(140, 210, 140)));
		item->setToolTip(0, QStringLiteral("函数  %1\n拖到图表可放置调用节点\nF2 重命名，Delete 删除").arg(name));
		function_tree_->addTopLevelItem(item);
		if (flow.guid == dto.current_flow_guid)
			function_tree_->setCurrentItem(item);
	}

	for (const auto& var : dto.vars) {
		const QString name = QString::fromStdString(var.name);
		const QString guid = QString::fromStdString(var.guid);
		const QString type_name = QString::fromStdString(var.type_name);
		auto* item = new QTreeWidgetItem(QStringList() << name);
		item->setFlags(item->flags() | Qt::ItemIsEditable);
		item->setData(0, Qt::UserRole, guid);
		item->setData(0, Qt::UserRole + 1, name);
		item->setForeground(0, QBrush(PinColor(type_name, PinKind::Data)));
		item->setToolTip(0, QStringLiteral("变量  %1 : %2\nF2 重命名，Delete 删除").arg(name, type_name));
		var_tree_->addTopLevelItem(item);
	}

	function_tree_->blockSignals(blocked_fn);
	var_tree_->blockSignals(blocked_var);

	RefreshGraphTabs();
	RefreshTypeCombo();
	UpdateBreadcrumb();
	SetEditorVisible(has_blueprint_);

	if (pending_edit_flow_) {
		pending_edit_flow_ = false;
		EditNewItem(function_tree_, QString::fromStdString(dto.current_flow_guid));
	}
}

void MainWindow::ApplyPaletteDto(const Palette& dto)
{
	if (!palette_tree_) return;
	palette_items_ = dto.items;
	palette_tree_->clear();
	std::unordered_map<QString, QTreeWidgetItem*> groups;
	auto ensure = [&](const QString& name) {
		if (auto it = groups.find(name); it != groups.end())
			return it->second;
		auto* item = new QTreeWidgetItem(QStringList() << name);
		item->setForeground(0, TextMuted());
		palette_tree_->addTopLevelItem(item);
		item->setExpanded(true);
		groups[name] = item;
		return item;
	};

	for (const auto& caller : dto.items) {
		const QString group = QString::fromStdString(caller.group);
		auto* parent = ensure(group.isEmpty() ? QStringLiteral("Functions") : group);
		auto* item = new QTreeWidgetItem(parent, QStringList() << QString::fromStdString(caller.name));
		item->setData(0, Qt::UserRole, QString::fromStdString(caller.key));
	}

	for (auto& [guid, view] : graph_views_) {
		if (view)
			view->SetPalette(palette_items_);
	}
}

void MainWindow::ApplyGraphDto(const Graph& dto)
{
	graph_dto_ = dto;
	const QString flow = QString::fromStdString(dto.flow_guid);
	GraphView* view = GraphViewOf(flow);
	if (!view)
		view = EnsureGraphView(flow);
	if (!view)
		view = CurrentGraphView();
	if (view)
		view->ApplyGraph(dto);
}

void MainWindow::ApplyAssetListDto(const AssetList& dto)
{
	if (!asset_tree_) return;
	const bool blocked = asset_tree_->blockSignals(true);
	asset_tree_->clear();

	std::unordered_map<QString, QTreeWidgetItem*> folders;
	auto ensure = [&](const QString& space) {
		if (auto it = folders.find(space); it != folders.end())
			return it->second;
		auto* folder = new QTreeWidgetItem(QStringList() << space);
		folder->setFlags(folder->flags() & ~Qt::ItemIsEditable);
		folder->setForeground(0, TextMuted());
		asset_tree_->addTopLevelItem(folder);
		folder->setExpanded(true);
		folders[space] = folder;
		return folder;
	};

	for (const auto& item : dto.items) {
		const QString name = QString::fromStdString(item.name);
		const QString guid = QString::fromStdString(item.key);
		const QString path = QString::fromStdString(item.path);
		auto* row = new QTreeWidgetItem(QStringList() << name);
		row->setFlags(row->flags() & ~Qt::ItemIsEditable);
		row->setData(0, Qt::UserRole, guid);
		row->setToolTip(0, path);
		if (item.space.empty())
			asset_tree_->addTopLevelItem(row);
		else
			ensure(QString::fromStdString(item.space))->addChild(row);
	}

	asset_tree_->blockSignals(blocked);
}

void MainWindow::ApplyDetails(const QString& data)
{
	const QString kind = QJsonDocument::fromJson(data.toUtf8()).object()
		.value(QStringLiteral("kind")).toString();
	if (kind == QLatin1String("node")) {
		NodeDetails node_dto;
		if (ParseNode(data.toStdString(), node_dto)) {
			selected_var_guid_.clear();
			FillNodeDetails(node_dto);
		}
		return;
	}
	if (kind == QLatin1String("var")) {
		VarDetails var_dto;
		if (ParseVar(data.toStdString(), var_dto)) {
			last_node_details_.reset();
			selected_var_guid_ = QString::fromStdString(var_dto.guid);
			FillVarDetails(var_dto);
		}
		return;
	}
	last_node_details_.reset();
	selected_var_guid_.clear();
	FillBlueprintDetails(data);
}

void MainWindow::ClearDetailsForm()
{
	if (!details_form_) return;
	while (details_form_->rowCount() > 0)
		details_form_->removeRow(0);
}

void MainWindow::AddDetailRow(const QString& key, const QString& value)
{
	auto* k = new QLabel(key);
	k->setStyleSheet(QStringLiteral("color:#9a9a9a;"));
	auto* v = new QLabel(value);
	v->setStyleSheet(QStringLiteral("color:#e0e0e0;"));
	v->setTextInteractionFlags(Qt::TextSelectableByMouse);
	details_form_->addRow(k, v);
}

void MainWindow::FillBlueprintDetails(const QString& json)
{
	if (details_busy_ || !details_form_) return;
	details_busy_ = true;
	struct Guard {
		bool& flag;
		~Guard() { flag = false; }
	} guard{ details_busy_ };

	ClearDetailsForm();

	const auto doc = QJsonDocument::fromJson(json.toUtf8());
	const auto obj = doc.object();
	if (!has_blueprint_ && obj.isEmpty()) {
		auto* hint = new QLabel(QStringLiteral("No Blueprint"));
		hint->setObjectName(QStringLiteral("PanelHint"));
		details_form_->addRow(hint);
		return;
	}

	const QString name = obj.value(QStringLiteral("name")).toString(
		QString::fromStdString(blueprint_dto_.name));
	const QString space = obj.value(QStringLiteral("space")).toString(
		QString::fromStdString(blueprint_dto_.space));
	const QString guid = obj.value(QStringLiteral("guid")).toString(
		QString::fromStdString(blueprint_dto_.guid));

	auto* name_edit = new QLineEdit(name);
	connect(name_edit, &QLineEdit::editingFinished, this, [this, name_edit]() {
		const QString text = name_edit->text().trimmed();
		if (text.isEmpty() || text == QString::fromStdString(blueprint_dto_.name))
			return;
		Dispatch(Action::RenameBP, QJsonObject{{QStringLiteral("name"), text}});
	});
	details_form_->addRow(QStringLiteral("Name"), name_edit);

	auto* space_edit = new QLineEdit(space);
	connect(space_edit, &QLineEdit::editingFinished, this, [this, space_edit]() {
		const QString text = space_edit->text().trimmed();
		if (text.isEmpty() || text == QString::fromStdString(blueprint_dto_.space))
			return;
		Dispatch(Action::SetSpace, QJsonObject{{QStringLiteral("space"), text}});
	});
	details_form_->addRow(QStringLiteral("Namespace"), space_edit);
	AddDetailRow(QStringLiteral("Guid"), guid);
}

void MainWindow::FillNodeDetails(const NodeDetails& dto)
{
	if (details_busy_ || !details_form_) return;
	details_busy_ = true;
	struct Guard {
		bool& flag;
		~Guard() { flag = false; }
	} guard{ details_busy_ };

	last_node_details_ = dto;
	ClearDetailsForm();

	AddDetailRow(QStringLiteral("Node"), QString::fromStdString(dto.title));
	if (!dto.caller_name.empty())
		AddDetailRow(QStringLiteral("Class"), QString::fromStdString(dto.caller_name));
	if (!dto.kind_label.empty())
		AddDetailRow(QStringLiteral("Kind"), QString::fromStdString(dto.kind_label));
	AddDetailRow(QStringLiteral("Guid"), QString::fromStdString(dto.node_guid));

	auto add_pin_rows = [this](const QString& title, PinDir dir, const std::vector<Pin>& pins) {
		bool any = false;
		for (const auto& pin : pins) {
			if (pin.direction != dir)
				continue;
			if (!any) {
				auto* label = new QLabel(title);
				label->setObjectName(QStringLiteral("SectionTitle"));
				details_form_->addRow(label);
				any = true;
			}
			const QString kind = pin.kind == PinKind::Exec
				? QStringLiteral("Exec") : QStringLiteral("Data");
			QString value = QString::fromStdString(pin.type_name);
			if (pin.kind == PinKind::Data && !pin.value_text.empty()) {
				if (!value.isEmpty())
					value += QStringLiteral("  ");
				value += QString::fromStdString(pin.value_text);
			}
			if (pin.connected) {
				if (!value.isEmpty())
					value += QStringLiteral("  ");
				value += QStringLiteral("linked");
			}
			const QString name = pin.name.empty()
				? kind : QStringLiteral("%1  (%2)").arg(
					QString::fromStdString(pin.name), kind);
			AddDetailRow(name, value);
		}
	};
	add_pin_rows(QStringLiteral("输入"), PinDir::In, dto.pins);
	add_pin_rows(QStringLiteral("输出"), PinDir::Out, dto.pins);

	if (!dto.can_edit_params)
		return;

	const QString node_guid = QString::fromStdString(dto.node_guid);
	auto* section = new QWidget(details_host_);
	section->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
	auto* v = new QVBoxLayout(section);
	v->setContentsMargins(0, 6, 0, 0);
	v->setSpacing(6);

	auto* header = new QWidget(section);
	auto* header_layout = new QHBoxLayout(header);
	header_layout->setContentsMargins(0, 0, 0, 0);
	header_layout->setSpacing(4);

	auto* title = new QLabel(dto.is_entry ? QStringLiteral("输入参数") : QStringLiteral("输出参数"), header);
	title->setObjectName(QStringLiteral("SectionTitle"));
	header_layout->addWidget(title);
	header_layout->addStretch(1);

	auto* add_type = new QComboBox(header);
	add_type->setObjectName(QStringLiteral("ParamTypeCombo"));
	add_type->setToolTip(QStringLiteral("新参数类型"));
	add_type->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
	add_type->setMinimumContentsLength(4);
	add_type->setFixedWidth(96);
	FillTypeCombo(add_type, dto.type_names, QStringLiteral("int"));
	header_layout->addWidget(add_type, 0, Qt::AlignVCenter);

	auto* add_btn = new QToolButton(header);
	add_btn->setObjectName(QStringLiteral("PlusButton"));
	add_btn->setText(QStringLiteral("+"));
	add_btn->setToolTip(dto.is_entry ? QStringLiteral("添加输入参数") : QStringLiteral("添加输出参数"));
	add_btn->setAutoRaise(false);
	add_btn->setFixedSize(22, 22);
	header_layout->addWidget(add_btn, 0, Qt::AlignVCenter);
	v->addWidget(header);

	connect(add_btn, &QToolButton::clicked, this, [this, node_guid, add_type]() {
		const QString type = add_type->currentText();
		QTimer::singleShot(0, this, [this, node_guid, type]() {
			Dispatch(Action::AddParam, QJsonObject{
				{QStringLiteral("guid"), node_guid},
				{QStringLiteral("type"), type},
			});
		});
	});

	for (const auto& arg : dto.params) {
		const QString arg_name = QString::fromStdString(arg.name);
		QLineEdit* name_edit = nullptr;
		QComboBox* type_combo = nullptr;
		QToolButton* del = nullptr;
		auto* row = MakeParamEditRow(section, arg_name, {}, &name_edit, &type_combo, &del, QStringLiteral("×"));
		FillTypeCombo(type_combo, dto.type_names,
			arg.type_name.empty() ? QStringLiteral("int") : QString::fromStdString(arg.type_name));
		if (del)
			del->setToolTip(QStringLiteral("删除参数"));

		connect(name_edit, &QLineEdit::editingFinished, this, [this, node_guid, arg_name, name_edit]() {
			const QString next = name_edit->text().trimmed();
			if (next == arg_name || next.isEmpty()) return;
			QTimer::singleShot(0, this, [this, node_guid, arg_name, next]() {
				Dispatch(Action::RenameParam, QJsonObject{
					{QStringLiteral("guid"), node_guid},
					{QStringLiteral("old"), arg_name},
					{QStringLiteral("name"), next},
				});
			});
		});
		connect(type_combo, QOverload<int>::of(&QComboBox::activated), this,
			[this, node_guid, arg_name, type_combo](int) {
				const QString type_name = type_combo->currentText();
				QTimer::singleShot(0, this, [this, node_guid, arg_name, type_name]() {
					Dispatch(Action::SetParamType, QJsonObject{
						{QStringLiteral("guid"), node_guid},
						{QStringLiteral("name"), arg_name},
						{QStringLiteral("type"), type_name},
					});
				});
			});
		connect(del, &QToolButton::clicked, this, [this, node_guid, arg_name]() {
			QTimer::singleShot(0, this, [this, node_guid, arg_name]() {
				Dispatch(Action::DelParam, QJsonObject{
					{QStringLiteral("guid"), node_guid},
					{QStringLiteral("name"), arg_name},
				});
			});
		});
		v->addWidget(row);
	}

	details_form_->addRow(section);
}

void MainWindow::FillVarDetails(const VarDetails& dto)
{
	if (details_busy_ || !details_form_) return;
	details_busy_ = true;
	struct Guard {
		bool& flag;
		~Guard() { flag = false; }
	} guard{ details_busy_ };

	ClearDetailsForm();
	AddDetailRow(QStringLiteral("Kind"), QStringLiteral("Variable"));
	AddDetailRow(QStringLiteral("Name"), QString::fromStdString(dto.name));

	const QString guid = QString::fromStdString(dto.guid);
	auto* type_combo = new QComboBox;
	FillTypeCombo(type_combo, dto.type_names, QString::fromStdString(dto.type_name));
	connect(type_combo, QOverload<int>::of(&QComboBox::activated), this,
		[this, guid, type_combo](int) {
			Dispatch(Action::SetVarType, QJsonObject{
				{QStringLiteral("guid"), guid},
				{QStringLiteral("type"), type_combo->currentText()},
			});
		});
	details_form_->addRow(QStringLiteral("Type"), type_combo);

	const QString value_text = QString::fromStdString(dto.value_text);
	if (dto.edit_kind == EditKind::Bool) {
		auto* box = new QCheckBox;
		box->setChecked(value_text == QStringLiteral("true")
			|| value_text == QStringLiteral("1")
			|| value_text == QStringLiteral("True"));
		connect(box, &QCheckBox::toggled, this, [this, guid](bool on) {
			const QString text = on ? QStringLiteral("true") : QStringLiteral("false");
			QTimer::singleShot(0, this, [this, guid, text]() {
				Dispatch(Action::SetVarValue, QJsonObject{
					{QStringLiteral("guid"), guid},
					{QStringLiteral("value"), text},
				});
			});
		});
		details_form_->addRow(QStringLiteral("Default Value"), box);
	} else if (dto.edit_kind != EditKind::None || !value_text.isEmpty()) {
		auto* edit = new QLineEdit(value_text);
		if (dto.edit_kind == EditKind::String)
			edit->setPlaceholderText(QStringLiteral("\"\""));
		else if (dto.edit_kind == EditKind::None)
			edit->setPlaceholderText(QStringLiteral("{ }"));
		connect(edit, &QLineEdit::editingFinished, this, [this, guid, edit]() {
			const QString text = edit->text();
			QTimer::singleShot(0, this, [this, guid, text]() {
				Dispatch(Action::SetVarValue, QJsonObject{
					{QStringLiteral("guid"), guid},
					{QStringLiteral("value"), text},
				});
			});
		});
		details_form_->addRow(QStringLiteral("Default Value"), edit);
	} else {
		AddDetailRow(QStringLiteral("Default Value"), QStringLiteral("—"));
	}
	AddDetailRow(QStringLiteral("Guid"), guid);
}

void MainWindow::EditNewItem(QTreeWidget* tree, const QString& guid)
{
	if (!tree || guid.isEmpty()) return;
	QPointer<QTreeWidget> tree_ptr(tree);
	QTimer::singleShot(0, this, [tree_ptr, guid]() {
		if (!tree_ptr)
			return;
		for (int i = 0; i < tree_ptr->topLevelItemCount(); ++i) {
			auto* item = tree_ptr->topLevelItem(i);
			if (!item || item->data(0, Qt::UserRole).toString() != guid)
				continue;
			tree_ptr->setCurrentItem(item);
			tree_ptr->scrollToItem(item);
			if (item->flags() & Qt::ItemIsEditable)
				tree_ptr->editItem(item, 0);
			return;
		}
	});
}

void MainWindow::OnAssetDoubleClicked(QTreeWidgetItem* item, int)
{
	if (!item) return;
	const QString guid = item->data(0, Qt::UserRole).toString();
	if (guid.isEmpty()) return;
	Dispatch(Action::OpenBP, QJsonObject{{QStringLiteral("guid"), guid}});
}

void MainWindow::OnAssetMenu(const QPoint& pos)
{
	if (!asset_tree_) return;
	auto* item = asset_tree_->itemAt(pos);
	if (!item || item->data(0, Qt::UserRole).toString().isEmpty())
		return;
	asset_tree_->setCurrentItem(item);
	QMenu menu(this);
	menu.addAction(QStringLiteral("打开"), this, [this, item]() { OnAssetDoubleClicked(item, 0); });
	menu.addAction(QStringLiteral("删除"), this, &MainWindow::OnDeleteAsset);
	menu.exec(asset_tree_->viewport()->mapToGlobal(pos));
}

void MainWindow::OnDeleteAsset()
{
	if (!asset_tree_) return;
	auto* item = asset_tree_->currentItem();
	if (!item) return;
	const QString guid = item->data(0, Qt::UserRole).toString();
	if (guid.isEmpty()) return;
	Dispatch(Action::DeleteBP, QJsonObject{{QStringLiteral("guid"), guid}});
}

void MainWindow::OnNewBlueprint()
{
	QDialog dialog(this);
	dialog.setWindowTitle(QStringLiteral("New Blueprint"));
	auto* form = new QFormLayout(&dialog);
	auto* space = new QLineEdit(QStringLiteral("user"));
	auto* name = new QLineEdit(QStringLiteral("NewBlueprint"));
	form->addRow(QStringLiteral("Namespace"), space);
	form->addRow(QStringLiteral("Name"), name);
	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	form->addRow(buttons);
	connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
	if (dialog.exec() != QDialog::Accepted)
		return;
	if (name->text().trimmed().isEmpty())
		return;
	Dispatch(Action::CreateBP, QJsonObject{
		{QStringLiteral("space"), space->text().trimmed()},
		{QStringLiteral("name"), name->text().trimmed()},
	});
}

void MainWindow::OnAddFlow()
{
	pending_edit_flow_ = true;
	Dispatch(Action::AddFlow);
}

void MainWindow::OnAddVar()
{
	const QString type_name = var_type_combo_ && var_type_combo_->count()
		? var_type_combo_->currentText() : QStringLiteral("int");
	pending_edit_var_ = true;
	Dispatch(Action::AddVar, QJsonObject{{QStringLiteral("type"), type_name}});
}

void MainWindow::OnAddExit()
{
	if (auto* view = CurrentGraphView())
		view->AddExitAtCenter();
}

void MainWindow::OnRunFlow()
{
	Dispatch(Action::RunFlow);
}

void MainWindow::OnGenerateCode()
{
	struct BusyGuard {
		QWidget* w{};
		explicit BusyGuard(QWidget* w) : w(w)
		{
			w->setEnabled(false);
			QApplication::setOverrideCursor(Qt::WaitCursor);
			QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
		}
		~BusyGuard()
		{
			QApplication::restoreOverrideCursor();
			w->setEnabled(true);
		}
	} busy(this);

	Dispatch(Action::GenerateCode);
}

void MainWindow::OnFlowClicked(QTreeWidgetItem* item, int)
{
	if (!item) return;
	const QString guid = item->data(0, Qt::UserRole).toString();
	if (guid.isEmpty())
		return;
	if (guid.toStdString() == blueprint_dto_.current_flow_guid)
		return;
	Dispatch(Action::SelectFlow, QJsonObject{{QStringLiteral("guid"), guid}});
}

void MainWindow::OnVarClicked(QTreeWidgetItem* item, int)
{
	if (!item) return;
	const QString guid = item->data(0, Qt::UserRole).toString();
	if (guid.isEmpty())
		return;
	Dispatch(Action::SelectVar, QJsonObject{{QStringLiteral("guid"), guid}});
}

void MainWindow::OnVariableDropped(const QString& guid, const QPointF& scene_pos, Qt::KeyboardModifiers mods)
{
	if (guid.isEmpty())
		return;

	QString name = guid;
	for (const auto& var : blueprint_dto_.vars) {
		if (var.guid == guid.toStdString()) {
			name = QString::fromStdString(var.name);
			break;
		}
	}

	bool make_set = false;
	if (mods & Qt::ControlModifier) {
		make_set = false;
	} else if (mods & Qt::AltModifier) {
		make_set = true;
	} else {
		QMenu menu;
		menu.setStyleSheet(QStringLiteral(
			"QMenu { min-width: 0px; padding: 2px; }"
			"QMenu::item { padding: 4px 10px; }"));
		auto* get_act = menu.addAction(QStringLiteral("Get %1").arg(name));
		auto* set_act = menu.addAction(QStringLiteral("Set %1").arg(name));
		auto* chosen = menu.exec(QCursor::pos());
		if (chosen == set_act)
			make_set = true;
		else if (chosen != get_act)
			return;
	}

	Dispatch(make_set ? Action::PlaceVarSet : Action::PlaceVarGet, QJsonObject{
		{QStringLiteral("guid"), guid},
		{QStringLiteral("x"), scene_pos.x()},
		{QStringLiteral("y"), scene_pos.y()},
	});
}

void MainWindow::OnFunctionDropped(const QString& guid, const QPointF& scene_pos)
{
	if (guid.isEmpty())
		return;
	Dispatch(Action::AddNode, QJsonObject{
		{QStringLiteral("key"), QStringLiteral("flow:%1").arg(guid)},
		{QStringLiteral("x"), scene_pos.x()},
		{QStringLiteral("y"), scene_pos.y()},
	});
}

void MainWindow::OnPaletteDoubleClicked(QTreeWidgetItem* item, int)
{
	auto* view = CurrentGraphView();
	if (!item || !view) return;
	const QString key = item->data(0, Qt::UserRole).toString();
	if (key.isEmpty()) return;
	const QPointF center = view->mapToScene(view->viewport()->rect().center());
	view->PlaceCaller(key, center);
}

void MainWindow::OnGraphTabChanged(int index)
{
	if (index < 0 || !graph_tabs_) return;
	const QString guid = graph_tabs_->tabData(index).toString();
	if (guid.isEmpty())
		return;
	ShowFlowView(guid, false);
	if (guid.toStdString() == blueprint_dto_.current_flow_guid)
		return;
	Dispatch(Action::SelectFlow, QJsonObject{{QStringLiteral("guid"), guid}});
}

void MainWindow::OnNodeSelected(const QString& guid)
{
	Dispatch(Action::SelectNode, QJsonObject{{QStringLiteral("guid"), guid}});
}

void MainWindow::OnBlueprintRenamed()
{
	if (!bp_name_edit_)
		return;
	const QString name = bp_name_edit_->text().trimmed();
	if (name.isEmpty() || name == QString::fromStdString(blueprint_dto_.name))
		return;
	Dispatch(Action::RenameBP, QJsonObject{{QStringLiteral("name"), name}});
}

void MainWindow::OnFunctionMenu(const QPoint& pos)
{
	QMenu menu(this);
	menu.addAction(QStringLiteral("添加函数"), this, &MainWindow::OnAddFlow);
	menu.addSeparator();
	menu.addAction(QStringLiteral("重命名"), this, [this]() {
		if (auto* item = function_tree_->currentItem())
			function_tree_->editItem(item, 0);
	});
	menu.addAction(QStringLiteral("删除"), this, &MainWindow::OnDeleteFunction);
	menu.exec(function_tree_->mapToGlobal(pos));
}

void MainWindow::OnVariableMenu(const QPoint& pos)
{
	QMenu menu(this);
	menu.addAction(QStringLiteral("添加变量"), this, &MainWindow::OnAddVar);
	menu.addSeparator();
	menu.addAction(QStringLiteral("重命名"), this, [this]() {
		if (auto* item = var_tree_->currentItem())
			var_tree_->editItem(item, 0);
	});
	menu.addAction(QStringLiteral("删除"), this, &MainWindow::OnDeleteVariable);
	menu.exec(var_tree_->mapToGlobal(pos));
}

void MainWindow::OnFunctionRenamed(QTreeWidgetItem* item, int)
{
	if (renaming_ || !item) return;
	const QString guid = item->data(0, Qt::UserRole).toString();
	const QString old_name = item->data(0, Qt::UserRole + 1).toString();
	const QString new_name = item->text(0).trimmed();
	if (guid.isEmpty()) return;

	if (new_name.isEmpty() || new_name == old_name) {
		renaming_ = true;
		item->setText(0, old_name);
		renaming_ = false;
		return;
	}
	Dispatch(Action::RenameFlow, QJsonObject{
		{QStringLiteral("guid"), guid},
		{QStringLiteral("name"), new_name},
	});
}

void MainWindow::OnVariableRenamed(QTreeWidgetItem* item, int)
{
	if (renaming_ || !item) return;
	const QString guid = item->data(0, Qt::UserRole).toString();
	const QString old_name = item->data(0, Qt::UserRole + 1).toString();
	const QString new_name = item->text(0).trimmed();
	if (guid.isEmpty()) return;

	if (new_name.isEmpty() || new_name == old_name) {
		renaming_ = true;
		item->setText(0, old_name);
		renaming_ = false;
		return;
	}
	Dispatch(Action::RenameVar, QJsonObject{
		{QStringLiteral("guid"), guid},
		{QStringLiteral("name"), new_name},
	});
}

void MainWindow::OnDeleteFunction()
{
	if (!function_tree_) return;
	auto* item = function_tree_->currentItem();
	if (!item) return;
	const QString guid = item->data(0, Qt::UserRole).toString();
	if (guid.isEmpty()) return;
	Dispatch(Action::DelFlow, QJsonObject{{QStringLiteral("guid"), guid}});
}

void MainWindow::OnDeleteVariable()
{
	if (!var_tree_) return;
	auto* item = var_tree_->currentItem();
	if (!item) return;
	const QString guid = item->data(0, Qt::UserRole).toString();
	if (guid.isEmpty()) return;
	Dispatch(Action::DelVar, QJsonObject{{QStringLiteral("guid"), guid}});
}

void MainWindow::OnVarTypeChosen(int)
{
	if (!var_type_combo_)
		return;
	const QString type_name = var_type_combo_->currentText().trimmed();
	if (type_name.isEmpty())
		return;
	AppendLog(QStringLiteral("新变量将使用类型 %1").arg(type_name));
}

void MainWindow::PromptAddNodeParam(const QString& node_guid, bool is_entry)
{
	QDialog dialog(this);
	dialog.setWindowTitle(is_entry ? QStringLiteral("添加输入参数") : QStringLiteral("添加输出参数"));
	auto* form = new QFormLayout(&dialog);
	auto* name = new QLineEdit(QStringLiteral("NewParam"));
	auto* type = new QComboBox;
	std::vector<std::string> types;
	if (last_node_details_ && last_node_details_->node_guid == node_guid.toStdString())
		types = last_node_details_->type_names;
	else {
		for (const auto& t : blueprint_dto_.types)
			types.push_back(t.name);
	}
	FillTypeCombo(type, types, QStringLiteral("int"));
	form->addRow(QStringLiteral("Name"), name);
	form->addRow(QStringLiteral("Type"), type);
	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	form->addRow(buttons);
	connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
	name->selectAll();
	name->setFocus();
	if (dialog.exec() != QDialog::Accepted)
		return;
	const QString n = name->text().trimmed();
	Dispatch(Action::AddParam, QJsonObject{
		{QStringLiteral("guid"), node_guid},
		{QStringLiteral("name"), n.isEmpty() ? QStringLiteral("NewParam") : n},
		{QStringLiteral("type"), type->currentText()},
	});
}

void MainWindow::OnNodeContextMenu(const QString& node_guid, const QPoint& global_pos)
{
	if (node_guid.isEmpty())
		return;

	bool is_entry = false;
	bool is_exit = false;
	std::vector<Param> params;

	if (last_node_details_ && last_node_details_->node_guid == node_guid.toStdString()) {
		is_entry = last_node_details_->is_entry;
		is_exit = last_node_details_->is_exit;
		params = last_node_details_->params;
	} else if (graph_dto_) {
		for (const auto& node : graph_dto_->nodes) {
			if (node.guid != node_guid.toStdString())
				continue;
			is_entry = node.is_entry;
			is_exit = node.is_exit;
			break;
		}
	}

	if (!is_entry && !is_exit)
		return;

	QMenu menu(this);
	menu.addAction(is_entry ? QStringLiteral("添加输入参数") : QStringLiteral("添加输出参数"),
		this, [this, node_guid, is_entry]() {
			PromptAddNodeParam(node_guid, is_entry);
		});
	if (!params.empty()) {
		auto* del_menu = menu.addMenu(QStringLiteral("删除参数"));
		for (const auto& arg : params) {
			const QString arg_name = QString::fromStdString(arg.name);
			del_menu->addAction(arg_name, this, [this, node_guid, arg_name]() {
				Dispatch(Action::DelParam, QJsonObject{
					{QStringLiteral("guid"), node_guid},
					{QStringLiteral("name"), arg_name},
				});
			});
		}
	}
	menu.exec(global_pos);
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
	if (event->type() == QEvent::KeyPress) {
		const auto key = static_cast<QKeyEvent*>(event)->key();
		if (key == Qt::Key_Delete) {
			if (watched == function_tree_) {
				OnDeleteFunction();
				return true;
			}
			if (watched == var_tree_) {
				OnDeleteVariable();
				return true;
			}
			if (watched == asset_tree_) {
				OnDeleteAsset();
				return true;
			}
		}
	}
	return QMainWindow::eventFilter(watched, event);
}
