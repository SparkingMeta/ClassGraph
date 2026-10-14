#include "Engine.h"
#include "Abstract/ACaller.h"
#include "Abstract/AFlow.h"
#include "Abstract/APin.h"
#include "Abstract/AScript.h"
#include "Abstract/AVariant.h"
#include "Base/VSystem.h"
#include "Base/VType.h"
#include "Realistic/VAsset.h"
#include "Realistic/VGraph.h"
#include "Realistic/VNode.h"
#include "Realistic/VOperator.h"
#include "Realistic/VVariant.h"
#include "Base/VGuid.h"
#include "Base/VRegister.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <format>
#include <fstream>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

namespace cg::engine {

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


void Fail(Message& msg, const char* reason)
{
	msg.code = 1;
	msg.data = json{{g_key_error, reason}}.dump();
}

void RiseFail(const std::function<void(const Message&)>& rise, const Message& msg, const char* reason, const std::string& guid = {})
{
	if (!rise) return;
	Message reply = msg;
	Fail(reply, reason);
	if (!guid.empty())
		reply.data = json{{g_key_error, reason}, {g_key_guid, guid}}.dump();
	rise(reply);
}

namespace fs = std::filesystem;

constexpr const char* kAssetExt = ".cg";

std::filesystem::path FromUtf8(const std::string& text)
{
	return fs::path(std::u8string(reinterpret_cast<const char8_t*>(text.data()), text.size()));
}

std::string ToUtf8(const fs::path& path)
{
	const auto u8 = path.generic_u8string();
	return { reinterpret_cast<const char*>(u8.data()), u8.size() };
}

bool IsSafeName(const std::string& text)
{
	if (text.empty() || text == "." || text == "..")
		return false;
	return text.find_first_of("\\/:*?\"<>|") == std::string::npos;
}

fs::path AssetFile(const fs::path& root, const std::string& space, const std::string& name)
{
	return root / FromUtf8(space) / FromUtf8(name + kAssetExt);
}

bool Exists(const fs::path& file)
{
	std::error_code ec;
	return fs::exists(file, ec);
}

bool StoreAsset(const fs::path& file, const CG::Core::VBlueprint& asset)
{
	std::error_code ec;
	fs::create_directories(file.parent_path(), ec);
	if (ec)
		return false;

	json doc = {
		{g_key_guid, asset.GetGuid()},
		{g_key_name, asset.GetName()},
		{g_key_space, asset.GetSpace()},
	};
	std::string body;
	if (asset.Serialize(body)) {
		json parsed = json::parse(body, nullptr, false);
		if (!parsed.is_discarded())
			doc[g_key_body] = std::move(parsed);
	}

	std::ofstream out(file, std::ios::binary | std::ios::trunc);
	if (!out)
		return false;
	const std::string text = doc.dump(2);
	out.write(text.data(), static_cast<std::streamsize>(text.size()));
	out.flush();
	if (!out) {
		out.close();
		fs::remove(file, ec);
		return false;
	}
	return true;
}

bool DropAsset(const fs::path& file)
{
	std::error_code ec;
	if (Exists(file) && !fs::remove(file, ec))
		return false;

	const fs::path dir = file.parent_path();
	if (!dir.empty() && fs::is_empty(dir, ec) && !ec)
		fs::remove(dir, ec);
	return true;
}

bool ReplaceAsset(const fs::path& from, const fs::path& to, const CG::Core::VBlueprint& asset)
{
	if (from != to && Exists(to))
		return false;
	if (!StoreAsset(to, asset))
		return false;
	if (from == to || !Exists(from))
		return true;
	if (DropAsset(from))
		return true;
	DropAsset(to);
	return false;
}

using AssetMap = std::unordered_map<std::string, std::shared_ptr<CG::Core::VBlueprint>>;

AssetMap::iterator FindAsset(AssetMap& assets, const std::string& guid)
{
	for (auto it = assets.begin(); it != assets.end(); ++it) {
		if (it->second && it->second->GetGuid() == guid)
			return it;
	}
	return assets.end();
}

struct OwnedVar {
	AssetMap::iterator asset;
	std::shared_ptr<CG::Core::AVariant> var;
};

OwnedVar FindOwnedVar(AssetMap& assets, const std::string& guid)
{
	for (auto it = assets.begin(); it != assets.end(); ++it) {
		if (!it->second) continue;
		if (auto var = it->second->FindVar(guid))
			return {it, var};
	}
	return {assets.end(), {}};
}

bool VarNameTaken(const CG::Core::VBlueprint& asset, const std::string& name, const std::string& except = {})
{
	for (const auto& [id, var] : asset.GetVars()) {
		(void)id;
		if (!var || var->GetGuid() == except)
			continue;
		if (var->GetName() == name)
			return true;
	}
	return false;
}

std::string MakeVarName(const CG::Core::VBlueprint& asset)
{
	if (!VarNameTaken(asset, "NewVar"))
		return "NewVar";
	for (int i = 1; i < 10000; ++i) {
		const std::string name = std::format("NewVar{}", i);
		if (!VarNameTaken(asset, name))
			return name;
	}
	return {};
}

struct OwnedFlow {
	AssetMap::iterator asset;
	std::shared_ptr<CG::Core::AFlow> flow;
};

OwnedFlow FindOwnedFlow(AssetMap& assets, const std::string& guid)
{
	for (auto it = assets.begin(); it != assets.end(); ++it) {
		if (!it->second) continue;
		if (auto flow = it->second->FindFlow(guid))
			return {it, flow};
	}
	return {assets.end(), {}};
}

bool FlowNameTaken(const CG::Core::VBlueprint& asset, const std::string& name, const std::string& except = {})
{
	for (const auto& [id, flow] : asset.GetFlows()) {
		(void)id;
		if (!flow || flow->GetGuid() == except)
			continue;
		if (flow->GetName() == name)
			return true;
	}
	return false;
}

std::string MakeFlowName(const CG::Core::VBlueprint& asset)
{
	if (!FlowNameTaken(asset, "NewFunc"))
		return "NewFunc";
	for (int i = 1; i < 10000; ++i) {
		const std::string name = std::format("NewFunc{}", i);
		if (!FlowNameTaken(asset, name))
			return name;
	}
	return {};
}

struct OwnedNode {
	AssetMap::iterator asset;
	std::shared_ptr<CG::Core::AFlow> flow;
	std::shared_ptr<CG::Core::VGraph> graph;
	std::shared_ptr<CG::Core::ANode> node;
};

std::shared_ptr<CG::Core::VGraph> FlowGraph(const std::shared_ptr<CG::Core::AFlow>& flow)
{
	if (!flow)
		return {};
	return std::dynamic_pointer_cast<CG::Core::VGraph>(flow->GetGraph());
}

OwnedNode FindOwnedNode(AssetMap& assets, const std::string& guid)
{
	for (auto it = assets.begin(); it != assets.end(); ++it) {
		if (!it->second) continue;
		for (const auto& [id, flow] : it->second->GetFlows()) {
			(void)id;
			auto graph = FlowGraph(flow);
			if (!graph) continue;
			if (auto node = graph->FindNode(guid))
				return {it, flow, graph, node};
		}
	}
	return {assets.end(), {}, {}, {}};
}

std::shared_ptr<CG::Core::VNode> MakeTermNode(const char* key, double x, double y)
{
	std::shared_ptr<CG::Core::ACaller> caller;
	if (std::string(key) == g_node_entry)
		caller = std::make_shared<CG::Core::FEntry>();
	else if (std::string(key) == g_node_exit)
		caller = std::make_shared<CG::Core::FExit>();
	else
		return {};

	caller->SetName(key);
	auto node = CG::Core::VNode::Make(caller);
	if (!node)
		return {};
	node->SetGuid(GenerateGUID());
	node->SetName(key);
	node->SetCoord(Vec2f{static_cast<float>(x), static_cast<float>(y)});
	return node;
}

json TermNodeJson(const CG::Core::VNode& node)
{
	json pins = json::array();
	auto push = [&](const std::shared_ptr<CG::Core::APin>& pin) {
		if (!pin) return;
		const auto& meta = pin->GetPinMeta();
		pins.push_back({
			{g_key_name, meta.name},
			{g_key_kind, static_cast<int>(meta.kind)},
			{g_key_direct, static_cast<int>(pin->GetDirect())},
			{g_key_type, meta.type ? meta.type->name_ : std::string{}},
			{g_key_connected, !pin->GetLines().empty()},
		});
	};
	for (const auto& pin : node.GetInputPins())
		push(pin);
	for (const auto& pin : node.GetOutputPins())
		push(pin);
	const char* group = nullptr;
	if (auto caller = node.GetCaller()) {
		switch (caller->GetKind()) {
		case CG::Core::CK_Function: group = g_group_function; break;
		case CG::Core::CK_Script: group = g_group_script; break;
		case CG::Core::CK_Controller: group = g_group_controller; break;
		case CG::Core::CK_Operator: group = g_group_operator; break;
		default: break;
		}
	}
	json body = {
		{g_key_guid, node.GetGuid()},
		{g_key_name, node.GetName()},
		{g_key_key, node.GetName()},
		{g_key_x, node.GetCoord().x},
		{g_key_y, node.GetCoord().y},
		{g_key_pins, std::move(pins)},
	};
	if (group)
		body[g_key_group] = group;
	return body;
}

std::shared_ptr<CG::Core::VNode> AttachTerm(CG::Core::VGraph& graph, const char* key, double x, double y, const char*& error)
{
	error = nullptr;
	const std::string kind = key ? key : "";
	if (kind == g_node_entry) {
		if (graph.GetEntry()) {
			error = "entry exists";
			return {};
		}
	} else if (kind == g_node_exit) {
		if (graph.GetExit()) {
			error = "exit exists";
			return {};
		}
	} else {
		error = "invalid node";
		return {};
	}

	auto node = MakeTermNode(key, x, y);
	if (!node) {
		error = "add node failed";
		return {};
	}
	graph.AddNode(node);
	if (kind == g_node_entry)
		graph.SetEntry(node);
	else
		graph.SetExit(node);
	return node;
}

} // namespace

