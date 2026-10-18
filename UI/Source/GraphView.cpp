#pragma execution_character_set("utf-8")

#include "GraphView.h"
#include "Theme.h"
#include "message.h"

#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QEvent>
#include <QGraphicsRectItem>
#include <QGraphicsPathItem>
#include <QGraphicsProxyWidget>
#include <QGraphicsSimpleTextItem>
#include <QCheckBox>
#include <QDoubleValidator>
#include <QIntValidator>
#include <QKeyEvent>
#include <QLineF>
#include <QPen>
#include <QFontMetrics>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStyleOptionGraphicsItem>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QLineEdit>
#include <QLinearGradient>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QWheelEvent>

#include <cmath>

GraphScene::GraphScene(QObject* parent)
	: QGraphicsScene(parent)
{
	setSceneRect(-8000, -8000, 16000, 16000);
}

void GraphScene::drawBackground(QPainter* painter, const QRectF& rect)
{
	const QRectF area = rect.normalized();
	painter->fillRect(area, CG::Ui::GraphBg());

	auto draw = [&](int step, const QColor& color) {
		painter->setPen(QPen(color, 0));
		const qreal left = std::floor(area.left() / step) * step;
		const qreal top = std::floor(area.top() / step) * step;
		for (qreal x = left; x < area.right(); x += step)
			painter->drawLine(QPointF(x, area.top()), QPointF(x, area.bottom()));
		for (qreal y = top; y < area.bottom(); y += step)
			painter->drawLine(QPointF(area.left(), y), QPointF(area.right(), y));
	};
	draw(20, CG::Ui::GridMinor());
	draw(100, CG::Ui::GridMajor());
}

QColor PinColor(const QString& type, bool exec)
{
	if (exec)
		return CG::Ui::ExecWhite();
	return CG::Ui::TypeColor(type);
}

QString PinDisplayValue(const QString& serialized)
{
	if (serialized.isEmpty()) return {};
	QJsonParseError error{};
	const auto doc = QJsonDocument::fromJson(("[" + serialized + "]").toUtf8(), &error);
	if (error.error == QJsonParseError::NoError && doc.isArray() && !doc.array().isEmpty()) {
		const auto value = doc.array().first();
		return value.isString() ? value.toString() : QString::fromUtf8(QJsonDocument(QJsonArray{value}).toJson(QJsonDocument::Compact)).mid(1).chopped(1);
	}
	return serialized;
}

QColor HeaderColor(const QString& key, const QString& group, bool has_exec)
{
	if (key == QString::fromUtf8(g_node_entry) || key == QString::fromUtf8(g_node_exit))
		return CG::Ui::EntryTitle();
	if (group == QString::fromUtf8(g_group_controller))
		return CG::Ui::ControllerTitle();
	if (group == QString::fromUtf8(g_group_operator))
		return CG::Ui::OperatorTitle();
	if (!has_exec)
		return CG::Ui::PureTitle();
	return CG::Ui::FunctionTitle();
}

struct DrawnPin {
	QString guid;
	QString name;
	bool input{};
	bool exec{};
	bool connected{};
	QString type;
	QString value;
	QJsonArray methods;
	QColor color;
};

class GraphNodeItem : public QGraphicsItem {
public:
	GraphNodeItem(const QString& guid, const QString& name, const QString& key, const QString& group, const QJsonArray& pins)
		: title_(name)
		, key_(key)
		, group_(group)
	{
		setData(0, guid);
		setData(1, key);
		setFlag(QGraphicsItem::ItemIsSelectable);
		setFlag(QGraphicsItem::ItemIsMovable);
		setPins(pins);
	}

	void setPins(const QJsonArray& pins)
	{
		prepareGeometryChange();
		inputs_.clear();
		outputs_.clear();
		for (const auto& row : pins) {
			const QJsonObject pin = row.toObject();
			DrawnPin drawn;
			drawn.guid = pin.value(g_key_guid).toString();
			drawn.name = pin.value(g_key_name).toString();
			drawn.exec = pin.value(g_key_kind).toInt() == 0;
			drawn.input = pin.value(g_key_direct).toInt() == 0;
			drawn.connected = pin.value(g_key_connected).toBool();
			drawn.type = pin.value(g_key_type).toString();
		const QJsonValue pin_value = pin.contains(g_key_value) ? pin.value(g_key_value) : pin.value(g_key_legacy_pin_default_value);
		drawn.value = PinDisplayValue(pin_value.toString());
			drawn.methods = pin.value(g_key_methods).toArray();
			drawn.color = PinColor(pin.value(g_key_type).toString(), drawn.exec);
			if (drawn.input)
				inputs_.append(drawn);
			else
				outputs_.append(drawn);
		}
		layout();
		update();
	}

