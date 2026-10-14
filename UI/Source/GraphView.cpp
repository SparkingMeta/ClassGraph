#pragma execution_character_set("utf-8")

#include "GraphView.h"
#include "Theme.h"

#include "IRouter.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFontMetrics>
#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QStyleOptionGraphicsItem>
#include <QTimer>
#include <QUuid>
#include <QWheelEvent>
#include <QtMath>
#include <QWidget>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace CG::Ui{

	constexpr qreal kPinR = 7;
	constexpr qreal kTitleH = 24;
	constexpr qreal kRowH = 22;
	constexpr qreal kNodeW = 150;
	constexpr qreal kEditorW = 72;
	constexpr qreal kBoolEditorW = 16;
	constexpr qreal kEditorH = 16;
	constexpr qreal kAddPinH = 16;

	qreal EditorWidth(EditKind kind)
	{
		if (kind == EditKind::None)
			return 0;
		if (kind == EditKind::Bool)
			return kBoolEditorW;
		return kEditorW;
	}

	QString CompactStyle()
	{
		return QStringLiteral(
			"QLineEdit { background:#141414; color:#d8d8d8; border:1px solid #3a3a3a; border-radius:2px;"
			" padding:0 3px; min-height:16px; max-height:16px; font-size:11px; }"
			"QLineEdit:focus { border:1px solid #4a90c8; }"
			"QCheckBox { background:transparent; spacing:0; }"
			"QCheckBox::indicator { width:13px; height:13px; border:1px solid #5a5a5a; background:#1a1a1a; }"
			"QCheckBox::indicator:checked { background:#4a90c8; border:1px solid #6ab0e8; }");
	}

	QPainterPath Bezier(const QPointF& a, const QPointF& b)
	{
		QPainterPath path(a);
		const qreal dx = qMax(qAbs(b.x() - a.x()) * 0.45, 48.0);
		path.cubicTo(a + QPointF(dx, 0), b - QPointF(dx, 0), b);
		return path;
	}

	QPainterPath ExecTriangle(bool output)
	{
		QPainterPath path;
		if (output) {
			path.moveTo(-5, -6);
			path.lineTo(6, 0);
			path.lineTo(-5, 6);
		} else {
			path.moveTo(-6, 0);
			path.lineTo(5, -6);
			path.lineTo(5, 6);
		}
		path.closeSubpath();
		return path;
	}

	QJsonObject XyBody(const QPointF& p)
	{
		return QJsonObject{
			{QStringLiteral("x"), p.x()},
			{QStringLiteral("y"), p.y()},
		};
	}

	bool ParseBoolText(const QString& text)
	{
		const QString t = text.trimmed().toLower();
		return t == QLatin1String("true") || t == QLatin1String("1") || t == QLatin1String("yes");
	}

	const Pin* FindPinDto(const Node& node, const QString& pin_id)
	{
		const std::string id = pin_id.toStdString();
		for (const auto& pin : node.pins) {
			if (pin.id == id)
				return &pin;
		}
		return nullptr;
	}

	std::function<void(const Message&)> g_sink;

	void DispatchMsg(Action action, const QJsonObject& body = {})
	{
		if (!g_sink)
			return;
		const QString token = QUuid::createUuid().toString(QUuid::WithoutBraces);
		const QByteArray json = QJsonDocument(body).toJson(QJsonDocument::Compact);
		g_sink(MakeMessage(action, token.toStdString(), json.toStdString()));
	}

} // namespace

using namespace CG::Ui;

PinGraphicsItem::PinGraphicsItem(const Pin& pin, NodeGraphicsItem* parent)
	: QGraphicsItem(parent)
	, pin_(pin)
{
	setZValue(2);
	setAcceptHoverEvents(true);
	setToolTip(QString::fromStdString(pin_.name));
}

NodeGraphicsItem* PinGraphicsItem::OwnerNode() const
{
	return static_cast<NodeGraphicsItem*>(parentItem());
}

QPointF PinGraphicsItem::CenterInScene() const
{
	return mapToScene(QPointF(0, 0));
}

QColor PinGraphicsItem::Color() const
{
	return PinColor(QString::fromStdString(pin_.type_name), pin_.kind);
}

QRectF PinGraphicsItem::boundingRect() const
{
	return QRectF(-kPinR - 2, -kPinR - 2, (kPinR + 2) * 2, (kPinR + 2) * 2);
}

QPainterPath PinGraphicsItem::shape() const
{
	QPainterPath path;
	path.addEllipse(QRectF(-kPinR, -kPinR, kPinR * 2, kPinR * 2));
	return path;
}

void PinGraphicsItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
	painter->setRenderHint(QPainter::Antialiasing);
	const QColor color = Color();
	painter->setBrush(color);
	painter->setPen(QPen(color.darker(160), 1.0));
	if (IsExec()) {
		painter->drawPath(ExecTriangle(IsOutput()));
	} else {
		painter->drawEllipse(QPointF(0, 0), 5.5, 5.5);
		if (!IsConnected()) {
			painter->setBrush(QColor(12, 12, 12));
			painter->drawEllipse(QPointF(0, 0), 2.2, 2.2);
		}
	}
}

NodeGraphicsItem::NodeGraphicsItem(const Node& node, QGraphicsItem* parent)
	: QGraphicsObject(parent)
	, node_(node)
	, width_(kNodeW)
{
	setFlag(ItemIsMovable, true);
	setFlag(ItemIsSelectable, true);
	setFlag(ItemSendsGeometryChanges, true);
	setZValue(1);
	setPos(node_.x, node_.y);
	setToolTip(Title());
	BuildPins();
}

QString NodeGraphicsItem::Title() const
{
	if (!node_.title.empty())
		return QString::fromStdString(node_.title);
	if (!node_.caller_name.empty())
		return QString::fromStdString(node_.caller_name);
	if (!node_.name.empty())
		return QString::fromStdString(node_.name);
	return QStringLiteral("Node");
}