class Engine::Impl {
public:
	void Init();
	void SetRoot(const std::string& root);
	void SetTypeDir(const std::string& dir);
	void LoadTypes();
	void SetRise(std::function<void(const Message&)> fn);
	void Apply(const Message& msg);

private:
	bool AddBP(const Message& msg);
	bool DelBP(const Message& msg);
	bool AltBP(const Message& msg);
	bool OpenBP(const Message& msg);
	bool CloseBP(const Message& msg);
	bool AddVar(const Message& msg);
	bool DelVar(const Message& msg);
	bool AltVar(const Message& msg);
	bool AddFunc(const Message& msg);
	bool DelFunc(const Message& msg);
	bool AltFunc(const Message& msg);
	bool AddNode(const Message& msg);
	bool DelNode(const Message& msg);
	bool AltNode(const Message& msg);
	bool Compile(const Message& msg);
	void PublishTypes();
	void PublishFuncs();

	std::mutex mutex_;
	fs::path root_;
	fs::path type_dir_;
	std::function<void(const Message&)> rise_;
	std::unordered_map<std::string, std::shared_ptr<CG::Core::VBlueprint>> assets_;
	std::unordered_map<int, std::function<void(const Message&)>> handlers_;
};

Engine::Engine() : impl_(std::make_unique<Impl>()) {
	impl_->Init();
}
Engine::~Engine() = default;

