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

signals:
	void functionDropped(const QString& guid, const QPointF& scene_pos);
	void variableDropped(const QString& name, const QString& guid, const QPointF& scene_pos, Qt::KeyboardModifiers mods);
	void contextMenuRequested(const QString& guid, const QPoint& global_pos, const QPointF& scene_pos);
	void deleteNodeRequested(const QString& guid, const QString& key);
	void nodeMoved(const QString& guid, const QPointF& pos);

protected:
	bool eventFilter(QObject* watched, QEvent* event) override;
	void dragEnterEvent(QDragEnterEvent* event) override;
	void dragMoveEvent(QDragMoveEvent* event) override;
	void dropEvent(QDropEvent* event) override;
	void wheelEvent(QWheelEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;

private:
	QString guid_;
	bool panning_{};
	QPoint pan_origin_{};
	QGraphicsItem* drag_item_{};
	QPointF drag_origin_{};
};