void NodeGraphicsItem::BuildPins()
{
	for (auto* pin : pins_)
		delete pin;
	pins_.clear();
	for (auto* child : childItems()) {
		if (dynamic_cast<QGraphicsProxyWidget*>(child))
			delete child;
	}
	editors_.clear();

	std::vector<const Pin*> ins;
	std::vector<const Pin*> outs;
	ins.reserve(node_.pins.size());
	outs.reserve(node_.pins.size());
	for (const auto& pin : node_.pins) {
		if (pin.direction == PinDir::Out)
			outs.push_back(&pin);
		else
			ins.push_back(&pin);
	}

	QFont title_font(QStringLiteral("Segoe UI"), 9, QFont::DemiBold);
	QFontMetrics title_fm(title_font);
	QFont pin_font(QStringLiteral("Segoe UI"), 8);
	QFontMetrics pin_fm(pin_font);

	qreal in_col = 12;
	for (const auto* pin : ins) {
		const QString label = QString::fromStdString(pin->name);
		qreal w = pin_fm.horizontalAdvance(label) + 8;
		if (pin->editable && pin->edit_kind != EditKind::None)
			w += 4 + EditorWidth(pin->edit_kind);
		in_col = qMax(in_col, w);
	}
	qreal out_col = 12;
	for (const auto* pin : outs) {
		const QString label = QString::fromStdString(pin->name);
		qreal w = pin_fm.horizontalAdvance(label) + 12;
		if (pin->editable && pin->edit_kind != EditKind::None)
			w += 4 + EditorWidth(pin->edit_kind);
		out_col = qMax(out_col, w);
	}

	width_ = qMax(kNodeW, static_cast<qreal>(title_fm.horizontalAdvance(Title()) + 36));
	width_ = qMax(width_, in_col + out_col + 16);

	const int rows = static_cast<int>(qMax(ins.size(), outs.size()));
	height_ = kTitleH + qMax(rows, 1) * kRowH + 8;
	if (IsSwitch())
		height_ += kAddPinH;

	for (int i = 0; i < static_cast<int>(ins.size()); ++i) {
		auto* item = new PinGraphicsItem(*ins[i], this);
		item->setPos(0, kTitleH + 10 + i * kRowH);
		pins_.push_back(item);
	}
	for (int i = 0; i < static_cast<int>(outs.size()); ++i) {
		auto* item = new PinGraphicsItem(*outs[i], this);
		item->setPos(width_, kTitleH + 10 + i * kRowH);
		pins_.push_back(item);
	}

	auto attach_editor = [&](const Pin* pin, qreal x, qreal y) {
		if (!pin->editable || pin->edit_kind == EditKind::None)
			return;

		const QString pin_id = QString::fromStdString(pin->id);
		QWidget* widget = nullptr;
		if (pin->edit_kind == EditKind::Bool) {
			auto* box = new QCheckBox();
			box->setObjectName(QStringLiteral("PinEditor"));
			box->setStyleSheet(CompactStyle());
			box->setFixedSize(static_cast<int>(kBoolEditorW), static_cast<int>(kEditorH));
			box->setChecked(ParseBoolText(QString::fromStdString(pin->value_text)));
			QObject::connect(box, &QCheckBox::toggled, box, [pin_id](bool on) {
				DispatchMsg(Action::SetPinValue, QJsonObject{
					{QStringLiteral("guid"), pin_id},
					{QStringLiteral("value"), on ? QStringLiteral("true") : QStringLiteral("false")},
				});
			});
			widget = box;
		} else {
			auto* edit = new QLineEdit();
			edit->setObjectName(QStringLiteral("PinEditor"));
			edit->setStyleSheet(CompactStyle());
			edit->setFixedSize(static_cast<int>(kEditorW), static_cast<int>(kEditorH));
			edit->setText(QString::fromStdString(pin->value_text));
			if (pin->edit_kind == EditKind::String)
				edit->setPlaceholderText(QStringLiteral("\"\""));
			QObject::connect(edit, &QLineEdit::editingFinished, edit, [pin_id, edit]() {
				DispatchMsg(Action::SetPinValue, QJsonObject{
					{QStringLiteral("guid"), pin_id},
					{QStringLiteral("value"), edit->text()},
				});
			});
			widget = edit;
		}

		auto* proxy = new QGraphicsProxyWidget(this);
		proxy->setWidget(widget);
		proxy->setZValue(3);
		const qreal editor_w = EditorWidth(pin->edit_kind);
		proxy->setPos(x, y);
		widget->resize(static_cast<int>(editor_w), static_cast<int>(kEditorH));
		editors_.push_back({ pin_id, widget });
	};

	for (int i = 0; i < static_cast<int>(ins.size()); ++i) {
		const Pin* pin = ins[i];
		const qreal label_w = pin_fm.horizontalAdvance(QString::fromStdString(pin->name));
		const qreal y = kTitleH + i * kRowH + (kRowH - kEditorH) / 2;
		attach_editor(pin, 12 + label_w + 6, y);
	}
	for (int i = 0; i < static_cast<int>(outs.size()); ++i) {
		const Pin* pin = outs[i];
		const qreal editor_w = EditorWidth(pin->edit_kind);
		const qreal y = kTitleH + i * kRowH + (kRowH - kEditorH) / 2;
		attach_editor(pin, width_ - 8 - editor_w, y);
	}
}

PinGraphicsItem* NodeGraphicsItem::FindPinItem(const QString& pin_id) const
{
	const std::string id = pin_id.toStdString();
	for (auto* item : pins_) {
		if (item && item->GetPin().id == id)
			return item;
	}
	return nullptr;
}

