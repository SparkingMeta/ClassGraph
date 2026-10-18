#include "Meta.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <objbase.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <mutex>
#include <queue>
#include <unordered_map>
#include <utility>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace cg::model {

namespace {

// 与 MainWindow::Action 同值，命令不在这里再定义一套。
std::shared_ptr<FlowMeta> FindFlow(AssetMeta& asset, const std::string& guid)
{
	if (auto it = asset.flows.find(guid); it != asset.flows.end())
		return it->second;
	return {};
}

std::shared_ptr<VarMeta> FindVar(AssetMeta& asset, const std::string& guid)
{
	if(auto it = asset.vars.find(guid); it != asset.vars.end())
		return it->second;
	return {};
}

std::shared_ptr<NodeMeta> FindNode(AssetMeta& asset, const std::string& guid)
{
	for (auto& [id, flow] : asset.flows) {
		(void)id;
		if (!flow) continue;
		if (auto it = flow->nodes.find(guid); it != flow->nodes.end())
			return it->second;
	}
	return {};
}

json Field(std::string key, std::string text, std::string show, json value)
{
	const bool editable = show == g_show_line || show == g_show_combo || show == g_show_check;
	return {
		{g_key_key, std::move(key)},
		{g_key_text, std::move(text)},
		{g_key_show, std::move(show)},
		{g_key_value, std::move(value)},
		{g_key_editable, editable},
	};
}

json BlueprintFields(const AssetMeta& asset)
{
	return json::array({
		Field(g_key_name, "名称", g_show_line, asset.name),
		Field(g_key_space, "命名空间", g_show_label, asset.space),
		Field(g_key_path, "路径", g_show_label, asset.path),
		Field(g_key_guid, "Guid", g_show_label, asset.guid),
	});
}

const char* FlowStatusName(FlowStatus status)
{
	switch (status) {
	case FlowStatus::Running: return "Running";
	case FlowStatus::Completed: return "Completed";
	case FlowStatus::Failed: return "Failed";
	default: return "Idle";
	}
}

FlowStatus ParseFlowStatus(const std::string& status, int code)
{
	if (status == "Running") return FlowStatus::Running;
	if (status == "Completed") return FlowStatus::Completed;
	if (status == "Failed") return FlowStatus::Failed;
	return code == ErrorCode::EC_SUCCESS ? FlowStatus::Completed : FlowStatus::Failed;
}

json FlowFields(const FlowMeta& flow)
{
	return json::array({
		Field(g_key_name, "名称", g_show_line, flow.name),
		Field(g_key_flow_status, "运行状态", g_show_label, FlowStatusName(flow.flow_status)),
		Field(g_key_guid, "Guid", g_show_label, flow.guid),
	});
}

json VarFields(const VarMeta& var)
{
	return json::array({
		Field(g_key_name, "名称", g_show_line, var.name),
		Field(g_key_type, "类型", g_show_label, var.type),
		Field(g_key_value, "值", g_show_label, var.value),
		Field(g_key_guid, "Guid", g_show_label, var.guid),
	});
}

const char* StatusName(NodeStatus status)
{
	switch (status) {
	case NS_ERROR: return "Error";
	case NS_WARN: return "Warn";
	case NS_OK: return "Ok";
	default: return "Silent";
	}
}

std::string DirectName(int direct)
{
	if (direct == 0) return "In";
	if (direct == 1) return "Out";
	return std::to_string(direct);
}

json NodeFields(const NodeMeta& node, const FlowMeta& flow)
{
	std::vector<std::shared_ptr<PinMeta>> pins;
	for (const auto& [id, pin] : node.pins) {
		(void)id;
		if (pin) pins.push_back(pin);
	}
	std::sort(pins.begin(), pins.end(), [](const std::shared_ptr<PinMeta>& a, const std::shared_ptr<PinMeta>& b) {
		return a->name < b->name;
	});

	json groups = json::array();
	for (const auto& pin : pins) {
		if (pin->kind != 1)
			continue;
		const bool connected = std::any_of(flow.lines.begin(), flow.lines.end(), [&](const auto& entry) {
			const auto& line = entry.second;
			return line && ((pin->direct == 0 && line->to_pin == pin->guid)
				|| (pin->direct == 1 && line->from_pin == pin->guid));
		});
		const bool editable_value = pin->direct == 0 && !connected && !pin->type.empty();
		const std::string editor = editable_value
			? (pin->type == "bool" ? g_show_check : g_show_line) : g_show_label;
		json details = json::array({
			Field(g_key_name, "名称", g_show_label, pin->name),
			Field(g_key_type, "类型", g_show_label, pin->type),
			Field(g_key_value, "值", editor, pin->value),
		});
		groups.push_back({
			{g_key_key, pin->guid},
			{g_key_text, pin->name},
			{g_key_fields, std::move(details)},
		});
	}

	json fields = json::array({
		Field(g_key_name, "名称", g_show_line, node.name),
		Field(g_key_x, "X", g_show_label, node.x),
		Field(g_key_y, "Y", g_show_label, node.y),
		Field(g_key_status, "状态", g_show_label, StatusName(node.status)),
		Field(g_key_guid, "Guid", g_show_label, node.guid),
	});
	json pins_field = Field(g_key_pins, "参数", g_show_list, std::move(groups));
	pins_field[g_key_editable] = node.name == g_node_entry || node.name == g_node_exit;
	fields.push_back(std::move(pins_field));
	fields[0][g_key_editable] = false;
	return fields;
}

void RiseMiss(const std::function<void(const Message&)>& rise, const Message& msg, const std::string& reason, const std::string& guid)
{
	if (!rise) return;
	Message out = msg;
	out.code = 1;
	out.data = json{{g_key_error, reason}, {g_key_guid, guid}}.dump();
	rise(out);
}

void RiseDetail(const std::function<void(const Message&)>& rise, const Message& msg, const char* kind, const std::string& guid, json fields)
{
	if (!rise) return;
	Message out = msg;
	out.code = 0;
	out.data = json{{g_key_target, {{g_key_kind, kind}, {g_key_guid, guid}}}, {g_key_fields, std::move(fields)}}.dump();
	rise(out);
}

} // namespace

