#pragma once
#pragma execution_character_set("utf-8")

#include "Export.h"
#include "IRouter.h"
#include "VDto.h"
#include <functional>

#include <QJsonObject>
#include <QMainWindow>
#include <QPoint>
#include <QPointF>
#include <QString>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

class GraphView;
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

using namespace CG::Router;

class MACRO_UI_API MainWindow : public QMainWindow {
	Q_OBJECT
public:
	explicit MainWindow(QWidget* parent = nullptr);
	~MainWindow() override;
	void Notify(const Message& msg);
	void SetSink(std::function<void(const Message&)> sink);
private:
	void Dispatch(Action action, const QJsonObject& body = {});
	
private slots:
	void OnNewBlueprint();
	void OnAddFlow();
	void OnAddVar();
	void OnAddExit();
	void OnRunFlow();
	void OnGenerateCode();
	void OnFlowClicked(QTreeWidgetItem* item, int column);
	void OnVarClicked(QTreeWidgetItem* item, int column);
	void OnPaletteDoubleClicked(QTreeWidgetItem* item, int column);
	void OnGraphTabChanged(int index);
	void OnNodeSelected(const QString& guid);
	void OnLog(const QString& text);
	void OnFunctionMenu(const QPoint& pos);
	void OnVariableMenu(const QPoint& pos);
	void OnFunctionRenamed(QTreeWidgetItem* item, int column);
	void OnVariableRenamed(QTreeWidgetItem* item, int column);
	void OnDeleteFunction();
	void OnDeleteVariable();
	void OnVariableDropped(const QString& guid, const QPointF& scene_pos, Qt::KeyboardModifiers mods);
	void OnFunctionDropped(const QString& guid, const QPointF& scene_pos);
	void OnBlueprintRenamed();
	void OnVarTypeChosen(int index);
	void OnNodeContextMenu(const QString& node_guid, const QPoint& global_pos);
	void OnAssetDoubleClicked(QTreeWidgetItem* item, int column);

private:
	void BuildUi();
	void AppendLog(const QString& text);
	void ClearLog();
	void ApplyBlueprintDto(const BlueprintDto& dto);
	void ApplyPaletteDto(const PaletteDto& dto);
	void ApplyGraphDto(const GraphDto& dto);
	void ApplyAssetListDto(const AssetListDto& dto);
	void ApplyRefreshDetails(const QString& data);
	void ApplyResponse(Action act, const QString& data);
	void FillBlueprintDetails(const QString& json);
	void FillNodeDetails(const NodeDetailsDto& dto);
	void FillVarDetails(const VarDetailsDto& dto);
	void RefreshTypeCombo();
	void UpdateBreadcrumb();
	void RefreshGraphTabs();
	void SetEditorVisible(bool visible);
	void EditNewItem(QTreeWidget* tree, const QString& guid);
	void PromptAddNodeParam(const QString& node_guid, bool is_entry);
	void ClearDetailsForm();
	void AddDetailRow(const QString& key, const QString& value);
	bool eventFilter(QObject* watched, QEvent* event) override;

	GraphView* graph_view_{};
	QFrame* graph_tab_frame_{};
	QTabBar* graph_tabs_{};
	QLabel* empty_hint_{};
	QTreeWidget* function_tree_{};
	QTreeWidget* var_tree_{};
	QComboBox* var_type_combo_{};
	QLineEdit* bp_name_edit_{};
	QTreeWidget* palette_tree_{};
	QTreeWidget* asset_tree_{};
	QWidget* details_host_{};
	QFormLayout* details_form_{};
	QTextEdit* log_{};
	QLabel* breadcrumb_{};
	QDockWidget* bp_dock_{};
	QDockWidget* detail_dock_{};
	QDockWidget* palette_dock_{};
	QToolButton* compile_btn_{};
	QToolButton* play_btn_{};

	BlueprintDto blueprint_dto_;
	std::optional<GraphDto> graph_dto_;
	std::optional<NodeDetailsDto> last_node_details_;
	QString selected_var_guid_;
	bool has_blueprint_{ false };
	bool renaming_{ false };
	bool details_busy_{ false };
	bool pending_edit_flow_{ false };
	bool pending_edit_var_{ false };

	std::function<void(const Message&)> sink_;
};