	QRectF boundingRect() const override
	{
		return bounds_.adjusted(-12, -4, 16, 10);
	}

	QPointF pinAnchor(const QString& guid) const
	{
		for (int i = 0; i < inputs_.size(); ++i)
			if (inputs_[i].guid == guid) return QPointF(bounds_.left() + 18, header_h_ + 4 + i * row_h_ + row_h_ * 0.5);
		for (int i = 0; i < outputs_.size(); ++i)
			if (outputs_[i].guid == guid) return QPointF(bounds_.right() - 18, header_h_ + 4 + i * row_h_ + row_h_ * 0.5);
		return {};
	}

	QString pinAt(const QPointF& pos) const
	{
		for (const auto& pin : inputs_) if (QLineF(pos, pinAnchor(pin.guid)).length() <= 12) return pin.guid;
		for (const auto& pin : outputs_) if (QLineF(pos, pinAnchor(pin.guid)).length() <= 12) return pin.guid;
		return {};
	}

	bool isOutputDataPin(const QString& guid) const
	{
		for (const auto& pin : outputs_)
			if (pin.guid == guid) return !pin.exec;
		return false;
	}

	QJsonArray methodsForOutputPin(const QString& guid) const
	{
		for (const auto& pin : outputs_)
			if (pin.guid == guid && !pin.exec) return pin.methods;
		return {};
	}

	void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) override
	{
		painter->setRenderHint(QPainter::Antialiasing);
		const QRectF shadow = bounds_.translated(3, 4);
		painter->setPen(Qt::NoPen);
		// Keep the offset shadow subtle; it must not become an opaque backing plate.
		painter->setBrush(QColor(0, 0, 0, 35));
		painter->drawRoundedRect(shadow, 6, 6);

		QLinearGradient body_gradient(bounds_.topLeft(), bounds_.bottomLeft());
		// UE-style node bodies are translucent so the graph grid remains visible underneath.
		body_gradient.setColorAt(0.0, QColor(18, 22, 23, 128));
		//body_gradient.setColorAt(0.18, QColor(11, 14, 14, 148));
		body_gradient.setColorAt(1.0, QColor(5, 7, 7, 55));
		painter->setBrush(body_gradient);
		painter->drawRoundedRect(bounds_, 6, 6);

		painter->save();
		painter->setClipRect(QRectF(bounds_.left(), bounds_.top(), bounds_.width(), header_h_));
		bool has_exec = false;
		for (const auto& pin : inputs_)
			has_exec = has_exec || pin.exec;
		for (const auto& pin : outputs_)
			has_exec = has_exec || pin.exec;
		const QColor header = HeaderColor(key_, group_, has_exec);
		QLinearGradient header_gradient(QPointF(bounds_.left(), bounds_.top()),
			QPointF(bounds_.left(), bounds_.top() + header_h_));
		QColor header_top = header.lighter(125);
		QColor header_bottom = header.darker(135);
		header_top.setAlpha(158);
		header_bottom.setAlpha(128);
		header_gradient.setColorAt(0.0, header_top);
		header_gradient.setColorAt(1.0, header_bottom);
		painter->setBrush(header_gradient);
		painter->drawRoundedRect(bounds_, 6, 6);
		painter->restore();
		painter->setPen(QPen(QColor(8, 12, 14, 210), 1.0));
		painter->drawLine(QPointF(bounds_.left() + 1, bounds_.top() + header_h_),
			QPointF(bounds_.right() - 1, bounds_.top() + header_h_));

		painter->setBrush(Qt::NoBrush);
		QColor border = CG::Ui::NodeBorder();
		if (runtime_status_ == QStringLiteral("Ok")) border = QColor(86, 184, 126);
		else if (runtime_status_ == QStringLiteral("Error")) border = QColor(226, 82, 72);
		if (isSelected()) border = CG::Ui::Selection();
		painter->setPen(QPen(border, isSelected() ? 2.0 : 1.6));
		painter->drawRoundedRect(bounds_.adjusted(0.5, 0.5, -0.5, -0.5), 6, 6);

		QFont title_font;
		title_font.setPixelSize(12);
		title_font.setBold(true);
		painter->setFont(title_font);
		painter->setPen(header.lightness() > 160 ? QColor(24, 24, 24) : CG::Ui::Text());
		painter->drawText(QRectF(bounds_.left() + 12, bounds_.top(), bounds_.width() - 24, header_h_),
			Qt::AlignVCenter | Qt::AlignLeft, title_);

		QFont label_font;
		label_font.setPixelSize(11);
		painter->setFont(label_font);
		const int rows = qMax(inputs_.size(), outputs_.size());
		for (int i = 0; i < rows; ++i) {
			const qreal cy = header_h_ + 4 + i * row_h_ + row_h_ * 0.5;
			if (i < inputs_.size())
				paintPin(painter, inputs_.at(i), bounds_.left(), cy);
			if (i < outputs_.size())
				paintPin(painter, outputs_.at(i), bounds_.right(), cy);
		}
	}

	void setRuntimeStatus(const QString& status)
	{
		runtime_status_ = status;
		update();
	}

	void setDefaultEditCallback(std::function<void(const QString&, const QString&, const QString&)> callback)
	{
		default_edit_callback_ = std::move(callback);
	}

