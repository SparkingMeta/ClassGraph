#pragma once
#pragma execution_character_set("utf-8")

#include "IRouter.h"
#include "VDto.h"

#include <functional>
#include <QGraphicsItem>
#include <QGraphicsObject>
#include <QGraphicsPathItem>
#include <QGraphicsView>
#include <QString>

#include <unordered_map>
#include <unordered_set>
#include <vector>

class QWidget;

inline const char kClassGraphVarMime[] = "application/x-classgraph-variable";
inline const char kClassGraphFlowMime[] = "application/x-classgraph-function";

class NodeGraphicsItem;
class LineGraphicsItem;

using namespace CG::Router;

class PinGraphicsItem : public QGraphicsItem {
public:
	PinGraphicsItem(const PinDto& pin, NodeGraphicsItem* parent);

	const PinDto& Pin() const { return pin_; }
	void SetPin(const PinDto& pin) { pin_ = pin; }
	NodeGraphicsItem* OwnerNode() const;
	QPointF CenterInScene() const;
	bool IsExec() const { return pin_.kind == DtoPinKind::Exec; }
	bool IsOutput() const { return pin_.direction == DtoPinDir::Out; }
	bool IsConnected() const { return pin_.connected; }
	QColor Color() const;

	QRectF boundingRect() const override;
	void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
	QPainterPath shape() const override;

private:
	PinDto pin_;
};

class NodeGraphicsItem : public QGraphicsObject {
public:
	explicit NodeGraphicsItem(const NodeDto& node, QGraphicsItem* parent = nullptr);

	const NodeDto& Node() const { return node_; }
	QString Guid() const { return QString::fromStdString(node_.guid); }
	PinGraphicsItem* FindPinItem(const QString& pin_id) const;
	void ApplyDto(const NodeDto& node);
	void SyncFromDto(const NodeDto& node);
	void SyncEditors();
	bool HitAddPin(const QPointF& scene_pos) const;

	QRectF boundingRect() const override;
	void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
	QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;

	const std::vector<PinGraphicsItem*>& Pins() const { return pins_; }

private:
	struct PinEditor {
		QString pin_id;
		QWidget* widget{};
	};

	void BuildPins();
	QString Title() const;
	QRectF AddPinRect() const;
	bool IsSwitch() const;
	static bool SamePinLayout(const NodeDto& a, const NodeDto& b);

	NodeDto node_;
	std::vector<PinGraphicsItem*> pins_;
	std::vector<PinEditor> editors_;
	qreal width_{ 150 };
	qreal height_{ 48 };
	bool syncing_{ false };
};

class LineGraphicsItem : public QGraphicsPathItem {
public:
	LineGraphicsItem(PinGraphicsItem* from, PinGraphicsItem* to,
		const LineDto& line, QGraphicsItem* parent = nullptr);
	void UpdatePath();
	PinGraphicsItem* From() const { return from_; }
	PinGraphicsItem* To() const { return to_; }
	QString LineId() const { return QString::fromStdString(line_.id); }

	QPainterPath shape() const override;
	void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

private:
	PinGraphicsItem* from_{};
	PinGraphicsItem* to_{};
	LineDto line_;
};

class GraphView : public QGraphicsView {
	Q_OBJECT
public:
	explicit GraphView(QWidget* parent = nullptr);

	void ApplyGraph(const GraphDto& graph);
	void SetSink(std::function<void(const Message&)> sink);
	void SetPalette(const std::vector<CallerItemDto>& callers);
	QString SelectedNodeGuid() const;

	void DeleteSelected();
	void AddExitAtCenter();
	void PlaceCaller(const QString& key, const QPointF& scene_pos);
	void RefreshLines();

signals:
	void NodeSelected(const QString& guid);
	void GraphChanged();
	void LogMessage(const QString& text);
	void VariableDropped(const QString& guid, const QPointF& scene_pos, Qt::KeyboardModifiers mods);
	void FunctionDropped(const QString& guid, const QPointF& scene_pos);
	void NodeContextMenu(const QString& node_guid, const QPoint& global_pos);

protected:
	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;
	void keyPressEvent(QKeyEvent* event) override;
	void wheelEvent(QWheelEvent* event) override;
	void contextMenuEvent(QContextMenuEvent* event) override;
	void dragEnterEvent(QDragEnterEvent* event) override;
	void dragMoveEvent(QDragMoveEvent* event) override;
	void dropEvent(QDropEvent* event) override;
	void drawBackground(QPainter* painter, const QRectF& rect) override;

private:
	PinGraphicsItem* PinAt(const QPoint& view_pos) const;
	NodeGraphicsItem* NodeAt(const QPoint& view_pos) const;
	LineGraphicsItem* LineAt(const QPoint& view_pos) const;
	bool EditorAt(const QPoint& view_pos) const;
	void ClearTempLine();
	void ClearLineItems();
	void RebuildAll(const GraphDto& graph, const std::unordered_set<std::string>& old_guids,
		const QString& pending);
	void ApplyGraphIncremental(const GraphDto& graph, const std::unordered_set<std::string>& old_guids,
		const QString& pending);
	void AddLineItem(PinGraphicsItem* from, PinGraphicsItem* to, const LineDto& line);
	void RebuildLinesFromGraph();
	std::unordered_map<std::string, NodeGraphicsItem*> NodeItemsByGuid() const;
	void ShowSpawnMenu(const QPoint& global_pos, const QPointF& scene_pos,
		const QString& connect_from_pin = {});
	void DispatchConnect(const QString& from_pin, const QString& to_pin);
	void TryConnectAfterSpawn(const QString& from_pin, const NodeDto* hint_unused);

	GraphDto graph_;
	std::vector<CallerItemDto> palette_;
	QGraphicsScene* scene_{};
	PinGraphicsItem* drag_pin_{};
	QGraphicsPathItem* temp_line_{};
	std::vector<LineGraphicsItem*> lines_;
	QPoint rmb_press_pos_;
	QPoint rmb_origin_;
	bool rmb_panning_{ false };
	QString pending_connect_from_;
};