void NodeGraphicsItem::ApplyDto(const Node& node)
{
	prepareGeometryChange();
	node_ = node;
	syncing_ = true;
	setPos(node_.x, node_.y);
	syncing_ = false;
	setToolTip(Title());
	BuildPins();
	update();
}

bool NodeGraphicsItem::SamePinLayout(const Node& a, const Node& b)
{
	if (a.pins.size() != b.pins.size())
		return false;
	for (size_t i = 0; i < a.pins.size(); ++i) {
		const auto& pa = a.pins[i];
		const auto& pb = b.pins[i];
		if (pa.id != pb.id || pa.name != pb.name || pa.kind != pb.kind
			|| pa.direction != pb.direction || pa.editable != pb.editable
			|| pa.edit_kind != pb.edit_kind || pa.type_name != pb.type_name)
			return false;
	}
	return a.title == b.title && a.name == b.name && a.caller_name == b.caller_name
		&& a.is_entry == b.is_entry && a.is_exit == b.is_exit && a.is_pure == b.is_pure;
}

void NodeGraphicsItem::SyncFromDto(const Node& node)
{
	if (!SamePinLayout(node_, node)) {
		ApplyDto(node);
		return;
	}

	node_ = node;
	const QPointF p(node_.x, node_.y);
	if (qAbs(p.x() - pos().x()) > 0.5 || qAbs(p.y() - pos().y()) > 0.5) {
		syncing_ = true;
		setPos(p);
		syncing_ = false;
	}
	setToolTip(Title());
	for (auto* pin : pins_) {
		if (!pin)
			continue;
		if (const Pin* dto = FindPinDto(node_, QString::fromStdString(pin->GetPin().id)))
			pin->SetPin(*dto);
		pin->update();
	}
	SyncEditors();
	update();
}

void NodeGraphicsItem::SyncEditors()
{
	for (const auto& editor : editors_) {
		if (!editor.widget)
			continue;
		const Pin* pin = FindPinDto(node_, editor.pin_id);
		if (!pin)
			continue;
		if (auto* box = qobject_cast<QCheckBox*>(editor.widget)) {
			QSignalBlocker blocker(box);
			box->setChecked(ParseBoolText(QString::fromStdString(pin->value_text)));
		} else if (auto* edit = qobject_cast<QLineEdit*>(editor.widget)) {
			QSignalBlocker blocker(edit);
			edit->setText(QString::fromStdString(pin->value_text));
		}
	}
}

bool NodeGraphicsItem::IsSwitch() const
{
	return node_.caller_name == "SwitchInt";
}

QRectF NodeGraphicsItem::AddPinRect() const
{
	constexpr qreal w = 18;
	return QRectF(width_ - w - 2, height_ - kAddPinH, w, kAddPinH);
}

bool NodeGraphicsItem::HitAddPin(const QPointF& scene_pos) const
{
	if (!IsSwitch())
		return false;
	return AddPinRect().contains(mapFromScene(scene_pos));
}

QRectF NodeGraphicsItem::boundingRect() const
{
	return QRectF(-kPinR - 4, -2, width_ + (kPinR + 4) * 2, height_ + 8);
}

void NodeGraphicsItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
	painter->setRenderHint(QPainter::Antialiasing);
	const QRectF body(0, 0, width_, height_);

	painter->setPen(Qt::NoPen);
	painter->setBrush(QColor(0, 0, 0, 90));
	painter->drawRoundedRect(body.translated(2, 3), 3, 3);

	const QColor border = isSelected() ? Selection() : NodeBorder();
	painter->setPen(QPen(border, isSelected() ? 1.8 : 1.0));
	painter->setBrush(NodeBody());
	painter->drawRoundedRect(body, 3, 3);

	const QRectF header(0, 0, width_, kTitleH);
	QLinearGradient grad(header.topLeft(), header.bottomLeft());
	const QColor head = NodeHeader(
		QString::fromStdString(node_.caller_name), node_.caller_kind, node_.is_pure);
	grad.setColorAt(0, head.lighter(112));
	grad.setColorAt(1, head.darker(118));
	painter->setBrush(grad);
	painter->setPen(Qt::NoPen);
	painter->drawRoundedRect(header, 3, 3);
	painter->drawRect(QRectF(0, 10, width_, kTitleH - 10));

	if (isSelected()) {
		painter->setPen(QPen(Selection(), 1.6));
		painter->setBrush(Qt::NoBrush);
		painter->drawRoundedRect(body, 3, 3);
	}

	painter->setPen(Qt::white);
	QFont title_font(QStringLiteral("Segoe UI"), 9, QFont::DemiBold);
	painter->setFont(title_font);
	painter->drawText(header.adjusted(10, 0, -10, 0), Qt::AlignVCenter | Qt::AlignLeft, Title());

	painter->setFont(QFont(QStringLiteral("Segoe UI"), 8));
	QFontMetrics pin_fm(painter->font());

	std::vector<const Pin*> ins;
	std::vector<const Pin*> outs;
	for (const auto& pin : node_.pins) {
		if (pin.direction == PinDir::Out)
			outs.push_back(&pin);
		else
			ins.push_back(&pin);
	}

	for (int i = 0; i < static_cast<int>(ins.size()); ++i) {
		painter->setPen(Text());
		const QString label = QString::fromStdString(ins[i]->name);
		const qreal text_w = pin_fm.horizontalAdvance(label) + 4;
		painter->drawText(QRectF(12, kTitleH + i * kRowH, text_w, kRowH),
			Qt::AlignVCenter | Qt::AlignLeft, label);
	}
	for (int i = 0; i < static_cast<int>(outs.size()); ++i) {
		painter->setPen(Text());
		qreal right = width_ - 12;
		if (outs[i]->editable && outs[i]->edit_kind != EditKind::None)
			right -= EditorWidth(outs[i]->edit_kind) + 6;
		painter->drawText(QRectF(12, kTitleH + i * kRowH, right - 12, kRowH),
			Qt::AlignVCenter | Qt::AlignRight, QString::fromStdString(outs[i]->name));
	}

	if (IsSwitch()) {
		painter->setPen(TextMuted());
		painter->setFont(QFont(QStringLiteral("Segoe UI"), 10, QFont::DemiBold));
		painter->drawText(AddPinRect(), Qt::AlignCenter, QStringLiteral("+"));
	}
}

