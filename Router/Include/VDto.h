#pragma once

#include "Export.h"

#include <cstdint>
#include <string>
#include <vector>

namespace CG::Router {

	enum class DtoPinKind : int32_t { Exec = 0, Data = 1 };
	enum class DtoPinDir : int32_t { In = 0, Out = 1 };
	enum class DtoEditKind : int32_t {
		None = 0, Bool, Int, Int64, Float, Double, String
	};

	struct ROUTER_API PinDto {
		std::string id;
		std::string name;
		DtoPinKind kind{ DtoPinKind::Data };
		DtoPinDir direction{ DtoPinDir::In };
		std::string type_name;
		bool connected{ false };
		bool editable{ false };
		DtoEditKind edit_kind{ DtoEditKind::None };
		std::string value_text;
	};

	struct ROUTER_API NodeDto {
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
		std::vector<PinDto> pins;
	};

	struct ROUTER_API LineDto {
		std::string id;
		std::string from_pin_id;
		std::string to_pin_id;
		bool is_exec{ false };
	};

	struct ROUTER_API GraphDto {
		std::string flow_guid;
		std::string entry_node_id;
		std::string exit_node_id;
		bool has_exit{ false };
		std::vector<NodeDto> nodes;
		std::vector<LineDto> lines;
	};

	struct ROUTER_API FlowItemDto {
		std::string guid;
		std::string name;
	};

	struct ROUTER_API VarItemDto {
		std::string guid;
		std::string name;
		std::string type_name;
	};

	struct ROUTER_API CallerItemDto {
		std::string key;       // palette key: caller name | flow:guid | method:cls/m | exit
		std::string name;
		std::string group;     // Flow Control / Utilities / ...
		int kind{ 0 };
		bool is_pure{ false };
	};

	struct ROUTER_API TypeItemDto {
		std::string name;
	};

	struct ROUTER_API BlueprintDto {
		std::string guid;
		std::string space;
		std::string name;
		std::string current_flow_guid;
		std::string current_flow_name;
		std::string self_type_name;
		std::vector<FlowItemDto> flows;
		std::vector<VarItemDto> vars;
		std::vector<TypeItemDto> types;
	};

	struct ROUTER_API ParamDto {
		std::string name;
		std::string type_name;
	};

	struct ROUTER_API NodeDetailsDto {
		std::string node_guid;
		std::string title;
		std::string kind_label;
		std::string caller_name;
		bool is_entry{ false };
		bool is_exit{ false };
		bool can_edit_params{ false };
		std::vector<ParamDto> params;
		std::vector<PinDto> pins;
		std::vector<std::string> type_names;
	};

	struct ROUTER_API VarDetailsDto {
		std::string guid;
		std::string name;
		std::string type_name;
		std::string value_text;
		DtoEditKind edit_kind{ DtoEditKind::None };
		std::vector<std::string> type_names;
	};

	struct ROUTER_API PaletteDto {
		std::vector<CallerItemDto> items;
	};

	struct ROUTER_API AssetItemDto {
		std::string path;   // absolute or host-relative asset path
		std::string space;
		std::string name;
		std::string key;    // space::name
	};

	struct ROUTER_API AssetListDto {
		std::string asset_root;
		std::vector<AssetItemDto> items;
	};

} // namespace sys
