#include "VDtoJson.h"

#include <nlohmann/json.hpp>

namespace CG::Router {
namespace {

	PinDto ParsePin(const nlohmann::json& j)
	{
		PinDto p;
		p.id = j.value("id", "");
		p.name = j.value("name", "");
		p.kind = static_cast<DtoPinKind>(j.value("kind", 1));
		p.direction = static_cast<DtoPinDir>(j.value("direction", 0));
		p.type_name = j.value("type_name", "");
		p.connected = j.value("connected", false);
		p.editable = j.value("editable", false);
		p.edit_kind = static_cast<DtoEditKind>(j.value("edit_kind", 0));
		p.value_text = j.value("value_text", "");
		return p;
	}

	NodeDto ParseNode(const nlohmann::json& j)
	{
		NodeDto n;
		n.guid = j.value("guid", "");
		n.name = j.value("name", "");
		n.title = j.value("title", "");
		n.caller_name = j.value("caller_name", "");
		n.caller_kind = j.value("caller_kind", 0);
		n.is_pure = j.value("is_pure", false);
		n.is_entry = j.value("is_entry", false);
		n.is_exit = j.value("is_exit", false);
		n.x = j.value("x", 0.f);
		n.y = j.value("y", 0.f);
		if (j.contains("pins") && j["pins"].is_array()) {
			for (const auto& pj : j["pins"])
				n.pins.push_back(ParsePin(pj));
		}
		return n;
	}

	LineDto ParseLine(const nlohmann::json& j)
	{
		LineDto l;
		l.id = j.value("id", "");
		l.from_pin_id = j.value("from_pin_id", "");
		l.to_pin_id = j.value("to_pin_id", "");
		l.is_exec = j.value("is_exec", false);
		return l;
	}

} // namespace

bool ParseGraphDto(const std::string& json, GraphDto& out)
{
	try {
		const auto j = nlohmann::json::parse(json);
		out = {};
		out.flow_guid = j.value("flow_guid", "");
		out.entry_node_id = j.value("entry_node_id", "");
		out.exit_node_id = j.value("exit_node_id", "");
		out.has_exit = j.value("has_exit", false);
		if (j.contains("nodes") && j["nodes"].is_array()) {
			for (const auto& nj : j["nodes"])
				out.nodes.push_back(ParseNode(nj));
		}
		if (j.contains("lines") && j["lines"].is_array()) {
			for (const auto& lj : j["lines"])
				out.lines.push_back(ParseLine(lj));
		}
		return true;
	} catch (...) {
		return false;
	}
}

bool ParseBlueprintDto(const std::string& json, BlueprintDto& out)
{
	try {
		const auto j = nlohmann::json::parse(json);
		out = {};
		out.guid = j.value("guid", "");
		out.space = j.value("space", "");
		out.name = j.value("name", "");
		out.current_flow_guid = j.value("current_flow_guid", "");
		out.current_flow_name = j.value("current_flow_name", "");
		out.self_type_name = j.value("self_type_name", "");
		if (j.contains("flows") && j["flows"].is_array()) {
			for (const auto& f : j["flows"])
				out.flows.push_back({ f.value("guid", ""), f.value("name", "") });
		}
		if (j.contains("vars") && j["vars"].is_array()) {
			for (const auto& v : j["vars"])
				out.vars.push_back({ v.value("guid", ""), v.value("name", ""), v.value("type_name", "") });
		}
		if (j.contains("types") && j["types"].is_array()) {
			for (const auto& t : j["types"])
				out.types.push_back({ t.value("name", "") });
		}
		return true;
	} catch (...) {
		return false;
	}
}

bool ParsePaletteDto(const std::string& json, PaletteDto& out)
{
	try {
		const auto j = nlohmann::json::parse(json);
		out = {};
		if (j.contains("items") && j["items"].is_array()) {
			for (const auto& c : j["items"]) {
				CallerItemDto item;
				item.key = c.value("key", "");
				item.name = c.value("name", "");
				item.group = c.value("group", "");
				item.kind = c.value("kind", 0);
				item.is_pure = c.value("is_pure", false);
				out.items.push_back(std::move(item));
			}
		}
		return true;
	} catch (...) {
		return false;
	}
}

bool ParseNodeDetailsDto(const std::string& json, NodeDetailsDto& out)
{
	try {
		const auto j = nlohmann::json::parse(json);
		out = {};
		out.node_guid = j.value("node_guid", "");
		out.title = j.value("title", "");
		out.kind_label = j.value("kind_label", "");
		out.caller_name = j.value("caller_name", "");
		out.is_entry = j.value("is_entry", false);
		out.is_exit = j.value("is_exit", false);
		out.can_edit_params = j.value("can_edit_params", false);
		if (j.contains("params") && j["params"].is_array()) {
			for (const auto& p : j["params"])
				out.params.push_back({ p.value("name", ""), p.value("type_name", "") });
		}
		if (j.contains("pins") && j["pins"].is_array()) {
			for (const auto& pj : j["pins"])
				out.pins.push_back(ParsePin(pj));
		}
		if (j.contains("type_names") && j["type_names"].is_array()) {
			for (const auto& t : j["type_names"])
				out.type_names.push_back(t.get<std::string>());
		}
		return true;
	} catch (...) {
		return false;
	}
}

bool ParseVarDetailsDto(const std::string& json, VarDetailsDto& out)
{
	try {
		const auto j = nlohmann::json::parse(json);
		out = {};
		out.guid = j.value("guid", "");
		out.name = j.value("name", "");
		out.type_name = j.value("type_name", "");
		out.value_text = j.value("value_text", "");
		out.edit_kind = static_cast<DtoEditKind>(j.value("edit_kind", 0));
		if (j.contains("type_names") && j["type_names"].is_array()) {
			for (const auto& t : j["type_names"])
				out.type_names.push_back(t.get<std::string>());
		}
		return true;
	} catch (...) {
		return false;
	}
}

bool ParseAssetListDto(const std::string& json, AssetListDto& out)
{
	try {
		const auto j = nlohmann::json::parse(json);
		out = {};
		out.asset_root = j.value("asset_root", "");
		if (j.contains("items") && j["items"].is_array()) {
			for (const auto& it : j["items"]) {
				AssetItemDto item;
				item.path = it.value("path", "");
				item.space = it.value("space", "");
				item.name = it.value("name", "");
				item.key = it.value("key", "");
				if (item.key.empty() && !item.space.empty() && !item.name.empty())
					item.key = item.space + "::" + item.name;
				out.items.push_back(std::move(item));
			}
		}
		return true;
	} catch (...) {
		return false;
	}
}

} // namespace sys
