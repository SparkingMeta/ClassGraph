#pragma execution_character_set("utf-8")

#include "GraphView.h"
#include "Theme.h"
#include "message.h"

#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QEvent>
#include <QGraphicsRectItem>
#include <QGraphicsSimpleTextItem>
#include <QKeyEvent>
#include <QPen>
#include <QFontMetrics>
#include <QJsonObject>
#include <QStyleOptionGraphicsItem>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QLinearGradient>
#include <QScrollBar>
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
	QString name;
	bool input{};
	bool exec{};
	bool connected{};
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
		for (const auto& row : pins) {
			const QJsonObject pin = row.toObject();
			DrawnPin drawn;
			drawn.name = pin.value(g_key_name).toString();
			drawn.exec = pin.value(g_key_kind).toInt() == 0;
			drawn.input = pin.value(g_key_direct).toInt() == 0;
			drawn.connected = pin.value(g_key_connected).toBool();
			drawn.color = PinColor(pin.value(g_key_type).toString(), drawn.exec);
			if (drawn.input)
				inputs_.append(drawn);
			else
				outputs_.append(drawn);
		}
		layout();
	}

	QRectF boundingRect() const override
	{
		return bounds_.adjusted(-12, -4, 16, 10);
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
		painter->setPen(QPen(isSelected() ? CG::Ui::Selection() : CG::Ui::NodeBorder(), isSelected() ? 2.0 : 1.0));
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
				row += label_fm.horizontalAdvance(inputs_.at(i).name);
			if (i < outputs_.size())
				row += label_fm.horizontalAdvance(outputs_.at(i).name);
			width = qMax(width, row);
		}
		width = qMax<qreal>(width, 168);
		const qreal height = header_h_ + 4 + qMax(1, rows) * row_h_ + 8;
		bounds_ = QRectF(0, 0, width, height);
	}

	void paintPin(QPainter* painter, const DrawnPin& pin, qreal edge, qreal cy) const
	{
		const qreal pin_center = edge + (pin.input ? 18.0 : -18.0);
		qreal text_gap = 30;
		if (pin.exec) {
			const qreal half = 7;
			const qreal len = 11;
			const qreal gap = 15;
			QPolygonF arrow;
			if (pin.input) {
				const qreal x0 = edge + gap;
				arrow << QPointF(x0, cy - half)
					<< QPointF(x0 + len * 0.52, cy - half)
					<< QPointF(x0 + len, cy)
					<< QPointF(x0 + len * 0.52, cy + half)
					<< QPointF(x0, cy + half);
			} else {
				const qreal x1 = edge - gap;
				arrow << QPointF(x1 - len, cy - half)
					<< QPointF(x1 - len * 0.48, cy - half)
					<< QPointF(x1, cy)
					<< QPointF(x1 - len * 0.48, cy + half)
					<< QPointF(x1 - len, cy + half);
			}
			painter->setPen(QPen(QColor(255, 255, 255, 220), 0.9));
			painter->setBrush(QColor(245, 245, 245, 225));
			painter->drawPolygon(arrow);
			text_gap = gap + len + 7;
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
		if (pin.input)
			painter->drawText(QRectF(edge + text_gap, cy - 9, bounds_.width() * 0.55, 18), Qt::AlignVCenter | Qt::AlignLeft, pin.name);
		else
			painter->drawText(QRectF(bounds_.left(), cy - 9, edge - bounds_.left() - text_gap, 18), Qt::AlignVCenter | Qt::AlignRight, pin.name);
	}

	QString title_;
	QString key_;
	QString group_;
	QVector<DrawnPin> inputs_;
	QVector<DrawnPin> outputs_;
	QRectF bounds_;
	qreal header_h_{32};
	qreal row_h_{28};
};

GraphView::GraphView(const QString& guid, QWidget* parent)
	: QGraphicsView(parent)
	, guid_(guid)
{
	setObjectName(QStringLiteral("GraphView"));
	setScene(new GraphScene(this));
	setRenderHint(QPainter::Antialiasing);
	setDragMode(QGraphicsView::NoDrag);
	setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
	setResizeAnchor(QGraphicsView::AnchorViewCenter);
	setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setAcceptDrops(true);
	viewport()->setAcceptDrops(false);
	viewport()->installEventFilter(this);
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
	node->setPos(pos);
	scene()->addItem(node);
}

void GraphView::moveNode(const QString& guid, const QPointF& pos)
{
	if (!scene() || guid.isEmpty())
		return;
	for (auto* item : scene()->items()) {
		if (item->data(0).toString() != guid)
			continue;
		item->setPos(pos);
		return;
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
		if (key_event->key() == Qt::Key_Delete) {
			for (auto* item : scene()->selectedItems()) {
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
	if (event->button() == Qt::MiddleButton) {
		panning_ = true;
		pan_origin_ = event->pos();
		setCursor(Qt::ClosedHandCursor);
		event->accept();
		return;
	}
	QGraphicsView::mousePressEvent(event);
	drag_item_ = scene() ? scene()->mouseGrabberItem() : nullptr;
	drag_origin_ = drag_item_ ? drag_item_->pos() : QPointF();
}

void GraphView::mouseMoveEvent(QMouseEvent* event)
{
	if (!panning_) {
		QGraphicsView::mouseMoveEvent(event);
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
	QGraphicsView::mouseReleaseEvent(event);
	if (event->button() == Qt::LeftButton && drag_item_) {
		const QString guid = drag_item_->data(0).toString();
		const QPointF pos = drag_item_->pos();
		if (!guid.isEmpty() && pos != drag_origin_)
			emit nodeMoved(guid, pos);
	}
	drag_item_ = nullptr;
}