QVariant NodeGraphicsItem::itemChange(GraphicsItemChange change, const QVariant& value)
{
	if (change == ItemPositionHasChanged && scene() && !syncing_) {
		const QPointF p = pos();
		if (qAbs(p.x() - static_cast<qreal>(node_.x)) > 0.5
			|| qAbs(p.y() - static_cast<qreal>(node_.y)) > 0.5) {
			node_.x = static_cast<float>(p.x());
			node_.y = static_cast<float>(p.y());
			DispatchMsg(Action::MoveNode, QJsonObject{
				{QStringLiteral("guid"), Guid()},
				{QStringLiteral("x"), p.x()},
				{QStringLiteral("y"), p.y()},
			});
			if (auto* view = qobject_cast<GraphView*>(scene()->views().value(0)))
				view->RefreshLines();
		} else if (auto* view = qobject_cast<GraphView*>(scene()->views().value(0))) {
			view->RefreshLines();
		}
	}
	return QGraphicsObject::itemChange(change, value);
}

LineGraphicsItem::LineGraphicsItem(PinGraphicsItem* from, PinGraphicsItem* to,
	const Line& line, QGraphicsItem* parent)
	: QGraphicsPathItem(parent)
	, from_(from)
	, to_(to)
	, line_(line)
{
	setZValue(0);
	setFlag(ItemIsSelectable, true);
	setAcceptHoverEvents(true);
	UpdatePath();
}

void LineGraphicsItem::UpdatePath()
{
	if (!from_ || !to_)
		return;
	setPath(Bezier(from_->CenterInScene(), to_->CenterInScene()));
}

QPainterPath LineGraphicsItem::shape() const
{
	QPainterPathStroker stroker;
	stroker.setWidth(12);
	stroker.setCapStyle(Qt::RoundCap);
	return stroker.createStroke(path());
}

void LineGraphicsItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
	if (!from_ || !to_)
		return;
	painter->setRenderHint(QPainter::Antialiasing);
	const QColor color = isSelected() ? Selection() : from_->Color();
	const qreal width = (from_->IsExec() ? 3.2 : 2.0) + (isSelected() ? 1.4 : 0.0);
	painter->setBrush(Qt::NoBrush);
	painter->setPen(QPen(QColor(0, 0, 0, 140), width + 1.6, Qt::SolidLine, Qt::RoundCap));
	painter->drawPath(path());
	painter->setPen(QPen(color, width, Qt::SolidLine, Qt::RoundCap));
	painter->drawPath(path());
}

GraphView::GraphView(QWidget* parent)
	: QGraphicsView(parent)
	, scene_(new QGraphicsScene(this))
{
	setScene(scene_);
	scene_->setSceneRect(-4000, -4000, 8000, 8000);
	setRenderHint(QPainter::Antialiasing);
	setDragMode(QGraphicsView::NoDrag);
	setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
	setBackgroundBrush(GraphBg());
	setFrameShape(QFrame::NoFrame);
	setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
	setContextMenuPolicy(Qt::PreventContextMenu);
	setAcceptDrops(true);
	viewport()->setAcceptDrops(true);
}

void GraphView::ApplyGraph(const Graph& graph)
{
	std::unordered_set<std::string> old_guids;
	old_guids.reserve(graph_.nodes.size());
	for (const auto& n : graph_.nodes)
		old_guids.insert(n.guid);

	const QString pending = pending_connect_from_;
	pending_connect_from_.clear();

	// Full rebuild only when switching graph identity (open / close / change flow).
	const bool replace_all = graph.flow_guid != graph_.flow_guid || !graph_loaded_;
	if (replace_all)
		RebuildAll(graph, old_guids, pending);
	else
		ApplyGraphIncremental(graph, old_guids, pending);
	graph_loaded_ = true;
}

void GraphView::RebuildAll(const Graph& graph, const std::unordered_set<std::string>& old_guids,
	const QString& pending)
{
	ClearTempLine();
	scene_->clear();
	lines_.clear();
	graph_ = graph;

	std::unordered_map<std::string, PinGraphicsItem*> pin_map;
	const Node* spawned = nullptr;

	for (const auto& node : graph_.nodes) {
		auto* item = new NodeGraphicsItem(node);
		scene_->addItem(item);
		for (auto* pin : item->Pins()) {
			if (pin)
				pin_map[pin->GetPin().id] = pin;
		}
		if (!pending.isEmpty() && old_guids.count(node.guid) == 0 && !spawned)
			spawned = &node;
	}

	for (const auto& line : graph_.lines) {
		auto* from = pin_map.count(line.from_pin_id) ? pin_map[line.from_pin_id] : nullptr;
		auto* to = pin_map.count(line.to_pin_id) ? pin_map[line.to_pin_id] : nullptr;
		if (from && to)
			AddLineItem(from, to, line);
	}

	if (!pending.isEmpty() && spawned)
		TryConnectAfterSpawn(pending, spawned);
}