void Engine::Apply(const Message& msg) { impl_->Apply(msg); }
void Engine::SetRoot(const std::string& root) { impl_->SetRoot(root); }
void Engine::SetTypeDir(const std::string& dir) { impl_->SetTypeDir(dir); }
void Engine::LoadTypes() { impl_->LoadTypes(); }
void Engine::SetRise(std::function<void(const Message&)> rise) { impl_->SetRise(std::move(rise)); }

void Engine::Impl::Init()
{
	handlers_.insert({ Act::AddBP, [this](const Message& msg) { AddBP(msg); } });
	handlers_.insert({ Act::DelBP, [this](const Message& msg) { DelBP(msg); } });
	handlers_.insert({ Act::AltBP, [this](const Message& msg) { AltBP(msg); } });
	handlers_.insert({ Act::OpenBP, [this](const Message& msg) { OpenBP(msg); } });
	handlers_.insert({ Act::CloseBP, [this](const Message& msg) { CloseBP(msg); } });
	handlers_.insert({ Act::Compile, [this](const Message& msg) { Compile(msg); } });
	handlers_.insert({ Act::AddVar, [this](const Message& msg) { AddVar(msg); } });
	handlers_.insert({ Act::DelVar, [this](const Message& msg) { DelVar(msg); } });
	handlers_.insert({ Act::AltVar, [this](const Message& msg) { AltVar(msg); } });
	handlers_.insert({ Act::AddFunc, [this](const Message& msg) { AddFunc(msg); } });
	handlers_.insert({ Act::DelFunc, [this](const Message& msg) { DelFunc(msg); } });
	handlers_.insert({ Act::AltFunc, [this](const Message& msg) { AltFunc(msg); } });
	handlers_.insert({ Act::AddNode, [this](const Message& msg) { AddNode(msg); } });
	handlers_.insert({ Act::DelNode, [this](const Message& msg) { DelNode(msg); } });
	handlers_.insert({ Act::AltNode, [this](const Message& msg) { AltNode(msg); } });
}

void Engine::Impl::SetRoot(const std::string& root)
{
	std::lock_guard lock(mutex_);
	root_ = FromUtf8(root);
}

void Engine::Impl::SetTypeDir(const std::string& dir)
{
	std::lock_guard lock(mutex_);
	type_dir_ = FromUtf8(dir);
}

void Engine::Impl::PublishTypes()
{
	if (!rise_) return;

	std::vector<std::string> names;
	for (const auto* type : CG::Core::VSystem::Get().GetTypes()) {
		if (type && !type->name_.empty())
			names.push_back(type->name_);
	}
	std::sort(names.begin(), names.end());
	names.erase(std::unique(names.begin(), names.end()), names.end());

	json types = json::array();
	for (const auto& name : names)
		types.push_back({{g_key_name, name}});

	Message msg{};
	msg.act = Act::TypeSync;
	msg.code = 0;
	msg.data = json{{g_key_types, std::move(types)}}.dump();
	rise_(msg);
}

void Engine::Impl::LoadTypes()
{
	CG::Core::VSystem::Get().Init();

	std::error_code ec;
	if (!type_dir_.empty() && fs::exists(type_dir_, ec)) {
		for (fs::directory_iterator it(type_dir_, ec), end; !ec && it != end; it.increment(ec)) {
			if (!it->is_regular_file(ec) || ec)
				continue;
			const auto ext = it->path().extension().string();
			if (ext != ".dll" && ext != ".DLL")
				continue;
			CG::Core::VSystem::Get().LoadDll(it->path().string());
		}
	}

	PublishTypes();
	PublishFuncs();
}

