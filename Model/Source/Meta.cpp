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
enum Act {
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
	return {{g_key_key, std::move(key)}, {g_key_text, std::move(text)}, {g_key_show, std::move(show)}, {g_key_value, std::move(value)}};
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

json FlowFields(const FlowMeta& flow)
{
	return json::array({
		Field(g_key_name, "名称", g_show_line, flow.name),
		Field(g_key_guid, "Guid", g_show_label, flow.guid),
	});
}

json VarFields(const VarMeta& var, const json& types)
{
	json type_field = Field(g_key_type, "类型", g_show_combo, var.type);
	type_field[g_key_items] = types;
	return json::array({
		Field(g_key_name, "名称", g_show_line, var.name),
		std::move(type_field),
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

json NodeFields(const NodeMeta& node)
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
		groups.push_back({
			{g_key_key, pin->guid},
			{g_key_text, pin->name},
			{g_key_fields, json::array({
				Field(g_key_direct, "方向", g_show_label, DirectName(pin->direct)),
				Field(g_key_type, "类型", g_show_label, pin->type),
				Field(g_key_value, "值", g_show_label, pin->value),
			})},
		});
	}

	json fields = json::array({
		Field(g_key_name, "名称", g_show_line, node.name),
		Field(g_key_x, "X", g_show_label, node.x),
		Field(g_key_y, "Y", g_show_label, node.y),
		Field(g_key_status, "状态", g_show_label, StatusName(node.status)),
		Field(g_key_guid, "Guid", g_show_label, node.guid),
	});
	json pins_field = Field(g_key_pins, "引脚", g_show_list, std::move(groups));
	fields.push_back(std::move(pins_field));
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
	void AddFunc(const Message& msg);
	void OnAddFunc(const Message& msg);
	void DelFunc(const Message& msg);
	void OnDelFunc(const Message& msg);
	void AltFunc(const Message& msg);
	void OnAltFunc(const Message& msg);
	void RunFunc(const Message& msg);
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
	std::unordered_map<int, Linker> linkers_;
};

Model::Model() : impl_(std::make_unique<Impl>()) {
	impl_->Init();
}
Model::~Model() = default;

void Model::Submit(const Message& msg) { impl_->Submit(msg); }
void Model::Reply(const Message& msg){ impl_->Reply(msg); }
void Model::SetRise(std::function<void(const Message&)> rise) { impl_->SetRise(std::move(rise)); }


void Model::Impl::Init() {
	linkers_.insert({ Act::AddBP, Linker{
		[this](const Message& msg) { AddBP(msg); },
		[this](const Message& msg) { OnAddBP(msg); },
	} });
	linkers_.insert({ Act::DelBP, Linker{
		[this](const Message& msg) { DelBP(msg); },
		[this](const Message& msg) { OnDelBP(msg); },
	} });
	linkers_.insert({ Act::AltBP, Linker{
		[this](const Message& msg) { AltBP(msg); },
		[this](const Message& msg) { OnAltBP(msg); },
	} });
	linkers_.insert({ Act::OpenBP, Linker{
		[this](const Message& msg) { OpenBP(msg); },
		[this](const Message& msg) { OnOpenBP(msg); },
	} });
	linkers_.insert({ Act::CloseBP, Linker{
		[this](const Message& msg) { CloseBP(msg); },
		[this](const Message& msg) { OnCloseBP(msg); },
	} });
	linkers_.insert({ Act::TypeSync, Linker{
		[this](const Message& msg) { TypeSync(msg); },
		[this](const Message& msg) { OnTypeSync(msg); },
	} });
	linkers_.insert({ Act::FuncSync, Linker{
		[this](const Message& msg) { FuncSync(msg); },
		[this](const Message& msg) { OnFuncSync(msg); },
	} });
	linkers_.insert({ Act::SelectFunc, Linker{
		[this](const Message& msg) { SelectFunc(msg); },
		[](const Message&) {},
	} });
	linkers_.insert({ Act::SelectVar, Linker{
		[this](const Message& msg) { SelectVar(msg); },
		[](const Message&) {},
	} });
	linkers_.insert({ Act::SelectNode, Linker{
		[this](const Message& msg) { SelectNode(msg); },
		[](const Message&) {},
	} });
	linkers_.insert({ Act::AddFunc, Linker{
		[this](const Message& msg) { AddFunc(msg); },
		[this](const Message& msg) { OnAddFunc(msg); },
	} });
	linkers_.insert({ Act::DelFunc, Linker{
		[this](const Message& msg) { DelFunc(msg); },
		[this](const Message& msg) { OnDelFunc(msg); },
	} });
	linkers_.insert({ Act::AltFunc, Linker{
		[this](const Message& msg) { AltFunc(msg); },
		[this](const Message& msg) { OnAltFunc(msg); },
	} });
	linkers_.insert({ Act::AddNode, Linker{
		[this](const Message& msg) { AddNode(msg); },
		[this](const Message& msg) { OnAddNode(msg); },
	} });
	linkers_.insert({ Act::DelNode, Linker{
		[this](const Message& msg) { DelNode(msg); },
		[this](const Message& msg) { OnDelNode(msg); },
	} });
	linkers_.insert({ Act::AltNode, Linker{
		[this](const Message& msg) { AltNode(msg); },
		[this](const Message& msg) { OnAltNode(msg); },
	} });
	linkers_.insert({ Act::AddVar, Linker{
		[this](const Message& msg) { AddVar(msg); },
		[this](const Message& msg) { OnAddVar(msg); },
	} });
	linkers_.insert({ Act::DelVar, Linker{
		[this](const Message& msg) { DelVar(msg); },
		[this](const Message& msg) { OnDelVar(msg); },
	} });
	linkers_.insert({ Act::AltVar, Linker{
		[this](const Message& msg) { AltVar(msg); },
		[this](const Message& msg) { OnAltVar(msg); },
	} });
}

void Model::Impl::Submit(const Message& msg)
{
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
			if (func->key.empty())
				func->key = func->name;
			if (func->name.empty() || func->group.empty()) continue;
			funcs_.insert({func->name, func});
		}

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
		out.push_back({{g_key_name, func->name}, {g_key_group, func->group}, {g_key_key, func->key.empty() ? func->name : func->key}});
	return out;
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
		asset->vars.insert({guid, var});
		rise_(msg);
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

		if (auto asset = FindAsset(asset_guid))
			asset->vars.erase(guid);
		rise_(msg);
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
		if (root.contains(g_key_type))
			var->type = root.value(g_key_type, std::string{});
		rise_(msg);
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
	std::string skip;
	if (asset)
		skip = asset->space + "::" + asset->name;
	json items = json::array();
	for (const auto& row : TypeList(skip)) {
		if (row.is_object())
			items.push_back(row.value(g_key_name, std::string{}));
	}
	RiseDetail(rise_, msg, g_kind_var, var->guid, VarFields(*var, items));
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
			if (auto flow = FindFlow(*asset, flow_guid))
				flow->nodes.erase(guid);
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
					it->second->x = root.value(g_key_x, 0.0);
					it->second->y = root.value(g_key_y, 0.0);
				}
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
	if (!node) {
		RiseMiss(rise_, msg, "node not found", guid);
		return;
	}
	RiseDetail(rise_, msg, g_kind_node, node->guid, NodeFields(*node));
}

void Model::Impl::Compile(const Message& msg)
{
}

} // namespace cg::model
