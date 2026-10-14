#pragma once

#include "Export.h"

#include <cstdint>
#include <string>
#include <vector>

namespace CG::Router {

	enum class PinKind : int32_t { Exec = 0, Data = 1 };
	enum class PinDir : int32_t { In = 0, Out = 1 };
	enum class EditKind : int32_t {
		None = 0, Bool, Int, Int64, Float, Double, String
	};

	struct ROUTER_API Pin {
		std::string id;
		std::string name;
		PinKind kind{ PinKind::Data };
		PinDir direction{ PinDir::In };
		std::string type_name;
		bool connected{ false };
		bool editable{ false };
		EditKind edit_kind{ EditKind::None };
		std::string value_text;
	};

	struct ROUTER_API Node {
		std::string guid;
		std::string name;
		std::string title;
		std::string caller_name;
		int caller_kind{ 0 };
		bool is_pure{ false };
		bool is_entry{ false };
		bool is_exit{ false };
		float x{ 0.f };
		float y{ 0.f };
		std::vector<Pin> pins;
	};

	struct ROUTER_API Line {
		std::string id;
		std::string from_pin_id;
		std::string to_pin_id;
		bool is_exec{ false };
	};

	struct ROUTER_API Graph {
		std::string flow_guid;
		std::string entry_node_id;
		std::string exit_node_id;
		bool has_exit{ false };
		std::vector<Node> nodes;
		std::vector<Line> lines;
	};

	struct ROUTER_API FlowItem {
		std::string guid;
		std::string name;
	};

	struct ROUTER_API VarItem {
		std::string guid;
		std::string name;
		std::string type_name;
	};

	struct ROUTER_API CallerItem {
		std::string key;       // palette key: caller name | flow:guid | method:cls/m | exit
		std::string name;
		std::string group;     // Flow Control / Utilities / ...
		int kind{ 0 };
		bool is_pure{ false };
	};

	struct ROUTER_API TypeItem {
		std::string name;
	};

	struct ROUTER_API Blueprint {
		std::string guid;
		std::string space;
		std::string name;
		std::string current_flow_guid;
		std::string current_flow_name;
		std::string self_type_name;
		std::vector<FlowItem> flows;
		std::vector<VarItem> vars;
		std::vector<TypeItem> types;
	};

	struct ROUTER_API Param {
		std::string name;
		std::string type_name;
	};

	struct ROUTER_API NodeDetails {
		std::string node_guid;
		std::string title;
		std::string kind_label;
		std::string caller_name;
		bool is_entry{ false };
		bool is_exit{ false };
		bool can_edit_params{ false };
		std::vector<Param> params;
		std::vector<Pin> pins;
		std::vector<std::string> type_names;
	};

	struct ROUTER_API VarDetails {
		std::string guid;
		std::string name;
		std::string type_name;
		std::string value_text;
		EditKind edit_kind{ EditKind::None };
		std::vector<std::string> type_names;
	};

	struct ROUTER_API Palette {
		std::vector<CallerItem> items;
	};

	struct ROUTER_API AssetItem {
		std::string path{};
		std::string space{};
		std::string name{};
		std::string key{};
	};

	struct ROUTER_API AssetList {
		std::string asset_root;
		std::vector<AssetItem> items;
	};

} // namespace sys