void Engine::Impl::PublishFuncs()
{
	if (!rise_) return;

	const auto group_of = [](const CG::Core::ACaller& caller) -> const char* {
		switch (caller.GetKind()) {
		case CG::Core::CK_Function: return g_group_function;
		case CG::Core::CK_Script: return g_group_script;
		case CG::Core::CK_Controller: return g_group_controller;
		case CG::Core::CK_Operator: return g_group_operator;
		default: return nullptr;
		}
	};

	std::vector<std::pair<std::string, const char*>> rows;
	for (const auto& caller : CG::Core::VSystem::Get().ListCallers()) {
		if (!caller)
			continue;
		const std::string name = caller->GetName();
		const char* group = group_of(*caller);
		if (!name.empty() && group)
			rows.emplace_back(name, group);
	}
	rows.emplace_back(g_node_exit, g_group_operator);
	std::sort(rows.begin(), rows.end());
	rows.erase(std::unique(rows.begin(), rows.end()), rows.end());

	json funcs = json::array();
	for (const auto& [name, group] : rows)
		funcs.push_back({{g_key_name, name}, {g_key_group, group}, {g_key_key, name}});

	Message msg{};
	msg.act = Act::FuncSync;
	msg.code = 0;
	msg.data = json{{g_key_funcs, std::move(funcs)}}.dump();
	rise_(msg);
}

void Engine::Impl::SetRise(std::function<void(const Message&)> fn)
{
	std::lock_guard lock(mutex_);
	rise_ = std::move(fn);
}

void Engine::Impl::Apply(const Message& msg)
{
	if(auto it = handlers_.find(msg.act); it != handlers_.end())
		it->second(msg);
}

bool Engine::Impl::AddBP(const Message& msg)
{
	bool ok = false;
	do {
		if(!rise_) break;

		json root = json::parse(msg.data, nullptr, false);
		if (root.is_discarded() || !root.is_object()) {
			RiseFail(rise_, msg, "invalid json");
			break;
		}

		std::string name = root.value(g_key_name, std::string{});
		std::string space = root.value(g_key_space, std::string{});
		if (name.empty() || space.empty()) {
			RiseFail(rise_, msg, "name or space empty");
			break;
		}

		std::string key = std::format("{}::{}", space, name);
		if(assets_.contains(key)) {
			RiseFail(rise_, msg, "blueprint exists");
			break;
		}
		if (!IsSafeName(space) || !IsSafeName(name)) {
			RiseFail(rise_, msg, "invalid name");
			break;
		}
		if (root_.empty()) {
			RiseFail(rise_, msg, "asset root missing");
			break;
		}

		const fs::path file = AssetFile(root_, space, name);
		if (Exists(file)) {
			RiseFail(rise_, msg, "blueprint exists");
			break;
		}

		auto asset = std::make_shared<CG::Core::VBlueprint>();
		asset->SetName(name);
		asset->SetSpace(space);
		if (!StoreAsset(file, *asset)) {
			RiseFail(rise_, msg, "write asset failed");
			break;
		}

		root[g_key_guid] = asset->GetGuid();
		root[g_key_path] = ToUtf8(file);

		assets_.emplace(key, std::move(asset));

		Message reply{};
		reply.act = msg.act;
		reply.code = 0;
		reply.token = msg.token;
		reply.data = root.dump();

		rise_(reply);
		ok = true;
	}while(0);
	return ok;
}

bool Engine::Impl::DelBP(const Message& msg)
{
	bool ok = false;
	do {
		if(!rise_) break;

		json root = json::parse(msg.data, nullptr, false);
		if (root.is_discarded() || !root.is_object()) {
			RiseFail(rise_, msg, "invalid json");
			break;
		}

		std::string guid = root.value(g_key_guid, std::string{});
		if (guid.empty()) {
			RiseFail(rise_, msg, "guid empty");
			break;
		}

		auto it = FindAsset(assets_, guid);
		if (it == assets_.end()) {
			RiseFail(rise_, msg, "blueprint not found", guid);
			break;
		}

		const fs::path file = AssetFile(root_, it->second->GetSpace(), it->second->GetName());
		if (!DropAsset(file)) {
			RiseFail(rise_, msg, "remove asset failed", guid);
			break;
		}

		assets_.erase(it);

		Message reply{};
		reply.act = msg.act;
		reply.code = 0;
		reply.token = msg.token;
		reply.data = json{{g_key_guid, guid}}.dump();

		rise_(reply);
		ok = true;
	}while(0);
	return ok;
}