struct Linker {
	std::function<void(const Message&)> request_handler;
	std::function<void(const Message&)> response_handler;
};

class Model::Impl {
public:
	void Init();
	void Submit(const Message& msg);
	void Reply(const Message& msg);
	void SetRise(std::function<void(const Message&)> rise);

private:
	void Apply(const Message& msg);

	void AddBP(const Message& msg);
	void OnAddBP(const Message& msg);

	void DelBP(const Message& msg);
	void OnDelBP(const Message& msg);
	void AltBP(const Message& msg);
	void OnAltBP(const Message& msg);
	void OpenBP(const Message& msg);
	void OnOpenBP(const Message& msg);
	void CloseBP(const Message& msg);
	void OnCloseBP(const Message& msg);
	void TypeSync(const Message& msg);
	void OnTypeSync(const Message& msg);
	json TypeList(const std::string& skip) const;
	void FuncSync(const Message& msg);
	void OnFuncSync(const Message& msg);
	json FuncList() const;
	void SyncVarFuncs();
	void EmitFuncSync();
	void AddFunc(const Message& msg);
	void OnAddFunc(const Message& msg);
	void DelFunc(const Message& msg);
	void OnDelFunc(const Message& msg);
	void AltFunc(const Message& msg);
	void OnAltFunc(const Message& msg);
	void RunFunc(const Message& msg);
	void OnRunFunc(const Message& msg);
	void RuntimeSync(const Message& msg);
	void SelectFunc(const Message& msg);
	void DelVar(const Message& msg);
	void OnDelVar(const Message& msg);
	void AddVar(const Message& msg);
	void OnAddVar(const Message& msg);
	void AltVar(const Message& msg);
	void OnAltVar(const Message& msg);
	void SelectVar(const Message& msg);
	void AddNode(const Message& msg);
	void OnAddNode(const Message& msg);
	void DelNode(const Message& msg);
	void OnDelNode(const Message& msg);
	void AltNode(const Message& msg);
	void OnAltNode(const Message& msg);
	void SelectNode(const Message& msg);
	void AddLine(const Message& msg);
	void OnAddLine(const Message& msg);
	void DelLine(const Message& msg);
	void OnDelLine(const Message& msg);
	void Compile(const Message& msg);
	void Reject(const Message& msg);
	
	std::shared_ptr<AssetMeta> FindAsset(const std::string& guid);

private:
	std::mutex mutex_;
	std::function<void(const Message&)> rise_;
	std::string opened_;
	std::unordered_map<std::string, std::shared_ptr<AssetMeta>> assets_;
	std::unordered_map<std::string, std::shared_ptr<TypeMeta>> types_;
	std::unordered_map<std::string, std::shared_ptr<FuncMeta>> funcs_;
	std::unordered_map<Action, Linker> linkers_;
};

Model::Model() : impl_(std::make_unique<Impl>()) {
	impl_->Init();
}
Model::~Model() = default;

void Model::Submit(const Message& msg) { impl_->Submit(msg); }
void Model::Reply(const Message& msg){ impl_->Reply(msg); }
void Model::SetRise(std::function<void(const Message&)> rise) { impl_->SetRise(std::move(rise)); }


void Model::Impl::Init() {
	linkers_.insert({ Action::AddBP, Linker{
		[this](const Message& msg) { AddBP(msg); },
		[this](const Message& msg) { OnAddBP(msg); },
	} });
	linkers_.insert({ Action::DelBP, Linker{
		[this](const Message& msg) { DelBP(msg); },
		[this](const Message& msg) { OnDelBP(msg); },
	} });
	linkers_.insert({ Action::AltBP, Linker{
		[this](const Message& msg) { AltBP(msg); },
		[this](const Message& msg) { OnAltBP(msg); },
	} });
	linkers_.insert({ Action::OpenBP, Linker{
		[this](const Message& msg) { OpenBP(msg); },
		[this](const Message& msg) { OnOpenBP(msg); },
	} });
	linkers_.insert({ Action::CloseBP, Linker{
		[this](const Message& msg) { CloseBP(msg); },
		[this](const Message& msg) { OnCloseBP(msg); },
	} });
	linkers_.insert({ Action::TypeSync, Linker{
		[this](const Message& msg) { TypeSync(msg); },
		[this](const Message& msg) { OnTypeSync(msg); },
	} });
	linkers_.insert({ Action::FuncSync, Linker{
		[this](const Message& msg) { FuncSync(msg); },
		[this](const Message& msg) { OnFuncSync(msg); },
	} });
	linkers_.insert({ Action::SelectFunc, Linker{
		[this](const Message& msg) { SelectFunc(msg); },
		[](const Message&) {},
	} });
	linkers_.insert({ Action::SelectVar, Linker{
		[this](const Message& msg) { SelectVar(msg); },
		[](const Message&) {},
	} });
	linkers_.insert({ Action::SelectNode, Linker{
		[this](const Message& msg) { SelectNode(msg); },
		[](const Message&) {},
	} });
	linkers_.insert({ Action::AddFunc, Linker{
		[this](const Message& msg) { AddFunc(msg); },
		[this](const Message& msg) { OnAddFunc(msg); },
	} });
	linkers_.insert({ Action::DelFunc, Linker{
		[this](const Message& msg) { DelFunc(msg); },
		[this](const Message& msg) { OnDelFunc(msg); },
	} });
	linkers_.insert({ Action::AltFunc, Linker{
		[this](const Message& msg) { AltFunc(msg); },
		[this](const Message& msg) { OnAltFunc(msg); },
	} });
	linkers_.insert({ Action::AddNode, Linker{
		[this](const Message& msg) { AddNode(msg); },
		[this](const Message& msg) { OnAddNode(msg); },
	} });
	linkers_.insert({ Action::DelNode, Linker{
		[this](const Message& msg) { DelNode(msg); },
		[this](const Message& msg) { OnDelNode(msg); },
	} });
	linkers_.insert({ Action::AltNode, Linker{
		[this](const Message& msg) { AltNode(msg); },
		[this](const Message& msg) { OnAltNode(msg); },
	} });
	linkers_.insert({ Action::AddLine, Linker{
		[this](const Message& msg) { AddLine(msg); },
		[this](const Message& msg) { OnAddLine(msg); },
	} });
	linkers_.insert({ Action::DelLine, Linker{
		[this](const Message& msg) { DelLine(msg); },
		[this](const Message& msg) { OnDelLine(msg); },
	} });
	linkers_.insert({ Action::AddVar, Linker{
		[this](const Message& msg) { AddVar(msg); },
		[this](const Message& msg) { OnAddVar(msg); },
	} });
	linkers_.insert({ Action::DelVar, Linker{
		[this](const Message& msg) { DelVar(msg); },
		[this](const Message& msg) { OnDelVar(msg); },
	} });
	linkers_.insert({ Action::AltVar, Linker{
		[this](const Message& msg) { AltVar(msg); },
		[this](const Message& msg) { OnAltVar(msg); },
	} });
	linkers_.insert({ Action::Compile, Linker{
		[this](const Message& msg) { Compile(msg); },
		[this](const Message& msg) { if (rise_) rise_(msg); },
	} });
	linkers_.insert({ Action::RunFunc, Linker{
		[this](const Message& msg) { RunFunc(msg); },
		[this](const Message& msg) { OnRunFunc(msg); },
	} });
	linkers_.insert({ Action::RuntimeSync, Linker{
		[this](const Message& msg) { RuntimeSync(msg); },
		[this](const Message& msg) { RuntimeSync(msg); },
	} });
	linkers_.insert({ Action::LogOutput, Linker{
		[](const Message&) {},
		[this](const Message& msg) { if (rise_) rise_(msg); },
	} });
}