private:
	void layout()
	{
		QFont title_font;
		title_font.setPixelSize(12);
		title_font.setBold(true);
		const QFontMetrics title_fm(title_font);
		QFont label_font;
		label_font.setPixelSize(11);
		const QFontMetrics label_fm(label_font);
		qreal width = title_fm.horizontalAdvance(title_) + 36;
		const int rows = qMax(inputs_.size(), outputs_.size());
		for (int i = 0; i < rows; ++i) {
			qreal row = 56;
			if (i < inputs_.size())
				row += label_fm.horizontalAdvance(inputs_.at(i).name)
					+ (isEditableInput(inputs_.at(i)) ? (inputs_.at(i).type == QStringLiteral("bool") ? 34 : 124) : 0);
			if (i < outputs_.size())
				row += label_fm.horizontalAdvance(pinLabel(outputs_.at(i)));
			width = qMax(width, row);
		}
		width = qMax<qreal>(width, 168);
		const qreal height = header_h_ + 4 + qMax(1, rows) * row_h_ + 8;
		bounds_ = QRectF(0, 0, width, height);
		QVector<QGraphicsProxyWidget*> active_editors;
		for (int i = 0; i < inputs_.size(); ++i) {
			const auto& pin = inputs_.at(i);
			if (!isEditableInput(pin)) continue;
			const qreal x = bounds_.left() + 30 + label_fm.horizontalAdvance(pin.name) + 10;
			const qreal y = header_h_ + 4 + i * row_h_ + (row_h_ - 20) * 0.5;
			const bool is_bool = pin.type == QStringLiteral("bool");
			QGraphicsProxyWidget* proxy = nullptr;
			for (auto* candidate : editors_) {
				QWidget* widget = candidate->widget();
				if (widget && widget->property("pinGuid").toString() == pin.guid) {
					proxy = candidate;
					break;
				}
			}
			if (proxy && proxy->widget()->property("pinType").toString() != pin.type) {
				editors_.removeOne(proxy);
				delete proxy;
				proxy = nullptr;
			}
			if (!proxy) {
				proxy = new QGraphicsProxyWidget(this);
				QWidget* editor = nullptr;
				if (is_bool) {
					auto* check = new QCheckBox;
					check->setObjectName(QStringLiteral("PinEditor"));
					check->setFixedSize(20, 20);
					check->setChecked(pin.value.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0 || pin.value == QStringLiteral("1"));
					QObject::connect(check, &QCheckBox::toggled, check, [this, id = pin.guid, type = pin.type](bool value) {
						if (default_edit_callback_) default_edit_callback_(id, type, value ? QStringLiteral("true") : QStringLiteral("false"));
					});
					editor = check;
				} else {
					auto* edit = new QLineEdit(pin.value);
					edit->setObjectName(QStringLiteral("PinEditor"));
					edit->setFixedSize(110, 20);
					if (pin.type == QStringLiteral("int")) edit->setValidator(new QIntValidator(edit));
					else if (pin.type == QStringLiteral("float") || pin.type == QStringLiteral("double"))
						edit->setValidator(new QDoubleValidator(edit));
					QObject::connect(edit, &QLineEdit::editingFinished, edit, [this, id = pin.guid, type = pin.type, edit] {
						if (default_edit_callback_) default_edit_callback_(id, type, edit->text());
					});
					editor = edit;
				}
				editor->setProperty("pinGuid", pin.guid);
				editor->setProperty("pinType", pin.type);
				editor->setStyleSheet(CG::Ui::PinEditorStyleSheet());
				proxy->setWidget(editor);
			} else if (is_bool) {
				auto* check = qobject_cast<QCheckBox*>(proxy->widget());
				const bool value = pin.value.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0 || pin.value == QStringLiteral("1");
				if (check && check->isChecked() != value) {
					const QSignalBlocker blocker(check);
					check->setChecked(value);
				}
			} else if (auto* edit = qobject_cast<QLineEdit*>(proxy->widget()); edit && edit->text() != pin.value) {
				const QSignalBlocker blocker(edit);
				edit->setText(pin.value);
			}
			active_editors.append(proxy);
			proxy->setPos(x, y);
			if (pin.type == QStringLiteral("bool")) proxy->setMaximumSize(20, 20);
			else proxy->setMaximumSize(110, 20);
		}
		for (auto* editor : editors_)
			if (!active_editors.contains(editor)) delete editor;
		editors_ = active_editors;
	}

	void paintPin(QPainter* painter, const DrawnPin& pin, qreal edge, qreal cy) const
	{
		const qreal pin_center = edge + (pin.input ? 18.0 : -18.0);
		qreal text_gap = 30;
		if (pin.exec) {
			const qreal half = 4.5;
			const qreal len = 10;
			// Keep the execution shape's left edge aligned with the visible left
			// edge of the data socket on the same row.
			const qreal socket_left = pin_center - half;
			QPolygonF arrow;
			arrow << QPointF(socket_left, cy - half)
				<< QPointF(socket_left + len * 0.52, cy - half)
				<< QPointF(socket_left + len, cy)
				<< QPointF(socket_left + len * 0.52, cy + half)
				<< QPointF(socket_left, cy + half);
			painter->setPen(QPen(QColor(255, 255, 255, 220), 0.9));
			painter->setBrush(QColor(245, 245, 245, 225));
			painter->drawPolygon(arrow);
			text_gap = 32;
		} else {
			// UE data sockets use a colored ring with a compact, consistently right-facing wedge.
			// Draw the wedge first so the ring remains the dominant part of the socket.
			const qreal direction = 1.0;
			const qreal wedge_base = 3.0;
			const qreal wedge_tip = 9.0;
			const qreal wedge_half = 2.8;
			QPolygonF data_arrow;
			data_arrow << QPointF(pin_center + direction * wedge_base, cy - wedge_half)
				<< QPointF(pin_center + direction * wedge_tip, cy)
				<< QPointF(pin_center + direction * wedge_base, cy + wedge_half);
			painter->setPen(Qt::NoPen);
			painter->setBrush(pin.color);
			painter->drawPolygon(data_arrow);

			painter->setPen(QPen(pin.color, 1.5));
			painter->setBrush(pin.connected ? pin.color : QColor(10, 12, 12));
			painter->drawEllipse(QRectF(pin_center - 4.5, cy - 4.5, 9, 9));
		}

		if (pin.name.isEmpty())
			return;
		painter->setPen(CG::Ui::Text());
		const QString label = pin.input && isEditableInput(pin) ? pin.name : pinLabel(pin);
		const qreal middle = bounds_.center().x();
		if (pin.input)
			painter->drawText(QRectF(edge + text_gap, cy - 9, middle - edge - text_gap - 4, 18), Qt::AlignVCenter | Qt::AlignLeft, label);
		else
			painter->drawText(QRectF(middle + 4, cy - 9, edge - middle - text_gap - 4, 18), Qt::AlignVCenter | Qt::AlignRight, label);
	}

	bool isEditableInput(const DrawnPin& pin) const
	{
		return pin.input && !pin.exec && !pin.connected && !pin.type.isEmpty();
	}

	QString pinLabel(const DrawnPin& pin) const
	{
		QString value = pin.input && !pin.connected ? pin.value : QString{};
		if (value.isEmpty() && pin.input && !pin.connected
			&& (pin.type == QStringLiteral("std::string") || pin.type == QStringLiteral("string")))
			value = QStringLiteral("\"\"");
		return value.isEmpty() ? pin.name : pin.name + QStringLiteral("  ") + value;
	}

	QString title_;
	QString key_;
	QString group_;
	QString runtime_status_;
	QVector<DrawnPin> inputs_;
	QVector<DrawnPin> outputs_;
	QVector<QGraphicsProxyWidget*> editors_;
	std::function<void(const QString&, const QString&, const QString&)> default_edit_callback_;
	QRectF bounds_;
	qreal header_h_{32};
	qreal row_h_{28};
};