bool Engine::Impl::AltBP(const Message& msg)
{
	bool ok = false;
	do {
		if(!rise_) break;

		json root = json::parse(msg.data, nullptr, false);
		if (root.is_discarded() || !root.is_object()) {
			RiseFail(rise_, msg, "invalid json");
			break;
		}

		std::string guid = root.value(g_key_guid, std::string{});
		std::string name = root.value(g_key_name, std::string{});
		if (guid.empty() || name.empty()) {
			RiseFail(rise_, msg, "guid or name empty", guid);
			break;
		}

		auto it = FindAsset(assets_, guid);
		if (it == assets_.end()) {
			RiseFail(rise_, msg, "blueprint not found", guid);
			break;
		}

		auto asset = it->second;
		const std::string space = asset->GetSpace();
		const std::string key = std::format("{}::{}", space, name);
		if (key != it->first && assets_.contains(key)) {
			RiseFail(rise_, msg, "blueprint exists", guid);
			break;
		}

		if (!IsSafeName(name) || root_.empty()) {
			RiseFail(rise_, msg, root_.empty() ? "asset root missing" : "invalid name", guid);
			break;
		}

		const fs::path file = AssetFile(root_, space, name);
		if (key != it->first) {
			const std::string old_name = asset->GetName();
			const fs::path from = AssetFile(root_, space, old_name);
			asset->SetName(name);
			if (!ReplaceAsset(from, file, *asset)) {
				asset->SetName(old_name);
				RiseFail(rise_, msg, "write asset failed", guid);
				break;
			}
			assets_.erase(it);
			assets_.emplace(key, asset);
		}

		Message reply{};
		reply.act = msg.act;
		reply.code = 0;
		reply.token = msg.token;
		reply.data = json{{g_key_guid, guid}, {g_key_name, name}, {g_key_space, space}, {g_key_path, ToUtf8(file)}}.dump();

		rise_(reply);
		ok = true;
	}while(0);
	return ok;
}

bool Engine::Impl::OpenBP(const Message& msg)
{
	bool ok = false;
	do {
		if(!rise_) break;

		json root = json::parse(msg.data, nullptr, false);
		if (root.is_discarded() || !root.is_object()) {
			RiseFail(rise_, msg, "invalid json");
			break;
		}

		const std::string guid = root.value(g_key_guid, std::string{});
		if (guid.empty()) {
			RiseFail(rise_, msg, "guid empty");
			break;
		}

		auto it = FindAsset(assets_, guid);
		if (it == assets_.end() || !it->second) {
			RiseFail(rise_, msg, "blueprint not found", guid);
			break;
		}

		auto asset = it->second;
		json flows = json::array();
		for (const auto& [id, flow] : asset->GetFlows()) {
			(void)id;
			if (!flow) continue;
			flows.push_back({{g_key_guid, flow->GetGuid()}, {g_key_name, flow->GetName()}});
		}
		json vars = json::array();
		for (const auto& [id, var] : asset->GetVars()) {
			(void)id;
			if (!var) continue;
			json item{{g_key_guid, var->GetGuid()}, {g_key_name, var->GetName()}};
			if (const auto* type = var->GetType())
				item[g_key_type] = type->name_;
			vars.push_back(std::move(item));
		}

		json body{
			{g_key_guid, asset->GetGuid()},
			{g_key_name, asset->GetName()},
			{g_key_space, asset->GetSpace()},
			{g_key_flows, std::move(flows)},
			{g_key_vars, std::move(vars)},
		};
		if (!root_.empty())
			body[g_key_path] = ToUtf8(AssetFile(root_, asset->GetSpace(), asset->GetName()));

		Message reply{};
		reply.act = msg.act;
		reply.code = 0;
		reply.token = msg.token;
		reply.data = body.dump();
		rise_(reply);
		ok = true;
	}while(0);
	return ok;
}

bool Engine::Impl::CloseBP(const Message& msg)
{
	bool ok = false;
	do {
		if(!rise_) break;

		json root = json::parse(msg.data, nullptr, false);
		if (root.is_discarded() || !root.is_object()) {
			RiseFail(rise_, msg, "invalid json");
			break;
		}

		const std::string guid = root.value(g_key_guid, std::string{});
		if (guid.empty()) {
			RiseFail(rise_, msg, "guid empty");
			break;
		}

		auto it = FindAsset(assets_, guid);
		if (it == assets_.end()) {
			RiseFail(rise_, msg, "blueprint not found", guid);
			break;
		}

		Message reply{};
		reply.act = msg.act;
		reply.code = 0;
		reply.token = msg.token;
		reply.data = json{{g_key_guid, guid}}.dump();
		rise_(reply);
		ok = true;
	}while(0);
	return ok;
}

