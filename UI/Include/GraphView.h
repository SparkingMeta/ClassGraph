#pragma once
#pragma execution_character_set("utf-8")

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QJsonArray>
#include <QPoint>
#include <QString>

class QContextMenuEvent;
class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;
class QEvent;
class QGraphicsPathItem;
class QKeyEvent;
class QMouseEvent;
class QObject;
class QPainter;
class QWheelEvent;

inline constexpr const char kFlowMime[] = "application/x-classgraph-function";
inline constexpr const char kVarMime[] = "application/x-classgraph-variable";

class GraphScene : public QGraphicsScene {
public:
	explicit GraphScene(QObject* parent = nullptr);

protected:
	void drawBackground(QPainter* painter, const QRectF& rect) override;
};

class GraphView : public QGraphicsView {
	Q_OBJECT
public:
	explicit GraphView(const QString& guid, QWidget* parent = nullptr);
	const QString& guid() const { return guid_; }
	void addNode(const QString& guid, const QString& name, const QString& key, const QString& group, const QPointF& pos, const QJsonArray& pins);
	void removeNode(const QString& guid);
	void moveNode(const QString& guid, const QPointF& pos);
	void setNodePins(const QString& guid, const QJsonArray& pins);
	void setNodeRuntimeStatus(const QString& guid, const QString& status);
	void addLine(const QString& guid, const QString& fromNode, const QString& fromPin, const QString& toNode, const QString& toPin);
	void removeLine(const QString& guid);

signals:
	void functionDropped(const QString& guid, const QPointF& scene_pos);
	void variableDropped(const QString& name, const QString& guid, const QPointF& scene_pos, Qt::KeyboardModifiers mods);
	void contextMenuRequested(const QString& guid, const QPoint& global_pos, const QPointF& scene_pos);
	void deleteNodeRequested(const QString& guid, const QString& key);
	void nodeMoved(const QString& guid, const QPointF& pos);
	void nodeSelected(const QString& guid);
	void pinValueEditRequested(const QString& node, const QString& pin, const QString& type, const QString& value);
	void lineRequested(const QString& flow, const QString& fromNode, const QString& fromPin, const QString& toNode, const QString& toPin);
	void methodMenuRequested(const QString& flow, const QString& fromNode, const QString& fromPin,
		const QJsonArray& methods, const QPointF& scene_pos, const QPoint& global_pos);
	void deleteLineRequested(const QString& guid);
	void dynamicPinActionRequested(const QString& node, const QString& pin, const QString& action);

protected:
	bool eventFilter(QObject* watched, QEvent* event) override;
	void dragEnterEvent(QDragEnterEvent* event) override;
	void dragMoveEvent(QDragMoveEvent* event) override;
	void dropEvent(QDropEvent* event) override;
	void wheelEvent(QWheelEvent* event) override;
	void keyPressEvent(QKeyEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;

private:
	void refreshLines();
	void updateLinePreview(const QPointF& scene_pos);
	void clearLinePreview();

	QString guid_;
	bool panning_{};
	QPoint pan_origin_{};
	QString drag_node_guid_;
	QPointF drag_origin_{};
	QString pendingLineNode_;
	QString pendingLinePin_;
	QJsonArray pendingLineMethods_;
	bool pendingLineIsOutputData_{};
	QGraphicsPathItem* previewLine_{};
};