void Model::Impl::Submit(const Message& msg)
{
	std::lock_guard lock(mutex_);
	Apply(msg);
}

void Model::Impl::Apply(const Message& msg) {
	std::function<void(const Message&)> handler;
	if (auto it = linkers_.find(msg.act); it != linkers_.end())
		handler = it->second.request_handler;

	if (handler) handler(msg);
}

void Model::Impl::SetRise(std::function<void(const Message&)> fn)
{
	std::lock_guard lock(mutex_);
	rise_ = std::move(fn);
}

void Model::Impl::Reply(const Message& msg)
{
	std::lock_guard lock(mutex_);
	std::function<void(const Message&)> handler;
	if(auto it = linkers_.find(msg.act); it != linkers_.end())
		handler = it->second.response_handler;

	if(handler) handler(msg);
}

void Model::Impl::Reject(const Message& msg)
{
}

std::shared_ptr<AssetMeta> Model::Impl::FindAsset(const std::string& guid)
{
	if(auto it = assets_.find(guid); it != assets_.end())
		return it->second;
	return {};
}


void Model::Impl::AddBP(const Message& msg)
{
	//不做任何处理
}

void Model::Impl::OnAddBP(const Message& msg)
{
	do {
		if(!rise_) break;

		if(msg.code != ErrorCode::EC_SUCCESS) {
			rise_(msg);
			break;
		}

		json root = json::parse(msg.data, nullptr, false);
		if (root.is_discarded() || !root.is_object()) break;

		auto asset = std::make_shared<AssetMeta>();
		asset->name = root.value(g_key_name, std::string{});
		asset->guid = root.value(g_key_guid, std::string{});
		asset->space = root.value(g_key_space, std::string{});
		asset->path = root.value(g_key_path, std::string{});
		if (asset->name.empty() || asset->guid.empty()) break;
		if (asset->space.empty())
			asset->space = "user";

		assets_.insert({asset->guid, asset});
		rise_(msg);

	} while (0);
}

void Model::Impl::OnDelBP(const Message& msg)
{
	do {
		if(!rise_) break;

		if(msg.code != ErrorCode::EC_SUCCESS) {
			rise_(msg);
			break;
		}

		json root = json::parse(msg.data, nullptr, false);
		if (root.is_discarded() || !root.is_object()) break;

		const std::string guid = root.value(g_key_guid, std::string{});
		if (guid.empty()) break;

		if (guid == opened_)
			opened_.clear();
		assets_.erase(guid);
		rise_(msg);
		SyncVarFuncs();
		EmitFuncSync();

	} while (0);
}

void Model::Impl::OnAltBP(const Message& msg)
{
	do {
		if(!rise_) break;

		if(msg.code != ErrorCode::EC_SUCCESS) {
			rise_(msg);
			break;
		}

		json root = json::parse(msg.data, nullptr, false);
		if (root.is_discarded() || !root.is_object()) break;

		const std::string guid = root.value(g_key_guid, std::string{});
		const std::string name = root.value(g_key_name, std::string{});
		if (guid.empty() || name.empty()) break;

		auto asset = FindAsset(guid);
		if (!asset) break;

		asset->name = name;
		if (root.contains(g_key_space)) {
			const std::string space = root.value(g_key_space, std::string{});
			if (!space.empty())
				asset->space = space;
		}
		if (root.contains(g_key_path))
			asset->path = root.value(g_key_path, std::string{});
		rise_(msg);

	} while (0);
}

void Model::Impl::DelBP(const Message& msg)
{
	
}

void Model::Impl::AltBP(const Message& msg)
{
	
}

void Model::Impl::OpenBP(const Message& msg)
{
}