void GraphView::ApplyGraphIncremental(const Graph& graph,
	const std::unordered_set<std::string>& old_guids, const QString& pending)
{
	ClearLineItems();

	auto existing = NodeItemsByGuid();
	std::unordered_set<std::string> keep;
	keep.reserve(graph.nodes.size());
	const Node* spawned = nullptr;

	for (const auto& node : graph.nodes) {
		keep.insert(node.guid);
		if (auto it = existing.find(node.guid); it != existing.end()) {
			it->second->SyncFromDto(node);
		} else {
			auto* item = new NodeGraphicsItem(node);
			scene_->addItem(item);
			if (!pending.isEmpty() && old_guids.count(node.guid) == 0 && !spawned)
				spawned = &node;
		}
	}

	for (auto& [guid, item] : existing) {
		if (keep.count(guid) || !item)
			continue;
		scene_->removeItem(item);
		delete item;
	}

	graph_ = graph;
	RebuildLinesFromGraph();

	if (!pending.isEmpty() && spawned)
		TryConnectAfterSpawn(pending, spawned);
}

void GraphView::SetFlowGuid(const QString& guid)
{
	graph_.flow_guid = guid.toStdString();
}

QString GraphView::FlowGuid() const
{
	return QString::fromStdString(graph_.flow_guid);
}

bool GraphView::GraphLoaded() const
{
	return graph_loaded_;
}

void GraphView::UpsertNode(const Node& node)
{
	if (node.guid.empty() || !scene_)
		return;

	const QString pending = pending_connect_from_;
	pending_connect_from_.clear();

	auto existing = NodeItemsByGuid();
	if (auto it = existing.find(node.guid); it != existing.end() && it->second)
		it->second->SyncFromDto(node);
	else
		scene_->addItem(new NodeGraphicsItem(node));

	bool found = false;
	for (auto& n : graph_.nodes) {
		if (n.guid != node.guid)
			continue;
		n = node;
		found = true;
		break;
	}
	if (!found)
		graph_.nodes.push_back(node);
	graph_loaded_ = true;
	if (node.is_exit) {
		graph_.has_exit = true;
		graph_.exit_node_id = node.guid;
	}

	const Node* hint = nullptr;
	for (const auto& n : graph_.nodes) {
		if (n.guid == node.guid) {
			hint = &n;
			break;
		}
	}
	if (!pending.isEmpty() && hint)
		TryConnectAfterSpawn(pending, hint);
}

void GraphView::RemoveNode(const QString& guid)
{
	if (guid.isEmpty() || !scene_)
		return;
	const std::string id = guid.toStdString();
	auto existing = NodeItemsByGuid();
	auto it = existing.find(id);
	if (it == existing.end() || !it->second)
		return;

	std::vector<QString> pin_ids;
	for (auto* pin : it->second->Pins()) {
		if (pin)
			pin_ids.push_back(QString::fromStdString(pin->GetPin().id));
	}
	for (const auto& pin_id : pin_ids)
		RemoveLinesOfPin(pin_id);

	scene_->removeItem(it->second);
	delete it->second;

	graph_.nodes.erase(
		std::remove_if(graph_.nodes.begin(), graph_.nodes.end(),
			[&](const Node& n) { return n.guid == id; }),
		graph_.nodes.end());
	if (graph_.exit_node_id == id) {
		graph_.exit_node_id.clear();
		graph_.has_exit = false;
	}
}

void GraphView::UpsertLine(const Line& line)
{
	if (!scene_ || (line.from_pin_id.empty() && line.to_pin_id.empty()))
		return;

	RemoveLinesOfPin(QString::fromStdString(line.to_pin_id));
	if (line.is_exec)
		RemoveLinesOfPin(QString::fromStdString(line.from_pin_id));

	PinGraphicsItem* from = nullptr;
	PinGraphicsItem* to = nullptr;
	for (auto* item : scene_->items()) {
		auto* node = dynamic_cast<NodeGraphicsItem*>(item);
		if (!node)
			continue;
		if (!from)
			from = node->FindPinItem(QString::fromStdString(line.from_pin_id));
		if (!to)
			to = node->FindPinItem(QString::fromStdString(line.to_pin_id));
	}
	if (!from || !to)
		return;

	AddLineItem(from, to, line);
	graph_.lines.push_back(line);

	auto mark = [](PinGraphicsItem* pin) {
		Pin dto = pin->GetPin();
		dto.connected = true;
		pin->SetPin(dto);
		pin->update();
	};
	mark(from);
	mark(to);
}

void GraphView::RemoveLine(const QString& id)
{
	if (id.isEmpty())
		return;
	const std::string line_id = id.toStdString();
	std::vector<LineGraphicsItem*> keep;
	keep.reserve(lines_.size());
	for (auto* line : lines_) {
		if (!line) {
			continue;
		}
		if (line->LineId() == id) {
			if (line->scene())
				scene_->removeItem(line);
			delete line;
			continue;
		}
		keep.push_back(line);
	}
	lines_ = std::move(keep);
	graph_.lines.erase(
		std::remove_if(graph_.lines.begin(), graph_.lines.end(),
			[&](const Line& l) { return l.id == line_id; }),
		graph_.lines.end());
}

void GraphView::RemoveLinesOfPin(const QString& pin_id)
{
	if (pin_id.isEmpty())
		return;
	const std::string id = pin_id.toStdString();
	std::vector<LineGraphicsItem*> keep;
	keep.reserve(lines_.size());
	for (auto* line : lines_) {
		if (!line) {
			continue;
		}
		const bool hit =
			(line->From() && line->From()->GetPin().id == id)
			|| (line->To() && line->To()->GetPin().id == id);
		if (hit) {
			if (line->scene())
				scene_->removeItem(line);
			delete line;
			continue;
		}
		keep.push_back(line);
	}
	lines_ = std::move(keep);
	graph_.lines.erase(
		std::remove_if(graph_.lines.begin(), graph_.lines.end(),
			[&](const Line& l) { return l.from_pin_id == id || l.to_pin_id == id; }),
		graph_.lines.end());
}

std::unordered_map<std::string, NodeGraphicsItem*> GraphView::NodeItemsByGuid() const
{
	std::unordered_map<std::string, NodeGraphicsItem*> map;
	if (!scene_)
		return map;
	for (auto* item : scene_->items()) {
		if (auto* node = dynamic_cast<NodeGraphicsItem*>(item))
			map[node->GetNode().guid] = node;
	}
	return map;
}