bool Engine::Impl::AddVar(const Message& msg)
{
	bool ok = false;
	do {
		if (!rise_) break;

		json root = json::parse(msg.data, nullptr, false);
		if (root.is_discarded() || !root.is_object()) {
			RiseFail(rise_, msg, "invalid json");
			break;
		}

		const std::string guid = root.value(g_key_guid, std::string{});
		const std::string type_name = root.value(g_key_type, std::string{});
		std::string name = root.value(g_key_name, std::string{});
		if (guid.empty()) {
			RiseFail(rise_, msg, "guid empty");
			break;
		}
		if (type_name.empty()) {
			RiseFail(rise_, msg, "type empty", guid);
			break;
		}

		auto it = FindAsset(assets_, guid);
		if (it == assets_.end() || !it->second) {
			RiseFail(rise_, msg, "blueprint not found", guid);
			break;
		}

		auto asset = it->second;
		if (name.empty())
			name = MakeVarName(*asset);
		if (!IsSafeName(name)) {
			RiseFail(rise_, msg, "invalid name", guid);
			break;
		}
		if (VarNameTaken(*asset, name)) {
			RiseFail(rise_, msg, "variable exists", guid);
			break;
		}

		const std::string self = std::format("{}::{}", asset->GetSpace(), asset->GetName());
		if (type_name == self || !CG::Core::VTypeRegister::Find(type_name)) {
			RiseFail(rise_, msg, "invalid type", guid);
			break;
		}

		const std::string var_guid = asset->AddVar(name, type_name);
		if (var_guid.empty()) {
			RiseFail(rise_, msg, "add variable failed", guid);
			break;
		}

		Message reply{};
		reply.act = msg.act;
		reply.code = 0;
		reply.token = msg.token;
		reply.data = json{{g_key_asset, guid}, {g_key_guid, var_guid}, {g_key_name, name}, {g_key_type, type_name}}.dump();
		rise_(reply);
		ok = true;
	} while (0);
	return ok;
}

bool Engine::Impl::DelVar(const Message& msg)
{
	bool ok = false;
	do {
		if (!rise_) break;

		json root = json::parse(msg.data, nullptr, false);
		if (root.is_discarded() || !root.is_object()) {
			RiseFail(rise_, msg, "invalid json");
			break;
		}

		const std::string guid = root.value(g_key_guid, std::string{});
		if (guid.empty()) {
			RiseFail(rise_, msg, "guid empty");
			break;
		}

		auto owned = FindOwnedVar(assets_, guid);
		if (owned.asset == assets_.end() || !owned.var || !owned.asset->second) {
			RiseFail(rise_, msg, "variable not found", guid);
			break;
		}

		const std::string asset = owned.asset->second->GetGuid();
		owned.asset->second->DelVar(guid);

		Message reply{};
		reply.act = msg.act;
		reply.code = 0;
		reply.token = msg.token;
		reply.data = json{{g_key_asset, asset}, {g_key_guid, guid}}.dump();
		rise_(reply);
		ok = true;
	} while (0);
	return ok;
}

bool Engine::Impl::AltVar(const Message& msg)
{
	bool ok = false;
	do {
		if (!rise_) break;

		json root = json::parse(msg.data, nullptr, false);
		if (root.is_discarded() || !root.is_object()) {
			RiseFail(rise_, msg, "invalid json");
			break;
		}

		const std::string guid = root.value(g_key_guid, std::string{});
		std::string name = root.value(g_key_name, std::string{});
		if (guid.empty() || name.empty()) {
			RiseFail(rise_, msg, "guid or name empty", guid);
			break;
		}

		auto owned = FindOwnedVar(assets_, guid);
		if (owned.asset == assets_.end() || !owned.var || !owned.asset->second) {
			RiseFail(rise_, msg, "variable not found", guid);
			break;
		}

		auto asset = owned.asset->second;
		if (!IsSafeName(name)) {
			RiseFail(rise_, msg, "invalid name", guid);
			break;
		}
		if (VarNameTaken(*asset, name, guid)) {
			RiseFail(rise_, msg, "variable exists", guid);
			break;
		}

		std::string type_name;
		if (const auto* type = owned.var->GetType())
			type_name = type->name_;

		if (root.contains(g_key_type)) {
			const std::string next = root.value(g_key_type, std::string{});
			const std::string self = std::format("{}::{}", asset->GetSpace(), asset->GetName());
			const CG::Core::VType* found = CG::Core::VTypeRegister::Find(next);
			if (next.empty() || next == self || !found) {
				RiseFail(rise_, msg, "invalid type", guid);
				break;
			}
			if (type_name != next) {
				auto variant = std::dynamic_pointer_cast<CG::Core::VVariant>(owned.var);
				if (!variant) {
					RiseFail(rise_, msg, "invalid type", guid);
					break;
				}
				variant->Reset(found);
			}
			type_name = next;
		}

		owned.var->SetName(name);

		Message reply{};
		reply.act = msg.act;
		reply.code = 0;
		reply.token = msg.token;
		reply.data = json{{g_key_asset, asset->GetGuid()}, {g_key_guid, guid}, {g_key_name, name}, {g_key_type, type_name}}.dump();
		rise_(reply);
		ok = true;
	} while (0);
	return ok;
}