void Model::Impl::OnOpenBP(const Message& msg)
{
	do {
		if(!rise_) break;

		if(msg.code != ErrorCode::EC_SUCCESS) {
			rise_(msg);
			break;
		}

		json root = json::parse(msg.data, nullptr, false);
		if (root.is_discarded() || !root.is_object()) break;

		const std::string guid = root.value(g_key_guid, std::string{});
		if (guid.empty()) break;

		if (auto asset = FindAsset(guid)) {
			asset->name = root.value(g_key_name, asset->name);
			const std::string space = root.value(g_key_space, std::string{});
			if (!space.empty())
				asset->space = space;
			if (root.contains(g_key_path))
				asset->path = root.value(g_key_path, std::string{});

			asset->flows.clear();
			if (root.contains(g_key_flows) && root[g_key_flows].is_array()) {
				for (const auto& row : root[g_key_flows]) {
					if (!row.is_object()) continue;
					auto flow = std::make_shared<FlowMeta>();
					flow->guid = row.value(g_key_guid, std::string{});
					flow->name = row.value(g_key_name, std::string{});
					if (flow->guid.empty()) continue;
					if (row.contains(g_key_nodes) && row[g_key_nodes].is_array()) {
						for (const auto& node_row : row[g_key_nodes]) {
							if (!node_row.is_object()) continue;
							auto node = std::make_shared<NodeMeta>();
							node->guid = node_row.value(g_key_guid, std::string{});
							node->name = node_row.value(g_key_name, std::string{});
							node->x = node_row.value(g_key_x, 0.0);
							node->y = node_row.value(g_key_y, 0.0);
							if (node->guid.empty()) continue;
							if (node_row.contains(g_key_pins) && node_row[g_key_pins].is_array()) {
								for (const auto& pin_row : node_row[g_key_pins]) {
									if (!pin_row.is_object()) continue;
									auto pin = std::make_shared<PinMeta>();
									pin->guid = pin_row.value(g_key_guid, std::string{});
									pin->name = pin_row.value(g_key_name, std::string{});
									pin->kind = pin_row.value(g_key_kind, 0);
									pin->direct = pin_row.value(g_key_direct, 0);
									pin->type = pin_row.value(g_key_type, std::string{});
									pin->value = pin_row.value(g_key_value, pin_row.value(g_key_legacy_pin_default_value, std::string{}));
									if (!pin->guid.empty()) node->pins.insert({pin->guid, std::move(pin)});
								}
							}
							flow->nodes.insert({node->guid, std::move(node)});
						}
					}
					if (row.contains(g_key_lines) && row[g_key_lines].is_array()) {
						for (const auto& line_row : row[g_key_lines]) {
							if (!line_row.is_object()) continue;
							auto line = std::make_shared<LineMeta>();
							line->guid = line_row.value(g_key_guid, std::string{});
							line->from_node = line_row.value(g_key_from_node, std::string{});
							line->from_pin = line_row.value(g_key_from_pin, std::string{});
							line->to_node = line_row.value(g_key_to_node, std::string{});
							line->to_pin = line_row.value(g_key_to_pin, std::string{});
							if (!line->guid.empty()) flow->lines.insert({line->guid, std::move(line)});
						}
					}
					asset->flows.insert({flow->guid, flow});
				}
			}

			asset->vars.clear();
			if (root.contains(g_key_vars) && root[g_key_vars].is_array()) {
				for (const auto& row : root[g_key_vars]) {
					if (!row.is_object()) continue;
					auto var = std::make_shared<VarMeta>();
					var->guid = row.value(g_key_guid, std::string{});
					var->name = row.value(g_key_name, std::string{});
					var->type = row.value(g_key_type, std::string{});
					var->value = row.value(g_key_value, std::string{});
					if (var->guid.empty()) continue;
					asset->vars.insert({var->guid, var});
				}
			}
		}

		std::string skip;
		if (auto asset = FindAsset(guid)) {
			opened_ = guid;
			skip = asset->space + "::" + asset->name;
		}
		root[g_key_types] = TypeList(skip);
		if (auto asset = FindAsset(guid)) {
			root[g_key_target] = {{g_key_kind, g_kind_blueprint}, {g_key_guid, asset->guid}};
			root[g_key_fields] = BlueprintFields(*asset);
		}

		Message out = msg;
		out.data = root.dump();
		rise_(out);
		SyncVarFuncs();
		EmitFuncSync();
	} while (0);
}

void Model::Impl::CloseBP(const Message& msg)
{
}

void Model::Impl::OnCloseBP(const Message& msg)
{
	do {
		if(!rise_) break;

		if (msg.code == ErrorCode::EC_SUCCESS) {
			json root = json::parse(msg.data, nullptr, false);
			if (root.is_object()) {
				const std::string guid = root.value(g_key_guid, std::string{});
				if (guid.empty() || guid == opened_)
					opened_.clear();
			}
		}
		rise_(msg);
		SyncVarFuncs();
		EmitFuncSync();
	} while (0);
}

void Model::Impl::TypeSync(const Message& msg)
{
}

void Model::Impl::OnTypeSync(const Message& msg)
{
	do {
		if(!rise_) break;

		json root = json::parse(msg.data, nullptr, false);
		if (root.is_discarded() || !root.is_object() || !root.contains(g_key_types) || !root[g_key_types].is_array())
			break;

		types_.clear();
		for (const auto& row : root[g_key_types]) {
			if (!row.is_object()) continue;
			auto type = std::make_shared<TypeMeta>();
			type->name = row.value(g_key_name, std::string{});
			if (type->name.empty()) continue;
			types_.insert({type->name, type});
		}

		std::string skip;
		if (auto asset = FindAsset(opened_))
			skip = asset->space + "::" + asset->name;

		root[g_key_types] = TypeList(skip);
		Message out = msg;
		out.data = root.dump();
		rise_(out);
	} while (0);
}

void Model::Impl::FuncSync(const Message& msg)
{
}