void GraphView::ClearLineItems()
{
	for (auto* line : lines_) {
		if (!line)
			continue;
		if (line->scene())
			scene_->removeItem(line);
		delete line;
	}
	lines_.clear();
}

void GraphView::RebuildLinesFromGraph()
{
	std::unordered_map<std::string, PinGraphicsItem*> pin_map;
	for (auto* item : scene_->items()) {
		auto* node = dynamic_cast<NodeGraphicsItem*>(item);
		if (!node)
			continue;
		for (auto* pin : node->Pins()) {
			if (pin)
				pin_map[pin->GetPin().id] = pin;
		}
	}

	for (const auto& line : graph_.lines) {
		auto* from = pin_map.count(line.from_pin_id) ? pin_map[line.from_pin_id] : nullptr;
		auto* to = pin_map.count(line.to_pin_id) ? pin_map[line.to_pin_id] : nullptr;
		if (from && to)
			AddLineItem(from, to, line);
	}
}

void GraphView::SetPalette(const std::vector<CallerItem>& callers)
{
	palette_ = callers;
}

void GraphView::SetSink(std::function<void(const Message&)> sink)
{
	g_sink = std::move(sink);
}

QString GraphView::SelectedNodeGuid() const
{
	for (auto* item : scene_->selectedItems()) {
		if (auto* node = dynamic_cast<NodeGraphicsItem*>(item))
			return node->Guid();
	}
	return {};
}

void GraphView::DeleteSelected()
{
	if (!scene_)
		return;

	std::vector<QString> line_ids;
	for (auto* item : scene_->selectedItems()) {
		if (auto* line = dynamic_cast<LineGraphicsItem*>(item))
			line_ids.push_back(line->LineId());
	}
	if (!line_ids.empty()) {
		for (const QString& id : line_ids)
			DispatchMsg(Action::DisconnectLine, QJsonObject{{QStringLiteral("guid"), id}});
		emit GraphChanged();
		emit NodeSelected({});
		return;
	}

	const QString guid = SelectedNodeGuid();
	if (guid.isEmpty())
		return;

	for (const auto& node : graph_.nodes) {
		if (node.guid == guid.toStdString() && node.is_entry) {
			emit LogMessage(QStringLiteral("Cannot delete Entry node"));
			return;
		}
	}

	DispatchMsg(Action::DelNode, QJsonObject{{QStringLiteral("guid"), guid}});
	emit GraphChanged();
	emit NodeSelected({});
}

void GraphView::AddExitAtCenter()
{
	const QPointF pos = mapToScene(viewport()->rect().center());
	DispatchMsg(Action::AddExitNode, XyBody(pos));
	emit GraphChanged();
}

void GraphView::PlaceCaller(const QString& key, const QPointF& scene_pos)
{
	if (key.isEmpty())
		return;
	if (key == QLatin1String("exit")) {
		auto body = XyBody(scene_pos);
		DispatchMsg(Action::AddExitNode, body);
	} else {
		auto body = XyBody(scene_pos);
		body.insert(QStringLiteral("key"), key);
		DispatchMsg(Action::AddNode, body);
	}
	emit GraphChanged();
}

void GraphView::AddLineItem(PinGraphicsItem* from, PinGraphicsItem* to, const Line& line)
{
	auto* item = new LineGraphicsItem(from, to, line);
	scene_->addItem(item);
	lines_.push_back(item);
}

void GraphView::RefreshLines()
{
	for (auto* line : lines_) {
		if (line)
			line->UpdatePath();
	}
}

PinGraphicsItem* GraphView::PinAt(const QPoint& view_pos) const
{
	for (auto* item : items(view_pos)) {
		if (auto* pin = dynamic_cast<PinGraphicsItem*>(item))
			return pin;
	}
	return nullptr;
}

NodeGraphicsItem* GraphView::NodeAt(const QPoint& view_pos) const
{
	QGraphicsItem* item = itemAt(view_pos);
	while (item) {
		if (auto* node = dynamic_cast<NodeGraphicsItem*>(item))
			return node;
		item = item->parentItem();
	}
	return nullptr;
}

LineGraphicsItem* GraphView::LineAt(const QPoint& view_pos) const
{
	for (auto* item : items(view_pos)) {
		if (dynamic_cast<NodeGraphicsItem*>(item) || dynamic_cast<PinGraphicsItem*>(item)
			|| dynamic_cast<QGraphicsProxyWidget*>(item))
			return nullptr;
		if (auto* line = dynamic_cast<LineGraphicsItem*>(item))
			return line;
	}
	return nullptr;
}

bool GraphView::EditorAt(const QPoint& view_pos) const
{
	QGraphicsItem* item = itemAt(view_pos);
	while (item) {
		if (dynamic_cast<QGraphicsProxyWidget*>(item))
			return true;
		item = item->parentItem();
	}
	return false;
}

void GraphView::ClearTempLine()
{
	if (temp_line_) {
		scene_->removeItem(temp_line_);
		delete temp_line_;
		temp_line_ = nullptr;
	}
	drag_pin_ = nullptr;
}

void GraphView::DispatchConnect(const QString& from_pin, const QString& to_pin)
{
	if (from_pin.isEmpty() || to_pin.isEmpty() || from_pin == to_pin)
		return;
	DispatchMsg(Action::ConnectPin, QJsonObject{
		{QStringLiteral("from"), from_pin},
		{QStringLiteral("to"), to_pin},
	});
	emit GraphChanged();
}

