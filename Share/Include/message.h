#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace cg {
enum class Action : int {
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
	AddLine,
	DelLine,

	Compile,
	TypeSync,
	FuncSync,
	RuntimeSync,
	LogOutput,
};
}

// 请求/响应信封。Client、Server、Service 只交换这种报文，不共享业务对象。
// act: 动作码（由 Service 解释）；token: 一次调用的关联 ID；data: 载荷。
struct Message {
  cg::Action act{cg::Action::AddBP};
  int code{};
  std::string token{};
  std::string data{};
};

enum ErrorCode : uint8_t
{
  EC_SUCCESS
};

inline constexpr const char* g_key_name = "name";
inline constexpr const char* g_key_type = "type";
inline constexpr const char* g_key_guid = "guid";
inline constexpr const char* g_key_space = "space";
inline constexpr const char* g_key_path = "path";
inline constexpr const char* g_key_asset = "asset";
inline constexpr const char* g_key_error = "error";
inline constexpr const char* g_key_fields = "fields";
inline constexpr const char* g_key_target = "target";
inline constexpr const char* g_key_kind = "kind";
inline constexpr const char* g_key_show = "show";
inline constexpr const char* g_key_key = "key";
inline constexpr const char* g_key_text = "text";
inline constexpr const char* g_key_value = "value";
inline constexpr const char* g_key_legacy_pin_default_value = "default_value";
inline constexpr const char* g_key_pin = "pin";
inline constexpr const char* g_key_param_action = "param_action";
inline constexpr const char* g_key_items = "items";
inline constexpr const char* g_key_columns = "columns";
inline constexpr const char* g_key_flows = "flows";
inline constexpr const char* g_key_nodes = "nodes";
inline constexpr const char* g_key_lines = "lines";
inline constexpr const char* g_key_flow = "flow";
inline constexpr const char* g_key_node = "node";
inline constexpr const char* g_key_vars = "vars";
inline constexpr const char* g_key_types = "types";
inline constexpr const char* g_key_funcs = "funcs";
inline constexpr const char* g_key_methods = "methods";
inline constexpr const char* g_key_input_types = "input_types";
inline constexpr const char* g_key_output_types = "output_types";
inline constexpr const char* g_key_group = "group";
inline constexpr const char* g_key_dll = "dll";
inline constexpr const char* g_key_body = "body";
inline constexpr const char* g_key_direct = "direct";
inline constexpr const char* g_key_pins = "pins";
inline constexpr const char* g_key_x = "x";
inline constexpr const char* g_key_y = "y";
inline constexpr const char* g_key_status = "status";
inline constexpr const char* g_key_flow_status = "flow_status";
inline constexpr const char* g_key_node_status = "node_status";
inline constexpr const char* g_key_connected = "connected";
inline constexpr const char* g_key_from_node = "from_node";
inline constexpr const char* g_key_from_pin = "from_pin";
inline constexpr const char* g_key_to_node = "to_node";
inline constexpr const char* g_key_to_pin = "to_pin";
inline constexpr const char* g_key_removed_lines = "removed_lines";
inline constexpr const char* g_key_removed_nodes = "removed_nodes";
inline constexpr const char* g_key_editable = "editable";
inline constexpr const char* g_key_phase = "phase";
inline constexpr const char* g_key_level = "level";
inline constexpr const char* g_key_source = "source";

inline constexpr const char* g_show_label = "label";
inline constexpr const char* g_show_line = "line";
inline constexpr const char* g_show_combo = "combo";
inline constexpr const char* g_show_check = "check";
inline constexpr const char* g_show_list = "list";

inline constexpr const char* g_kind_blueprint = "blueprint";
inline constexpr const char* g_kind_flow = "flow";
inline constexpr const char* g_kind_var = "var";
inline constexpr const char* g_kind_node = "node";

inline constexpr const char* g_group_function = "Function";
inline constexpr const char* g_group_script = "Script";
inline constexpr const char* g_group_controller = "Controller";
inline constexpr const char* g_group_operator = "Operator";
inline constexpr const char* g_group_user = "User";

inline constexpr const char* g_node_entry = "Entry";
inline constexpr const char* g_node_exit = "Exit";
inline constexpr const char* g_node_get_var = "GetVar";
inline constexpr const char* g_node_set_var = "SetVar";
inline constexpr const char* g_node_class_method = "ClassMethod";