void Model::Impl::OnFuncSync(const Message& msg)
{
	do {
		if (!rise_) break;

		json root = json::parse(msg.data, nullptr, false);
		if (root.is_discarded() || !root.is_object() || !root.contains(g_key_funcs) || !root[g_key_funcs].is_array())
			break;

		funcs_.clear();
		for (const auto& row : root[g_key_funcs]) {
			if (!row.is_object()) continue;
			auto func = std::make_shared<FuncMeta>();
			func->name = row.value(g_key_name, std::string{});
			func->group = row.value(g_key_group, std::string{});
			func->key = row.value(g_key_key, func->name);
			if (row.contains(g_key_input_types) && row[g_key_input_types].is_array()) {
				for (const auto& type : row[g_key_input_types])
					if (type.is_string() && !type.get<std::string>().empty())
						func->input_types.push_back(type.get<std::string>());
			}
			if (row.contains(g_key_output_types) && row[g_key_output_types].is_array()) {
				for (const auto& type : row[g_key_output_types])
					if (type.is_string() && !type.get<std::string>().empty())
						func->output_types.push_back(type.get<std::string>());
			}
			if (func->key.empty())
				func->key = func->name;
			if (func->name.empty() || func->group.empty()) continue;
			funcs_.insert({func->name, func});
		}

		SyncVarFuncs();
		root[g_key_funcs] = FuncList();
		Message out = msg;
		out.data = root.dump();
		rise_(out);
	} while (0);
}

json Model::Impl::FuncList() const
{
	std::vector<std::shared_ptr<FuncMeta>> rows;
	for (const auto& [name, func] : funcs_) {
		(void)name;
		if (!func || func->name.empty() || func->group.empty())
			continue;
		rows.push_back(func);
	}
	std::sort(rows.begin(), rows.end(), [](const std::shared_ptr<FuncMeta>& a, const std::shared_ptr<FuncMeta>& b) {
		return a->name < b->name;
	});

	json out = json::array();
	for (const auto& func : rows)
		out.push_back({
			{g_key_name, func->name},
			{g_key_group, func->group},
			{g_key_key, func->key.empty() ? func->name : func->key},
			{g_key_input_types, func->input_types},
			{g_key_output_types, func->output_types},
		});
	return out;
}

void Model::Impl::SyncVarFuncs()
{
	for (auto it = funcs_.begin(); it != funcs_.end();) {
		if (it->second && it->second->group == g_group_user)
			it = funcs_.erase(it);
		else
			++it;
	}

	const auto asset = FindAsset(opened_);
	if (!asset)
		return;
	for (const auto& [var_guid, var] : asset->vars) {
		(void)var_guid;
		if (!var || var->guid.empty() || var->name.empty())
			continue;
		for (const char* prefix : {g_node_get_var, g_node_set_var}) {
			auto func = std::make_shared<FuncMeta>();
			func->name = std::string(std::string(prefix) == g_node_get_var ? "Get " : "Set ") + var->name;
			func->group = g_group_user;
			func->key = std::string(prefix) + ":" + var->guid;
			if (!var->type.empty()) {
				if (std::string(prefix) == g_node_get_var)
					func->output_types.push_back(var->type);
				else
					func->input_types.push_back(var->type);
			}
			funcs_[func->key] = std::move(func);
		}
	}
}

void Model::Impl::EmitFuncSync()
{
	if (!rise_)
		return;
	Message out{};
	out.act = Action::FuncSync;
	out.code = 0;
	out.data = json{{g_key_funcs, FuncList()}}.dump();
	rise_(out);
}

json Model::Impl::TypeList(const std::string& skip) const
{
	std::vector<std::string> names;
	for (const auto& [name, type] : types_) {
		if (!type || name.empty() || name == skip)
			continue;
		names.push_back(name);
	}
	std::sort(names.begin(), names.end());

	json rows = json::array();
	for (const auto& name : names)
		rows.push_back({{g_key_name, name}});
	return rows;
}

void Model::Impl::AddFunc(const Message& msg)
{
}

void Model::Impl::OnAddFunc(const Message& msg)
{
	do {
		if (!rise_) break;

		if (msg.code != ErrorCode::EC_SUCCESS) {
			rise_(msg);
			break;
		}

		json root = json::parse(msg.data, nullptr, false);
		if (root.is_discarded() || !root.is_object()) break;

		const std::string asset_guid = root.value(g_key_asset, std::string{});
		const std::string guid = root.value(g_key_guid, std::string{});
		const std::string name = root.value(g_key_name, std::string{});
		if (asset_guid.empty() || guid.empty() || name.empty()) break;

		auto asset = FindAsset(asset_guid);
		if (!asset) break;

		auto flow = std::make_shared<FlowMeta>();
		flow->guid = guid;
		flow->name = name;
		if (root.contains(g_key_node) && root[g_key_node].is_object()) {
			const json row = root[g_key_node];
			auto node = std::make_shared<NodeMeta>();
			node->guid = row.value(g_key_guid, std::string{});
			node->name = row.value(g_key_name, std::string{});
			node->x = row.value(g_key_x, 0.0);
			node->y = row.value(g_key_y, 0.0);
			if (!node->guid.empty())
				flow->nodes.insert({node->guid, node});
		}
		asset->flows.insert({guid, flow});
		rise_(msg);
	} while (0);
}

void Model::Impl::DelFunc(const Message& msg)
{
}

void Model::Impl::OnDelFunc(const Message& msg)
{
	do {
		if (!rise_) break;

		if (msg.code != ErrorCode::EC_SUCCESS) {
			rise_(msg);
			break;
		}

		json root = json::parse(msg.data, nullptr, false);
		if (root.is_discarded() || !root.is_object()) break;

		const std::string asset_guid = root.value(g_key_asset, std::string{});
		const std::string guid = root.value(g_key_guid, std::string{});
		if (guid.empty()) break;

		if (auto asset = FindAsset(asset_guid))
			asset->flows.erase(guid);
		rise_(msg);
	} while (0);
}

void Model::Impl::AltFunc(const Message& msg)
{
}