void GraphView::TryConnectAfterSpawn(const QString& from_pin, const Node* hint)
{
	if (from_pin.isEmpty() || !hint)
		return;

	const Pin* from = nullptr;
	for (const auto& node : graph_.nodes) {
		from = FindPinDto(node, from_pin);
		if (from)
			break;
	}

	auto pick = [&](bool prefer_self) -> const Pin* {
		const Pin* fallback = nullptr;
		for (const auto& pin : hint->pins) {
			if (from) {
				const bool want_in = from->direction == PinDir::Out;
				const bool is_in = pin.direction == PinDir::In;
				if (want_in != is_in)
					continue;
				if (from->kind != pin.kind)
					continue;
			}
			if (prefer_self && pin.name == "self" && pin.direction == PinDir::In)
				return &pin;
			if (!fallback)
				fallback = &pin;
		}
		return prefer_self ? nullptr : fallback;
	};

	const Pin* target = pick(true);
	if (!target)
		target = pick(false);
	if (!target && !hint->pins.empty())
		target = &hint->pins.front();
	if (!target)
		return;

	DispatchConnect(from_pin, QString::fromStdString(target->id));
}

void GraphView::ShowSpawnMenu(const QPoint& global_pos, const QPointF& scene_pos,
	const QString& connect_from_pin)
{
	QMenu menu(this);

	auto place_key = [this, scene_pos, connect_from_pin](const QString& key) {
		pending_connect_from_ = connect_from_pin;
		PlaceCaller(key, scene_pos);
	};

	QAction* return_act = menu.addAction(QStringLiteral("Return"));
	return_act->setEnabled(!graph_.has_exit);
	connect(return_act, &QAction::triggered, this, [this, scene_pos, connect_from_pin]() {
		pending_connect_from_ = connect_from_pin;
		DispatchMsg(Action::AddExitNode, XyBody(scene_pos));
		emit GraphChanged();
	});

	std::unordered_map<std::string, QMenu*> group_menus;
	const std::string self_key = graph_.flow_guid.empty() ? std::string() : "flow:" + graph_.flow_guid;
	for (const auto& caller : palette_) {
		if (!self_key.empty() && caller.key == self_key)
			continue;
		const QString group_name = caller.group.empty()
			? QStringLiteral("Utilities")
			: QString::fromStdString(caller.group);
		QMenu*& sub = group_menus[caller.group.empty() ? std::string("Utilities") : caller.group];
		if (!sub)
			sub = menu.addMenu(group_name);
		const QString key = QString::fromStdString(caller.key);
		const QString name = QString::fromStdString(caller.name);
		sub->addAction(name.isEmpty() ? key : name, this, [place_key, key]() {
			place_key(key);
		});
	}

	menu.exec(global_pos);
}

void GraphView::mousePressEvent(QMouseEvent* event)
{
	if (event->button() == Qt::LeftButton) {
		if (auto* pin = PinAt(event->pos())) {
			if (event->modifiers() & Qt::AltModifier) {
				DispatchMsg(Action::DisconnectPin, QJsonObject{
					{QStringLiteral("guid"), QString::fromStdString(pin->GetPin().id)},
				});
				emit GraphChanged();
				event->accept();
				return;
			}
			drag_pin_ = pin;
			const QColor color = pin->Color();
			temp_line_ = scene_->addPath(QPainterPath(),
				QPen(color, pin->IsExec() ? 3.0 : 2.0, Qt::DashLine, Qt::RoundCap));
			temp_line_->setZValue(3);
			event->accept();
			return;
		}
		if (auto* node = NodeAt(event->pos())) {
			if (node->HitAddPin(mapToScene(event->pos()))) {
				DispatchMsg(Action::AddParam, QJsonObject{
					{QStringLiteral("guid"), node->Guid()},
				});
				emit GraphChanged();
				event->accept();
				return;
			}
		}
		if (EditorAt(event->pos())) {
			if (auto* item = itemAt(event->pos())) {
				while (item && !dynamic_cast<NodeGraphicsItem*>(item))
					item = item->parentItem();
				if (auto* node = dynamic_cast<NodeGraphicsItem*>(item)) {
					scene_->clearSelection();
					node->setSelected(true);
					const QString guid = node->Guid();
					emit NodeSelected(guid);
					// Defer SelectNode so focus/editingFinished on pin editors
					// cannot rebuild the scene while Qt is still in this press.
					QTimer::singleShot(0, this, [guid]() {
						DispatchMsg(Action::SelectNode, QJsonObject{{QStringLiteral("guid"), guid}});
					});
				}
			}
			QGraphicsView::mousePressEvent(event);
			return;
		}
		if (LineAt(event->pos())) {
			setDragMode(QGraphicsView::NoDrag);
			QGraphicsView::mousePressEvent(event);
			return;
		}
		setDragMode(QGraphicsView::RubberBandDrag);
	}
	if (event->button() == Qt::RightButton || event->button() == Qt::MiddleButton) {
		rmb_origin_ = event->pos();
		rmb_press_pos_ = event->pos();
		rmb_panning_ = true;
		setCursor(Qt::ClosedHandCursor);
		event->accept();
		return;
	}
	QGraphicsView::mousePressEvent(event);
	const QString guid = SelectedNodeGuid();
	DispatchMsg(Action::SelectNode, QJsonObject{{QStringLiteral("guid"), guid}});
	emit NodeSelected(guid);
}

void GraphView::mouseMoveEvent(QMouseEvent* event)
{
	if (drag_pin_ && temp_line_) {
		temp_line_->setPath(Bezier(drag_pin_->CenterInScene(), mapToScene(event->pos())));
		event->accept();
		return;
	}
	if (rmb_panning_) {
		const QPoint delta = event->pos() - rmb_press_pos_;
		rmb_press_pos_ = event->pos();
		horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
		verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
		event->accept();
		return;
	}
	QGraphicsView::mouseMoveEvent(event);
	if (!drag_pin_)
		RefreshLines();
}