class GraphLineItem : public QGraphicsPathItem {
public:
	GraphLineItem(const QString& guid, const QString& fromNode, const QString& fromPin, const QString& toNode, const QString& toPin)
		: fromNode_(fromNode), fromPin_(fromPin), toNode_(toNode), toPin_(toPin)
	{
		setData(0, guid);
		setFlag(QGraphicsItem::ItemIsSelectable);
		setZValue(-0.5);
		setPen(QPen(QColor(235, 190, 70, 235), 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
	}

	void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) override
	{
		if (!painter || path().isEmpty()) return;
		painter->setRenderHint(QPainter::Antialiasing);
		const QColor color = isSelected() ? QColor(255, 245, 150, 255) : QColor(235, 190, 70, 235);
		painter->setPen(QPen(color, isSelected() ? 3.5 : 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
		painter->setBrush(Qt::NoBrush);
		painter->drawPath(path());
	}

	void updatePath(QGraphicsItem* from, QGraphicsItem* to)
	{
		auto* fromNode = dynamic_cast<GraphNodeItem*>(from);
		auto* toNode = dynamic_cast<GraphNodeItem*>(to);
		if (!fromNode || !toNode) return;
		QPointF start = fromNode->pinAnchor(fromPin_);
		QPointF end = toNode->pinAnchor(toPin_);
		if (start.isNull()) start = QPointF(fromNode->boundingRect().right(), fromNode->boundingRect().center().y());
		if (end.isNull()) end = QPointF(toNode->boundingRect().left(), toNode->boundingRect().center().y());
		start = fromNode->sceneTransform().map(start);
		end = toNode->sceneTransform().map(end);
		const qreal bend = qMax<qreal>(40.0, qAbs(end.x() - start.x()) * 0.45);
		const qreal direction = end.x() >= start.x() ? 1.0 : -1.0;
		QPainterPath path(start);
		path.cubicTo(start + QPointF(direction * bend, 0), end - QPointF(direction * bend, 0), end);
		setPath(path);
	}

	QString fromNode_, fromPin_, toNode_, toPin_;
};

GraphView::GraphView(const QString& guid, QWidget* parent)
	: QGraphicsView(parent)
	, guid_(guid)
{
	setObjectName(QStringLiteral("GraphView"));
	setScene(new GraphScene(this));
	setFocusPolicy(Qt::StrongFocus);
	setRenderHint(QPainter::Antialiasing);
	setDragMode(QGraphicsView::NoDrag);
	setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
	setResizeAnchor(QGraphicsView::AnchorViewCenter);
	setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setAcceptDrops(true);
	viewport()->setAcceptDrops(true);
	viewport()->installEventFilter(this);
	connect(scene(), &QGraphicsScene::selectionChanged, this, [this] {
		if (!scene())
			return;
		const auto selected = scene()->selectedItems();
		if (selected.isEmpty())
			return;
		if (dynamic_cast<GraphLineItem*>(selected.front())) return;
		const QString guid = selected.front()->data(0).toString();
		if (!guid.isEmpty())
			emit nodeSelected(guid);
	});
	centerOn(0, 0);
}

void GraphView::addNode(const QString& guid, const QString& name, const QString& key, const QString& group, const QPointF& pos, const QJsonArray& pins)
{
	if (!scene() || guid.isEmpty())
		return;
	for (auto* item : scene()->items()) {
		if (item->data(0).toString() == guid)
			return;
	}
	auto* node = new GraphNodeItem(guid, name, key, group, pins);
	node->setDefaultEditCallback([this, guid](const QString& pin, const QString& type, const QString& value) {
		emit pinValueEditRequested(guid, pin, type, value);
	});
	node->setPos(pos);
	scene()->addItem(node);
	refreshLines();
}

void GraphView::refreshLines()
{
	if (!scene()) return;
	for (auto* candidate : scene()->items()) {
		auto* line = dynamic_cast<GraphLineItem*>(candidate);
		if (!line) continue;
		QGraphicsItem* from = nullptr;
		QGraphicsItem* to = nullptr;
		for (auto* item : scene()->items()) {
			if (item->data(0).toString() == line->fromNode_) from = item;
			if (item->data(0).toString() == line->toNode_) to = item;
		}
		line->updatePath(from, to);
	}
}

void GraphView::updateLinePreview(const QPointF& scene_pos)
{
	if (!scene() || pendingLineNode_.isEmpty() || !previewLine_)
		return;

	GraphNodeItem* source = nullptr;
	for (auto* item : scene()->items()) {
		if (item->data(0).toString() == pendingLineNode_) {
			source = dynamic_cast<GraphNodeItem*>(item);
			break;
		}
	}
	if (!source)
		return;

	QPointF start = source->pinAnchor(pendingLinePin_);
	if (start.isNull())
		start = QPointF(source->boundingRect().right(), source->boundingRect().center().y());
	start = source->sceneTransform().map(start);

	QPointF end = scene_pos;
	if (auto* target = dynamic_cast<GraphNodeItem*>(scene()->itemAt(scene_pos, QTransform()))) {
		const QString target_pin = target->pinAt(target->mapFromScene(scene_pos));
		if (!target_pin.isEmpty())
			end = target->sceneTransform().map(target->pinAnchor(target_pin));
	}
	const qreal bend = qMax<qreal>(40.0, qAbs(end.x() - start.x()) * 0.45);
	const qreal direction = end.x() >= start.x() ? 1.0 : -1.0;
	QPainterPath path(start);
	path.cubicTo(start + QPointF(direction * bend, 0), end - QPointF(direction * bend, 0), end);
	previewLine_->setPath(path);
}

void GraphView::clearLinePreview()
{
	pendingLineNode_.clear();
	pendingLinePin_.clear();
	pendingLineMethods_ = {};
	pendingLineIsOutputData_ = false;
	if (!previewLine_)
		return;
	if (scene())
		scene()->removeItem(previewLine_);
	delete previewLine_;
	previewLine_ = nullptr;
}

void GraphView::moveNode(const QString& guid, const QPointF& pos)
{
	if (!scene() || guid.isEmpty())
		return;
	for (auto* item : scene()->items()) {
		if (item->data(0).toString() != guid)
			continue;
		item->setPos(pos);
		refreshLines();
		return;
	}
}

void GraphView::setNodePins(const QString& guid, const QJsonArray& pins)
{
	if (!scene()) return;
	for (auto* item : scene()->items()) {
		if (item->data(0).toString() != guid) continue;
		if (auto* node = dynamic_cast<GraphNodeItem*>(item)) {
			node->setPins(pins);
			refreshLines();
		}
		return;
	}
}

void GraphView::setNodeRuntimeStatus(const QString& guid, const QString& status)
{
	if (!scene() || guid.isEmpty()) return;
	for (auto* item : scene()->items()) {
		if (item->data(0).toString() == guid) {
			if (auto* node = dynamic_cast<GraphNodeItem*>(item)) node->setRuntimeStatus(status);
			return;
		}
	}
}

void GraphView::addLine(const QString& guid, const QString& fromNode, const QString& fromPin, const QString& toNode, const QString& toPin)
{
	if (!scene() || guid.isEmpty()) return;
	for (auto* item : scene()->items()) if (item->data(0).toString() == guid) return;
	auto* line = new GraphLineItem(guid, fromNode, fromPin, toNode, toPin);
	scene()->addItem(line);
	QGraphicsItem* from = nullptr; QGraphicsItem* to = nullptr;
	for (auto* item : scene()->items()) {
		if (item->data(0).toString() == fromNode) from = item;
		if (item->data(0).toString() == toNode) to = item;
	}
	line->updatePath(from, to);
	refreshLines();
}

void GraphView::removeLine(const QString& guid)
{
	if (!scene()) return;
	for (auto* item : scene()->items()) {
		auto* line = dynamic_cast<GraphLineItem*>(item);
		if (line && line->data(0).toString() == guid) { scene()->removeItem(item); delete item; return; }
	}
}

void GraphView::removeNode(const QString& guid)
{
	if (!scene() || guid.isEmpty())
		return;
	for (auto* item : scene()->items()) {
		if (item->data(0).toString() != guid)
			continue;
		scene()->removeItem(item);
		delete item;
		return;
	}
}

bool GraphView::eventFilter(QObject* watched, QEvent* event)
{
	if (watched == viewport() && event->type() == QEvent::ContextMenu) {
		auto* menu_event = static_cast<QContextMenuEvent*>(event);
		const QPoint view_pos = viewport()->mapTo(this, menu_event->pos());
		emit contextMenuRequested(guid_, menu_event->globalPos(), mapToScene(view_pos));
		return true;
	}
	if (watched == viewport() && event->type() == QEvent::KeyPress && scene()) {
		auto* key_event = static_cast<QKeyEvent*>(event);
		if (key_event->key() == Qt::Key_Escape && !pendingLineNode_.isEmpty()) {
			clearLinePreview();
			key_event->accept();
			return true;
		}
		if (key_event->key() == Qt::Key_Delete) {
			for (auto* item : scene()->selectedItems()) {
				if (dynamic_cast<QGraphicsPathItem*>(item)) {
					emit deleteLineRequested(item->data(0).toString());
					return true;
				}
				const QString guid = item->data(0).toString();
				const QString key = item->data(1).toString();
				if (guid.isEmpty())
					continue;
				if (key == QString::fromUtf8(g_node_entry))
					return true;
				emit deleteNodeRequested(guid, key);
				return true;
			}
		}
	}
	return QGraphicsView::eventFilter(watched, event);
}

void GraphView::keyPressEvent(QKeyEvent* event)
{
	if (!event) return;
	if (event->key() == Qt::Key_Escape && !pendingLineNode_.isEmpty()) {
		clearLinePreview();
		event->accept();
		return;
	}
	if (event->key() == Qt::Key_Delete && scene()) {
		for (auto* item : scene()->selectedItems()) {
			if (dynamic_cast<GraphLineItem*>(item)) {
				emit deleteLineRequested(item->data(0).toString());
				event->accept();
				return;
			}
			const QString guid = item->data(0).toString();
			const QString key = item->data(1).toString();
			if (guid.isEmpty()) continue;
			if (key == QString::fromUtf8(g_node_entry)) {
				event->accept();
				return;
			}
			emit deleteNodeRequested(guid, key);
			event->accept();
			return;
		}
	}
	QGraphicsView::keyPressEvent(event);
}

void GraphView::dragEnterEvent(QDragEnterEvent* event)
{
	const QMimeData* data = event->mimeData();
	if (data && (data->hasFormat(kFlowMime) || data->hasFormat(kVarMime)))
		event->acceptProposedAction();
	else
		event->ignore();
}

void GraphView::dragMoveEvent(QDragMoveEvent* event)
{
	const QMimeData* data = event->mimeData();
	if (data && (data->hasFormat(kFlowMime) || data->hasFormat(kVarMime)))
		event->acceptProposedAction();
	else
		event->ignore();
}

void GraphView::dropEvent(QDropEvent* event)
{
	const QMimeData* data = event->mimeData();
	if (!data) {
		event->ignore();
		return;
	}
	const QPointF scene_pos = mapToScene(event->pos());
	if (data->hasFormat(kFlowMime)) {
		emit functionDropped(QString::fromUtf8(data->data(kFlowMime)), scene_pos);
		event->acceptProposedAction();
		return;
	}
	if (data->hasFormat(kVarMime)) {
		emit variableDropped(data->text(), QString::fromUtf8(data->data(kVarMime)), scene_pos, event->keyboardModifiers());
		event->acceptProposedAction();
		return;
	}
	event->ignore();
}

void GraphView::wheelEvent(QWheelEvent* event)
{
	const qreal factor = event->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
	const qreal next = transform().m11() * factor;
	if (next < 0.2 || next > 2.5)
		return;
	scale(factor, factor);
}

void GraphView::mousePressEvent(QMouseEvent* event)
{
	drag_node_guid_.clear();
	if (event->button() == Qt::MiddleButton) {
		panning_ = true;
		pan_origin_ = event->pos();
		setCursor(Qt::ClosedHandCursor);
		event->accept();
		return;
	}
	if (event->button() == Qt::LeftButton && scene()) {
		const QPointF scene_pos = mapToScene(event->pos());
		if (auto* item = dynamic_cast<GraphNodeItem*>(scene()->itemAt(scene_pos, QTransform()))) {
			const QString pin = item->pinAt(item->mapFromScene(scene_pos));
			if (!pin.isEmpty()) {
				pendingLineNode_ = item->data(0).toString();
				pendingLinePin_ = pin;
				pendingLineIsOutputData_ = item->isOutputDataPin(pin);
				pendingLineMethods_ = item->methodsForOutputPin(pin);
				previewLine_ = new QGraphicsPathItem();
				previewLine_->setPen(QPen(QColor(255, 220, 100, 220), 2.0, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin));
				previewLine_->setZValue(-0.4);
				previewLine_->setAcceptedMouseButtons(Qt::NoButton);
				scene()->addItem(previewLine_);
				updateLinePreview(scene_pos);
				event->accept();
				return;
			}
		}
	}
	QString clicked_selected_node;
	if (event->button() == Qt::LeftButton && scene()) {
		auto* node = dynamic_cast<GraphNodeItem*>(scene()->itemAt(mapToScene(event->pos()), QTransform()));
		if (node && node->isSelected())
			clicked_selected_node = node->data(0).toString();
	}
	QGraphicsView::mousePressEvent(event);
	if (!clicked_selected_node.isEmpty())
		emit nodeSelected(clicked_selected_node);
	for (QGraphicsItem* item = scene() ? scene()->mouseGrabberItem() : nullptr; item; item = item->parentItem()) {
		if (dynamic_cast<GraphNodeItem*>(item)) {
			drag_node_guid_ = item->data(0).toString();
			drag_origin_ = item->pos();
			break;
		}
	}
}

void GraphView::mouseMoveEvent(QMouseEvent* event)
{
	if (!pendingLineNode_.isEmpty()) {
		updateLinePreview(mapToScene(event->pos()));
		event->accept();
		return;
	}
	if (!panning_) {
		QGraphicsView::mouseMoveEvent(event);
		if (!drag_node_guid_.isEmpty())
			refreshLines();
		return;
	}
	const QPoint delta = event->pos() - pan_origin_;
	pan_origin_ = event->pos();
	horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
	verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
	event->accept();
}

void GraphView::mouseReleaseEvent(QMouseEvent* event)
{
	if (event->button() == Qt::MiddleButton && panning_) {
		panning_ = false;
		unsetCursor();
		event->accept();
		return;
	}
	if (event->button() == Qt::LeftButton && !pendingLineNode_.isEmpty()) {
		const QPointF scene_pos = mapToScene(event->pos());
		QString target_node;
		QString target_pin;
		bool over_node = false;
		if (auto* item = dynamic_cast<GraphNodeItem*>(scene()->itemAt(scene_pos, QTransform()))) {
			over_node = true;
			const QString pin = item->pinAt(item->mapFromScene(scene_pos));
			if (!pin.isEmpty() && item->data(0).toString() != pendingLineNode_) {
				target_node = item->data(0).toString();
				target_pin = pin;
			}
		}
		const QString source_node = pendingLineNode_;
		const QString source_pin = pendingLinePin_;
		const bool source_is_output_data = pendingLineIsOutputData_;
		const QJsonArray methods = pendingLineMethods_;
		clearLinePreview();
		if (!target_node.isEmpty())
			emit lineRequested(guid_, source_node, source_pin, target_node, target_pin);
		else if (!over_node && source_is_output_data && !methods.isEmpty())
			emit methodMenuRequested(guid_, source_node, source_pin, methods, scene_pos, event->globalPos());
		event->accept();
		return;
	}
	QGraphicsView::mouseReleaseEvent(event);
	if (event->button() == Qt::LeftButton && !drag_node_guid_.isEmpty() && scene()) {
		for (auto* item : scene()->items()) {
			if (item->data(0).toString() != drag_node_guid_ || !dynamic_cast<GraphNodeItem*>(item))
				continue;
			const QPointF pos = item->pos();
			if (pos != drag_origin_)
				emit nodeMoved(drag_node_guid_, pos);
			break;
		}
	}
	drag_node_guid_.clear();
}