void Model::Impl::OnAltFunc(const Message& msg)
{
	do {
		if (!rise_) break;

		if (msg.code != ErrorCode::EC_SUCCESS) {
			rise_(msg);
			break;
		}

		json root = json::parse(msg.data, nullptr, false);
		if (root.is_discarded() || !root.is_object()) break;

		const std::string asset_guid = root.value(g_key_asset, std::string{});
		const std::string guid = root.value(g_key_guid, std::string{});
		const std::string name = root.value(g_key_name, std::string{});
		if (guid.empty() || name.empty()) break;

		auto asset = FindAsset(asset_guid);
		auto flow = asset ? FindFlow(*asset, guid) : nullptr;
		if (!flow) break;

		flow->name = name;
		rise_(msg);
	} while (0);
}

void Model::Impl::RunFunc(const Message& msg)
{
	(void)msg;
}

void Model::Impl::OnRunFunc(const Message& msg)
{
	if (rise_) rise_(msg);
}

void Model::Impl::RuntimeSync(const Message& msg)
{
	if (!rise_) return;
	const json root = json::parse(msg.data, nullptr, false);
	if (!root.is_object()) return;
	const std::string asset_guid = root.value(g_key_asset, std::string{});
	const std::string flow_guid = root.value(g_key_flow, std::string{});
	const std::string node_guid = root.value(g_key_guid, std::string{});
	const std::string phase = root.value(g_key_phase, std::string{});
	auto asset = FindAsset(asset_guid);
	auto flow = asset ? FindFlow(*asset, flow_guid) : nullptr;
	if (!flow) return;
	if (phase == "finished") {
		for (const auto& row : root.value(g_key_vars, json::array())) {
			if (!row.is_object()) continue;
			auto var = FindVar(*asset, row.value(g_key_guid, std::string{}));
			if (var) var->value = row.value(g_key_value, var->value);
		}
	}

	if (phase == "started") {
		flow->flow_status = FlowStatus::Running;
		for (auto& [guid, node] : flow->nodes) {
			(void)guid;
			if (!node) continue;
			node->status = NS_SILENT;
		}
		rise_(msg);
		return;
	}

	if (phase == "node") {
		auto node_it = flow->nodes.find(node_guid);
		if (node_it == flow->nodes.end() || !node_it->second) return;
		auto& node = *node_it->second;
		const std::string status = root.value(g_key_node_status, std::string("Ok"));
		node.status = status == "Error" ? NS_ERROR : NS_OK;
		for (const auto& row : root.value(g_key_pins, json::array())) {
			if (!row.is_object()) continue;
			const std::string pin_guid = row.value(g_key_guid, std::string{});
			auto pin_it = node.pins.find(pin_guid);
			if (pin_it != node.pins.end() && pin_it->second)
				pin_it->second->value = row.value(g_key_value, pin_it->second->value);
		}
		rise_(msg);
		return;
	}

	if (phase == "finished") {
		flow->flow_status = ParseFlowStatus(root.value(g_key_flow_status, std::string{}), msg.code);
		rise_(msg);
		return;
	}

	rise_(msg);
}

void Model::Impl::SelectFunc(const Message& msg)
{
	json root = json::parse(msg.data, nullptr, false);
	const std::string guid = root.is_object() ? root.value(g_key_guid, std::string{}) : std::string{};
	auto asset = FindAsset(opened_);
	auto flow = asset ? FindFlow(*asset, guid) : nullptr;
	if (!flow) {
		RiseMiss(rise_, msg, "function not found", guid);
		return;
	}
	RiseDetail(rise_, msg, g_kind_flow, flow->guid, FlowFields(*flow));
}

void Model::Impl::AddVar(const Message& msg)
{
}

void Model::Impl::OnAddVar(const Message& msg)
{
	do {
		if (!rise_) break;

		if (msg.code != ErrorCode::EC_SUCCESS) {
			rise_(msg);
			break;
		}

		json root = json::parse(msg.data, nullptr, false);
		if (root.is_discarded() || !root.is_object()) break;

		const std::string asset_guid = root.value(g_key_asset, std::string{});
		const std::string guid = root.value(g_key_guid, std::string{});
		const std::string name = root.value(g_key_name, std::string{});
		if (asset_guid.empty() || guid.empty() || name.empty()) break;

		auto asset = FindAsset(asset_guid);
		if (!asset) break;

		auto var = std::make_shared<VarMeta>();
		var->guid = guid;
		var->name = name;
		var->type = root.value(g_key_type, std::string{});
		var->value = root.value(g_key_value, std::string{});
		asset->vars.insert({guid, var});
		rise_(msg);
		SyncVarFuncs();
		EmitFuncSync();
	} while (0);
}

void Model::Impl::DelVar(const Message& msg)
{
}

void Model::Impl::OnDelVar(const Message& msg)
{
	do {
		if (!rise_) break;

		if (msg.code != ErrorCode::EC_SUCCESS) {
			rise_(msg);
			break;
		}

		json root = json::parse(msg.data, nullptr, false);
		if (root.is_discarded() || !root.is_object()) break;

		const std::string asset_guid = root.value(g_key_asset, std::string{});
		const std::string guid = root.value(g_key_guid, std::string{});
		if (guid.empty()) break;

		if (auto asset = FindAsset(asset_guid)) {
			for (const auto& row : root.value(g_key_removed_nodes, json::array())) {
				if (!row.is_object()) continue;
				const std::string flow_guid = row.value(g_key_flow, std::string{});
				const std::string node_guid = row.value(g_key_guid, std::string{});
				if (auto flow = FindFlow(*asset, flow_guid)) flow->nodes.erase(node_guid);
			}
			for (const auto& row : root.value(g_key_removed_lines, json::array())) {
				if (!row.is_object()) continue;
				const std::string flow_guid = row.value(g_key_flow, std::string{});
				const std::string line_guid = row.value(g_key_guid, std::string{});
				if (auto flow = FindFlow(*asset, flow_guid)) flow->lines.erase(line_guid);
			}
			asset->vars.erase(guid);
			SyncVarFuncs();
		}
		rise_(msg);
		EmitFuncSync();
	} while (0);
}