bool Engine::Impl::AddFunc(const Message& msg)
{
	bool ok = false;
	do {
		if (!rise_) break;

		json root = json::parse(msg.data, nullptr, false);
		if (root.is_discarded() || !root.is_object()) {
			RiseFail(rise_, msg, "invalid json");
			break;
		}

		const std::string guid = root.value(g_key_guid, std::string{});
		std::string name = root.value(g_key_name, std::string{});
		if (guid.empty()) {
			RiseFail(rise_, msg, "guid empty");
			break;
		}

		auto it = FindAsset(assets_, guid);
		if (it == assets_.end() || !it->second) {
			RiseFail(rise_, msg, "blueprint not found", guid);
			break;
		}

		auto asset = it->second;
		if (name.empty())
			name = MakeFlowName(*asset);
		if (!IsSafeName(name)) {
			RiseFail(rise_, msg, "invalid name", guid);
			break;
		}
		if (FlowNameTaken(*asset, name)) {
			RiseFail(rise_, msg, "function exists", guid);
			break;
		}

		const std::string flow_guid = asset->AddFlow(name);
		if (flow_guid.empty()) {
			RiseFail(rise_, msg, "add function failed", guid);
			break;
		}

		auto flow = asset->FindFlow(flow_guid);
		auto graph = FlowGraph(flow);
		const char* error = nullptr;
		auto entry = graph ? AttachTerm(*graph, g_node_entry, 0, 0, error) : nullptr;
		if (!entry) {
			asset->DelFlow(flow_guid);
			RiseFail(rise_, msg, error ? error : "add entry failed", guid);
			break;
		}

		Message reply{};
		reply.act = msg.act;
		reply.code = 0;
		reply.token = msg.token;
		reply.data = json{{g_key_asset, guid}, {g_key_guid, flow_guid}, {g_key_name, name}, {g_key_node, TermNodeJson(*entry)}}.dump();
		rise_(reply);
		ok = true;
	} while (0);
	return ok;
}

bool Engine::Impl::DelFunc(const Message& msg)
{
	bool ok = false;
	do {
		if (!rise_) break;

		json root = json::parse(msg.data, nullptr, false);
		if (root.is_discarded() || !root.is_object()) {
			RiseFail(rise_, msg, "invalid json");
			break;
		}

		const std::string guid = root.value(g_key_guid, std::string{});
		if (guid.empty()) {
			RiseFail(rise_, msg, "guid empty");
			break;
		}

		auto owned = FindOwnedFlow(assets_, guid);
		if (owned.asset == assets_.end() || !owned.flow || !owned.asset->second) {
			RiseFail(rise_, msg, "function not found", guid);
			break;
		}

		const std::string asset = owned.asset->second->GetGuid();
		owned.asset->second->DelFlow(guid);

		Message reply{};
		reply.act = msg.act;
		reply.code = 0;
		reply.token = msg.token;
		reply.data = json{{g_key_asset, asset}, {g_key_guid, guid}}.dump();
		rise_(reply);
		ok = true;
	} while (0);
	return ok;
}

bool Engine::Impl::AltFunc(const Message& msg)
{
	bool ok = false;
	do {
		if (!rise_) break;

		json root = json::parse(msg.data, nullptr, false);
		if (root.is_discarded() || !root.is_object()) {
			RiseFail(rise_, msg, "invalid json");
			break;
		}

		const std::string guid = root.value(g_key_guid, std::string{});
		const std::string name = root.value(g_key_name, std::string{});
		if (guid.empty() || name.empty()) {
			RiseFail(rise_, msg, "guid or name empty", guid);
			break;
		}

		auto owned = FindOwnedFlow(assets_, guid);
		if (owned.asset == assets_.end() || !owned.flow || !owned.asset->second) {
			RiseFail(rise_, msg, "function not found", guid);
			break;
		}

		auto asset = owned.asset->second;
		if (!IsSafeName(name)) {
			RiseFail(rise_, msg, "invalid name", guid);
			break;
		}
		if (FlowNameTaken(*asset, name, guid)) {
			RiseFail(rise_, msg, "function exists", guid);
			break;
		}

		owned.flow->SetName(name);
		if (auto script = owned.flow->GetScript())
			script->SetName(name);

		Message reply{};
		reply.act = msg.act;
		reply.code = 0;
		reply.token = msg.token;
		reply.data = json{{g_key_asset, asset->GetGuid()}, {g_key_guid, guid}, {g_key_name, name}}.dump();
		rise_(reply);
		ok = true;
	} while (0);
	return ok;
}

