#pragma execution_character_set("utf-8")

#include "MainWindow.h"
#include "GraphView.h"
#include "Theme.h"


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
#include <QDoubleValidator>
#include <QDockWidget>
#include <QEvent>
#include <QEventLoop>
#include <QFontMetrics>
#include <QFormLayout>
#include <QFrame>
#include <QHash>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QIntValidator>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMetaType>
#include <QMimeData>
#include <QPainter>
#include <QPixmap>
#include <QPixmap>
#include <QPointer>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QStringList>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
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
#include <cmath>
#include <functional>
#include <limits>
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

	void FillTypeCombo(QComboBox* combo, const QJsonArray& types)
	{
		if (!combo)
			return;
		const QString current = combo->currentText();
		const QSignalBlocker blocker(combo);
		combo->clear();
		for (const auto& row : types) {
			const QString name = row.toObject().value(g_key_name).toString();
			if (!name.isEmpty())
				combo->addItem(name);
		}
		const int index = combo->findText(current);
		if (index >= 0)
			combo->setCurrentIndex(index);
	}

	QIcon TypeMark(const QString& type)
	{
		if (type.isEmpty())
			return {};
		QPixmap pix(12, 12);
		pix.fill(Qt::transparent);
		QPainter painter(&pix);
		painter.setRenderHint(QPainter::Antialiasing);
		painter.setPen(Qt::NoPen);
		painter.setBrush(CG::Ui::TypeColor(type));
		painter.drawEllipse(QRectF(0.5, 0.5, 11, 11));
		return QIcon(pix);
	}

	void FillNamedItems(QTreeWidget* tree, const QJsonArray& rows)
	{
		if (!tree)
			return;
		const QSignalBlocker blocker(tree);
		tree->clear();
		for (const auto& row : rows) {
			const QJsonObject obj = row.toObject();
			const QString guid = obj.value(g_key_guid).toString();
			const QString name = obj.value(g_key_name).toString();
			if (guid.isEmpty() || name.isEmpty())
				continue;
			auto* item = new QTreeWidgetItem(tree);
			item->setFlags(item->flags() | Qt::ItemIsEditable);
			item->setData(0, Qt::UserRole, guid);
			item->setData(0, Qt::UserRole + 1, name);
			item->setText(0, name);
			const QString type = obj.value(g_key_type).toString();
			if (!type.isEmpty())
				item->setIcon(0, TypeMark(type));
		}
	}

	QString DetailText(const QJsonValue& value)
	{
		if (value.isBool())
			return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
		if (value.isDouble())
			return QString::number(value.toDouble());
		if (value.isString())
			return value.toString();
		return {};
	}

	QString DecodePinValue(const QString& value)
	{
		if (value.isEmpty()) return {};
		QJsonParseError error{};
		const auto doc = QJsonDocument::fromJson(("[" + value + "]").toUtf8(), &error);
		if (error.error != QJsonParseError::NoError || !doc.isArray() || doc.array().isEmpty()) return value;
		const auto decoded = doc.array().first();
		if (decoded.isString()) return decoded.toString();
		return QString::fromUtf8(QJsonDocument(QJsonArray{decoded}).toJson(QJsonDocument::Compact)).mid(1).chopped(1);
	}

	QString EncodePinValue(const QString& type, const QString& value)
	{
		QJsonValue encoded;
		if (type == QStringLiteral("bool"))
			encoded = value.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0 || value == QStringLiteral("1");
		else if (type == QStringLiteral("int") || type == QStringLiteral("int64") || type == QStringLiteral("uint") || type == QStringLiteral("uint64")) {
			if (type == QStringLiteral("uint") || type == QStringLiteral("uint64")) {
				bool ok = false;
				const qulonglong number = value.toULongLong(&ok);
				if (!ok) return {};
				return QString::number(number);
			}
			bool ok = false;
			const qlonglong number = value.toLongLong(&ok);
			if (!ok) return {};
			return QString::number(number);
		} else if (type == QStringLiteral("float") || type == QStringLiteral("double")) {
			bool ok = false;
			const double number = value.toDouble(&ok);
			if (!ok || !std::isfinite(number)) return {};
			encoded = number;
		} else if (type == QStringLiteral("std::string") || type == QStringLiteral("string"))
			encoded = value;
		else
			return value;
		return QString::fromUtf8(QJsonDocument(QJsonArray{encoded}).toJson(QJsonDocument::Compact)).mid(1).chopped(1);
	}

	enum class DetailKind { Leaf, List };

	struct DetailControl {
		DetailKind kind{DetailKind::Leaf};
		std::function<QWidget*(QWidget*, const QJsonObject&)> make;
		std::function<void(QWidget*, const QJsonValue&)> set;
	};

	const std::unordered_map<std::string, DetailControl>& DetailControls()
	{
		static const std::unordered_map<std::string, DetailControl> table = {
			{g_show_label, {DetailKind::Leaf,
				[](QWidget* parent, const QJsonObject& field) -> QWidget* {
					auto* label = new QLabel(DetailText(field.value(g_key_value)), parent);
					label->setWordWrap(true);
					label->setTextInteractionFlags(Qt::TextSelectableByMouse);
					return label;
				},
				[](QWidget* widget, const QJsonValue& value) {
					if (auto* label = qobject_cast<QLabel*>(widget))
						label->setText(DetailText(value));
				}}},
			{g_show_line, {DetailKind::Leaf,
				[](QWidget* parent, const QJsonObject& field) -> QWidget* {
					auto* edit = new QLineEdit(parent);
					edit->setText(DetailText(field.value(g_key_value)));
					return edit;
				},
				[](QWidget* widget, const QJsonValue& value) {
					if (auto* edit = qobject_cast<QLineEdit*>(widget))
						edit->setText(DetailText(value));
				}}},
			{g_show_combo, {DetailKind::Leaf,
				[](QWidget* parent, const QJsonObject& field) -> QWidget* {
					auto* box = new QComboBox(parent);
					const QString current = DetailText(field.value(g_key_value));
					bool found = current.isEmpty();
					for (const auto& item : field.value(g_key_items).toArray()) {
						const QString text = DetailText(item);
						if (text.isEmpty())
							continue;
						box->addItem(text);
						if (text == current)
							found = true;
					}
					if (!found)
						box->insertItem(0, current);
					box->setCurrentText(current);
					return box;
				},
				[](QWidget* widget, const QJsonValue& value) {
					auto* box = qobject_cast<QComboBox*>(widget);
					if (!box)
						return;
					const QString current = DetailText(value);
					if (box->findText(current) < 0 && !current.isEmpty())
						box->insertItem(0, current);
					box->setCurrentText(current);
				}}},
			{g_show_check, {DetailKind::Leaf,
				[](QWidget* parent, const QJsonObject& field) -> QWidget* {
					auto* box = new QCheckBox(parent);
					const QJsonValue value = field.value(g_key_value);
					box->setChecked(value.isBool() ? value.toBool() : DetailText(value) == QStringLiteral("true") || DetailText(value) == QStringLiteral("1"));
					return box;
				},
				[](QWidget* widget, const QJsonValue& value) {
					if (auto* box = qobject_cast<QCheckBox*>(widget))
						box->setChecked(value.isBool() ? value.toBool() : DetailText(value) == QStringLiteral("true") || DetailText(value) == QStringLiteral("1"));
				}}},
			{g_show_list, {DetailKind::List, {}, {}}},
		};
		return table;
	}

	const DetailControl* FindDetailControl(const QString& show)
	{
		const auto& table = DetailControls();
		const std::string key = show.isEmpty() ? std::string(g_show_label) : show.toStdString();
		if (auto it = table.find(key); it != table.end())
			return &it->second;
		if (auto it = table.find(g_show_label); it != table.end())
			return &it->second;
		return nullptr;
	}

	QTreeWidgetItem* FindTreeItem(QTreeWidget* tree, const QString& guid)
	{
		if (!tree || guid.isEmpty())
			return nullptr;
		for (int i = 0; i < tree->topLevelItemCount(); ++i) {
			auto* item = tree->topLevelItem(i);
			if (item && item->data(0, Qt::UserRole).toString() == guid)
				return item;
		}
		return nullptr;
	}

	QTreeWidgetItem* FindAssetItem(QTreeWidget* tree, const QString& guid)
	{
		if (!tree || guid.isEmpty())
			return nullptr;
		for (int i = 0; i < tree->topLevelItemCount(); ++i) {
			auto* folder = tree->topLevelItem(i);
			if (!folder)
				continue;
			for (int j = 0; j < folder->childCount(); ++j) {
				auto* item = folder->child(j);
				if (item && item->data(0, Qt::UserRole).toString() == guid)
					return item;
			}
		}
		return nullptr;
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
		if (plus_out)
			*plus_out = plus;
		h->addWidget(plus, 0, Qt::AlignVCenter);

		layout->addWidget(header);
		layout->addWidget(tree, 1);
		return root;
	}

	class FlowTree : public QTreeWidget {
	public:
		explicit FlowTree(QWidget* parent = nullptr) : QTreeWidget(parent)
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
				data->setData(kFlowMime, items.front()->data(0, Qt::UserRole).toString().toUtf8());
				data->setText(items.front()->text(0));
			}
			return data;
		}
	};

	class VarTree : public QTreeWidget {
	public:
		explicit VarTree(QWidget* parent = nullptr) : QTreeWidget(parent)
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
				data->setData(kVarMime, items.front()->data(0, Qt::UserRole).toString().toUtf8());
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

Q_DECLARE_METATYPE(Message)

MainWindow::MainWindow(QWidget* parent)
	: QMainWindow(parent)
{
	qRegisterMetaType<Message>();
	setStyleSheet(StyleSheet());
	setWindowTitle(QStringLiteral("ClassGraph"));

	BuildUi();
	InitAction();
	
	Load();
}

MainWindow::~MainWindow() = default;

void MainWindow::Dispatch(Action action, const QJsonObject& body)
{
	if (!sink_)return;
	const QString token = QUuid::createUuid().toString(QUuid::WithoutBraces);
	const QByteArray json = QJsonDocument(body).toJson(QJsonDocument::Compact);
	sink_(Message{action, 0,token.toStdString(), json.toStdString()});
}

void MainWindow::SetSink(std::function<void(const Message&)> sink)
{
	sink_ = std::move(sink);
}

void MainWindow::Notify(const Message& msg)
{
	auto emiter = FindEmiter(msg.act);
	if (emiter)
		emiter(msg);
}


void MainWindow::BuildUi()
{
	resize(1440, 900);

	setDockOptions(AllowTabbedDocks | AllowNestedDocks | AnimatedDocks);
	setTabPosition(Qt::AllDockWidgetAreas, QTabWidget::North);

	auto* file_menu = menuBar()->addMenu(QStringLiteral("File"));
	file_menu->addAction(QStringLiteral("New Blueprint"), this, &MainWindow::OnCreateBP, QKeySequence::New);
	auto* window_menu = menuBar()->addMenu(QStringLiteral("Window"));

	auto* toolbar = addToolBar(QStringLiteral("Blueprint"));
	toolbar->setMovable(false);
	toolbar->setIconSize(QSize(16, 16));

	compile_btn_ = new QToolButton;
	compile_btn_->setObjectName(QStringLiteral("CompileButton"));
	compile_btn_->setText(QStringLiteral("Compile"));
	compile_btn_->setToolTip(QStringLiteral("Generate C++ from this Blueprint"));
	connect(compile_btn_, &QToolButton::clicked, this, &MainWindow::OnCompile);
	toolbar->addWidget(compile_btn_);

	play_btn_ = new QToolButton;
	play_btn_->setObjectName(QStringLiteral("PlayButton"));
	play_btn_->setText(QStringLiteral("Play"));
	play_btn_->setToolTip(QStringLiteral("Run the current graph"));
	connect(play_btn_, &QToolButton::clicked, this, &MainWindow::OnRunFunc);
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
	graph_tabs_->setTabsClosable(true);
	tab_layout->addWidget(graph_tabs_);
	center_layout->addWidget(graph_tab_frame_);

	graph_stack_ = new QStackedWidget(center);
	center_layout->addWidget(graph_stack_, 1);

	empty_hint_ = new QLabel(tr("从底部 Content Browser 打开蓝图"));
	empty_hint_->setObjectName(QStringLiteral("PanelHint"));
	empty_hint_->setAlignment(Qt::AlignCenter);
	center_layout->addWidget(empty_hint_, 1);
	setCentralWidget(center);

	QToolButton* fn_plus = nullptr;
	QToolButton* var_plus = nullptr;
	func_tree_ = new FlowTree(this);
	var_tree_ = new VarTree(this);
	auto* fn_section = MakeAddSection(tr("函数"), func_tree_, &fn_plus, nullptr);
	auto* var_section = MakeAddSection(tr("变量"), var_tree_, &var_plus, &var_type_combo_);

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
	bp_name_edit_->setToolTip(tr("蓝图名称（C++ 类名）"));
	connect(bp_name_edit_, &QLineEdit::editingFinished, this, &MainWindow::OnAlterBP);
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
	bp_plus->setToolTip(tr("新建蓝图"));
	bp_plus->setAutoRaise(false);
	bp_plus->setFixedSize(22, 22);
	bp_title_row->addWidget(bp_plus, 0, Qt::AlignVCenter);
	bp_dock_->setTitleBarWidget(bp_title);
	connect(bp_plus, &QToolButton::clicked, this, &MainWindow::OnCreateBP);
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
	asset_plus->setToolTip(tr("新建蓝图"));
	asset_plus->setAutoRaise(false);
	asset_plus->setFixedSize(22, 22);
	asset_title_row->addWidget(asset_plus, 0, Qt::AlignVCenter);
	asset_dock->setTitleBarWidget(asset_title);
	connect(asset_plus, &QToolButton::clicked, this, &MainWindow::OnCreateBP);
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
	log_clear->setToolTip(tr("清空日志"));
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

	connect(fn_plus, &QToolButton::clicked, this, &MainWindow::OnAddFunc);
	connect(var_plus, &QToolButton::clicked, this, &MainWindow::OnAddVar);

	connect(func_tree_, &QTreeWidget::itemClicked, this, &MainWindow::OnFuncSelected);
	connect(func_tree_, &QTreeWidget::itemDoubleClicked, this, &MainWindow::OnFuncSelected);
	connect(func_tree_, &QTreeWidget::itemChanged, this, &MainWindow::OnAlterFunc);
	connect(func_tree_, &QTreeWidget::customContextMenuRequested, this, &MainWindow::OnFuncMenu);

	connect(var_tree_, &QTreeWidget::itemClicked, this, &MainWindow::OnVarSelected);
	connect(var_tree_, &QTreeWidget::itemChanged, this, &MainWindow::OnAlterVar);
	connect(var_tree_, &QTreeWidget::customContextMenuRequested, this, &MainWindow::OnVarMenu);

	if (var_type_combo_) {
		if (auto* view = var_type_combo_->view())
			view->viewport()->setAttribute(Qt::WA_NoMouseReplay, true);
		connect(var_type_combo_, QOverload<int>::of(&QComboBox::activated),
			this, &MainWindow::OnVarTypeChosen);
	}

	connect(graph_tabs_, &QTabBar::currentChanged, this, &MainWindow::OnGraphTabChanged);
	connect(graph_tabs_, &QTabBar::tabCloseRequested, this, &MainWindow::OnGraphTabClose);
	connect(asset_tree_, &QTreeWidget::itemDoubleClicked, this, &MainWindow::OnOpenBP);
	connect(asset_tree_, &QTreeWidget::itemChanged, this, &MainWindow::OnAlterAsset);
	connect(asset_tree_, &QTreeWidget::customContextMenuRequested, this, &MainWindow::OnAssetMenu);

	func_tree_->installEventFilter(this);
	var_tree_->installEventFilter(this);
	asset_tree_->installEventFilter(this);
}

void MainWindow::InitAction()
{
	RegisterAction(Action::AddBP, &MainWindow::Sig_AddBP, &MainWindow::Slot_AddBP);
	RegisterAction(Action::DelBP, &MainWindow::Sig_DelBP, &MainWindow::Slot_DelBP);
	RegisterAction(Action::AltBP, &MainWindow::Sig_AltBP, &MainWindow::Slot_AltBP);
	RegisterAction(Action::OpenBP, &MainWindow::Sig_OpenBP, &MainWindow::Slot_OpenBP);
	RegisterAction(Action::CloseBP, &MainWindow::Sig_CloseBP, &MainWindow::Slot_CloseBP);
	RegisterAction(Action::TypeSync, &MainWindow::Sig_TypeSync, &MainWindow::Slot_TypeSync);
	RegisterAction(Action::FuncSync, &MainWindow::Sig_FuncSync, &MainWindow::Slot_FuncSync);
	RegisterAction(Action::AddFunc, &MainWindow::Sig_AddFunc, &MainWindow::Slot_AddFunc);
	RegisterAction(Action::DelFunc, &MainWindow::Sig_DelFunc, &MainWindow::Slot_DelFunc);
	RegisterAction(Action::AltFunc, &MainWindow::Sig_AltFunc, &MainWindow::Slot_AltFunc);
	RegisterAction(Action::AddVar, &MainWindow::Sig_AddVar, &MainWindow::Slot_AddVar);
	RegisterAction(Action::DelVar, &MainWindow::Sig_DelVar, &MainWindow::Slot_DelVar);
	RegisterAction(Action::AltVar, &MainWindow::Sig_AltVar, &MainWindow::Slot_AltVar);
	RegisterAction(Action::SelectFunc, &MainWindow::Sig_SelectFunc, &MainWindow::Slot_SelectFunc);
	RegisterAction(Action::SelectVar, &MainWindow::Sig_SelectVar, &MainWindow::Slot_SelectVar);
	RegisterAction(Action::SelectNode, &MainWindow::Sig_SelectNode, &MainWindow::Slot_SelectNode);
	RegisterAction(Action::AddNode, &MainWindow::Sig_AddNode, &MainWindow::Slot_AddNode);
	RegisterAction(Action::DelNode, &MainWindow::Sig_DelNode, &MainWindow::Slot_DelNode);
	RegisterAction(Action::AltNode, &MainWindow::Sig_AltNode, &MainWindow::Slot_AltNode);
	RegisterAction(Action::AddLine, &MainWindow::Sig_AddLine, &MainWindow::Slot_AddLine);
	RegisterAction(Action::DelLine, &MainWindow::Sig_DelLine, &MainWindow::Slot_DelLine);
	RegisterAction(Action::Compile, &MainWindow::Sig_Compile, &MainWindow::Slot_Compile);
	RegisterAction(Action::RunFunc, &MainWindow::Sig_RunFunc, &MainWindow::Slot_RunFunc);
	RegisterAction(Action::RuntimeSync, &MainWindow::Sig_RuntimeSync, &MainWindow::Slot_RuntimeSync);
	RegisterAction(Action::LogOutput, &MainWindow::Sig_Output, &MainWindow::Slot_Output);
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

Emiter MainWindow::FindEmiter(Action action) const
{
	if(auto it = emiters_.find(action); it != emiters_.end())
		return it->second;
	return {};
}

void MainWindow::OnLog(const QString& text)
{
	AppendLog(text);
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

void MainWindow::ResetEditor()
{
	ClearGraphPages();
	SetEditorVisible(false);
	current_asset_guid_.clear();
	if (bp_name_edit_) {
		QSignalBlocker blocker(bp_name_edit_);
		bp_name_edit_->clear();
	}
	if (func_tree_) {
		QSignalBlocker blocker(func_tree_);
		func_tree_->clear();
	}
	if (var_tree_) {
		QSignalBlocker blocker(var_tree_);
		var_tree_->clear();
	}
	if (breadcrumb_)
		breadcrumb_->setText(QStringLiteral("Blueprints"));
	ClearDetails();
}

GraphView* MainWindow::FindGraphView(const QString& guid) const
{
	const int index = FindGraphTab(guid);
	if (index < 0 || !graph_stack_)
		return nullptr;
	return qobject_cast<GraphView*>(graph_stack_->widget(index));
}

void MainWindow::PutGraphNode(const QString& flow, const QString& guid, const QString& name, const QString& key, const QString& group, double x, double y, const QJsonArray& pins)
{
	if (flow.isEmpty() || guid.isEmpty() || name.isEmpty())
		return;
	auto& nodes = flow_nodes_[flow.toStdString()];
	bool found = false;
	for (auto& node : nodes) {
		if (node.guid != guid)
			continue;
		node.name = name;
		node.key = key;
		node.group = group;
		node.x = x;
		node.y = y;
		node.pins = pins;
		found = true;
		break;
	}
	if (!found)
		nodes.push_back(GraphNodeState{guid, name, key, group, x, y, pins});
	if (auto* view = FindGraphView(flow)) {
		if (found) view->setNodePins(guid, pins);
		else view->addNode(guid, name, key, group, QPointF(x, y), pins);
	}
}

void MainWindow::ForgetGraphNode(const QString& flow, const QString& guid)
{
	if (flow.isEmpty() || guid.isEmpty())
		return;
	auto it = flow_nodes_.find(flow.toStdString());
	if (it == flow_nodes_.end())
		return;
	auto& nodes = it->second;
	nodes.erase(std::remove_if(nodes.begin(), nodes.end(), [&](const GraphNodeState& node) {
		return node.guid == guid;
	}), nodes.end());
	if (auto* view = FindGraphView(flow))
		view->removeNode(guid);
	auto line_it = flow_lines_.find(flow.toStdString());
	if (line_it != flow_lines_.end()) {
		for (const auto& line : line_it->second)
			if (line.fromNode == guid || line.toNode == guid)
				if (auto* view = FindGraphView(flow)) view->removeLine(line.guid);
		line_it->second.erase(std::remove_if(line_it->second.begin(), line_it->second.end(), [&](const GraphLineState& line) {
			return line.fromNode == guid || line.toNode == guid;
		}), line_it->second.end());
	}
}

void MainWindow::MoveGraphNode(const QString& flow, const QString& guid, double x, double y)
{
	if (flow.isEmpty() || guid.isEmpty())
		return;
	auto it = flow_nodes_.find(flow.toStdString());
	if (it != flow_nodes_.end()) {
		for (auto& node : it->second) {
			if (node.guid != guid)
				continue;
			node.x = x;
			node.y = y;
			break;
		}
	}
	if (auto* view = FindGraphView(flow))
		view->moveNode(guid, QPointF(x, y));
}

void MainWindow::SetGraphNodeRuntimeStatus(const QString& flow, const QString& guid, const QString& status)
{
	if (auto it = flow_nodes_.find(flow.toStdString()); it != flow_nodes_.end()) {
		for (auto& node : it->second) {
			if (node.guid == guid) {
				node.runtime_status = status;
				break;
			}
		}
	}
	if (auto* view = FindGraphView(flow))
		view->setNodeRuntimeStatus(guid, status);
}

bool MainWindow::FlowHasNode(const QString& flow, const QString& key) const
{
	auto it = flow_nodes_.find(flow.toStdString());
	if (it == flow_nodes_.end())
		return false;
	for (const auto& node : it->second) {
		if (node.key == key)
			return true;
	}
	return false;
}

void MainWindow::RestoreGraphNodes(GraphView* view)
{
	if (!view)
		return;
	auto it = flow_nodes_.find(view->guid().toStdString());
	if (it != flow_nodes_.end()) for (const auto& node : it->second)
		view->addNode(node.guid, node.name, node.key, node.group, QPointF(node.x, node.y), node.pins);
	const auto line_it = flow_lines_.find(view->guid().toStdString());
	if (line_it != flow_lines_.end()) for (const auto& line : line_it->second)
		view->addLine(line.guid, line.fromNode, line.fromPin, line.toNode, line.toPin);
	if (it != flow_nodes_.end()) for (const auto& node : it->second)
		view->setNodeRuntimeStatus(node.guid, node.runtime_status);
}

void MainWindow::PutGraphLine(const QString& flow, const QString& guid, const QString& fromNode, const QString& fromPin, const QString& toNode, const QString& toPin)
{
	if (flow.isEmpty() || guid.isEmpty()) return;
	auto& lines = flow_lines_[flow.toStdString()];
	for (auto& line : lines) if (line.guid == guid) {
		line.fromNode = fromNode; line.fromPin = fromPin; line.toNode = toNode; line.toPin = toPin;
		if (auto* view = FindGraphView(flow)) view->addLine(guid, fromNode, fromPin, toNode, toPin);
		RefreshGraphPinConnections(flow);
		return;
	}
	lines.push_back({guid, fromNode, fromPin, toNode, toPin});
	if (auto* view = FindGraphView(flow)) view->addLine(guid, fromNode, fromPin, toNode, toPin);
	RefreshGraphPinConnections(flow);
}

void MainWindow::ForgetGraphLine(const QString& flow, const QString& guid)
{
	if (flow.isEmpty() || guid.isEmpty()) return;
	auto it = flow_lines_.find(flow.toStdString());
	if (it != flow_lines_.end()) it->second.erase(std::remove_if(it->second.begin(), it->second.end(), [&](const GraphLineState& line) { return line.guid == guid; }), it->second.end());
	if (auto* view = FindGraphView(flow)) view->removeLine(guid);
	RefreshGraphPinConnections(flow);
}

void MainWindow::RefreshGraphPinConnections(const QString& flow)
{
	auto nodes = flow_nodes_.find(flow.toStdString());
	if (nodes == flow_nodes_.end()) return;
	auto lines = flow_lines_.find(flow.toStdString());
	for (auto& node : nodes->second) {
		for (int i = 0; i < node.pins.size(); ++i) {
			auto row = node.pins.at(i);
			QJsonObject pin = row.toObject();
			const QString pin_guid = pin.value(g_key_guid).toString();
			const bool input = pin.value(g_key_direct).toInt() == 0;
			bool connected = false;
			if (lines != flow_lines_.end()) for (const auto& line : lines->second) {
				if ((input && line.toNode == node.guid && line.toPin == pin_guid)
					|| (!input && line.fromNode == node.guid && line.fromPin == pin_guid)) {
					connected = true;
					break;
				}
			}
			pin[g_key_connected] = connected;
			node.pins[i] = pin;
		}
		if (auto* view = FindGraphView(flow)) view->setNodePins(node.guid, node.pins);
	}
}

int MainWindow::FindGraphTab(const QString& guid) const
{
	if (!graph_tabs_ || guid.isEmpty())
		return -1;
	for (int i = 0; i < graph_tabs_->count(); ++i) {
		if (graph_tabs_->tabData(i).toString() == guid)
			return i;
	}
	return -1;
}

void MainWindow::OpenGraphPage(const QString& guid, const QString& name)
{
	if (!graph_tabs_ || !graph_stack_ || guid.isEmpty())
		return;

	const QString title = name.isEmpty() ? guid : name;
	int index = FindGraphTab(guid);
	if (index < 0) {
		auto* view = new GraphView(guid, graph_stack_);
		connect(view, &GraphView::functionDropped, this, &MainWindow::OnFuncDropped);
		connect(view, &GraphView::variableDropped, this, &MainWindow::OnVarDropped);
		connect(view, &GraphView::contextMenuRequested, this, &MainWindow::OnGraphMenu);
		connect(view, &GraphView::nodeMoved, this, &MainWindow::OnNodeMoved);
		connect(view, &GraphView::nodeSelected, this, &MainWindow::OnNodeSelected);
		connect(view, &GraphView::pinValueEditRequested, this,
			[this](const QString& node, const QString& pin, const QString& type, const QString& value) {
				const QString serialized = EncodePinValue(type, value);
				if (serialized.isEmpty() && !value.isEmpty()) {
					AppendLog(QStringLiteral("Invalid pin value."));
					return;
				}
				Dispatch(Action::AltNode, QJsonObject{{g_key_guid, node}, {g_key_pin, pin}, {g_key_value, serialized}});
			});
		connect(view, &GraphView::lineRequested, this, &MainWindow::OnLineRequested);
		connect(view, &GraphView::methodMenuRequested, this, &MainWindow::OnPinMethodMenu);
		connect(view, &GraphView::deleteLineRequested, this, [this](const QString& guid) {
			Dispatch(Action::DelLine, QJsonObject{{g_key_guid, guid}});
		});
		connect(view, &GraphView::deleteNodeRequested, this, [this](const QString& node_guid, const QString& key) {
			if (key == QString::fromUtf8(g_node_entry) || node_guid.isEmpty())
				return;
			Dispatch(Action::DelNode, QJsonObject{{g_key_guid, node_guid}});
		});
		index = graph_stack_->addWidget(view);
		RestoreGraphNodes(view);
		QSignalBlocker blocker(graph_tabs_);
		const int tab = graph_tabs_->addTab(title);
		graph_tabs_->setTabData(tab, guid);
		graph_tabs_->setCurrentIndex(tab);
		graph_stack_->setCurrentIndex(index);
		return;
	}

	graph_tabs_->setTabText(index, title);
	QSignalBlocker blocker(graph_tabs_);
	graph_tabs_->setCurrentIndex(index);
	graph_stack_->setCurrentIndex(index);
}

void MainWindow::CloseGraphPage(const QString& guid)
{
	if (!graph_tabs_ || !graph_stack_)
		return;
	const int index = FindGraphTab(guid);
	if (index < 0)
		return;

	QWidget* page = graph_stack_->widget(index);
	{
		QSignalBlocker blocker(graph_tabs_);
		graph_tabs_->removeTab(index);
		if (page)
			graph_stack_->removeWidget(page);
	}
	delete page;

	const int current = graph_tabs_->currentIndex();
	if (current < 0 || current >= graph_stack_->count())
		return;
	graph_stack_->setCurrentIndex(current);
	const QString next = graph_tabs_->tabData(current).toString();
	if (!next.isEmpty())
		Dispatch(Action::SelectFunc, QJsonObject{{g_key_guid, next}});
}

void MainWindow::RenameGraphPage(const QString& guid, const QString& name)
{
	const int index = FindGraphTab(guid);
	if (index >= 0 && !name.isEmpty())
		graph_tabs_->setTabText(index, name);
}

void MainWindow::ClearGraphPages()
{
	if (!graph_tabs_ || !graph_stack_)
		return;
	QSignalBlocker blocker(graph_tabs_);
	while (graph_tabs_->count() > 0)
		graph_tabs_->removeTab(graph_tabs_->count() - 1);
	while (graph_stack_->count() > 0) {
		QWidget* page = graph_stack_->widget(0);
		graph_stack_->removeWidget(page);
		delete page;
	}
	flow_nodes_.clear();
	flow_lines_.clear();
}

void MainWindow::ClearDetails()
{
	detail_fields_.clear();
	detail_kind_.clear();
	detail_guid_.clear();
	if (!details_form_)
		return;
	while (details_form_->rowCount() > 0)
		details_form_->removeRow(0);
}

void MainWindow::ShowDetails(const QJsonObject& detail)
{
	ClearDetails();
	const QJsonObject target = detail.value(g_key_target).toObject();
	detail_kind_ = target.value(g_key_kind).toString();
	detail_guid_ = target.value(g_key_guid).toString();
	AppendDetails(detail.value(g_key_fields).toArray(), QString());
}

void MainWindow::AppendDetails(const QJsonArray& fields, const QString& prefix)
{
	if (!details_form_ || !details_host_)
		return;
	AppendDetailsTo(details_form_, fields, prefix);
}

void MainWindow::AppendDetailsTo(QFormLayout* form, const QJsonArray& fields, const QString& prefix)
{
	if (!form || !details_host_)
		return;

	const auto makeSection = [this](const QString& title, const std::function<void(QFormLayout*)>& fill, const QString& badge) {
		auto* section = new QWidget(details_host_);
		section->setObjectName(QStringLiteral("DetailSection"));
		auto* layout = new QVBoxLayout(section);
		layout->setContentsMargins(0, 2, 0, 2);
		layout->setSpacing(0);

		auto* header = new QWidget(section);
		auto* header_layout = new QHBoxLayout(header);
		header_layout->setContentsMargins(0, 0, 0, 0);
		header_layout->setSpacing(8);
		auto* toggle = new QToolButton(section);
		toggle->setObjectName(QStringLiteral("DetailSectionToggle"));
		toggle->setText(title);
		toggle->setCheckable(true);
		toggle->setChecked(true);
		toggle->setAutoRaise(true);
		toggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
		toggle->setArrowType(Qt::DownArrow);
		toggle->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
		header_layout->addWidget(toggle, 1);
		if (!badge.isEmpty()) {
			auto* badge_label = new QLabel(badge, header);
			badge_label->setObjectName(QStringLiteral("DetailSectionBadge"));
			header_layout->addWidget(badge_label);
		}
		layout->addWidget(header);

		auto* body = new QWidget(section);
		auto* body_layout = new QFormLayout(body);
		body_layout->setContentsMargins(18, 0, 0, 2);
		body_layout->setHorizontalSpacing(8);
		body_layout->setVerticalSpacing(6);
		body_layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
		layout->addWidget(body);
		fill(body_layout);
		connect(toggle, &QToolButton::toggled, section, [body, toggle](bool visible) {
			body->setVisible(visible);
			toggle->setArrowType(visible ? Qt::DownArrow : Qt::RightArrow);
		});
		return section;
	};

	for (const auto& row : fields) {
		QJsonObject field = row.toObject();
		const QString show = field.value(g_key_show).toString();
		const QString key = field.value(g_key_key).toString();
		if (detail_kind_ == QString::fromUtf8(g_kind_var)
			&& key == QString::fromUtf8(g_key_value)
			&& field.value(g_key_value).isString())
			field[g_key_value] = DecodePinValue(field.value(g_key_value).toString());
		const QString text = field.value(g_key_text).toString();
		const DetailControl* control = FindDetailControl(show);
		if (!control)
			continue;

		if (control->kind == DetailKind::List) {
			const auto groups = field.value(g_key_value).toArray();
			const QString list_title = text.isEmpty() ? key : text;
			const QJsonArray columns = field.value(g_key_columns).toArray();
			if (detail_kind_ == QString::fromUtf8(g_kind_node) && key == QString::fromUtf8(g_key_pins)) {
				const bool can_edit_params = field.value(g_key_editable).toBool();
				QString pin_prefix = prefix.isEmpty() ? key : prefix + QStringLiteral(".") + key;
				if (!pin_prefix.isEmpty()) pin_prefix += QLatin1Char('.');
				auto* pins_section = makeSection(list_title, [this, groups, pin_prefix, can_edit_params, &makeSection](QFormLayout* pin_list) {
					for (const auto& group_row : groups) {
						const QJsonObject group = group_row.toObject();
						const QString pin_guid = group.value(g_key_key).toString();
						const QString pin_name = group.value(g_key_text).toString();
						const QJsonArray pin_fields = group.value(g_key_fields).toArray();
						QString pin_type;
						for (const auto& pin_field_row : pin_fields) {
							const QJsonObject pin_field = pin_field_row.toObject();
							if (pin_field.value(g_key_key).toString() == QString::fromUtf8(g_key_type)) {
								pin_type = DetailText(pin_field.value(g_key_value));
								break;
							}
						}
						const QString type_key = pin_prefix + pin_guid + QLatin1Char('.') + QString::fromUtf8(g_key_type);
						auto* pin_section = makeSection(pin_name, [this, pin_fields, pin_type, pin_prefix, pin_guid](QFormLayout* pin_form) {
							for (const auto& pin_field_row : pin_fields) {
								QJsonObject pin_field = pin_field_row.toObject();
								const QString field_key = pin_field.value(g_key_key).toString();
								if (field_key == QString::fromUtf8(g_key_name) || field_key == QString::fromUtf8(g_key_type))
									continue;
								if (field_key == QString::fromUtf8(g_key_value))
									pin_field[g_key_value] = DecodePinValue(DetailText(pin_field.value(g_key_value)));
								const QString show = pin_field.value(g_key_show).toString();
								const DetailControl* pin_control = FindDetailControl(show);
								if (!pin_control || !pin_control->make)
									continue;
								QWidget* widget = pin_control->make(details_host_, pin_field);
								if (!widget)
									continue;
								const bool is_value = field_key == QString::fromUtf8(g_key_value);
								const bool editable = is_value && (pin_field.contains(g_key_editable)
									? pin_field.value(g_key_editable).toBool()
									: show != QString::fromUtf8(g_show_label));
								if (auto* edit = qobject_cast<QLineEdit*>(widget)) {
									edit->setObjectName(is_value ? QStringLiteral("PinValueEdit") : QStringLiteral("DetailValueEdit"));
									if (pin_type == QStringLiteral("int")) edit->setValidator(new QIntValidator(edit));
									else if (pin_type == QStringLiteral("float") || pin_type == QStringLiteral("double")) edit->setValidator(new QDoubleValidator(edit));
									if (!editable) edit->setReadOnly(true);
									else edit->setPlaceholderText(QStringLiteral("输入值"));
								} else if (auto* label = qobject_cast<QLabel*>(widget); label && is_value) {
									label->setObjectName(QStringLiteral("PinValueDisplay"));
								} else if (auto* check = qobject_cast<QCheckBox*>(widget)) {
									check->setObjectName(QStringLiteral("PinValueCheck"));
									check->setText(QString{});
									check->setEnabled(editable);
								}
								const QString detail_key = pin_prefix + pin_guid + QLatin1Char('.') + field_key;
								if (!detail_key.isEmpty() && pin_control->set)
									detail_fields_.insert({detail_key.toStdString(), DetailField{widget, pin_control->set}});
								auto* field_label = new QLabel(pin_field.value(g_key_text).toString(), details_host_);
								field_label->setObjectName(QStringLiteral("PinParamLabel"));
								pin_form->addRow(field_label, widget);
								if (!editable || detail_key.isEmpty())
									continue;
								if (auto* edit = qobject_cast<QLineEdit*>(widget)) {
									connect(edit, &QLineEdit::editingFinished, this, [this, detail_key, edit] {
										CommitDetail(detail_key, edit->text());
									});
								} else if (auto* check = qobject_cast<QCheckBox*>(widget)) {
									connect(check, &QCheckBox::toggled, this, [this, detail_key](bool value) {
										CommitDetail(detail_key, value);
									});
								}
							}
						}, pin_type);
						if (auto* type_label = pin_section->findChild<QLabel*>(QStringLiteral("DetailSectionBadge")))
							detail_fields_.insert({type_key.toStdString(), DetailField{type_label, [](QWidget* widget, const QJsonValue& value) {
								if (auto* label = qobject_cast<QLabel*>(widget)) label->setText(DetailText(value));
							}}});
						if (can_edit_params) {
							auto* toggle = pin_section->findChild<QToolButton*>(QStringLiteral("DetailSectionToggle"));
							auto* header = toggle ? toggle->parentWidget() : nullptr;
							auto* header_layout = header ? qobject_cast<QHBoxLayout*>(header->layout()) : nullptr;
							if (header_layout) {
								auto* remove = new QToolButton(pin_section);
								remove->setIcon(style()->standardIcon(QStyle::SP_TrashIcon));
								remove->setToolTip(QStringLiteral("删除参数"));
								remove->setAutoRaise(true);
								header_layout->addWidget(remove);
								connect(remove, &QToolButton::clicked, this, [this, node_guid = detail_guid_, pin_guid] {
									Dispatch(Action::AltNode, QJsonObject{
										{g_key_guid, node_guid},
										{g_key_param_action, QStringLiteral("delete")},
										{g_key_pin, pin_guid},
									});
								});
							}
						}
						pin_list->addRow(pin_section);
					}
					if (can_edit_params) {
						auto* editor = new QWidget(details_host_);
						auto* editor_layout = new QHBoxLayout(editor);
						editor_layout->setContentsMargins(0, 4, 0, 0);
						editor_layout->setSpacing(6);
						auto* name_edit = new QLineEdit(editor);
						name_edit->setObjectName(QStringLiteral("DetailParamNameEdit"));
						name_edit->setPlaceholderText(QStringLiteral("参数名称"));
						auto* type_combo = new QComboBox(editor);
						type_combo->setObjectName(QStringLiteral("DetailParamTypeCombo"));
						if (var_type_combo_) {
							for (int i = 0; i < var_type_combo_->count(); ++i) {
								const QString type = var_type_combo_->itemText(i);
								if (!type.isEmpty()) type_combo->addItem(type, var_type_combo_->itemData(i));
							}
						}
						auto* add = new QToolButton(editor);
						add->setText(QStringLiteral("+"));
						add->setToolTip(QStringLiteral("添加参数"));
						add->setAutoRaise(true);
						add->setEnabled(type_combo->count() > 0);
						editor_layout->addWidget(name_edit, 1);
						editor_layout->addWidget(type_combo);
						editor_layout->addWidget(add);
						pin_list->addRow(editor);
						auto add_param = [this, node_guid = detail_guid_, name_edit, type_combo] {
							const QString name = name_edit->text().trimmed();
							if (name.isEmpty() || type_combo->currentText().isEmpty()) return;
							Dispatch(Action::AltNode, QJsonObject{
								{g_key_guid, node_guid},
								{g_key_param_action, QStringLiteral("add")},
								{g_key_name, name},
								{g_key_type, type_combo->currentText()},
							});
						};
						connect(add, &QToolButton::clicked, this, add_param);
						connect(name_edit, &QLineEdit::returnPressed, this, add_param);
					}
				}, {});
				form->addRow(pins_section);
				continue;
			}
			QString list_prefix = prefix;
			if (!key.isEmpty())
				list_prefix = prefix.isEmpty() ? key : prefix + QStringLiteral(".") + key;
			if (!columns.isEmpty()) {
				auto* table = new QTreeWidget(details_host_);
				table->setRootIsDecorated(false);
				table->setIndentation(0);
				table->setColumnCount(columns.size());
				QStringList headers;
				for (const auto& column : columns)
					headers.append(column.toObject().value(g_key_text).toString());
				table->setHeaderLabels(headers);
				table->setSelectionMode(QAbstractItemView::NoSelection);
				table->setFocusPolicy(Qt::NoFocus);
				table->setEditTriggers(QAbstractItemView::NoEditTriggers);
				table->header()->setStretchLastSection(true);
				for (int i = 0; i < columns.size(); ++i)
					table->header()->setSectionResizeMode(i, QHeaderView::Stretch);

				for (const auto& group_row : groups) {
					const QJsonObject group = group_row.toObject();
					QHash<QString, QString> values;
					for (const auto& detail_row : group.value(g_key_fields).toArray()) {
						const QJsonObject detail = detail_row.toObject();
						values.insert(detail.value(g_key_key).toString(), DetailText(detail.value(g_key_value)));
					}
					auto* item = new QTreeWidgetItem(table);
					for (int i = 0; i < columns.size(); ++i) {
						const QString column_key = columns.at(i).toObject().value(g_key_key).toString();
						item->setText(i, column_key == QString::fromUtf8(g_key_name)
							? group.value(g_key_text).toString()
							: values.value(column_key));
					}
				}

				auto* section = makeSection(list_title, [table](QFormLayout* body) {
					body->addRow(table);
				}, {});
				form->addRow(section);
				continue;
			}
				auto* list = makeSection(list_title, [this, groups, list_prefix, &makeSection](QFormLayout* list_form) {
				for (const auto& group_row : groups) {
					const QJsonObject group = group_row.toObject();
					const QString group_key = group.value(g_key_key).toString();
					const QString group_text = group.value(g_key_text).toString();
					QString next = list_prefix;
					if (!group_key.isEmpty())
						next = list_prefix.isEmpty() ? group_key : list_prefix + QStringLiteral(".") + group_key;
					if (!next.isEmpty())
						next += QStringLiteral(".");
						auto* group_widget = makeSection(group_text.isEmpty() ? group_key : group_text,
						[this, fields = group.value(g_key_fields).toArray(), next](QFormLayout* group_form) {
							AppendDetailsTo(group_form, fields, next);
							}, {});
					list_form->addRow(group_widget);
				}
				}, {});
			form->addRow(list);
			continue;
		}

		if (!control->make)
			continue;
		QWidget* widget = control->make(details_host_, field);
		if (!widget)
			continue;

		const bool editable = field.contains(g_key_editable)
			? field.value(g_key_editable).toBool()
			: show != QString::fromUtf8(g_show_label);
		if (!editable) {
			if (auto* edit = qobject_cast<QLineEdit*>(widget))
				edit->setReadOnly(true);
			else if (qobject_cast<QComboBox*>(widget) || qobject_cast<QCheckBox*>(widget))
				widget->setEnabled(false);
		}
		form->addRow(text, widget);

		const QString detail_key = prefix + key;
		if (!key.isEmpty() && control->set)
			detail_fields_.insert({detail_key.toStdString(), DetailField{widget, control->set}});
		if (!editable || key.isEmpty())
			continue;

		if (auto* edit = qobject_cast<QLineEdit*>(widget)) {
			connect(edit, &QLineEdit::editingFinished, this, [this, detail_key, edit] {
				CommitDetail(detail_key, edit->text());
			});
		} else if (auto* combo = qobject_cast<QComboBox*>(widget)) {
			connect(combo, &QComboBox::currentTextChanged, this, [this, detail_key](const QString& value) {
				CommitDetail(detail_key, value);
			});
		} else if (auto* check = qobject_cast<QCheckBox*>(widget)) {
			connect(check, &QCheckBox::toggled, this, [this, detail_key](bool value) {
				CommitDetail(detail_key, value);
			});
		}
	}
}

void MainWindow::SetDetailValue(const QString& key, const QJsonValue& value)
{
	auto it = detail_fields_.find(key.toStdString());
	if (it == detail_fields_.end() || !it->second.widget || !it->second.set)
		return;
	const QSignalBlocker blocker(it->second.widget);
	it->second.set(it->second.widget, value);
}

void MainWindow::CommitDetail(const QString& key, const QJsonValue& value)
{
	if (key.isEmpty() || detail_guid_.isEmpty())
		return;

	if (detail_kind_ == QString::fromUtf8(g_kind_blueprint) && key == QString::fromUtf8(g_key_name)) {
		Dispatch(Action::AltBP, QJsonObject{{g_key_guid, detail_guid_}, {g_key_name, value}});
	} else if (detail_kind_ == QString::fromUtf8(g_kind_flow) && key == QString::fromUtf8(g_key_name)) {
		Dispatch(Action::AltFunc, QJsonObject{{g_key_guid, detail_guid_}, {g_key_name, value}});
	} else if (detail_kind_ == QString::fromUtf8(g_kind_var)
		&& key == QString::fromUtf8(g_key_name)) {
		Dispatch(Action::AltVar, QJsonObject{{g_key_guid, detail_guid_}, {g_key_name, value}});
	} else if (detail_kind_ == QString::fromUtf8(g_kind_node)) {
		if (key == QString::fromUtf8(g_key_x) || key == QString::fromUtf8(g_key_y)) {
			QJsonObject body{{g_key_guid, detail_guid_}};
			body.insert(key, value);
			Dispatch(Action::AltNode, body);
			return;
		}
		const QStringList parts = key.split(QLatin1Char('.'));
		if (parts.size() == 3 && parts[0] == QString::fromUtf8(g_key_pins)
			&& parts[2] == QString::fromUtf8(g_key_value)) {
			const QString type_key = parts[0] + QLatin1Char('.') + parts[1] + QLatin1Char('.') + QString::fromUtf8(g_key_type);
			const auto type_it = detail_fields_.find(type_key.toStdString());
			const QString type = type_it != detail_fields_.end() && qobject_cast<QLabel*>(type_it->second.widget)
				? qobject_cast<QLabel*>(type_it->second.widget)->text() : QString{};
			const QString plain_value = DetailText(value);
			const QString serialized = EncodePinValue(type, plain_value);
			if (serialized.isEmpty() && !plain_value.isEmpty()) {
				AppendLog(QStringLiteral("Invalid pin value."));
				return;
			}
			Dispatch(Action::AltNode, QJsonObject{
				{g_key_guid, detail_guid_},
				{g_key_pin, parts[1]},
				{g_key_value, serialized},
			});
		}
	}
}


void MainWindow::OnAssetMenu(const QPoint& pos)
{
	if (!asset_tree_) return;
	auto* item = asset_tree_->itemAt(pos);
	if (!item || item->data(0, Qt::UserRole).toString().isEmpty())
		return;
	asset_tree_->setCurrentItem(item);
	QMenu menu(this);
	menu.addAction(tr("打开蓝图"), this, [this, item]() { OnOpenBP(item); });
	menu.addAction(tr("重命名"), this, [this]() {
		if (auto* current = asset_tree_->currentItem())
			asset_tree_->editItem(current, 0);
	});
	menu.addAction(tr("删除蓝图"), this, &MainWindow::OnDeleteBP);
	menu.exec(asset_tree_->viewport()->mapToGlobal(pos));
}

void MainWindow::Slot_AddBP(const Message& msg)
{
	do {
		if (!asset_tree_) break;

		const QJsonObject obj = QJsonDocument::fromJson(QByteArray::fromStdString(msg.data)).object();
		if (msg.code != 0) {
			const QString error = obj.value(g_key_error).toString();
			if (!error.isEmpty())
				AppendLog(error);
			break;
		}

		const QString guid = obj.value(g_key_guid).toString();
		const QString name = obj.value(g_key_name).toString();
		QString space = obj.value(g_key_space).toString();
		if (guid.isEmpty() || name.isEmpty()) break;
		if (space.isEmpty())
			space = "user";

		QTreeWidgetItem* folder = nullptr;
		for (int i = 0; i < asset_tree_->topLevelItemCount(); ++i) {
			auto* item = asset_tree_->topLevelItem(i);
			if (item && item->text(0) == space) {
				folder = item;
				break;
			}
		}
		if (!folder) {
			folder = new QTreeWidgetItem(asset_tree_);
			folder->setText(0, space);
		}

		auto* item = new QTreeWidgetItem(folder);
		item->setFlags(item->flags() | Qt::ItemIsEditable);
		item->setData(0, Qt::UserRole, guid);
		item->setData(0, Qt::UserRole + 1, name);
		item->setText(0, name);
		folder->setExpanded(true);
		asset_tree_->setCurrentItem(item);
		if (current_asset_guid_.isEmpty()) {
			current_asset_guid_ = guid;
			if (bp_name_edit_) {
				QSignalBlocker blocker(bp_name_edit_);
				bp_name_edit_->setText(name);
			}
		}
	} while (0);
}

void MainWindow::Slot_DelBP(const Message& msg)
{
	do {
		if (!asset_tree_) break;

		const QJsonObject obj = QJsonDocument::fromJson(QByteArray::fromStdString(msg.data)).object();
		if (msg.code != 0) {
			const QString error = obj.value(g_key_error).toString();
			if (!error.isEmpty())
				AppendLog(error);
			break;
		}

		const QString guid = obj.value(g_key_guid).toString();
		auto* item = FindAssetItem(asset_tree_, guid);
		if (!item) break;

		auto* folder = item->parent();
		delete item;
		if (folder && folder->childCount() == 0)
			delete folder;

		if (current_asset_guid_ == guid)
			ResetEditor();
	} while (0);
}

void MainWindow::Slot_AltBP(const Message& msg)
{
	do {
		const QJsonObject obj = QJsonDocument::fromJson(QByteArray::fromStdString(msg.data)).object();
		const QString guid = obj.value(g_key_guid).toString();
		auto* item = FindAssetItem(asset_tree_, guid.isEmpty() ? current_asset_guid_ : guid);
		if (msg.code != 0) {
			const QString error = obj.value(g_key_error).toString();
			if (!error.isEmpty())
				AppendLog(error);
			if (item) {
				const QString old_name = item->data(0, Qt::UserRole + 1).toString();
				if (!old_name.isEmpty()) {
					QSignalBlocker blocker(asset_tree_);
					item->setText(0, old_name);
				}
				if (bp_name_edit_ && current_asset_guid_ == item->data(0, Qt::UserRole).toString()) {
					QSignalBlocker blocker(bp_name_edit_);
					bp_name_edit_->setText(item->text(0));
				}
			}
			break;
		}

		const QString name = obj.value(g_key_name).toString();
		if (!item || name.isEmpty()) break;

		{
			QSignalBlocker blocker(asset_tree_);
			item->setData(0, Qt::UserRole + 1, name);
			item->setText(0, name);
		}
		if (bp_name_edit_ && current_asset_guid_ == guid) {
			QSignalBlocker blocker(bp_name_edit_);
			bp_name_edit_->setText(name);
		}
		if (detail_kind_ == g_kind_blueprint && detail_guid_ == guid) {
			SetDetailValue(g_key_name, name);
			if (obj.contains(g_key_path))
				SetDetailValue(g_key_path, obj.value(g_key_path));
			if (obj.contains(g_key_space))
				SetDetailValue(g_key_space, obj.value(g_key_space));
		}
	} while (0);
}

void MainWindow::Slot_AddNode(const Message& msg)
{
	const QJsonObject obj = QJsonDocument::fromJson(QByteArray::fromStdString(msg.data)).object();
	if (msg.code != 0) {
		const QString error = obj.value(g_key_error).toString();
		if (!error.isEmpty())
			AppendLog(error);
		return;
	}
	PutGraphNode(obj.value(g_key_flow).toString(),
		obj.value(g_key_guid).toString(),
		obj.value(g_key_name).toString(),
		obj.value(g_key_key).toString(),
		obj.value(g_key_group).toString(),
		obj.value(g_key_x).toDouble(),
		obj.value(g_key_y).toDouble(),
		obj.value(g_key_pins).toArray());
	const QString from_node = obj.value(g_key_from_node).toString();
	const QString from_pin = obj.value(g_key_from_pin).toString();
	if (!from_node.isEmpty() && !from_pin.isEmpty()) {
		QString target_pin = obj.value(g_key_to_pin).toString();
		if (target_pin.isEmpty()) {
			for (const auto& row : obj.value(g_key_pins).toArray()) {
				const QJsonObject pin = row.toObject();
				if (pin.value(g_key_name).toString() != QStringLiteral("self")
					|| pin.value(g_key_direct).toInt() != 0
					|| pin.value(g_key_kind).toInt() != 1)
					continue;
				target_pin = pin.value(g_key_guid).toString();
				break;
			}
		}
		if (!target_pin.isEmpty()) {
			Dispatch(Action::AddLine, QJsonObject{
				{g_key_flow, obj.value(g_key_flow)},
				{g_key_from_node, from_node},
				{g_key_from_pin, from_pin},
				{g_key_to_node, obj.value(g_key_guid)},
				{g_key_to_pin, target_pin},
			});
		}
	}
}

void MainWindow::Slot_DelNode(const Message& msg)
{
	const QJsonObject obj = QJsonDocument::fromJson(QByteArray::fromStdString(msg.data)).object();
	if (msg.code != 0) {
		const QString error = obj.value(g_key_error).toString();
		if (!error.isEmpty())
			AppendLog(error);
		return;
	}
	ForgetGraphNode(obj.value(g_key_flow).toString(), obj.value(g_key_guid).toString());
}

void MainWindow::Slot_AltNode(const Message& msg)
{
	const QJsonObject obj = QJsonDocument::fromJson(QByteArray::fromStdString(msg.data)).object();
	const QString guid = obj.value(g_key_guid).toString();
	if (msg.code != 0) {
		const QString error = obj.value(g_key_error).toString();
		if (!error.isEmpty())
			AppendLog(error);
		for (const auto& [flow, nodes] : flow_nodes_) {
			for (const auto& node : nodes) {
				if (node.guid != guid)
					continue;
				if (auto* view = FindGraphView(QString::fromStdString(flow))) {
					view->moveNode(guid, QPointF(node.x, node.y));
					view->setNodePins(guid, node.pins);
				}
				if (detail_kind_ == g_kind_node && detail_guid_ == guid)
					Dispatch(Action::SelectNode, QJsonObject{{g_key_guid, guid}});
				return;
			}
		}
		return;
	}

	const QString flow = obj.value(g_key_flow).toString();
	if (obj.value(g_key_pins).isArray()) {
		for (const auto& row : obj.value(g_key_removed_lines).toArray())
			ForgetGraphLine(flow, row.toString());
		PutGraphNode(flow, guid, obj.value(g_key_name).toString(), obj.value(g_key_key).toString(),
			obj.value(g_key_group).toString(), obj.value(g_key_x).toDouble(), obj.value(g_key_y).toDouble(),
			obj.value(g_key_pins).toArray());
		if (detail_kind_ == g_kind_node && detail_guid_ == guid)
			Dispatch(Action::SelectNode, QJsonObject{{g_key_guid, guid}});
		return;
	}
	const double x = obj.value(g_key_x).toDouble();
	const double y = obj.value(g_key_y).toDouble();
	MoveGraphNode(flow, guid, x, y);
	if (detail_kind_ == g_kind_node && detail_guid_ == guid) {
		SetDetailValue(g_key_x, x);
		SetDetailValue(g_key_y, y);
	}
}

void MainWindow::Slot_AddLine(const Message& msg)
{
	const QJsonObject obj = QJsonDocument::fromJson(QByteArray::fromStdString(msg.data)).object();
	if (msg.code != 0) { AppendLog(obj.value(g_key_error).toString()); return; }
	const QString flow = obj.value(g_key_flow).toString();
	for (const auto& old : obj.value(g_key_removed_lines).toArray())
		ForgetGraphLine(flow, old.toString());
	PutGraphLine(flow, obj.value(g_key_guid).toString(), obj.value(g_key_from_node).toString(), obj.value(g_key_from_pin).toString(), obj.value(g_key_to_node).toString(), obj.value(g_key_to_pin).toString());
	if (detail_kind_ == g_kind_node) Dispatch(Action::SelectNode, QJsonObject{{g_key_guid, detail_guid_}});
}

void MainWindow::Slot_DelLine(const Message& msg)
{
	const QJsonObject obj = QJsonDocument::fromJson(QByteArray::fromStdString(msg.data)).object();
	if (msg.code != 0) { AppendLog(obj.value(g_key_error).toString()); return; }
	const QString flow = obj.value(g_key_flow).toString();
	ForgetGraphLine(flow, obj.value(g_key_guid).toString());
	const QString target_guid = obj.value(g_key_to_node).toString();
	const QString target_pin = obj.value(g_key_to_pin).toString();
	if (!target_guid.isEmpty() && !target_pin.isEmpty()) {
		auto nodes = flow_nodes_.find(flow.toStdString());
		if (nodes != flow_nodes_.end()) for (auto& node : nodes->second) {
			if (node.guid != target_guid || (node.key != QStringLiteral("Print") && node.name != QStringLiteral("Print"))) continue;
			for (int i = 0; i < node.pins.size(); ++i) {
				QJsonObject pin = node.pins.at(i).toObject();
				if (pin.value(g_key_guid).toString() != target_pin) continue;
				pin[g_key_value] = QString{};
				node.pins[i] = pin;
			}
			if (auto* view = FindGraphView(flow)) view->setNodePins(node.guid, node.pins);
			break;
		}
	}
	if (detail_kind_ == g_kind_node) Dispatch(Action::SelectNode, QJsonObject{{g_key_guid, detail_guid_}});
}

void MainWindow::Slot_SelectFunc(const Message& msg)
{
	const QJsonObject obj = QJsonDocument::fromJson(QByteArray::fromStdString(msg.data)).object();
	if (msg.code != 0) {
		const QString error = obj.value(g_key_error).toString();
		if (!error.isEmpty())
			AppendLog(error);
		return;
	}
	ShowDetails(obj);
}

void MainWindow::Slot_SelectVar(const Message& msg)
{
	const QJsonObject obj = QJsonDocument::fromJson(QByteArray::fromStdString(msg.data)).object();
	if (msg.code != 0) {
		const QString error = obj.value(g_key_error).toString();
		if (!error.isEmpty())
			AppendLog(error);
		return;
	}
	ShowDetails(obj);
}

void MainWindow::Slot_SelectNode(const Message& msg)
{
	const QJsonObject obj = QJsonDocument::fromJson(QByteArray::fromStdString(msg.data)).object();
	if (msg.code != 0) {
		const QString error = obj.value(g_key_error).toString();
		if (!error.isEmpty())
			AppendLog(error);
		return;
	}
	ShowDetails(obj);
}

void MainWindow::Slot_AddFunc(const Message& msg)
{
	do {
		if (!func_tree_) break;

		const QJsonObject obj = QJsonDocument::fromJson(QByteArray::fromStdString(msg.data)).object();
		if (msg.code != 0) {
			const QString error = obj.value(g_key_error).toString();
			if (!error.isEmpty())
				AppendLog(error);
			break;
		}

		const QString asset = obj.value(g_key_asset).toString();
		const QString guid = obj.value(g_key_guid).toString();
		const QString name = obj.value(g_key_name).toString();
		if (asset != current_asset_guid_ || guid.isEmpty() || name.isEmpty()) break;

		{
			QSignalBlocker blocker(func_tree_);
			auto* item = new QTreeWidgetItem(func_tree_);
			item->setFlags(item->flags() | Qt::ItemIsEditable);
			item->setData(0, Qt::UserRole, guid);
			item->setData(0, Qt::UserRole + 1, name);
			item->setText(0, name);
			func_tree_->setCurrentItem(item);
		}
		const QJsonObject node = obj.value(g_key_node).toObject();
		PutGraphNode(guid,
			node.value(g_key_guid).toString(),
			node.value(g_key_name).toString(),
			node.value(g_key_key).toString(),
			node.value(g_key_group).toString(),
			node.value(g_key_x).toDouble(),
			node.value(g_key_y).toDouble(),
			node.value(g_key_pins).toArray());
		OpenGraphPage(guid, name);
		Dispatch(Action::SelectFunc, QJsonObject{{g_key_guid, guid}});
	} while (0);
}

void MainWindow::Slot_DelFunc(const Message& msg)
{
	do {
		if (!func_tree_) break;

		const QJsonObject obj = QJsonDocument::fromJson(QByteArray::fromStdString(msg.data)).object();
		if (msg.code != 0) {
			const QString error = obj.value(g_key_error).toString();
			if (!error.isEmpty())
				AppendLog(error);
			break;
		}

		const QString guid = obj.value(g_key_guid).toString();
		auto* item = FindTreeItem(func_tree_, guid);
		if (!item) break;
		delete item;
		flow_nodes_.erase(guid.toStdString());
		CloseGraphPage(guid);

		if (detail_kind_ == g_kind_flow && detail_guid_ == guid)
			ClearDetails();
	} while (0);
}

void MainWindow::Slot_AltFunc(const Message& msg)
{
	do {
		if (!func_tree_) break;

		const QJsonObject obj = QJsonDocument::fromJson(QByteArray::fromStdString(msg.data)).object();
		const QString guid = obj.value(g_key_guid).toString();
		auto* item = FindTreeItem(func_tree_, guid);
		if (msg.code != 0) {
			const QString error = obj.value(g_key_error).toString();
			if (!error.isEmpty())
				AppendLog(error);
			if (item) {
				const QString old_name = item->data(0, Qt::UserRole + 1).toString();
				if (!old_name.isEmpty()) {
					QSignalBlocker blocker(func_tree_);
					item->setText(0, old_name);
					if (detail_kind_ == g_kind_flow && detail_guid_ == guid)
						SetDetailValue(g_key_name, old_name);
				}
			}
			break;
		}

		const QString name = obj.value(g_key_name).toString();
		if (!item || name.isEmpty()) break;

		{
			QSignalBlocker blocker(func_tree_);
			item->setData(0, Qt::UserRole + 1, name);
			item->setText(0, name);
		}
		RenameGraphPage(guid, name);
		if (detail_kind_ == g_kind_flow && detail_guid_ == guid)
			SetDetailValue(g_key_name, name);
	} while (0);
}

void MainWindow::Slot_AddVar(const Message& msg)
{
	do {
		if (!var_tree_) break;

		const QJsonObject obj = QJsonDocument::fromJson(QByteArray::fromStdString(msg.data)).object();
		if (msg.code != 0) {
			const QString error = obj.value(g_key_error).toString();
			if (!error.isEmpty())
				AppendLog(error);
			break;
		}

		const QString asset = obj.value(g_key_asset).toString();
		const QString guid = obj.value(g_key_guid).toString();
		const QString name = obj.value(g_key_name).toString();
		if (asset != current_asset_guid_ || guid.isEmpty() || name.isEmpty()) break;

		QSignalBlocker blocker(var_tree_);
		auto* item = new QTreeWidgetItem(var_tree_);
		item->setFlags(item->flags() | Qt::ItemIsEditable);
		item->setData(0, Qt::UserRole, guid);
		item->setData(0, Qt::UserRole + 1, name);
		item->setText(0, name);
		item->setIcon(0, TypeMark(obj.value(g_key_type).toString()));
	} while (0);
}

void MainWindow::Slot_DelVar(const Message& msg)
{
	do {
		const QJsonObject obj = QJsonDocument::fromJson(QByteArray::fromStdString(msg.data)).object();
		if (msg.code != 0) {
			const QString error = obj.value(g_key_error).toString();
			if (!error.isEmpty())
				AppendLog(error);
			break;
		}
		for (const auto& row : obj.value(g_key_removed_lines).toArray()) {
			const QJsonObject line = row.toObject();
			ForgetGraphLine(line.value(g_key_flow).toString(), line.value(g_key_guid).toString());
		}
		for (const auto& row : obj.value(g_key_removed_nodes).toArray()) {
			const QJsonObject node = row.toObject();
			const QString flow = node.value(g_key_flow).toString();
			const QString node_guid = node.value(g_key_guid).toString();
			ForgetGraphNode(flow, node_guid);
			if (detail_kind_ == g_kind_node && detail_guid_ == node_guid)
				ClearDetails();
		}
		if (!var_tree_) break;

		const QString guid = obj.value(g_key_guid).toString();
		auto* item = FindTreeItem(var_tree_, guid);
		if (!item) break;
		delete item;

		if (detail_kind_ == g_kind_var && detail_guid_ == guid)
			ClearDetails();
	} while (0);
}

void MainWindow::Slot_AltVar(const Message& msg)
{
	do {
		if (!var_tree_) break;

		const QJsonObject obj = QJsonDocument::fromJson(QByteArray::fromStdString(msg.data)).object();
		const QString guid = obj.value(g_key_guid).toString();
		auto* item = FindTreeItem(var_tree_, guid);
		if (msg.code != 0) {
			const QString error = obj.value(g_key_error).toString();
			if (!error.isEmpty())
				AppendLog(error);
			if (item) {
				const QString old_name = item->data(0, Qt::UserRole + 1).toString();
				if (!old_name.isEmpty()) {
					QSignalBlocker blocker(var_tree_);
					item->setText(0, old_name);
				}
			}
			break;
		}

		const QString name = obj.value(g_key_name).toString();
		if (!item || name.isEmpty()) break;
		for (const auto& row : obj.value(g_key_removed_lines).toArray()) {
			const QJsonObject line = row.toObject();
			ForgetGraphLine(line.value(g_key_flow).toString(), line.value(g_key_guid).toString());
		}
		for (const auto& row : obj.value(g_key_nodes).toArray()) {
			const QJsonObject entry = row.toObject();
			const QString flow = entry.value(g_key_flow).toString();
			const QJsonObject node = entry.value(g_key_node).toObject();
			const QString node_guid = node.value(g_key_guid).toString();
			PutGraphNode(flow, node_guid, node.value(g_key_name).toString(),
				node.value(g_key_key).toString(), node.value(g_key_group).toString(),
				node.value(g_key_x).toDouble(), node.value(g_key_y).toDouble(),
				node.value(g_key_pins).toArray());
			if (detail_kind_ == g_kind_node && detail_guid_ == node_guid)
				Dispatch(Action::SelectNode, QJsonObject{{g_key_guid, node_guid}});
		}

		{
			QSignalBlocker blocker(var_tree_);
			item->setData(0, Qt::UserRole + 1, name);
			item->setText(0, name);
			if (obj.contains(g_key_type))
				item->setIcon(0, TypeMark(obj.value(g_key_type).toString()));
		}
		if (detail_kind_ == g_kind_var && detail_guid_ == guid) {
			SetDetailValue(g_key_name, name);
			if (obj.contains(g_key_type))
				SetDetailValue(g_key_type, obj.value(g_key_type));
		}
	} while (0);
}


void MainWindow::OnCreateBP()
{
	QDialog dialog(this);
	dialog.setWindowTitle(tr("New Blueprint"));
	auto* form = new QFormLayout(&dialog);
	auto* space = new QLineEdit(QStringLiteral("user"));
	auto* name = new QLineEdit(QStringLiteral("NewBlueprint"));
	form->addRow(tr("Namespace"), space);
	form->addRow(tr("Name"), name);
	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	form->addRow(buttons);
	connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
	if (dialog.exec() != QDialog::Accepted)
		return;
	if (name->text().trimmed().isEmpty())
		return;
	Dispatch(Action::AddBP, QJsonObject{{g_key_space, space->text().trimmed()},{g_key_name, name->text().trimmed()}});
}

void MainWindow::OnDeleteBP()
{
	if (!asset_tree_) return;
	auto* item = asset_tree_->currentItem();
	if (!item) return;
	const QString guid = item->data(0, Qt::UserRole).toString();
	if (guid.isEmpty()) return;
	Dispatch(Action::DelBP, QJsonObject{ {g_key_guid, guid} });
}

void MainWindow::OnAlterBP()
{
	if (!bp_name_edit_)
		return;
	const QString name = bp_name_edit_->text().trimmed();
	auto* item = FindAssetItem(asset_tree_, current_asset_guid_);
	if (name.isEmpty() || current_asset_guid_.isEmpty() || !item) {
		if (item && bp_name_edit_) {
			QSignalBlocker blocker(bp_name_edit_);
			bp_name_edit_->setText(item->text(0));
		}
		return;
	}
	if (item->text(0) == name)
		return;
	Dispatch(Action::AltBP, QJsonObject{ {g_key_guid, current_asset_guid_}, {g_key_name, name} });
}

void MainWindow::OnAlterAsset(QTreeWidgetItem* item)
{
	if (!item) return;
	const QString guid = item->data(0, Qt::UserRole).toString();
	const QString old_name = item->data(0, Qt::UserRole + 1).toString();
	const QString new_name = item->text(0).trimmed();
	if (guid.isEmpty()) return;

	if (new_name.isEmpty() || new_name == old_name) {
		item->setText(0, old_name);
		return;
	}
	Dispatch(Action::AltBP, QJsonObject{ {g_key_guid, guid}, {g_key_name, new_name} });
}

void MainWindow::Slot_OpenBP(const Message& msg)
{
	do {
		const QJsonObject obj = QJsonDocument::fromJson(QByteArray::fromStdString(msg.data)).object();
		if (msg.code != 0) {
			const QString error = obj.value(g_key_error).toString();
			if (!error.isEmpty())
				AppendLog(error);
			break;
		}

		const QString guid = obj.value(g_key_guid).toString();
		const QString name = obj.value(g_key_name).toString();
		QString space = obj.value(g_key_space).toString();
		if (guid.isEmpty() || name.isEmpty()) break;
		if (space.isEmpty())
			space = QStringLiteral("user");

		current_asset_guid_ = guid;
		ClearGraphPages();
		SetEditorVisible(true);
		if (bp_name_edit_) {
			QSignalBlocker blocker(bp_name_edit_);
			bp_name_edit_->setText(name);
		}
		if (breadcrumb_)
			breadcrumb_->setText(QStringLiteral("%1 / %2").arg(space, name));
		FillNamedItems(func_tree_, obj.value(g_key_flows).toArray());
		FillNamedItems(var_tree_, obj.value(g_key_vars).toArray());
		for (const auto& flow_value : obj.value(g_key_flows).toArray()) {
			const QJsonObject flow = flow_value.toObject();
			const QString flow_guid = flow.value(g_key_guid).toString();
			for (const auto& node_value : flow.value(g_key_nodes).toArray()) {
				const QJsonObject node = node_value.toObject();
				PutGraphNode(flow_guid,
					node.value(g_key_guid).toString(),
					node.value(g_key_name).toString(),
					node.value(g_key_key).toString(),
					node.value(g_key_group).toString(),
					node.value(g_key_x).toDouble(),
					node.value(g_key_y).toDouble(),
					node.value(g_key_pins).toArray());
			}
			for (const auto& line_value : flow.value(g_key_lines).toArray()) {
				const QJsonObject line = line_value.toObject();
				PutGraphLine(flow_guid, line.value(g_key_guid).toString(), line.value(g_key_from_node).toString(), line.value(g_key_from_pin).toString(), line.value(g_key_to_node).toString(), line.value(g_key_to_pin).toString());
			}
		}
		FillTypeCombo(var_type_combo_, obj.value(g_key_types).toArray());
		ShowDetails(obj);
		if (auto* item = FindAssetItem(asset_tree_, guid))
			asset_tree_->setCurrentItem(item);
	} while (0);
}

void MainWindow::Slot_TypeSync(const Message& msg)
{
	do {
		if (msg.code != 0) break;
		const QJsonObject obj = QJsonDocument::fromJson(QByteArray::fromStdString(msg.data)).object();
		FillTypeCombo(var_type_combo_, obj.value(g_key_types).toArray());
	} while (0);
}

void MainWindow::Slot_FuncSync(const Message& msg)
{
	do {
		if (msg.code != 0) break;
		const QJsonObject obj = QJsonDocument::fromJson(QByteArray::fromStdString(msg.data)).object();
		const QJsonArray funcs = obj.value(g_key_funcs).toArray();
		if (funcs.isEmpty() && !obj.contains(g_key_funcs))
			break;
		system_funcs_ = funcs;
	} while (0);
}

void MainWindow::Slot_CloseBP(const Message& msg)
{
	do {
		const QJsonObject obj = QJsonDocument::fromJson(QByteArray::fromStdString(msg.data)).object();
		if (msg.code != 0) {
			const QString error = obj.value(g_key_error).toString();
			if (!error.isEmpty())
				AppendLog(error);
			break;
		}
		const QString guid = obj.value(g_key_guid).toString();
		if (!guid.isEmpty() && guid != current_asset_guid_)
			break;
		ResetEditor();
	} while (0);
}

void MainWindow::OnOpenBP(QTreeWidgetItem* item)
{
	if (!item) return;
	const QString guid = item->data(0, Qt::UserRole).toString();
	if (guid.isEmpty()) return;
	Dispatch(Action::OpenBP, QJsonObject{ {g_key_guid, guid} });
}

void MainWindow::OnAddFunc()
{
	if (current_asset_guid_.isEmpty())
		return;
	Dispatch(Action::AddFunc, QJsonObject{{g_key_guid, current_asset_guid_}});
}

void MainWindow::OnDelFunc()
{
	if (!func_tree_) return;
	auto* item = func_tree_->currentItem();
	if (!item) return;
	const QString guid = item->data(0, Qt::UserRole).toString();
	if (guid.isEmpty()) return;
	Dispatch(Action::DelFunc, QJsonObject{ {g_key_guid, guid} });
}

void MainWindow::OnAltFunc()
{
}

void MainWindow::OnAddVar()
{
	do {
		if(!var_type_combo_) break;

		auto type_name = var_type_combo_->currentText();
		if (type_name.isEmpty() || current_asset_guid_.isEmpty()) break;

		Dispatch(Action::AddVar, QJsonObject{{g_key_guid, current_asset_guid_}, {g_key_type, type_name}});
	}while(0);
}

void MainWindow::OnDelVar()
{
	if (!var_tree_) return;
	auto* item = var_tree_->currentItem();
	if (!item) return;
	const QString guid = item->data(0, Qt::UserRole).toString();
	if (guid.isEmpty()) return;
	Dispatch(Action::DelVar, QJsonObject{ {g_key_guid, guid} });
}

void MainWindow::OnAltVar()
{
}

void MainWindow::OnAddNode()
{
}

void MainWindow::OnDelNode()
{
}

void MainWindow::OnAltNode()
{
}

void MainWindow::OnNodeMoved(const QString& guid, const QPointF& pos)
{
	if (guid.isEmpty())
		return;
	Dispatch(Action::AltNode, QJsonObject{
		{g_key_guid, guid},
		{g_key_x, pos.x()},
		{g_key_y, pos.y()},
	});
}

void MainWindow::OnLineRequested(const QString& flow, const QString& fromNode, const QString& fromPin, const QString& toNode, const QString& toPin)
{
	Dispatch(Action::AddLine, QJsonObject{
		{g_key_flow, flow}, {g_key_from_node, fromNode}, {g_key_from_pin, fromPin},
		{g_key_to_node, toNode}, {g_key_to_pin, toPin}
	});
}

void MainWindow::OnPinMethodMenu(const QString& flow, const QString& fromNode, const QString& fromPin,
	const QJsonArray& methods, const QPointF& scene_pos, const QPoint& global_pos)
{
	QMenu menu(this);
	for (const auto& row : methods) {
		const QJsonObject method = row.toObject();
		const QString name = method.value(g_key_name).toString();
		const QString key = method.value(g_key_key).toString();
		if (name.isEmpty() || key.isEmpty())
			continue;
		auto* action = menu.addAction(name);
		connect(action, &QAction::triggered, this, [this, flow, fromNode, fromPin, key, scene_pos] {
			Dispatch(Action::AddNode, QJsonObject{
				{g_key_guid, flow},
				{g_key_key, key},
				{g_key_x, scene_pos.x()},
				{g_key_y, scene_pos.y()},
				{g_key_from_node, fromNode},
				{g_key_from_pin, fromPin},
			});
		});
	}
	if (!menu.isEmpty())
		menu.exec(global_pos);
}

void MainWindow::OnFuncSelected(QTreeWidgetItem* item)
{
	if (!item) return;
	const QString guid = item->data(0, Qt::UserRole).toString();
	if (guid.isEmpty())
		return;
	OpenGraphPage(guid, item->text(0));
	Dispatch(Action::SelectFunc, QJsonObject{ {g_key_guid, guid} });
}

void MainWindow::OnVarSelected(QTreeWidgetItem* item)
{
	if (!item) return;
	const QString guid = item->data(0, Qt::UserRole).toString();
	if (guid.isEmpty())
		return;
	Dispatch(Action::SelectVar, QJsonObject{ {g_key_guid, guid} });
}

void MainWindow::OnNodeSelected(const QString& guid)
{
	Dispatch(Action::SelectNode, QJsonObject{ {g_key_guid, guid} });
}

void MainWindow::OnRunFunc()
{
	if (!graph_tabs_ || graph_tabs_->currentIndex() < 0) {
		AppendLog(QStringLiteral("Select a flow before running."));
		return;
	}
	const QString flow_guid = graph_tabs_->tabData(graph_tabs_->currentIndex()).toString();
	Dispatch(Action::RunFunc, QJsonObject{{g_key_asset, current_asset_guid_}, {g_key_flow, flow_guid}});
}

void MainWindow::OnCompile()
{
	if (current_asset_guid_.isEmpty()) {
		AppendLog(QStringLiteral("Select a Blueprint before compiling."));
		return;
	}
	if (compile_btn_) compile_btn_->setEnabled(false);
	AppendLog(QStringLiteral("Compiling Blueprint..."));
	Dispatch(Action::Compile, QJsonObject{{g_key_guid, current_asset_guid_}});
}

void MainWindow::Slot_Compile(const Message& msg)
{
	if (compile_btn_) compile_btn_->setEnabled(true);
	const QJsonObject obj = QJsonDocument::fromJson(QByteArray::fromStdString(msg.data)).object();
	if (msg.code != 0) {
		AppendLog(obj.value(g_key_error).toString().isEmpty()
			? QStringLiteral("Blueprint compilation failed.")
			: obj.value(g_key_error).toString());
		const QString details = obj.value(g_key_text).toString();
		if (!details.isEmpty()) AppendLog(details);
		return;
	}
}

void MainWindow::Slot_RunFunc(const Message& msg)
{
	const QJsonObject obj = QJsonDocument::fromJson(QByteArray::fromStdString(msg.data)).object();
	if (msg.code != 0) {
		const QString error = obj.value(g_key_error).toString().isEmpty()
			? QStringLiteral("Flow execution failed.") : obj.value(g_key_error).toString();
		AppendLog(error);
		statusBar()->showMessage(QStringLiteral("Flow execution failed."), 5000);
		return;
	}
}

void MainWindow::Slot_Output(const Message& msg)
{
	const QJsonObject obj = QJsonDocument::fromJson(QByteArray::fromStdString(msg.data)).object();
	const QString text = obj.value(g_key_text).toString();
	if (!text.isEmpty()) AppendLog(text);
}

void MainWindow::Slot_RuntimeSync(const Message& msg)
{
	const QJsonObject obj = QJsonDocument::fromJson(QByteArray::fromStdString(msg.data)).object();
	const QString flow = obj.value(g_key_flow).toString();
	const QString phase = obj.value(g_key_phase).toString();
	if (phase == QStringLiteral("started")) {
		statusBar()->showMessage(QStringLiteral("Flow executing..."));
		auto it = flow_nodes_.find(flow.toStdString());
		if (it != flow_nodes_.end())
			for (auto& node : it->second) {
				SetGraphNodeRuntimeStatus(flow, node.guid, {});
			}
		return;
	}
	if (phase == QStringLiteral("node")) {
		const QString guid = obj.value(g_key_guid).toString();
		SetGraphNodeRuntimeStatus(flow, guid, obj.value(g_key_node_status).toString());
		if (auto nodes = flow_nodes_.find(flow.toStdString()); nodes != flow_nodes_.end()) {
			for (auto& node : nodes->second) {
				if (node.guid != guid) continue;
				for (const auto& runtime_row : obj.value(g_key_pins).toArray()) {
					const QJsonObject runtime_pin = runtime_row.toObject();
					for (int i = 0; i < node.pins.size(); ++i) {
						auto pin_row = node.pins.at(i);
						QJsonObject pin = pin_row.toObject();
						if (pin.value(g_key_guid).toString() != runtime_pin.value(g_key_guid).toString()) continue;
						pin[g_key_value] = runtime_pin.value(g_key_value);
						node.pins[i] = pin;
					}
				}
				if (auto* view = FindGraphView(flow)) view->setNodePins(node.guid, node.pins);
				break;
			}
		}
		if (detail_kind_ == g_kind_node && detail_guid_ == guid)
			Dispatch(Action::SelectNode, QJsonObject{{g_key_guid, guid}});
		return;
	}
	if (phase == QStringLiteral("finished")) {
		if (detail_kind_ == g_kind_var) {
			for (const auto& row : obj.value(g_key_vars).toArray()) {
				const QJsonObject variable = row.toObject();
				if (variable.value(g_key_guid).toString() == detail_guid_) {
					SetDetailValue(g_key_value, DecodePinValue(variable.value(g_key_value).toString()));
					break;
				}
			}
		}
		const QString status = obj.value(g_key_flow_status).toString(
			msg.code == 0 ? QStringLiteral("Completed") : QStringLiteral("Failed"));
		statusBar()->showMessage(status == QStringLiteral("Completed")
			? QStringLiteral("Flow execution completed.")
			: QStringLiteral("Flow execution failed."), 5000);
		if (detail_kind_ == g_kind_flow && detail_guid_ == flow)
			Dispatch(Action::SelectFunc, QJsonObject{{g_key_guid, flow}});
	}
}

void MainWindow::OnVarDropped(const QString& name, const QString& guid, const QPointF& scene_pos, Qt::KeyboardModifiers)
{
	auto* view = qobject_cast<GraphView*>(sender());
	if (!view || guid.isEmpty())
		return;

	QMenu menu(this);
	auto* get = menu.addAction(QStringLiteral("Get %1").arg(name));
	auto* set = menu.addAction(QStringLiteral("Set %1").arg(name));
	const QAction* chosen = menu.exec(QCursor::pos());
	if (chosen != get && chosen != set)
		return;

	const char* prefix = chosen == get ? g_node_get_var : g_node_set_var;
	const QString key = QString::fromUtf8(prefix) + QStringLiteral(":") + guid;
	Dispatch(Action::AddNode, QJsonObject{
		{g_key_guid, view->guid()},
		{g_key_key, key},
		{g_key_target, guid},
		{g_key_x, scene_pos.x()},
		{g_key_y, scene_pos.y()},
	});
}

void MainWindow::OnFuncDropped(const QString& guid, const QPointF& scene_pos)
{
	auto* view = qobject_cast<GraphView*>(sender());
	if (!view || guid.isEmpty())
		return;
	Dispatch(Action::AddNode, QJsonObject{
		{g_key_guid, view->guid()},
		{g_key_key, guid},
		{g_key_x, scene_pos.x()},
		{g_key_y, scene_pos.y()},
	});
}

void MainWindow::OnGraphTabChanged(int index)
{
	if (index < 0 || !graph_tabs_ || !graph_stack_) return;
	if (index < graph_stack_->count())
		graph_stack_->setCurrentIndex(index);
	const QString guid = graph_tabs_->tabData(index).toString();
	if (guid.isEmpty())
		return;
	if (func_tree_) {
		if (auto* item = FindTreeItem(func_tree_, guid)) {
			QSignalBlocker blocker(func_tree_);
			func_tree_->setCurrentItem(item);
		}
	}
	Dispatch(Action::SelectFunc, QJsonObject{{g_key_guid, guid}});
}

void MainWindow::OnGraphTabClose(int index)
{
	if (!graph_tabs_ || index < 0)
		return;
	CloseGraphPage(graph_tabs_->tabData(index).toString());
}

void MainWindow::OnGraphMenu(const QString& flow_guid, const QPoint& global_pos, const QPointF& scene_pos)
{
	QMenu menu(this);
	const bool has_exit = FlowHasNode(flow_guid, QString::fromUtf8(g_node_exit));
	const char* groups[] = {g_group_function, g_group_script, g_group_controller, g_group_operator};
	for (const char* group : groups) {
		QList<QAction*> actions;
		for (const auto& row : system_funcs_) {
			const QJsonObject obj = row.toObject();
			if (obj.value(g_key_group).toString() != QString::fromUtf8(group))
				continue;
			const QString name = obj.value(g_key_name).toString();
			const QString key = obj.value(g_key_key).toString().isEmpty()
				? name
				: obj.value(g_key_key).toString();
			if (name.isEmpty() || key.isEmpty())
				continue;
			if (key == QString::fromUtf8(g_node_exit) && has_exit)
				continue;
			auto* action = new QAction(name, &menu);
			connect(action, &QAction::triggered, this, [this, flow_guid, scene_pos, key] {
				Dispatch(Action::AddNode, QJsonObject{
					{g_key_guid, flow_guid},
					{g_key_key, key},
					{g_key_x, scene_pos.x()},
					{g_key_y, scene_pos.y()},
				});
			});
			actions.append(action);
		}
		if (actions.isEmpty())
			continue;
		auto* sub = menu.addMenu(QString::fromUtf8(group));
		sub->addActions(actions);
	}

	auto* user = menu.addMenu(tr("用户函数"));
	if (func_tree_) {
		for (int i = 0; i < func_tree_->topLevelItemCount(); ++i) {
			auto* item = func_tree_->topLevelItem(i);
			const QString func_guid = item ? item->data(0, Qt::UserRole).toString() : QString();
			if (!item || item->text(0).isEmpty() || func_guid.isEmpty())
				continue;
			auto* action = user->addAction(item->text(0));
			connect(action, &QAction::triggered, this, [this, flow_guid, scene_pos, func_guid] {
				Dispatch(Action::AddNode, QJsonObject{
					{g_key_guid, flow_guid},
					{g_key_key, func_guid},
					{g_key_x, scene_pos.x()},
					{g_key_y, scene_pos.y()},
				});
			});
		}
	}
	for (const auto& row : system_funcs_) {
		const QJsonObject obj = row.toObject();
		if (obj.value(g_key_group).toString() != QString::fromUtf8(g_group_user))
			continue;
		const QString name = obj.value(g_key_name).toString();
		const QString key = obj.value(g_key_key).toString();
		if (name.isEmpty() || key.isEmpty())
			continue;
		auto* action = user->addAction(name);
		connect(action, &QAction::triggered, this, [this, flow_guid, scene_pos, key] {
			Dispatch(Action::AddNode, QJsonObject{
				{g_key_guid, flow_guid},
				{g_key_key, key},
				{g_key_x, scene_pos.x()},
				{g_key_y, scene_pos.y()},
			});
		});
	}
	menu.exec(global_pos);
}


void MainWindow::OnFuncMenu(const QPoint& pos)
{
	QMenu menu(this);
	menu.addAction(tr("添加函数"), this, &MainWindow::OnAddFunc);
	menu.addSeparator();
	menu.addAction(tr("重命名"), this, [this]() {
		if (auto* item = func_tree_->currentItem())
			func_tree_->editItem(item, 0);
	});
	menu.addAction(tr("删除函数"), this, &MainWindow::OnDelFunc);
	menu.exec(func_tree_->mapToGlobal(pos));
}

void MainWindow::OnVarMenu(const QPoint& pos)
{
	QMenu menu(this);
	menu.addAction(tr("添加变量"), this, &MainWindow::OnAddVar);
	menu.addSeparator();
	menu.addAction(tr("重命名"), this, [this]() {
		if (auto* item = var_tree_->currentItem())
			var_tree_->editItem(item, 0);
	});
	menu.addAction(tr("删除变量"), this, &MainWindow::OnDelVar);
	menu.exec(var_tree_->mapToGlobal(pos));
}

void MainWindow::OnAlterFunc(QTreeWidgetItem* item)
{
	if (!item) return;
	const QString guid = item->data(0, Qt::UserRole).toString();
	const QString old_name = item->data(0, Qt::UserRole + 1).toString();
	const QString new_name = item->text(0).trimmed();
	if (guid.isEmpty()) return;

	if (new_name.isEmpty() || new_name == old_name) {
		item->setText(0, old_name);
		return;
	}
	Dispatch(Action::AltFunc, QJsonObject{{g_key_guid, guid}, {g_key_name, new_name}});
}

void MainWindow::OnAlterVar(QTreeWidgetItem* item)
{
	if (!item) return;
	const QString guid = item->data(0, Qt::UserRole).toString();
	const QString old_name = item->data(0, Qt::UserRole + 1).toString();
	const QString new_name = item->text(0).trimmed();
	if (guid.isEmpty()) return;

	if (new_name.isEmpty() || new_name == old_name) {
		item->setText(0, old_name);
		return;
	}
	Dispatch(Action::AltVar, QJsonObject{{g_key_guid, guid},{g_key_name, new_name}});
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

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
	if (event->type() == QEvent::KeyPress) {
		const auto key = static_cast<QKeyEvent*>(event)->key();
		if (key == Qt::Key_Delete) {
			if (watched == func_tree_) {
				OnDelFunc();
				return true;
			}
			if (watched == var_tree_) {
				OnDelVar();
				return true;
			}
			if (watched == asset_tree_) {
				OnDeleteBP();
				return true;
			}
		}
	}
	return QMainWindow::eventFilter(watched, event);
}

void MainWindow::Load()
{
	//加载蓝图
}



void MainWindow::OnCloseBP()
{
	if (current_asset_guid_.isEmpty())
		return;
	Dispatch(Action::CloseBP, QJsonObject{ {g_key_guid, current_asset_guid_} });
}