void Model::Impl::AltVar(const Message& msg)
{
}

void Model::Impl::OnAltVar(const Message& msg)
{
	do {
		if (!rise_) break;

		if (msg.code != ErrorCode::EC_SUCCESS) {
			rise_(msg);
			break;
		}

		json root = json::parse(msg.data, nullptr, false);
		if (root.is_discarded() || !root.is_object()) break;

		const std::string asset_guid = root.value(g_key_asset, std::string{});
		const std::string guid = root.value(g_key_guid, std::string{});
		const std::string name = root.value(g_key_name, std::string{});
		if (guid.empty() || name.empty()) break;

		auto asset = FindAsset(asset_guid);
		auto var = asset ? FindVar(*asset, guid) : nullptr;
		if (!var) break;

		var->name = name;
		for (const auto& row : root.value(g_key_nodes, json::array())) {
			if (!row.is_object()) continue;
			const std::string flow_guid = row.value(g_key_flow, std::string{});
			const json node_row = row.value(g_key_node, json::object());
			if (!node_row.is_object()) continue;
			auto flow = FindFlow(*asset, flow_guid);
			if (!flow) continue;
			const std::string node_guid = node_row.value(g_key_guid, std::string{});
			if (node_guid.empty()) continue;
			auto& node = flow->nodes[node_guid];
			if (!node) node = std::make_shared<NodeMeta>();
			node->guid = node_guid;
			node->name = node_row.value(g_key_name, std::string{});
			node->x = node_row.value(g_key_x, 0.0);
			node->y = node_row.value(g_key_y, 0.0);
			node->pins.clear();
			for (const auto& pin_row : node_row.value(g_key_pins, json::array())) {
				if (!pin_row.is_object()) continue;
				auto pin = std::make_shared<PinMeta>();
				pin->guid = pin_row.value(g_key_guid, std::string{});
				pin->name = pin_row.value(g_key_name, std::string{});
				pin->kind = pin_row.value(g_key_kind, 0);
				pin->direct = pin_row.value(g_key_direct, 0);
				pin->type = pin_row.value(g_key_type, std::string{});
				pin->value = pin_row.value(g_key_value, std::string{});
				pin->connected = pin_row.value(g_key_connected, false);
				if (!pin->guid.empty()) node->pins[pin->guid] = std::move(pin);
			}
		}
		for (const auto& row : root.value(g_key_removed_lines, json::array())) {
			if (!row.is_object()) continue;
			const std::string flow_guid = row.value(g_key_flow, std::string{});
			const std::string line_guid = row.value(g_key_guid, std::string{});
			if (auto flow = FindFlow(*asset, flow_guid)) flow->lines.erase(line_guid);
		}
		rise_(msg);
		SyncVarFuncs();
		EmitFuncSync();
	} while (0);
}

void Model::Impl::SelectVar(const Message& msg)
{
	json root = json::parse(msg.data, nullptr, false);
	const std::string guid = root.is_object() ? root.value(g_key_guid, std::string{}) : std::string{};
	auto asset = FindAsset(opened_);
	auto var = asset ? FindVar(*asset, guid) : nullptr;
	if (!var) {
		RiseMiss(rise_, msg, "variable not found", guid);
		return;
	}
	RiseDetail(rise_, msg, g_kind_var, var->guid, VarFields(*var));
}

void Model::Impl::AddNode(const Message& msg)
{
}

void Model::Impl::OnAddNode(const Message& msg)
{
	do {
		if (!rise_) break;

		if (msg.code != ErrorCode::EC_SUCCESS) {
			rise_(msg);
			break;
		}

		json root = json::parse(msg.data, nullptr, false);
		if (root.is_discarded() || !root.is_object()) break;

		const std::string asset_guid = root.value(g_key_asset, std::string{});
		const std::string flow_guid = root.value(g_key_flow, std::string{});
		const std::string guid = root.value(g_key_guid, std::string{});
		const std::string name = root.value(g_key_name, std::string{});
		if (!guid.empty() && !name.empty()) {
			if (auto asset = FindAsset(asset_guid)) {
				if (auto flow = FindFlow(*asset, flow_guid)) {
					auto node = std::make_shared<NodeMeta>();
					node->guid = guid;
					node->name = name;
					node->x = root.value(g_key_x, 0.0);
					node->y = root.value(g_key_y, 0.0);
					for (const auto& pin_row : root.value(g_key_pins, json::array())) {
						if (!pin_row.is_object())
							continue;
						auto pin = std::make_shared<PinMeta>();
						pin->guid = pin_row.value(g_key_guid, std::string{});
						pin->name = pin_row.value(g_key_name, std::string{});
						pin->kind = pin_row.value(g_key_kind, 0);
						pin->direct = pin_row.value(g_key_direct, 0);
						pin->type = pin_row.value(g_key_type, std::string{});
						pin->value = pin_row.value(g_key_value, pin_row.value(g_key_legacy_pin_default_value, std::string{}));
						if (!pin->guid.empty())
							node->pins.insert({pin->guid, std::move(pin)});
					}
					flow->nodes.insert({guid, node});
				}
			}
		}
		rise_(msg);
	} while (0);
}

void Model::Impl::DelNode(const Message& msg)
{
}

void Model::Impl::OnDelNode(const Message& msg)
{
	do {
		if (!rise_) break;

		if (msg.code != ErrorCode::EC_SUCCESS) {
			rise_(msg);
			break;
		}

		json root = json::parse(msg.data, nullptr, false);
		if (root.is_discarded() || !root.is_object()) break;

		const std::string asset_guid = root.value(g_key_asset, std::string{});
		const std::string flow_guid = root.value(g_key_flow, std::string{});
		const std::string guid = root.value(g_key_guid, std::string{});
		if (guid.empty()) break;

		if (auto asset = FindAsset(asset_guid)) {
			if (auto flow = FindFlow(*asset, flow_guid)) {
				flow->nodes.erase(guid);
				std::erase_if(flow->lines, [&](const auto& entry) {
					const auto& line = entry.second;
					return line && (line->from_node == guid || line->to_node == guid);
				});
			}
		}
		rise_(msg);
	} while (0);
}