bool Engine::Impl::AddNode(const Message& msg)
{
	bool ok = false;
	do {
		if (!rise_) break;

		json root = json::parse(msg.data, nullptr, false);
		if (root.is_discarded() || !root.is_object()) {
			RiseFail(rise_, msg, "invalid json");
			break;
		}

		const std::string guid = root.value(g_key_guid, std::string{});
		const std::string key = root.value(g_key_key, std::string{});
		if (guid.empty() || key.empty()) {
			RiseFail(rise_, msg, "guid empty");
			break;
		}

		auto owned = FindOwnedFlow(assets_, guid);
		auto graph = FlowGraph(owned.flow);
		if (owned.asset == assets_.end() || !owned.asset->second || !graph) {
			RiseFail(rise_, msg, "function not found", guid);
			break;
		}

		const double x = root.value(g_key_x, 0.0);
		const double y = root.value(g_key_y, 0.0);
		std::shared_ptr<CG::Core::VNode> node;
		if (key == g_node_entry || key == g_node_exit) {
			const char* error = nullptr;
			node = AttachTerm(*graph, key.c_str(), x, y, error);
			if (!node) {
				RiseFail(rise_, msg, error ? error : "add node failed", guid);
				break;
			}
		} else {
			std::shared_ptr<CG::Core::ACaller> caller = CG::Core::VSystem::Get().FindCaller(key);
			if (!caller) {
				auto source = FindOwnedFlow(assets_, key);
				if (source.asset == owned.asset && source.flow)
					caller = source.flow->GetScript();
			}
			if (!caller) {
				RiseFail(rise_, msg, "invalid node", guid);
				break;
			}
			node = CG::Core::VNode::Make(caller);
			if (!node) {
				RiseFail(rise_, msg, "add node failed", guid);
				break;
			}
			node->SetGuid(GenerateGUID());
			node->SetName(caller->GetName());
			node->SetCoord(Vec2f{static_cast<float>(x), static_cast<float>(y)});
			graph->AddNode(node);
		}

		json body = TermNodeJson(*node);
		body[g_key_asset] = owned.asset->second->GetGuid();
		body[g_key_flow] = guid;
		if (key == g_node_entry || key == g_node_exit)
			body[g_key_key] = key;

		Message reply{};
		reply.act = msg.act;
		reply.code = 0;
		reply.token = msg.token;
		reply.data = body.dump();
		rise_(reply);
		ok = true;
	} while (0);
	return ok;
}

bool Engine::Impl::DelNode(const Message& msg)
{
	bool ok = false;
	do {
		if (!rise_) break;

		json root = json::parse(msg.data, nullptr, false);
		if (root.is_discarded() || !root.is_object()) {
			RiseFail(rise_, msg, "invalid json");
			break;
		}

		const std::string guid = root.value(g_key_guid, std::string{});
		if (guid.empty()) {
			RiseFail(rise_, msg, "guid empty");
			break;
		}

		auto owned = FindOwnedNode(assets_, guid);
		if (owned.asset == assets_.end() || !owned.asset->second || !owned.flow || !owned.graph || !owned.node) {
			RiseFail(rise_, msg, "node not found", guid);
			break;
		}

		if (auto entry = owned.graph->GetEntry(); entry && entry->GetGuid() == guid) {
			RiseFail(rise_, msg, "entry cannot be deleted", guid);
			break;
		}

		const std::string key = owned.node->GetName();
		if (auto exit = owned.graph->GetExit(); exit && exit->GetGuid() == guid)
			owned.graph->SetExit({});
		owned.graph->DelNode(guid);

		Message reply{};
		reply.act = msg.act;
		reply.code = 0;
		reply.token = msg.token;
		reply.data = json{
			{g_key_asset, owned.asset->second->GetGuid()},
			{g_key_flow, owned.flow->GetGuid()},
			{g_key_guid, guid},
			{g_key_key, key},
		}.dump();
		rise_(reply);
		ok = true;
	} while (0);
	return ok;
}

bool Engine::Impl::AltNode(const Message& msg)
{
	bool ok = false;
	do {
		if (!rise_) break;

		json root = json::parse(msg.data, nullptr, false);
		if (root.is_discarded() || !root.is_object()) {
			RiseFail(rise_, msg, "invalid json");
			break;
		}

		const std::string guid = root.value(g_key_guid, std::string{});
		if (guid.empty() || !root.contains(g_key_x) || !root.contains(g_key_y)) {
			RiseFail(rise_, msg, "guid empty", guid);
			break;
		}

		auto owned = FindOwnedNode(assets_, guid);
		auto node = std::dynamic_pointer_cast<CG::Core::VNode>(owned.node);
		if (owned.asset == assets_.end() || !owned.asset->second || !owned.flow || !node) {
			RiseFail(rise_, msg, "node not found", guid);
			break;
		}

		const double x = root.value(g_key_x, 0.0);
		const double y = root.value(g_key_y, 0.0);
		node->SetCoord(Vec2f{static_cast<float>(x), static_cast<float>(y)});

		Message reply{};
		reply.act = msg.act;
		reply.code = 0;
		reply.token = msg.token;
		reply.data = json{
			{g_key_asset, owned.asset->second->GetGuid()},
			{g_key_flow, owned.flow->GetGuid()},
			{g_key_guid, guid},
			{g_key_x, node->GetCoord().x},
			{g_key_y, node->GetCoord().y},
		}.dump();
		rise_(reply);
		ok = true;
	} while (0);
	return ok;
}

bool Engine::Impl::Compile(const Message& msg)
{
	json root = json::parse(msg.data, nullptr, false);
	if (root.is_discarded() || !root.is_object())
		return false;

	const std::string dll = root.value(g_key_dll, std::string{});
	if (dll.empty())
		return false;

	if (!CG::Core::VSystem::Get().LoadDll(dll)) {
		RiseFail(rise_, msg, "load type failed");
		return false;
	}

	PublishTypes();
	return true;
}

}