void GraphView::mouseReleaseEvent(QMouseEvent* event)
{
	if (event->button() == Qt::LeftButton)
		setDragMode(QGraphicsView::NoDrag);

	if (rmb_panning_ && (event->button() == Qt::RightButton || event->button() == Qt::MiddleButton)) {
		const bool short_click = (event->pos() - rmb_origin_).manhattanLength() < 8;
		rmb_panning_ = false;
		unsetCursor();
		if (short_click && event->button() == Qt::RightButton) {
			if (auto* pin = PinAt(event->pos())) {
				const auto& p = pin->GetPin();
				if (p.kind == PinKind::Exec && p.direction == PinDir::Out
					&& !p.type_name.empty() && p.name != "default") {
					if (auto* owner = pin->OwnerNode()) {
						scene_->clearSelection();
						owner->setSelected(true);
						const QString node_guid = owner->Guid();
						const QString pin_name = QString::fromStdString(p.name);
						QMenu menu(this);
						menu.setStyleSheet(QStringLiteral(
							"QMenu { min-width: 0px; padding: 2px; }"
							"QMenu::item { padding: 4px 10px; }"));
						menu.addAction(QStringLiteral("Remove Execution Pin"), this,
							[this, node_guid, pin_name]() {
								DispatchMsg(Action::DelParam, QJsonObject{
									{QStringLiteral("guid"), node_guid},
									{QStringLiteral("name"), pin_name},
								});
								emit GraphChanged();
							});
						menu.exec(event->globalPos());
						event->accept();
						return;
					}
				}
			}
			if (auto* item = NodeAt(event->pos())) {
				const auto& node = item->GetNode();
				if (node.is_entry || node.is_exit) {
					scene_->clearSelection();
					item->setSelected(true);
					const QString guid = item->Guid();
					DispatchMsg(Action::SelectNode, QJsonObject{{QStringLiteral("guid"), guid}});
					emit NodeSelected(guid);
					emit NodeContextMenu(guid, event->globalPos());
					event->accept();
					return;
				}
			}
			ShowSpawnMenu(event->globalPos(), mapToScene(event->pos()));
		}
		event->accept();
		return;
	}

	if (drag_pin_) {
		const QString from_id = QString::fromStdString(drag_pin_->GetPin().id);
		if (auto* target = PinAt(event->pos())) {
			ClearTempLine();
			DispatchConnect(from_id, QString::fromStdString(target->GetPin().id));
		} else {
			const QPoint global = event->globalPos();
			const QPointF scene = mapToScene(event->pos());
			ClearTempLine();
			ShowSpawnMenu(global, scene, from_id);
		}
		event->accept();
		return;
	}
	QGraphicsView::mouseReleaseEvent(event);
	RefreshLines();
	const QString guid = SelectedNodeGuid();
	DispatchMsg(Action::SelectNode, QJsonObject{{QStringLiteral("guid"), guid}});
	emit NodeSelected(guid);
}

void GraphView::keyPressEvent(QKeyEvent* event)
{
	if (QWidget* focus = QApplication::focusWidget()) {
		if (focus != this && focus != viewport()
			&& (qobject_cast<QLineEdit*>(focus) || qobject_cast<QCheckBox*>(focus)
				|| focus->graphicsProxyWidget() != nullptr || isAncestorOf(focus))) {
			QGraphicsView::keyPressEvent(event);
			return;
		}
	}

	// Backspace is for text editing; only Delete removes nodes/lines.
	if (event->key() == Qt::Key_Delete)
		DeleteSelected();
	else
		QGraphicsView::keyPressEvent(event);
}

void GraphView::wheelEvent(QWheelEvent* event)
{
	const qreal factor = event->angleDelta().y() > 0 ? 1.12 : 1.0 / 1.12;
	scale(factor, factor);
}

void GraphView::contextMenuEvent(QContextMenuEvent* event)
{
	ShowSpawnMenu(event->globalPos(), mapToScene(event->pos()));
}

void GraphView::dragEnterEvent(QDragEnterEvent* event)
{
	if (event->mimeData()->hasFormat(kClassGraphVarMime) || event->mimeData()->hasFormat(kClassGraphFlowMime))
		event->acceptProposedAction();
}

void GraphView::dragMoveEvent(QDragMoveEvent* event)
{
	if (event->mimeData()->hasFormat(kClassGraphVarMime) || event->mimeData()->hasFormat(kClassGraphFlowMime))
		event->acceptProposedAction();
}

void GraphView::dropEvent(QDropEvent* event)
{
	if (event->mimeData()->hasFormat(kClassGraphFlowMime)) {
		const QString guid = QString::fromUtf8(event->mimeData()->data(kClassGraphFlowMime));
		if (guid.isEmpty())
			return;
		event->acceptProposedAction();
		emit FunctionDropped(guid, mapToScene(event->pos()));
		return;
	}
	if (!event->mimeData()->hasFormat(kClassGraphVarMime))
		return;
	const QString guid = QString::fromUtf8(event->mimeData()->data(kClassGraphVarMime));
	if (guid.isEmpty())
		return;
	event->acceptProposedAction();
	emit VariableDropped(guid, mapToScene(event->pos()), event->keyboardModifiers());
}

void GraphView::drawBackground(QPainter* painter, const QRectF& rect)
{
	painter->fillRect(rect, GraphBg());

	auto draw_grid = [&](int step, const QColor& color) {
		painter->setPen(QPen(color, 1));
		const int x0 = static_cast<int>(std::floor(rect.left() / step) * step);
		const int y0 = static_cast<int>(std::floor(rect.top() / step) * step);
		for (int x = x0; x < rect.right(); x += step)
			painter->drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
		for (int y = y0; y < rect.bottom(); y += step)
			painter->drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));
	};
	draw_grid(16, GridMinor());
	draw_grid(128, GridMajor());
}