void Model::Impl::AltNode(const Message& msg)
{
}

void Model::Impl::OnAltNode(const Message& msg)
{
	do {
		if (!rise_) break;

		if (msg.code != ErrorCode::EC_SUCCESS) {
			rise_(msg);
			break;
		}

		json root = json::parse(msg.data, nullptr, false);
		if (root.is_discarded() || !root.is_object()) break;

		const std::string asset_guid = root.value(g_key_asset, std::string{});
		const std::string flow_guid = root.value(g_key_flow, std::string{});
		const std::string guid = root.value(g_key_guid, std::string{});
		if (guid.empty()) break;

		if (auto asset = FindAsset(asset_guid)) {
			if (auto flow = FindFlow(*asset, flow_guid)) {
				if (auto it = flow->nodes.find(guid); it != flow->nodes.end() && it->second) {
					if (root.contains(g_key_x)) it->second->x = root.value(g_key_x, it->second->x);
					if (root.contains(g_key_y)) it->second->y = root.value(g_key_y, it->second->y);
					if (root.contains(g_key_pins) && root[g_key_pins].is_array()) {
						it->second->pins.clear();
						for (const auto& pin_row : root[g_key_pins]) {
							if (!pin_row.is_object()) continue;
							auto pin = std::make_shared<PinMeta>();
							pin->guid = pin_row.value(g_key_guid, std::string{});
							pin->name = pin_row.value(g_key_name, std::string{});
							pin->kind = pin_row.value(g_key_kind, 0);
							pin->direct = pin_row.value(g_key_direct, 0);
							pin->type = pin_row.value(g_key_type, std::string{});
							pin->value = pin_row.value(g_key_value, pin_row.value(g_key_legacy_pin_default_value, std::string{}));
							pin->connected = pin_row.value(g_key_connected, false);
							if (!pin->guid.empty()) it->second->pins[pin->guid] = std::move(pin);
						}
					}
				}
				for (const auto& line_guid : root.value(g_key_removed_lines, json::array()))
					flow->lines.erase(line_guid.get<std::string>());
			}
		}
		rise_(msg);
	} while (0);
}

void Model::Impl::SelectNode(const Message& msg)
{
	json root = json::parse(msg.data, nullptr, false);
	const std::string guid = root.is_object() ? root.value(g_key_guid, std::string{}) : std::string{};
	auto asset = FindAsset(opened_);
	auto node = asset ? FindNode(*asset, guid) : nullptr;
	std::shared_ptr<FlowMeta> flow;
	if (asset) {
		for (const auto& [flow_guid, candidate] : asset->flows) {
			(void)flow_guid;
			if (candidate && candidate->nodes.contains(guid)) { flow = candidate; break; }
		}
	}
	if (!node) {
		RiseMiss(rise_, msg, "node not found", guid);
		return;
	}
	if (!flow) {
		RiseMiss(rise_, msg, "flow not found", guid);
		return;
	}
	RiseDetail(rise_, msg, g_kind_node, node->guid, NodeFields(*node, *flow));
}

void Model::Impl::AddLine(const Message&) {}

void Model::Impl::OnAddLine(const Message& msg)
{
	if (!rise_) return;
	if (msg.code != ErrorCode::EC_SUCCESS) { rise_(msg); return; }
	const json root = json::parse(msg.data, nullptr, false);
	if (!root.is_object()) return;
	const auto asset_guid = root.value(g_key_asset, std::string{});
	const auto flow_guid = root.value(g_key_flow, std::string{});
	const auto line_guid = root.value(g_key_guid, std::string{});
	if (auto asset = FindAsset(asset_guid)) if (auto flow = FindFlow(*asset, flow_guid)) {
		for (const auto& old : root.value(g_key_removed_lines, json::array()))
			flow->lines.erase(old.get<std::string>());
		auto line = std::make_shared<LineMeta>();
		line->guid = line_guid;
		line->from_node = root.value(g_key_from_node, std::string{});
		line->from_pin = root.value(g_key_from_pin, std::string{});
		line->to_node = root.value(g_key_to_node, std::string{});
		line->to_pin = root.value(g_key_to_pin, std::string{});
		if (!line->guid.empty()) flow->lines[line->guid] = line;
	}
	rise_(msg);
}

void Model::Impl::DelLine(const Message&) {}

void Model::Impl::OnDelLine(const Message& msg)
{
	if (!rise_) return;
	if (msg.code != ErrorCode::EC_SUCCESS) { rise_(msg); return; }
	const json root = json::parse(msg.data, nullptr, false);
	if (root.is_object()) {
		const auto asset_guid = root.value(g_key_asset, std::string{});
		const auto flow_guid = root.value(g_key_flow, std::string{});
		const auto line_guid = root.value(g_key_guid, std::string{});
		const auto to_node_guid = root.value(g_key_to_node, std::string{});
		const auto to_pin_guid = root.value(g_key_to_pin, std::string{});
		if (auto asset = FindAsset(asset_guid)) if (auto flow = FindFlow(*asset, flow_guid)) {
			flow->lines.erase(line_guid);
			auto node_it = flow->nodes.find(to_node_guid);
			if (node_it != flow->nodes.end() && node_it->second
				&& node_it->second->name == "Print") {
				auto pin_it = node_it->second->pins.find(to_pin_guid);
				if (pin_it != node_it->second->pins.end() && pin_it->second)
					pin_it->second->value.clear();
			}
		}
	}
	rise_(msg);
}

void Model::Impl::Compile(const Message&)
{
}

} // namespace cg::model
