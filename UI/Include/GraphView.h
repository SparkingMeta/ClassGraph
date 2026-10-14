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
	PinGraphicsItem(const Pin& pin, NodeGraphicsItem* parent);

	const Pin& GetPin() const { return pin_; }
	void SetPin(const Pin& pin) { pin_ = pin; }
	NodeGraphicsItem* OwnerNode() const;
	QPointF CenterInScene() const;
	bool IsExec() const { return pin_.kind == PinKind::Exec; }
	bool IsOutput() const { return pin_.direction == PinDir::Out; }
	bool IsConnected() const { return pin_.connected; }
	QColor Color() const;

	QRectF boundingRect() const override;
	void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
	QPainterPath shape() const override;

private:
	Pin pin_;
};

class NodeGraphicsItem : public QGraphicsObject {
public:
	explicit NodeGraphicsItem(const Node& node, QGraphicsItem* parent = nullptr);

	const Node& GetNode() const { return node_; }
	QString Guid() const { return QString::fromStdString(node_.guid); }
	PinGraphicsItem* FindPinItem(const QString& pin_id) const;
	void ApplyDto(const Node& node);
	void SyncFromDto(const Node& node);
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
	static bool SamePinLayout(const Node& a, const Node& b);

	Node node_;
	std::vector<PinGraphicsItem*> pins_;
	std::vector<PinEditor> editors_;
	qreal width_{ 150 };
	qreal height_{ 48 };
	bool syncing_{ false };
};

class LineGraphicsItem : public QGraphicsPathItem {
public:
	LineGraphicsItem(PinGraphicsItem* from, PinGraphicsItem* to,
		const Line& line, QGraphicsItem* parent = nullptr);
	void UpdatePath();
	PinGraphicsItem* From() const { return from_; }
	PinGraphicsItem* To() const { return to_; }
	QString LineId() const { return QString::fromStdString(line_.id); }

	QPainterPath shape() const override;
	void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

private:
	PinGraphicsItem* from_{};
	PinGraphicsItem* to_{};
	Line line_;
};

class GraphView : public QGraphicsView {
	Q_OBJECT
public:
	explicit GraphView(QWidget* parent = nullptr);

	void ApplyGraph(const Graph& graph);
	void SetFlowGuid(const QString& guid);
	QString FlowGuid() const;
	bool GraphLoaded() const;
	void UpsertNode(const Node& node);
	void RemoveNode(const QString& guid);
	void UpsertLine(const Line& line);
	void RemoveLine(const QString& id);
	void RemoveLinesOfPin(const QString& pin_id);
	void SetSink(std::function<void(const Message&)> sink);
	void SetPalette(const std::vector<CallerItem>& callers);
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
	void RebuildAll(const Graph& graph, const std::unordered_set<std::string>& old_guids,
		const QString& pending);
	void ApplyGraphIncremental(const Graph& graph, const std::unordered_set<std::string>& old_guids,
		const QString& pending);
	void AddLineItem(PinGraphicsItem* from, PinGraphicsItem* to, const Line& line);
	void RebuildLinesFromGraph();
	std::unordered_map<std::string, NodeGraphicsItem*> NodeItemsByGuid() const;
	void ShowSpawnMenu(const QPoint& global_pos, const QPointF& scene_pos,
		const QString& connect_from_pin = {});
	void DispatchConnect(const QString& from_pin, const QString& to_pin);
	void TryConnectAfterSpawn(const QString& from_pin, const Node* hint_unused);

	Graph graph_;
	bool graph_loaded_{ false };
	std::vector<CallerItem> palette_;
	QGraphicsScene* scene_{};
	PinGraphicsItem* drag_pin_{};
	QGraphicsPathItem* temp_line_{};
	std::vector<LineGraphicsItem*> lines_;
	QPoint rmb_press_pos_;
	QPoint rmb_origin_;
	bool rmb_panning_{ false };
	QString pending_connect_from_;
};
