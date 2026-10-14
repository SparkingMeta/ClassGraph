#pragma once
#pragma execution_character_set("utf-8")

#include "Export.h"
#include "bus.h"
#include <functional>

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QMainWindow>
#include <QPoint>
#include <QPointF>
#include <QString>

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class QTreeWidget;
class QTreeWidgetItem;
class QTabBar;
class QFormLayout;
class QLabel;
class QTextEdit;
class QWidget;
class QComboBox;
class QEvent;
class QLineEdit;
class QDockWidget;
class QToolButton;
class QFrame;
class QStackedWidget;

enum Action {
	AddBP,
	DelBP,
	AltBP,
	OpenBP,
	CloseBP,

	AddFunc,
	DelFunc,
	AltFunc,
	RunFunc,
	SelectFunc,

	AddVar,
	DelVar,
	AltVar,
	SelectVar,

	AddNode,
	DelNode,
	AltNode,
	SelectNode,

	Compile,
	TypeSync,
	FuncSync,
};

using Emiter = std::function<void(const Message&)>;



class MACRO_UI_API MainWindow : public QMainWindow {
	Q_OBJECT
public:
	explicit MainWindow(QWidget* parent = nullptr);
	~MainWindow() override;
	void Notify(const Message& msg);
	void SetSink(std::function<void(const Message&)> sink);

private:
	void Dispatch(Action action, const QJsonObject& body = {});

	void Load();
private slots:
	void OnRunFunc();
	void OnCompile();

	void OnCreateBP();
	void OnDeleteBP();
	void OnAlterBP();
	void OnOpenBP(QTreeWidgetItem* item);
	void OnCloseBP();
	

	void OnAddFunc();
	void OnDelFunc();
	void OnAltFunc();

	void OnAddVar();
	void OnDelVar();
	void OnAltVar();

	void OnAddNode();
	void OnDelNode();
	void OnAltNode();

	void OnFuncSelected(QTreeWidgetItem* item);
	void OnVarSelected(QTreeWidgetItem* item);
	void OnNodeSelected(const QString& guid);

	void OnGraphTabChanged(int index);
	void OnGraphTabClose(int index);
	void OnGraphMenu(const QString& flow_guid, const QPoint& global_pos, const QPointF& scene_pos);
	void OnNodeMoved(const QString& guid, const QPointF& pos);
	
	void OnLog(const QString& text);

	void OnFuncMenu(const QPoint& pos);
	void OnVarMenu(const QPoint& pos);

	void OnAlterFunc(QTreeWidgetItem* item);
	void OnAlterVar(QTreeWidgetItem* item);
	void OnAlterAsset(QTreeWidgetItem* item);

	void OnVarDropped(const QString& name, const QString& guid, const QPointF& scene_pos, Qt::KeyboardModifiers mods);
	void OnFuncDropped(const QString& guid, const QPointF& scene_pos);

	void OnVarTypeChosen(int index);
	void OnAssetMenu(const QPoint& pos);

	void Slot_AddBP(const Message& msg);
	void Slot_DelBP(const Message& msg);
	void Slot_AltBP(const Message& msg);
	void Slot_OpenBP(const Message& msg);
	void Slot_CloseBP(const Message& msg);
	void Slot_TypeSync(const Message& msg);
	void Slot_FuncSync(const Message& msg);
	void Slot_AddFunc(const Message& msg);
	void Slot_DelFunc(const Message& msg);
	void Slot_AltFunc(const Message& msg);
	void Slot_AddVar(const Message& msg);
	void Slot_DelVar(const Message& msg);
	void Slot_AltVar(const Message& msg);
	void Slot_SelectFunc(const Message& msg);
	void Slot_SelectVar(const Message& msg);
	void Slot_SelectNode(const Message& msg);
	void Slot_AddNode(const Message& msg);
	void Slot_DelNode(const Message& msg);
	void Slot_AltNode(const Message& msg);
signals:
	void Sig_AddBP(const Message& msg);
	void Sig_DelBP(const Message& msg);
	void Sig_AltBP(const Message& msg);
	void Sig_OpenBP(const Message& msg);
	void Sig_CloseBP(const Message& msg);
	void Sig_TypeSync(const Message& msg);
	void Sig_FuncSync(const Message& msg);
	void Sig_AddFunc(const Message& msg);
	void Sig_DelFunc(const Message& msg);
	void Sig_AltFunc(const Message& msg);
	void Sig_AddVar(const Message& msg);
	void Sig_DelVar(const Message& msg);
	void Sig_AltVar(const Message& msg);
	void Sig_SelectFunc(const Message& msg);
	void Sig_SelectVar(const Message& msg);
	void Sig_SelectNode(const Message& msg);
	void Sig_AddNode(const Message& msg);
	void Sig_DelNode(const Message& msg);
	void Sig_AltNode(const Message& msg);

private:
	template <typename Signal, typename Slot>
	void RegisterAction(Action act, Signal sig, Slot slot) {
		connect(this, sig, this, slot);
		emiters_.insert({ act, [this, sig](const Message& msg) {(this->*sig)(msg);}});
	}

	void BuildUi();
	void InitAction();

	void AppendLog(const QString& text);
	void ClearLog();
	
	Emiter FindEmiter(int act) const;

	void SetEditorVisible(bool visible);
	void ResetEditor();
	void OpenGraphPage(const QString& guid, const QString& name);
	void CloseGraphPage(const QString& guid);
	void RenameGraphPage(const QString& guid, const QString& name);
	void ClearGraphPages();
	int FindGraphTab(const QString& guid) const;
	class GraphView* FindGraphView(const QString& guid) const;
	void PutGraphNode(const QString& flow, const QString& guid, const QString& name, const QString& key, const QString& group, double x, double y, const QJsonArray& pins);
	void ForgetGraphNode(const QString& flow, const QString& guid);
	void MoveGraphNode(const QString& flow, const QString& guid, double x, double y);
	bool FlowHasNode(const QString& flow, const QString& key) const;
	void RestoreGraphNodes(class GraphView* view);
	void ShowDetails(const QJsonObject& detail);
	void AppendDetails(const QJsonArray& fields, const QString& prefix);
	void ClearDetails();
	void SetDetailValue(const QString& key, const QJsonValue& value);
	bool eventFilter(QObject* watched, QEvent* event) override;

	QStackedWidget* graph_stack_{};
	QFrame* graph_tab_frame_{};
	QTabBar* graph_tabs_{};
	QLabel* empty_hint_{};
	QTreeWidget* asset_tree_{};
	QTreeWidget* func_tree_{};
	QTreeWidget* var_tree_{};
	QComboBox* var_type_combo_{};
	QLineEdit* bp_name_edit_{};
	QTreeWidget* palette_tree_{};
	
	struct DetailField {
		QWidget* widget{};
		std::function<void(QWidget*, const QJsonValue&)> set;
	};

	QWidget* details_host_{};
	QFormLayout* details_form_{};
	QString detail_kind_;
	QString detail_guid_;
	std::unordered_map<std::string, DetailField> detail_fields_{};
	QTextEdit* log_{};
	QLabel* breadcrumb_{};
	QDockWidget* bp_dock_{};
	QDockWidget* detail_dock_{};
	QDockWidget* palette_dock_{};
	QToolButton* compile_btn_{};
	QToolButton* play_btn_{};

	QString selected_var_guid_;

	std::function<void(const Message&)> sink_;
	TBus<TEvent<Action, Message>> handlers_;
	QString current_asset_guid_;
	QJsonArray system_funcs_;

	struct GraphNodeState {
		QString guid;
		QString name;
		QString key;
		QString group;
		double x{};
		double y{};
		QJsonArray pins;
	};
	std::unordered_map<std::string, std::vector<GraphNodeState>> flow_nodes_;
	
	std::unordered_map<int, Emiter> emiters_;
};
