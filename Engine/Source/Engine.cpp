#include "Engine.h"
#include "Abstract/ACaller.h"
#include "Abstract/AFlow.h"
#include "Abstract/APin.h"
#include "Abstract/AScript.h"
#include "Abstract/AVariant.h"
#include "Base/VSystem.h"
#include "Base/VConvert.h"
#include "Base/VType.h"
#include "Base/VClass.h"
#include "Realistic/VAsset.h"
#include "Realistic/VFlow.h"
#include "Realistic/VGraph.h"
#include "Realistic/VNode.h"
#include "Realistic/VOperator.h"
#include "Realistic/VScript.h"
#include "Realistic/VVariant.h"
#include "Realistic/VVarFunction.h"
#include "Realistic/VFunction.h"
#include "Realistic/VLine.h"
#include "Base/VGuid.h"
#include "Base/VRegister.h"
#include "core_paths.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <format>
#include <fstream>
#include <sstream>
#include <mutex>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <tuple>
#include <nlohmann/json.hpp>
#ifdef _WIN32
#include <Windows.h>
#endif
using json = nlohmann::json;

namespace cg::engine {

namespace {

// 与 MainWindow::Action 同值，命令不在这里再定义一套。
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

void RiseOutput(const std::function<void(const Message&)>& rise, const Message& request,
	const std::string& text, const std::string& source, const std::string& level = "info")
{
	if (!rise) return;
	Message output{};
	output.act = Action::LogOutput;
	output.token = request.token;
	output.data = json{{g_key_text, text}, {g_key_source, source}, {g_key_level, level}}.dump();
	rise(output);
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

bool IsSafeIdentifier(const std::string& text)
{
	if (text.empty() || !(std::isalpha(static_cast<unsigned char>(text.front())) || text.front() == '_'))
		return false;
	return std::all_of(text.begin() + 1, text.end(), [](unsigned char ch) {
		return std::isalnum(ch) || ch == '_';
	});
}

bool ParseVarNodeKey(const std::string& key, bool& setter, std::string& guid)
{
	const std::string get_prefix = std::string(g_node_get_var) + ":";
	const std::string set_prefix = std::string(g_node_set_var) + ":";
	if (key.rfind(get_prefix, 0) == 0) {
		setter = false;
		guid = key.substr(get_prefix.size());
		return !guid.empty();
	}
	if (key.rfind(set_prefix, 0) == 0) {
		setter = true;
		guid = key.substr(set_prefix.size());
		return !guid.empty();
	}
	return false;
}

std::string ClassMethodKey(const CG::Core::VClass& owner, const std::string& method)
{
	return std::format("{}:{}::{}::{}", g_node_class_method, owner.GetSpace(), owner.GetName(), method);
}

bool ParseClassMethodKey(const std::string& key, std::string& owner, std::string& method)
{
	const std::string prefix = std::string(g_node_class_method) + ":";
	if (key.rfind(prefix, 0) != 0)
		return false;
	const std::string value = key.substr(prefix.size());
	const size_t split = value.rfind("::");
	if (split == std::string::npos || split == 0 || split + 2 >= value.size())
		return false;
	owner = value.substr(0, split);
	method = value.substr(split + 2);
	return true;
}

std::shared_ptr<CG::Core::VFunction> FindClassMethod(const std::string& key)
{
	std::string owner_name, method_name;
	if (!ParseClassMethodKey(key, owner_name, method_name))
		return {};
	auto* owner = CG::Core::VSystem::Get().FindClass(owner_name);
	if (!owner)
		return {};
	return std::dynamic_pointer_cast<CG::Core::VFunction>(owner->GetMethod(method_name));
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

fs::path LatestBlueprintDll(const fs::path& generated_dir, const std::string& config)
{
	const fs::path build_root = generated_dir / "build";
	std::error_code ec;
	fs::path latest_dll;
	fs::file_time_type latest_time{};
	for (fs::directory_iterator build(build_root, ec), end; !ec && build != end; build.increment(ec)) {
		const fs::path config_dir = build->path() / FromUtf8(config);
		std::error_code config_ec;
		for (fs::directory_iterator file(config_dir, config_ec), file_end; !config_ec && file != file_end; file.increment(config_ec)) {
			if (!file->is_regular_file(config_ec) || config_ec)
				continue;
			const std::string filename = ToUtf8(file->path().filename());
			if (file->path().extension() != ".dll" || filename.rfind("BlueprintRuntime", 0) != 0)
				continue;
			const auto modified = fs::last_write_time(file->path(), config_ec);
			if (!config_ec && (latest_dll.empty() || modified > latest_time)) {
				latest_dll = file->path();
				latest_time = modified;
			}
		}
	}
	return latest_dll;
}

std::pair<fs::path, fs::path> LatestBlueprintLinkArtifacts(const fs::path& generated_dir,
	const std::string& config)
{
	const fs::path build_root = generated_dir / "build";
	std::error_code ec;
	fs::path latest_dll, latest_lib;
	fs::file_time_type latest_time{};
	for (fs::directory_iterator build(build_root, ec), end; !ec && build != end; build.increment(ec)) {
		const fs::path config_dir = build->path() / FromUtf8(config);
		std::error_code config_ec;
		for (fs::directory_iterator file(config_dir, config_ec), file_end; !config_ec && file != file_end; file.increment(config_ec)) {
			if (!file->is_regular_file(config_ec) || config_ec || file->path().extension() != ".dll")
				continue;
			const std::string filename = ToUtf8(file->path().filename());
			if (filename.rfind("BlueprintRuntime_", 0) != 0)
				continue;
			fs::path lib = file->path();
			lib.replace_extension(".lib");
			std::error_code lib_ec;
			if (!fs::is_regular_file(lib, lib_ec) || lib_ec)
				continue;
			const auto modified = fs::last_write_time(file->path(), config_ec);
			if (!config_ec && (latest_dll.empty() || modified > latest_time)) {
				latest_dll = file->path();
				latest_lib = std::move(lib);
				latest_time = modified;
			}
		}
	}
	return {latest_dll, latest_lib};
}

std::set<std::string> BlueprintTypeHeaders(const CG::Core::VBlueprint& asset)
{
	std::set<std::string> headers;
	auto add_type = [&](const CG::Core::VType* type) {
		if (type && !type->header_.empty())
			headers.insert(type->header_);
	};
	for (const auto& [guid, var] : asset.GetVars()) {
		(void)guid;
		if (var)
			add_type(var->GetType());
	}
	for (const auto& [guid, flow_base] : asset.GetFlows()) {
		(void)guid;
		if (!flow_base)
			continue;
		for (const auto& param : flow_base->GetFlowMeta().params)
			add_type(param.type);
		if (auto graph = flow_base->GetGraph()) {
			for (const auto& [node_guid, node_base] : graph->GetNodes()) {
				(void)node_guid;
				auto node = std::dynamic_pointer_cast<CG::Core::VNode>(node_base);
				if (!node || !node->GetCaller())
					continue;
				for (const auto& arg : node->GetCaller()->GetCallMeta().args)
					add_type(arg.type);
			}
		}
	}
	return headers;
}

bool WriteText(const fs::path& file, const std::string& text)
{
	std::ofstream out(file, std::ios::binary | std::ios::trunc);
	if (!out) return false;
	out.write(text.data(), static_cast<std::streamsize>(text.size()));
	return static_cast<bool>(out);
}

#ifdef _WIN32
std::wstring QuoteWindowsArg(const std::wstring& arg)
{
	std::wstring quoted = L"\"";
	size_t slashes = 0;
	for (const wchar_t ch : arg) {
		if (ch == L'\\') {
			++slashes;
			continue;
		}
		if (ch == L'\"') {
			quoted.append(slashes * 2 + 1, L'\\');
			quoted.push_back(ch);
			slashes = 0;
			continue;
		}
		quoted.append(slashes, L'\\');
		slashes = 0;
		quoted.push_back(ch);
	}
	quoted.append(slashes * 2, L'\\');
	quoted.push_back(L'\"');
	return quoted;
}

bool RunProcess(const fs::path& executable, const std::vector<std::wstring>& args,
	const fs::path& working_dir, const fs::path& log_file, DWORD& exit_code)
{
	SECURITY_ATTRIBUTES security{};
	security.nLength = sizeof(security);
	security.bInheritHandle = TRUE;
	HANDLE log = CreateFileW(log_file.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
		&security, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (log == INVALID_HANDLE_VALUE) return false;
	std::wstring command = QuoteWindowsArg(executable.native());
	for (const auto& arg : args) {
		command.push_back(L' ');
		command += QuoteWindowsArg(arg);
	}
	STARTUPINFOW startup{};
	startup.cb = sizeof(startup);
	startup.dwFlags = STARTF_USESTDHANDLES;
	startup.hStdOutput = log;
	startup.hStdError = log;
	startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
	PROCESS_INFORMATION process{};
	const BOOL started = CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, TRUE,
		CREATE_NO_WINDOW, nullptr, working_dir.c_str(), &startup, &process);
	CloseHandle(log);
	if (!started) return false;
	WaitForSingleObject(process.hProcess, INFINITE);
	const BOOL got_exit_code = GetExitCodeProcess(process.hProcess, &exit_code);
	CloseHandle(process.hThread);
	CloseHandle(process.hProcess);
	return got_exit_code != FALSE;
}
#endif

std::string ReadText(const fs::path& file)
{
	std::ifstream in(file, std::ios::binary);
	if (!in) return {};
	return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

std::string NodeKey(const CG::Core::VBlueprint& asset, const CG::Core::AFlow& flow,
	const CG::Core::VNode& node)
{
	auto caller = node.GetCaller();
	if (!caller)
		return {};
	if (caller->GetName() == g_node_entry || caller->GetName() == g_node_exit)
		return caller->GetName();
	if (auto get = std::dynamic_pointer_cast<CG::Core::FGetVar>(caller))
		return std::string(g_node_get_var) + ":" + (get->GetVar() ? get->GetVar()->GetGuid() : std::string{});
	if (auto set = std::dynamic_pointer_cast<CG::Core::FSetVar>(caller))
		return std::string(g_node_set_var) + ":" + (set->GetVar() ? set->GetVar()->GetGuid() : std::string{});
	if (auto function = std::dynamic_pointer_cast<CG::Core::VFunction>(caller)) {
		if (const auto* owner = function->GetFuncMeta().owner)
			return ClassMethodKey(*owner, function->GetName());
	}
	if (auto script = std::dynamic_pointer_cast<CG::Core::VScript>(caller)) {
		for (const auto& [guid, candidate] : asset.GetFlows()) {
			if (candidate && candidate.get() == &flow && candidate->GetScript() == script)
				return guid;
		}
	}
	return caller->GetName();
}

json TermNodeJson(const CG::Core::VNode& node);

json FunctionTypeTags(const CG::Core::CallMeta& call)
{
	std::vector<std::string> input_types;
	std::vector<std::string> output_types;
	for (const auto& arg : call.args) {
		if (!arg.is_runtime || !arg.type || arg.type->name_.empty())
			continue;
		if (arg.is_output)
			output_types.push_back(arg.type->name_);
		else
			input_types.push_back(arg.type->name_);
	}
	return {
		{g_key_input_types, std::move(input_types)},
		{g_key_output_types, std::move(output_types)},
	};
}

bool AcceptsInputType(const CG::Core::CallMeta& call, const std::string& type_name)
{
	if (type_name.empty())
		return false;
	return std::any_of(call.args.begin(), call.args.end(), [&](const auto& arg) {
		return arg.is_runtime && !arg.is_output && arg.type
			&& arg.type->name_ == type_name;
	});
}

json FunctionMenuRow(const std::string& name, const std::string& key,
	const CG::Core::CallMeta& call)
{
	json row = {
		{g_key_name, name},
		{g_key_key, key},
	};
	row.update(FunctionTypeTags(call));
	return row;
}

json StoredNode(const CG::Core::VBlueprint& asset, const CG::Core::AFlow& flow,
	const std::shared_ptr<CG::Core::VNode>& node)
{
	if (!node)
		return {};
	json body = TermNodeJson(*node);
	body[g_key_key] = NodeKey(asset, flow, *node);
	return body;
}

json StoredLine(const std::shared_ptr<CG::Core::ALine>& line)
{
	if (!line || !line->GetFrom() || !line->GetTo()) return {};
	const auto from_node = line->GetFrom()->GetOwner();
	const auto to_node = line->GetTo()->GetOwner();
	if (!from_node || !to_node) return {};
	return {
		{g_key_guid, line->GetGuid()},
		{g_key_from_node, from_node->GetGuid()},
		{g_key_from_pin, line->GetFrom()->GetGuid()},
		{g_key_to_node, to_node->GetGuid()},
		{g_key_to_pin, line->GetTo()->GetGuid()},
	};
}

json VariableRows(const CG::Core::VBlueprint& asset)
{
	json rows = json::array();
	for (const auto& [guid, var] : asset.GetVars()) {
		(void)guid;
		if (!var) continue;
		rows.push_back({{g_key_guid, var->GetGuid()}, {g_key_value, CG::Core::ToString(var->Getter())}});
	}
	return rows;
}

bool StoreAsset(const fs::path& file, const CG::Core::VBlueprint& asset)
{
	std::error_code ec;
	fs::create_directories(file.parent_path(), ec);
	if (ec)
		return false;

	json body = {
		{g_key_vars, json::array()},
		{g_key_flows, json::array()},
	};
	for (const auto& [guid, var] : asset.GetVars()) {
		if (!var)
			continue;
		std::string type;
		if (const auto* type_info = var->GetType())
			type = type_info->name_;
		body[g_key_vars].push_back({
			{g_key_guid, var->GetGuid()},
			{g_key_name, var->GetName()},
			{g_key_type, type},
			{g_key_kind, var->GetID()},
			{g_key_value, CG::Core::ToString(var->Getter())},
		});
	}
	for (const auto& [guid, flow_base] : asset.GetFlows()) {
		auto flow = std::dynamic_pointer_cast<CG::Core::VFlow>(flow_base);
		if (!flow)
			continue;
		json row = {
			{g_key_guid, flow->GetGuid()},
			{g_key_name, flow->GetName()},
			{g_key_nodes, json::array()},
			{g_key_lines, json::array()},
		};
		if (auto graph = std::dynamic_pointer_cast<CG::Core::VGraph>(flow->GetGraph())) {
			for (const auto& [node_guid, node_base] : graph->GetNodes()) {
				auto node = std::dynamic_pointer_cast<CG::Core::VNode>(node_base);
				if (!node) continue;
				json stored = StoredNode(asset, *flow, node);
				for (auto& pin : stored[g_key_pins])
					pin.erase(g_key_methods);
				row[g_key_nodes].push_back(std::move(stored));
			}
			for (const auto& [line_guid, line] : graph->GetLines()) {
				(void)line_guid;
				if (auto stored = StoredLine(line); !stored.empty())
					row[g_key_lines].push_back(std::move(stored));
			}
		}
		body[g_key_flows].push_back(std::move(row));
	}

	json doc = {
		{g_key_guid, asset.GetGuid()},
		{g_key_name, asset.GetName()},
		{g_key_space, asset.GetSpace()},
		{g_key_body, std::move(body)},
	};

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
	if (asset.GetName() != "NewFunc" && !FlowNameTaken(asset, "NewFunc"))
		return "NewFunc";
	for (int i = 1; i < 10000; ++i) {
		const std::string name = std::format("NewFunc{}", i);
		if (name != asset.GetName() && !FlowNameTaken(asset, name))
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

struct OwnedLine {
	AssetMap::iterator asset;
	std::shared_ptr<CG::Core::AFlow> flow;
	std::shared_ptr<CG::Core::VGraph> graph;
	std::shared_ptr<CG::Core::ALine> line;
};

std::shared_ptr<CG::Core::VGraph> FlowGraph(const std::shared_ptr<CG::Core::AFlow>& flow)
{
	if (!flow)
		return {};
	return std::dynamic_pointer_cast<CG::Core::VGraph>(flow->GetGraph());
}

std::shared_ptr<CG::Core::APin> FindNodePin(const std::shared_ptr<CG::Core::ANode>& node, const std::string& key)
{
	if (!node || key.empty()) return {};
	if (auto pin = node->GetPin(key)) return pin;
	const auto vnode = std::dynamic_pointer_cast<CG::Core::VNode>(node);
	if (!vnode) return {};
	for (const auto& pin : vnode->GetInputPins())
		if (pin && pin->GetPinMeta().name == key) return pin;
	for (const auto& pin : vnode->GetOutputPins())
		if (pin && pin->GetPinMeta().name == key) return pin;
	return {};
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

OwnedLine FindOwnedLine(AssetMap& assets, const std::string& guid)
{
	for (auto it = assets.begin(); it != assets.end(); ++it) {
		if (!it->second) continue;
		for (const auto& [id, flow] : it->second->GetFlows()) {
			(void)id;
			auto graph = FlowGraph(flow);
			if (!graph) continue;
			if (auto line = graph->FindLine(guid)) return {it, flow, graph, line};
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
		json pin_row = {
			{g_key_guid, pin->GetGuid()},
			{g_key_name, meta.name},
			{g_key_kind, static_cast<int>(meta.kind)},
			{g_key_direct, static_cast<int>(pin->GetDirect())},
			{g_key_type, meta.type ? meta.type->name_ : std::string{}},
			{g_key_value, meta.value},
			{g_key_connected, !pin->GetLines().empty()},
		};
		if (meta.kind == CG::Core::PinKind::Data && pin->GetDirect() == CG::Core::EDirect::Out && meta.type) {
			json methods = json::array();
			std::unordered_set<std::string> method_keys;
			auto* type_class = CG::Core::VSystem::Get().FindClass(meta.type->name_);
			if (type_class) {
				for (const auto& method : type_class->GetMethods()) {
					auto function = std::dynamic_pointer_cast<CG::Core::VFunction>(method);
					if (!function || function->GetFuncMeta().owner != type_class)
						continue;
					const auto call = function->GetCallMeta();
					const auto self = std::find_if(call.args.begin(), call.args.end(), [](const auto& arg) {
						return arg.name == "self";
					});
					if (self == call.args.end() || !self->type || self->type->name_ != meta.type->name_)
						continue;
					const std::string key = ClassMethodKey(*type_class, function->GetName());
					method_keys.insert(key);
					methods.push_back(FunctionMenuRow(function->GetName(), key, call));
				}
			}
			for (const auto& caller : CG::Core::VSystem::Get().ListCallers()) {
				if (!caller || caller->GetName().empty())
					continue;
				const auto call = caller->GetCallMeta();
				const std::string key = caller->GetName();
				if (!AcceptsInputType(call, meta.type->name_) || method_keys.contains(key))
					continue;
				method_keys.insert(key);
				methods.push_back(FunctionMenuRow(caller->GetName(), key, call));
			}
			pin_row[g_key_methods] = std::move(methods);
		}
		pins.push_back(std::move(pin_row));
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

void RestorePinGuids(const std::shared_ptr<CG::Core::VNode>& node, const json& rows)
{
	if (!node || !rows.is_array()) return;
	std::vector<std::shared_ptr<CG::Core::APin>> pins;
	for (const auto& pin : node->GetInputPins()) pins.push_back(pin);
	for (const auto& pin : node->GetOutputPins()) pins.push_back(pin);
	for (size_t i = 0; i < pins.size() && i < rows.size(); ++i) {
		if (pins[i] && rows[i].is_object()) {
			const auto guid = rows[i].value(g_key_guid, std::string{});
			if (!guid.empty()) pins[i]->SetGuid(guid);
			auto meta = pins[i]->GetPinMeta();
			if (meta.kind == CG::Core::PinKind::Data && pins[i]->GetDirect() == CG::Core::EDirect::In) {
				meta.value = rows[i].value(g_key_value, rows[i].value(g_key_legacy_pin_default_value, meta.value));
				pins[i]->SetPinMeta(meta);
				if (auto frame = pins[i]->GetFrame()) {
					if (auto value = CG::Core::FromString(meta.value, meta.type))
						frame->Update(value);
				}
			}
		}
	}
}

struct VariableNodeSnapshot {
	std::shared_ptr<CG::Core::AFlow> flow;
	std::shared_ptr<CG::Core::VGraph> graph;
	std::shared_ptr<CG::Core::VNode> node;
	json pins;
};

struct VariableLineSnapshot {
	std::shared_ptr<CG::Core::VGraph> graph;
	std::string guid;
	std::shared_ptr<CG::Core::APin> from;
	std::shared_ptr<CG::Core::APin> to;
};

struct VariableGraphSnapshot {
	std::vector<VariableNodeSnapshot> nodes;
	std::vector<VariableLineSnapshot> lines;
};

VariableGraphSnapshot SnapshotVariableNodes(const std::shared_ptr<CG::Core::VBlueprint>& asset,
	const std::string& var_guid)
{
	VariableGraphSnapshot snapshot;
	if (!asset || var_guid.empty()) return snapshot;

	for (const auto& [flow_guid, flow] : asset->GetFlows()) {
		(void)flow_guid;
		auto graph = FlowGraph(flow);
		if (!flow || !graph) continue;
		std::unordered_set<std::string> node_guids;
		for (const auto& [node_guid, node_base] : graph->GetNodes()) {
			auto node = std::dynamic_pointer_cast<CG::Core::VNode>(node_base);
			auto caller = node ? std::dynamic_pointer_cast<CG::Core::AVarFunction>(node->GetCaller()) : nullptr;
			auto var = caller ? caller->GetVar() : nullptr;
			if (!var || var->GetGuid() != var_guid) continue;
			node_guids.insert(node_guid);
			snapshot.nodes.push_back({flow, graph, node, TermNodeJson(*node).value(g_key_pins, json::array())});
		}
		for (const auto& [line_guid, line] : graph->GetLines()) {
			if (!line || !line->GetFrom() || !line->GetTo()) continue;
			auto from_node = line->GetFrom()->GetOwner();
			auto to_node = line->GetTo()->GetOwner();
			if ((from_node && node_guids.contains(from_node->GetGuid()))
				|| (to_node && node_guids.contains(to_node->GetGuid())))
				snapshot.lines.push_back({graph, line_guid, line->GetFrom(), line->GetTo()});
		}
	}
	return snapshot;
}

void RemoveVariableGraph(const VariableGraphSnapshot& snapshot)
{
	for (const auto& line : snapshot.lines) {
		if (auto existing = line.graph->FindLine(line.guid)) {
			CG::Core::DisconnectLine(existing);
			line.graph->DelLine(line.guid);
		}
	}
	for (const auto& node : snapshot.nodes)
		node.graph->DelNode(node.node->GetGuid());
}

void RestoreVariableGraph(const VariableGraphSnapshot& snapshot)
{
	for (const auto& node : snapshot.nodes)
		node.graph->AddNode(node.node);
	for (const auto& line : snapshot.lines) {
		auto restored = CG::Core::MakeLine(line.from, line.to);
		if (!restored) continue;
		restored->SetGuid(line.guid);
		line.graph->AddLine(restored);
	}
}

bool RebuildVariableNodes(const VariableGraphSnapshot& snapshot,
	const std::shared_ptr<CG::Core::AVariant>& var,
	std::vector<std::shared_ptr<CG::Core::VNode>>& replacements,
	std::vector<VariableLineSnapshot>& removed_lines)
{
	if (!var) return false;
	std::unordered_map<std::string, std::shared_ptr<CG::Core::VNode>> by_guid;
	for (const auto& saved : snapshot.nodes) {
		auto old_caller = saved.node->GetCaller();
		std::shared_ptr<CG::Core::ANode> replacement_base;
		if (std::dynamic_pointer_cast<CG::Core::FGetVar>(old_caller))
			replacement_base = CG::Core::MakeGetVarNode(var);
		else if (std::dynamic_pointer_cast<CG::Core::FSetVar>(old_caller))
			replacement_base = CG::Core::MakeSetVarNode(var);
		auto replacement = std::dynamic_pointer_cast<CG::Core::VNode>(replacement_base);
		if (!replacement) return false;
		replacement->SetGuid(saved.node->GetGuid());
		replacement->SetCoord(saved.node->GetCoord());
		RestorePinGuids(replacement, saved.pins);
		by_guid.emplace(replacement->GetGuid(), replacement);
		replacements.push_back(std::move(replacement));
	}

	RemoveVariableGraph(snapshot);
	for (size_t i = 0; i < snapshot.nodes.size(); ++i)
		snapshot.nodes[i].graph->AddNode(replacements[i]);

	for (const auto& line : snapshot.lines) {
		auto from_owner = line.from ? line.from->GetOwner() : nullptr;
		auto to_owner = line.to ? line.to->GetOwner() : nullptr;
		auto from_node = from_owner && by_guid.contains(from_owner->GetGuid()) ? by_guid.at(from_owner->GetGuid()) : nullptr;
		auto to_node = to_owner && by_guid.contains(to_owner->GetGuid()) ? by_guid.at(to_owner->GetGuid()) : nullptr;
		auto from_pin = from_node ? from_node->GetPin(line.from->GetGuid()) : line.from;
		auto to_pin = to_node ? to_node->GetPin(line.to->GetGuid()) : line.to;
		auto rebuilt = CG::Core::MakeLine(from_pin, to_pin);
		if (!rebuilt) {
			removed_lines.push_back(line);
			continue;
		}
		rebuilt->SetGuid(line.guid);
		line.graph->AddLine(rebuilt);
	}
	return true;
}

void RemoveRebuiltVariableGraph(const VariableGraphSnapshot& snapshot,
	const std::vector<std::shared_ptr<CG::Core::VNode>>& replacements)
{
	for (const auto& line : snapshot.lines) {
		if (auto existing = line.graph->FindLine(line.guid)) {
			CG::Core::DisconnectLine(existing);
			line.graph->DelLine(line.guid);
		}
	}
	for (size_t i = 0; i < snapshot.nodes.size() && i < replacements.size(); ++i)
		snapshot.nodes[i].graph->DelNode(replacements[i]->GetGuid());
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
	void Load();
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
	bool AddLine(const Message& msg);
	bool DelLine(const Message& msg);
	bool Compile(const Message& msg);
	bool RunFunc(const Message& msg);
	void PublishTypes();
	void PublishFuncs();
	void LoadAssets();
	void PublishAssets();
	bool Persist(const std::shared_ptr<CG::Core::VBlueprint>& asset);

	std::mutex mutex_;
	fs::path root_;
	fs::path type_dir_;
	std::function<void(const Message&)> rise_;
	std::unordered_map<std::string, std::shared_ptr<CG::Core::VBlueprint>> assets_;
	std::unordered_map<Action, std::function<void(const Message&)>> handlers_;
};

Engine::Engine() : impl_(std::make_unique<Impl>()) {
	impl_->Init();
}
Engine::~Engine() = default;

void Engine::Apply(const Message& msg) { impl_->Apply(msg); }
void Engine::SetRoot(const std::string& root) { impl_->SetRoot(root); }
void Engine::SetTypeDir(const std::string& dir) { impl_->SetTypeDir(dir); }
void Engine::Load() { impl_->Load(); }
void Engine::SetRise(std::function<void(const Message&)> rise) { impl_->SetRise(std::move(rise)); }

void Engine::Impl::Init()
{
	handlers_.insert({ Action::AddBP, [this](const Message& msg) { AddBP(msg); } });
	handlers_.insert({ Action::DelBP, [this](const Message& msg) { DelBP(msg); } });
	handlers_.insert({ Action::AltBP, [this](const Message& msg) { AltBP(msg); } });
	handlers_.insert({ Action::OpenBP, [this](const Message& msg) { OpenBP(msg); } });
	handlers_.insert({ Action::CloseBP, [this](const Message& msg) { CloseBP(msg); } });
	handlers_.insert({ Action::Compile, [this](const Message& msg) { Compile(msg); } });
	handlers_.insert({ Action::RunFunc, [this](const Message& msg) { RunFunc(msg); } });
	handlers_.insert({ Action::AddVar, [this](const Message& msg) { AddVar(msg); } });
	handlers_.insert({ Action::DelVar, [this](const Message& msg) { DelVar(msg); } });
	handlers_.insert({ Action::AltVar, [this](const Message& msg) { AltVar(msg); } });
	handlers_.insert({ Action::AddFunc, [this](const Message& msg) { AddFunc(msg); } });
	handlers_.insert({ Action::DelFunc, [this](const Message& msg) { DelFunc(msg); } });
	handlers_.insert({ Action::AltFunc, [this](const Message& msg) { AltFunc(msg); } });
	handlers_.insert({ Action::AddNode, [this](const Message& msg) { AddNode(msg); } });
	handlers_.insert({ Action::DelNode, [this](const Message& msg) { DelNode(msg); } });
	handlers_.insert({ Action::AltNode, [this](const Message& msg) { AltNode(msg); } });
	handlers_.insert({ Action::AddLine, [this](const Message& msg) { AddLine(msg); } });
	handlers_.insert({ Action::DelLine, [this](const Message& msg) { DelLine(msg); } });
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

bool Engine::Impl::Persist(const std::shared_ptr<CG::Core::VBlueprint>& asset)
{
	if (!asset || root_.empty())
		return false;
	return StoreAsset(AssetFile(root_, asset->GetSpace(), asset->GetName()), *asset);
}

void Engine::Impl::LoadAssets()
{
	assets_.clear();
	if (root_.empty())
		return;

	std::error_code ec;
	if (!fs::exists(root_, ec) || !fs::is_directory(root_, ec))
		return;

	for (fs::directory_iterator spaces(root_, ec), end; !ec && spaces != end; spaces.increment(ec)) {
		if (!spaces->is_directory(ec) || ec)
			continue;
		const std::string space = ToUtf8(spaces->path().filename());
		if (!IsSafeName(space))
			continue;

		std::error_code file_ec;
		for (fs::directory_iterator files(spaces->path(), file_ec), file_end; !file_ec && files != file_end; files.increment(file_ec)) {
			if (!files->is_regular_file(file_ec) || file_ec || files->path().extension() != kAssetExt)
				continue;

			std::ifstream in(files->path(), std::ios::binary);
			if (!in)
				continue;
			std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
			json root = json::parse(text, nullptr, false);
			if (root.is_discarded() || !root.is_object())
				continue;

			const std::string name = root.value(g_key_name, std::string{});
			const std::string asset_guid = root.value(g_key_guid, std::string{});
		if (name.empty() || asset_guid.empty() || !IsSafeName(name))
				continue;

			auto asset = std::make_shared<CG::Core::VBlueprint>();
			asset->SetGuid(asset_guid);
			asset->SetName(name);
			asset->SetSpace(root.value(g_key_space, space));
			const json body = root.value(g_key_body, json::object());

			if (body.is_object() && body.contains(g_key_vars) && body[g_key_vars].is_array()) {
				for (const auto& row : body[g_key_vars]) {
					if (!row.is_object()) continue;
					const std::string var_guid = row.value(g_key_guid, std::string{});
					const std::string var_name = row.value(g_key_name, std::string{});
					const std::string type = row.value(g_key_type, std::string{});
					if (var_guid.empty() || var_name.empty() || type.empty()) continue;
					if (asset->AddVar(var_name, type, var_guid).empty()) continue;
					if (auto var = asset->FindVar(var_guid)) {
						const std::string value = row.value(g_key_value, std::string{});
						if (auto frame = CG::Core::FromString(value, var->GetType()))
							var->Setter(frame);
					}
				}
			}

			if (body.is_object() && body.contains(g_key_flows) && body[g_key_flows].is_array()) {
				for (const auto& row : body[g_key_flows]) {
					if (!row.is_object()) continue;
					const std::string flow_guid = row.value(g_key_guid, std::string{});
					const std::string flow_name = row.value(g_key_name, std::string{});
					if (flow_guid.empty() || flow_name.empty()) continue;
					asset->AddFlow(flow_name, flow_guid);
				}
			}

			if (body.is_object() && body.contains(g_key_flows) && body[g_key_flows].is_array()) {
				for (const auto& row : body[g_key_flows]) {
					if (!row.is_object()) continue;
					const std::string flow_guid = row.value(g_key_guid, std::string{});
					auto flow = asset->FindFlow(flow_guid);
					auto graph = FlowGraph(flow);
					if (!flow || !graph) continue;

					for (const auto& node_row : row.value(g_key_nodes, json::array())) {
						if (!node_row.is_object()) continue;
						const std::string node_guid = node_row.value(g_key_guid, std::string{});
						const std::string key = node_row.value(g_key_key, std::string{});
						if (node_guid.empty() || key.empty()) continue;
						const double x = node_row.value(g_key_x, 0.0);
						const double y = node_row.value(g_key_y, 0.0);
						std::shared_ptr<CG::Core::VNode> node;

						if (key == g_node_entry || key == g_node_exit) {
							node = MakeTermNode(key.c_str(), x, y);
							if (node) {
								const auto saved_pins = node_row.value(g_key_pins, json::array());
								for (const auto& pin_row : saved_pins) {
									if (!pin_row.is_object() || pin_row.value(g_key_kind, 0) != static_cast<int>(CG::Core::PinKind::Data))
										continue;
								const auto* type = CG::Core::VTypeRegister::Find(pin_row.value(g_key_type, std::string{}));
								const std::string param_name = pin_row.value(g_key_name, std::string{});
								if (!type || !IsSafeIdentifier(param_name)) continue;
								if (auto entry = std::dynamic_pointer_cast<CG::Core::FEntry>(node->GetCaller())) entry->AddParam(param_name, type);
								else if (auto exit = std::dynamic_pointer_cast<CG::Core::FExit>(node->GetCaller())) exit->AddParam(param_name, type);
								}
								node->ApplyMeta();
								node->SetGuid(node_guid);
								RestorePinGuids(node, saved_pins);
								graph->AddNode(node);
								if (key == g_node_entry) graph->SetEntry(node);
								else graph->SetExit(node);
							}
						} else {
							bool setter = false;
							std::string var_guid;
							if (ParseVarNodeKey(key, setter, var_guid)) {
								auto var = asset->FindVar(var_guid);
								if (var)
									node = std::dynamic_pointer_cast<CG::Core::VNode>(setter ? CG::Core::MakeSetVarNode(var) : CG::Core::MakeGetVarNode(var));
							} else if (auto method = FindClassMethod(key)) {
								node = CG::Core::VNode::Make(method);
							} else {
								std::shared_ptr<CG::Core::ACaller> caller = CG::Core::VSystem::Get().FindCaller(key);
								if (!caller) {
									auto source = asset->FindFlow(key);
									if (source) {
										auto source_flow = std::dynamic_pointer_cast<CG::Core::VFlow>(source);
										if (source_flow) caller = std::dynamic_pointer_cast<CG::Core::ACaller>(source_flow->GetScript());
									}
								}
								if (caller) node = CG::Core::VNode::Make(caller);
							}
							if (node) {
								node->SetGuid(node_guid);
								node->SetName(node_row.value(g_key_name, node->GetName()));
								node->SetCoord(Vec2f{static_cast<float>(x), static_cast<float>(y)});
								RestorePinGuids(node, node_row.value(g_key_pins, json::array()));
								graph->AddNode(node);
							}
						}
						if (node && key != g_node_entry && key != g_node_exit)
							RestorePinGuids(node, node_row.value(g_key_pins, json::array()));
					}
					for (const auto& line_row : row.value(g_key_lines, json::array())) {
						if (!line_row.is_object()) continue;
						auto from_node = graph->FindNode(line_row.value(g_key_from_node, std::string{}));
						auto to_node = graph->FindNode(line_row.value(g_key_to_node, std::string{}));
						if (!from_node || !to_node) continue;
						auto line = CG::Core::MakeLine(FindNodePin(from_node, line_row.value(g_key_from_pin, std::string{})), FindNodePin(to_node, line_row.value(g_key_to_pin, std::string{})));
						if (!line) continue;
						line->SetGuid(line_row.value(g_key_guid, GenerateGUID()));
						graph->AddLine(line);
					}
				}
			}

			const std::string map_key = std::format("{}::{}", asset->GetSpace(), asset->GetName());
			if (!assets_.contains(map_key))
				assets_.emplace(map_key, std::move(asset));
		}
	}
}

void Engine::Impl::PublishAssets()
{
	if (!rise_) return;
	for (const auto& [key, asset] : assets_) {
		(void)key;
		if (!asset) continue;
		Message msg{};
		msg.act = Action::AddBP;
		msg.code = 0;
		msg.data = json{
			{g_key_guid, asset->GetGuid()},
			{g_key_name, asset->GetName()},
			{g_key_space, asset->GetSpace()},
			{g_key_path, ToUtf8(AssetFile(root_, asset->GetSpace(), asset->GetName()))},
		}.dump();
		rise_(msg);
	}
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
	msg.act = Action::TypeSync;
	msg.code = 0;
	msg.data = json{{g_key_types, std::move(types)}}.dump();
	rise_(msg);
}

void Engine::Impl::Load()
{
	CG::Core::VSystem::Get().Init();
	std::vector<fs::path> modules;
	std::error_code ec;
	if (!type_dir_.empty() && fs::exists(type_dir_, ec)) {
		for (fs::directory_iterator it(type_dir_, ec), end; !ec && it != end; it.increment(ec)) {
			if (!it->is_regular_file(ec) || ec)
				continue;
			const auto ext = it->path().extension().string();
			if (ext == ".dll" || ext == ".DLL")
				modules.push_back(it->path());
		}
	}

	ec.clear();
	if (!root_.empty() && fs::exists(root_, ec) && fs::is_directory(root_, ec)) {
		for (fs::directory_iterator spaces(root_, ec), end; !ec && spaces != end; spaces.increment(ec)) {
			if (!spaces->is_directory(ec) || ec)
				continue;
			std::error_code generated_ec;
			for (fs::directory_iterator generated(spaces->path(), generated_ec), generated_end;
				!generated_ec && generated != generated_end; generated.increment(generated_ec)) {
				if (!generated->is_directory(generated_ec) || generated_ec)
					continue;
				// A generated module is owned by the blueprint asset with the same
				// name. This prevents stale build directories from registering the
				// same class a second time during startup.
				const fs::path asset_file = spaces->path()
					/ FromUtf8(ToUtf8(generated->path().filename()) + kAssetExt);
				std::error_code asset_ec;
				if (!fs::is_regular_file(asset_file, asset_ec) || asset_ec)
					continue;
				const fs::path build_root = generated->path() / "build";
				std::error_code build_ec;
				if (!fs::is_directory(build_root, build_ec) || build_ec
					|| !fs::is_regular_file(generated->path() / "CMakeLists.txt", build_ec))
					continue;
				const fs::path latest_dll = LatestBlueprintDll(generated->path(), CLASSGRAPH_BUILD_CONFIG);
				if (!latest_dll.empty())
					modules.push_back(latest_dll);
			}
		}
	}

	for (const auto& module : modules)
		CG::Core::VSystem::Get().LoadDll(ToUtf8(module));

	LoadAssets();
	PublishAssets();
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

	json funcs = json::array();
	for (const auto& caller : CG::Core::VSystem::Get().ListCallers()) {
		if (!caller)
			continue;
		const std::string name = caller->GetName();
		const char* group = group_of(*caller);
		if (!name.empty() && group) {
			json row = {{g_key_name, name}, {g_key_group, group}, {g_key_key, name}};
			row.update(FunctionTypeTags(caller->GetCallMeta()));
			funcs.push_back(std::move(row));
		}
	}
	funcs.push_back({{g_key_name, g_node_exit}, {g_key_group, g_group_operator}, {g_key_key, g_node_exit},
		{g_key_input_types, json::array()}, {g_key_output_types, json::array()}});

	Message msg{};
	msg.act = Action::FuncSync;
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
		if (std::any_of(asset->GetFlows().begin(), asset->GetFlows().end(), [&](const auto& entry) {
			return entry.second && entry.second->GetName() == name;
		})) {
			RiseFail(rise_, msg, "blueprint name conflicts with function name", guid);
			break;
		}
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
		if (!Persist(asset)) {
			RiseFail(rise_, msg, "write asset failed", guid);
			break;
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
			json row = {{g_key_guid, flow->GetGuid()}, {g_key_name, flow->GetName()}, {g_key_nodes, json::array()}, {g_key_lines, json::array()}};
			if (auto graph = FlowGraph(flow)) {
				for (const auto& [node_guid, node_base] : graph->GetNodes()) {
					(void)node_guid;
					auto node = std::dynamic_pointer_cast<CG::Core::VNode>(node_base);
					if (node)
						row[g_key_nodes].push_back(StoredNode(*asset, *flow, node));
				}
				for (const auto& [line_guid, line] : graph->GetLines()) {
					(void)line_guid;
					if (auto stored = StoredLine(line); !stored.empty()) row[g_key_lines].push_back(std::move(stored));
				}
			}
			flows.push_back(std::move(row));
		}
		json vars = json::array();
		for (const auto& [id, var] : asset->GetVars()) {
			(void)id;
			if (!var) continue;
			json item{{g_key_guid, var->GetGuid()}, {g_key_name, var->GetName()}};
			if (const auto* type = var->GetType())
				item[g_key_type] = type->name_;
			item[g_key_value] = CG::Core::ToString(var->Getter());
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
		if (!Persist(asset)) {
			asset->DelVar(var_guid);
			RiseFail(rise_, msg, "write asset failed", guid);
			break;
		}

		Message reply{};
		reply.act = msg.act;
		reply.code = 0;
		reply.token = msg.token;
		reply.data = json{{g_key_asset, guid}, {g_key_guid, var_guid}, {g_key_name, name},
			{g_key_type, type_name}, {g_key_value, CG::Core::ToString(asset->FindVar(var_guid)->Getter())}}.dump();
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
		auto asset_object = owned.asset->second;
		const std::string old_name = owned.var->GetName();
		std::string old_type;
		if (const auto* type = owned.var->GetType()) old_type = type->name_;
		const std::string old_value = CG::Core::ToString(owned.var->Getter());
		auto graph_snapshot = SnapshotVariableNodes(asset_object, guid);
		RemoveVariableGraph(graph_snapshot);
		owned.asset->second->DelVar(guid);
		if (!Persist(asset_object)) {
			if (!old_type.empty()) {
				asset_object->AddVar(old_name, old_type, guid);
				auto restored_var = asset_object->FindVar(guid);
				if (restored_var) {
					if (auto frame = restored_var->Getter())
						if (auto value = CG::Core::FromString(old_value, restored_var->GetType())) frame->Update(value);
					std::vector<std::shared_ptr<CG::Core::VNode>> replacements;
					std::vector<VariableLineSnapshot> removed_lines;
					if (!graph_snapshot.nodes.empty())
						RebuildVariableNodes(graph_snapshot, restored_var, replacements, removed_lines);
					else
						RestoreVariableGraph(graph_snapshot);
				}
			}
			RiseFail(rise_, msg, "write asset failed", guid);
			break;
		}

		json removed_nodes = json::array();
		for (const auto& node : graph_snapshot.nodes)
			removed_nodes.push_back({{g_key_flow, node.flow->GetGuid()}, {g_key_guid, node.node->GetGuid()}});
		json removed_lines = json::array();
		for (const auto& line : graph_snapshot.lines) {
			auto flow = line.graph->GetFlow();
			removed_lines.push_back({{g_key_flow, flow ? flow->GetGuid() : std::string{}}, {g_key_guid, line.guid}});
		}
		Message reply{};
		reply.act = msg.act;
		reply.code = 0;
		reply.token = msg.token;
		reply.data = json{
			{g_key_asset, asset},
			{g_key_guid, guid},
			{g_key_removed_nodes, std::move(removed_nodes)},
			{g_key_removed_lines, std::move(removed_lines)},
		}.dump();
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
		const std::string old_name = owned.var->GetName();
		const auto* old_type_info = owned.var->GetType();
		const std::string type_name = old_type_info ? old_type_info->name_ : std::string{};
		auto graph_snapshot = SnapshotVariableNodes(asset, guid);
		if (!IsSafeName(name)) {
			RiseFail(rise_, msg, "invalid name", guid);
			break;
		}
		if (VarNameTaken(*asset, name, guid)) {
			RiseFail(rise_, msg, "variable exists", guid);
			break;
		}

		if (root.contains(g_key_type)) {
			RiseFail(rise_, msg, "variable type is immutable after creation", guid);
			break;
		}

		owned.var->SetName(name);
		std::vector<std::shared_ptr<CG::Core::VNode>> replacements;
		std::vector<VariableLineSnapshot> removed_lines;
		if (!RebuildVariableNodes(graph_snapshot, owned.var, replacements, removed_lines)) {
			owned.var->SetName(old_name);
			RiseFail(rise_, msg, "rebuild variable nodes failed", guid);
			break;
		}
		if (!Persist(asset)) {
			RemoveRebuiltVariableGraph(graph_snapshot, replacements);
			owned.var->SetName(old_name);
			RestoreVariableGraph(graph_snapshot);
			RiseFail(rise_, msg, "write asset failed", guid);
			break;
		}

		json updated_nodes = json::array();
		for (size_t i = 0; i < replacements.size(); ++i) {
			const auto& saved = graph_snapshot.nodes[i];
			updated_nodes.push_back({
				{g_key_flow, saved.flow->GetGuid()},
				{g_key_node, StoredNode(*asset, *saved.flow, replacements[i])},
			});
		}
		json removed_line_rows = json::array();
		for (const auto& line : removed_lines) {
			auto flow = line.graph->GetFlow();
			removed_line_rows.push_back({{g_key_flow, flow ? flow->GetGuid() : std::string{}}, {g_key_guid, line.guid}});
		}
		Message reply{};
		reply.act = msg.act;
		reply.code = 0;
		reply.token = msg.token;
		reply.data = json{
			{g_key_asset, asset->GetGuid()},
			{g_key_guid, guid},
			{g_key_name, name},
			{g_key_type, type_name},
			{g_key_nodes, std::move(updated_nodes)},
			{g_key_removed_lines, std::move(removed_line_rows)},
		}.dump();
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
		if (name == asset->GetName()) {
			RiseFail(rise_, msg, "function name must differ from blueprint class name", guid);
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
		if (!Persist(asset)) {
			asset->DelFlow(flow_guid);
			RiseFail(rise_, msg, "write asset failed", guid);
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
		auto asset_object = owned.asset->second;
		owned.asset->second->DelFlow(guid);
		if (!Persist(asset_object)) {
			RiseFail(rise_, msg, "write asset failed", guid);
			break;
		}

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
		const std::string old_name = owned.flow->GetName();
		if (!IsSafeName(name)) {
			RiseFail(rise_, msg, "invalid name", guid);
			break;
		}
		if (name == asset->GetName()) {
			RiseFail(rise_, msg, "function name must differ from blueprint class name", guid);
			break;
		}
		if (FlowNameTaken(*asset, name, guid)) {
			RiseFail(rise_, msg, "function exists", guid);
			break;
		}

		owned.flow->SetName(name);
		if (auto script = owned.flow->GetScript())
			script->SetName(name);
		if (!Persist(asset)) {
			owned.flow->SetName(old_name);
			if (auto script = owned.flow->GetScript()) script->SetName(old_name);
			RiseFail(rise_, msg, "write asset failed", guid);
			break;
		}

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
		bool variable_node_set = false;
		std::string variable_guid;
		if (key == g_node_entry || key == g_node_exit) {
			const char* error = nullptr;
			node = AttachTerm(*graph, key.c_str(), x, y, error);
			if (!node) {
				RiseFail(rise_, msg, error ? error : "add node failed", guid);
				break;
			}
		} else if (ParseVarNodeKey(key, variable_node_set, variable_guid)) {
			auto variable = FindOwnedVar(assets_, variable_guid);
			if (variable.asset != owned.asset || !variable.var) {
				RiseFail(rise_, msg, "variable not found", variable_guid);
				break;
			}
			node = variable_node_set
				? std::dynamic_pointer_cast<CG::Core::VNode>(CG::Core::MakeSetVarNode(variable.var))
				: std::dynamic_pointer_cast<CG::Core::VNode>(CG::Core::MakeGetVarNode(variable.var));
			if (!node) {
				RiseFail(rise_, msg, "add variable node failed", guid);
				break;
			}
			node->SetGuid(GenerateGUID());
			node->SetCoord(Vec2f{static_cast<float>(x), static_cast<float>(y)});
			graph->AddNode(node);
		} else if (auto method = FindClassMethod(key)) {
			node = CG::Core::VNode::Make(method);
			if (!node) {
				RiseFail(rise_, msg, "add class method node failed", guid);
				break;
			}
			node->SetGuid(GenerateGUID());
			node->SetName(method->GetName());
			node->SetCoord(Vec2f{static_cast<float>(x), static_cast<float>(y)});
			graph->AddNode(node);
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
		body[g_key_key] = key;
		const std::string auto_from_node = root.value(g_key_from_node, std::string{});
		const std::string auto_from_pin = root.value(g_key_from_pin, std::string{});
		if (!auto_from_node.empty() && !auto_from_pin.empty()) {
			body[g_key_from_node] = auto_from_node;
			body[g_key_from_pin] = auto_from_pin;
			if (auto source_node = graph->FindNode(auto_from_node)) {
				const auto source_pin = FindNodePin(source_node, auto_from_pin);
				const auto source_type = source_pin && source_pin->GetPinMeta().type
					? source_pin->GetPinMeta().type->name_ : std::string{};
				std::shared_ptr<CG::Core::APin> target_pin;
				for (const auto& candidate : node->GetInputPins()) {
					if (!candidate || candidate->GetPinMeta().kind != CG::Core::PinKind::Data
						|| !candidate->GetPinMeta().type || candidate->GetPinMeta().type->name_ != source_type)
						continue;
					if (candidate->GetPinMeta().name == "self") {
						target_pin = candidate;
						break;
					}
					if (!target_pin)
						target_pin = candidate;
				}
				if (target_pin)
					body[g_key_to_pin] = target_pin->GetGuid();
			}
		}
		if (!Persist(owned.asset->second)) {
			if (node && node->GetCaller() && (key == g_node_entry || key == g_node_exit)) {
				if (key == g_node_entry) graph->SetEntry({});
				else graph->SetExit({});
			}
			graph->DelNode(node->GetGuid());
			RiseFail(rise_, msg, "write asset failed", guid);
			break;
		}

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
		const bool was_exit = owned.graph->GetExit() && owned.graph->GetExit()->GetGuid() == guid;
		std::vector<std::tuple<std::string, std::shared_ptr<CG::Core::APin>, std::shared_ptr<CG::Core::APin>>> attached_lines;
		for (const auto& [line_guid, line] : owned.graph->GetLines()) {
			if (!line) continue;
			const auto from = line->GetFrom() ? line->GetFrom()->GetOwner() : nullptr;
			const auto to = line->GetTo() ? line->GetTo()->GetOwner() : nullptr;
			if ((from && from->GetGuid() == guid) || (to && to->GetGuid() == guid)) attached_lines.emplace_back(line_guid, line->GetFrom(), line->GetTo());
		}
		for (const auto& [line_guid, from_pin, to_pin] : attached_lines) {
			if (auto line = owned.graph->FindLine(line_guid)) CG::Core::DisconnectLine(line);
			owned.graph->DelLine(line_guid);
		}
		if (auto exit = owned.graph->GetExit(); exit && exit->GetGuid() == guid)
			owned.graph->SetExit({});
		owned.graph->DelNode(guid);
		if (!Persist(owned.asset->second)) {
			owned.graph->AddNode(owned.node);
			if (was_exit) owned.graph->SetExit(owned.node);
			for (const auto& [line_guid, from_pin, to_pin] : attached_lines) {
				auto restored = CG::Core::MakeLine(from_pin, to_pin);
				if (restored) { restored->SetGuid(line_guid); owned.graph->AddLine(restored); }
			}
			RiseFail(rise_, msg, "write asset failed", guid);
			break;
		}

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
		if (guid.empty()) {
			RiseFail(rise_, msg, "guid empty", guid);
			break;
		}

		auto owned = FindOwnedNode(assets_, guid);
		auto node = std::dynamic_pointer_cast<CG::Core::VNode>(owned.node);
		if (owned.asset == assets_.end() || !owned.asset->second || !owned.flow || !owned.graph || !node) {
			RiseFail(rise_, msg, "node not found", guid);
			break;
		}
		if (root.contains(g_key_param_action)) {
			const std::string operation = root.value(g_key_param_action, std::string{});
			auto caller = node->GetCaller();
			auto entry = std::dynamic_pointer_cast<CG::Core::FEntry>(caller);
			auto exit = std::dynamic_pointer_cast<CG::Core::FExit>(caller);
			if (!entry && !exit) {
				RiseFail(rise_, msg, "only Entry and Exit parameters can be changed", guid);
				break;
			}

			if (operation == "add") {
				const std::string name = root.value(g_key_name, std::string{});
				const std::string type_name = root.value(g_key_type, std::string{});
				const auto* type = CG::Core::VTypeRegister::Find(type_name);
				if (!IsSafeIdentifier(name) || !type) {
					RiseFail(rise_, msg, "invalid parameter name or type", guid);
					break;
				}
				const bool added = entry ? entry->AddParam(name, type) : exit->AddParam(name, type);
				if (!added) {
					RiseFail(rise_, msg, "parameter already exists or cannot be added", guid);
					break;
				}
				node->ApplyMeta();
				if (!Persist(owned.asset->second)) {
					if (entry) entry->DelParam(name); else exit->DelParam(name);
					node->ApplyMeta();
					RiseFail(rise_, msg, "write asset failed", guid);
					break;
				}
				json body = StoredNode(*owned.asset->second, *owned.flow, node);
				body[g_key_asset] = owned.asset->second->GetGuid();
				body[g_key_flow] = owned.flow->GetGuid();
				Message reply{};
				reply.act = msg.act;
				reply.code = 0;
				reply.token = msg.token;
				reply.data = body.dump();
				rise_(reply);
				ok = true;
				break;
			}

			if (operation == "delete") {
				const std::string pin_guid = root.value(g_key_pin, std::string{});
				auto pin = node->GetPin(pin_guid);
				const auto expected_direct = entry ? CG::Core::EDirect::Out : CG::Core::EDirect::In;
				if (!pin || pin->GetDirect() != expected_direct || pin->GetPinMeta().kind != CG::Core::PinKind::Data) {
					RiseFail(rise_, msg, "parameter pin not found", guid);
					break;
				}
				const auto old_meta = pin->GetPinMeta();
				std::vector<std::tuple<std::string, std::shared_ptr<CG::Core::APin>, std::shared_ptr<CG::Core::APin>>> attached_lines;
				for (const auto& [line_guid, line] : owned.graph->GetLines()) {
					if (!line || !line->GetFrom() || !line->GetTo()) continue;
					if (line->GetFrom() == pin || line->GetTo() == pin)
						attached_lines.emplace_back(line_guid, line->GetFrom(), line->GetTo());
				}
				const bool removed = entry ? entry->DelParam(old_meta.name) : exit->DelParam(old_meta.name);
				if (!removed) {
					RiseFail(rise_, msg, "parameter cannot be removed", guid);
					break;
				}
				node->ApplyMeta();
				json removed_lines = json::array();
				for (const auto& [line_guid, from_pin, to_pin] : attached_lines) {
					(void)from_pin;
					(void)to_pin;
					if (auto line = owned.graph->FindLine(line_guid)) CG::Core::DisconnectLine(line);
					owned.graph->DelLine(line_guid);
					removed_lines.push_back(line_guid);
				}
				if (!Persist(owned.asset->second)) {
					if (entry) entry->AddParam(old_meta.name, old_meta.type); else exit->AddParam(old_meta.name, old_meta.type);
					node->ApplyMeta();
					std::shared_ptr<CG::Core::APin> restored_pin;
					const auto candidates = entry ? node->GetOutputPins() : node->GetInputPins();
					for (const auto& candidate : candidates)
						if (candidate && candidate->GetPinMeta().name == old_meta.name) { restored_pin = candidate; break; }
					if (restored_pin) {
						restored_pin->SetGuid(pin_guid);
						restored_pin->SetPinMeta(old_meta);
						if (auto frame = restored_pin->GetFrame())
							if (auto value = CG::Core::FromString(old_meta.value, old_meta.type)) frame->Update(value);
					}
					for (const auto& [line_guid, old_from, old_to] : attached_lines) {
						auto from_pin = old_from == pin ? restored_pin : old_from;
						auto to_pin = old_to == pin ? restored_pin : old_to;
						auto restored = CG::Core::MakeLine(from_pin, to_pin);
						if (!restored) continue;
						restored->SetGuid(line_guid);
						owned.graph->AddLine(restored);
					}
					RiseFail(rise_, msg, "write asset failed", guid);
					break;
				}
				json body = StoredNode(*owned.asset->second, *owned.flow, node);
				body[g_key_asset] = owned.asset->second->GetGuid();
				body[g_key_flow] = owned.flow->GetGuid();
				body[g_key_removed_lines] = std::move(removed_lines);
				Message reply{};
				reply.act = msg.act;
				reply.code = 0;
				reply.token = msg.token;
				reply.data = body.dump();
				rise_(reply);
				ok = true;
				break;
			}
			RiseFail(rise_, msg, "invalid parameter operation", guid);
			break;
		}
		if (root.contains(g_key_pin) && (root.contains(g_key_value) || root.contains(g_key_legacy_pin_default_value))) {
			const std::string pin_guid = root.value(g_key_pin, std::string{});
			const std::string value_text = root.value(g_key_value, root.value(g_key_legacy_pin_default_value, std::string{}));
			auto pin = node->GetPin(pin_guid);
			if (!pin || pin->GetDirect() != CG::Core::EDirect::In
				|| pin->GetPinMeta().kind != CG::Core::PinKind::Data || !pin->GetLines().empty()) {
				RiseFail(rise_, msg, "pin value is not editable", guid);
				break;
			}
			const auto old_meta = pin->GetPinMeta();
			const std::string old_frame_value = CG::Core::ToString(pin->GetFrame());
			auto value = CG::Core::FromString(value_text, old_meta.type);
			if (!value) {
				RiseFail(rise_, msg, "invalid pin value", guid);
				break;
			}
			auto next_meta = old_meta;
			next_meta.value = value_text;
			pin->SetPinMeta(next_meta);
			if (auto frame = pin->GetFrame()) frame->Update(value);
			if (!Persist(owned.asset->second)) {
				pin->SetPinMeta(old_meta);
				if (auto frame = pin->GetFrame())
					if (auto previous = CG::Core::FromString(old_frame_value, old_meta.type)) frame->Update(previous);
				RiseFail(rise_, msg, "write asset failed", guid);
				break;
			}
			json body = StoredNode(*owned.asset->second, *owned.flow, node);
			body[g_key_asset] = owned.asset->second->GetGuid();
			body[g_key_flow] = owned.flow->GetGuid();
			Message reply{};
			reply.act = msg.act;
			reply.code = 0;
			reply.token = msg.token;
			reply.data = body.dump();
			rise_(reply);
			ok = true;
			break;
		}
		if (!root.contains(g_key_x) || !root.contains(g_key_y)) {
			RiseFail(rise_, msg, "node coordinates missing", guid);
			break;
		}

		const double x = root.value(g_key_x, 0.0);
		const double y = root.value(g_key_y, 0.0);
		const Vec2f old_coord = node->GetCoord();
		node->SetCoord(Vec2f{static_cast<float>(x), static_cast<float>(y)});
		if (!Persist(owned.asset->second)) {
			node->SetCoord(old_coord);
			RiseFail(rise_, msg, "write asset failed", guid);
			break;
		}

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

bool Engine::Impl::AddLine(const Message& msg)
{
	if (!rise_) return false;
	json root = json::parse(msg.data, nullptr, false);
	if (root.is_discarded() || !root.is_object()) { RiseFail(rise_, msg, "invalid json"); return false; }
	const std::string flow_guid = root.value(g_key_flow, std::string{});
	const std::string from_node_guid = root.value(g_key_from_node, std::string{});
	const std::string from_pin_guid = root.value(g_key_from_pin, std::string{});
	const std::string to_node_guid = root.value(g_key_to_node, std::string{});
	const std::string to_pin_guid = root.value(g_key_to_pin, std::string{});
	if (flow_guid.empty() || from_node_guid.empty() || from_pin_guid.empty() || to_node_guid.empty() || to_pin_guid.empty()) {
		RiseFail(rise_, msg, "line endpoint missing"); return false;
	}
	auto owned = FindOwnedFlow(assets_, flow_guid);
	auto graph = FlowGraph(owned.flow);
	if (owned.asset == assets_.end() || !graph) { RiseFail(rise_, msg, "function not found", flow_guid); return false; }
	auto from_node = graph->FindNode(from_node_guid);
	auto to_node = graph->FindNode(to_node_guid);
	if (!from_node || !to_node) { RiseFail(rise_, msg, "node not found", flow_guid); return false; }
	auto from_pin = FindNodePin(from_node, from_pin_guid);
	auto to_pin = FindNodePin(to_node, to_pin_guid);
	if (!from_pin || !to_pin) { RiseFail(rise_, msg, "invalid pin connection", flow_guid); return false; }
	std::vector<std::tuple<std::string, std::shared_ptr<CG::Core::APin>, std::shared_ptr<CG::Core::APin>>> stale_lines;
	auto capture_lines = [&](const std::shared_ptr<CG::Core::APin>& pin) {
		if (!pin) return;
		for (const auto& old_line : pin->GetLines()) {
			if (!old_line) continue;
			const auto old_guid = old_line->GetGuid();
			if (old_guid.empty()) continue;
			const auto duplicate = std::find_if(stale_lines.begin(), stale_lines.end(), [&](const auto& row) {
				return std::get<0>(row) == old_guid;
			});
			if (duplicate == stale_lines.end())
				stale_lines.emplace_back(old_guid, old_line->GetFrom(), old_line->GetTo());
		}
	};
	if (from_pin->GetDirect() == CG::Core::EDirect::In || from_pin->GetPinMeta().kind == CG::Core::PinKind::Exec)
		capture_lines(from_pin);
	if (to_pin->GetDirect() == CG::Core::EDirect::In || to_pin->GetPinMeta().kind == CG::Core::PinKind::Exec)
		capture_lines(to_pin);
	auto line = CG::Core::MakeLine(from_pin, to_pin);
	if (!line) { RiseFail(rise_, msg, "invalid pin connection", flow_guid); return false; }
	line->SetGuid(GenerateGUID());
	for (const auto& [old_guid, from_pin, to_pin] : stale_lines) {
		(void)from_pin;
		(void)to_pin;
		graph->DelLine(old_guid);
	}
	graph->AddLine(line);
	if (!Persist(owned.asset->second)) {
		CG::Core::DisconnectLine(line);
		graph->DelLine(line->GetGuid());
		for (const auto& [old_guid, from_pin, to_pin] : stale_lines) {
			auto restored = CG::Core::MakeLine(from_pin, to_pin);
			if (restored) { restored->SetGuid(old_guid); graph->AddLine(restored); }
		}
		RiseFail(rise_, msg, "write asset failed", flow_guid);
		return false;
	}
	Message reply{};
	reply.act = msg.act; reply.code = 0; reply.token = msg.token;
	reply.data = StoredLine(line).dump();
	json out = json::parse(reply.data);
	out[g_key_asset] = owned.asset->second->GetGuid();
	out[g_key_flow] = flow_guid;
	out[g_key_removed_lines] = json::array();
	for (const auto& [old_guid, from_pin, to_pin] : stale_lines) {
		(void)from_pin;
		(void)to_pin;
		out[g_key_removed_lines].push_back(old_guid);
	}
	reply.data = out.dump();
	rise_(reply);
	return true;
}

bool Engine::Impl::DelLine(const Message& msg)
{
	if (!rise_) return false;
	json root = json::parse(msg.data, nullptr, false);
	if (root.is_discarded() || !root.is_object()) { RiseFail(rise_, msg, "invalid json"); return false; }
	const std::string guid = root.value(g_key_guid, std::string{});
	if (guid.empty()) { RiseFail(rise_, msg, "guid empty"); return false; }
	auto owned = FindOwnedLine(assets_, guid);
	if (owned.asset == assets_.end() || !owned.graph || !owned.line) { RiseFail(rise_, msg, "line not found", guid); return false; }
	const auto old_from = owned.line->GetFrom();
	const auto old_to = owned.line->GetTo();
	CG::Core::DisconnectLine(owned.line);
	owned.graph->DelLine(guid);
	if (!Persist(owned.asset->second)) {
		auto restored = CG::Core::MakeLine(old_from, old_to);
		if (restored) { restored->SetGuid(guid); owned.graph->AddLine(restored); }
		RiseFail(rise_, msg, "write asset failed", guid);
		return false;
	}
	Message reply{};
	reply.act = msg.act; reply.code = 0; reply.token = msg.token;
	auto to_node = old_to ? old_to->GetOwner() : nullptr;
	reply.data = json{
		{g_key_asset, owned.asset->second->GetGuid()}, {g_key_flow, owned.flow->GetGuid()}, {g_key_guid, guid},
		{g_key_to_node, to_node ? to_node->GetGuid() : std::string{}},
		{g_key_to_pin, old_to ? old_to->GetGuid() : std::string{}}
	}.dump();
	rise_(reply);
	return true;
}

bool Engine::Impl::Compile(const Message& msg)
{
	if (!rise_) return false;
	const json root = json::parse(msg.data, nullptr, false);
	if (root.is_discarded() || !root.is_object()) {
		RiseFail(rise_, msg, "invalid compile request");
		return false;
	}
	const std::string guid = root.value(g_key_guid, std::string{});
	std::shared_ptr<CG::Core::VBlueprint> asset;
	for (const auto& [key, candidate] : assets_) {
		(void)key;
		if (candidate && candidate->GetGuid() == guid) { asset = candidate; break; }
	}
	if (!asset) { RiseFail(rise_, msg, "blueprint not found", guid); return false; }
	if (root_.empty()) { RiseFail(rise_, msg, "asset root is not configured", guid); return false; }
	for (const auto& [flow_guid, flow] : asset->GetFlows()) {
		(void)flow_guid;
		if (flow && flow->GetName() == asset->GetName()) {
			RiseFail(rise_, msg, "function name must differ from blueprint class name", guid);
			return false;
		}
	}

	std::string source, header, generated_header, generated_source;
	const std::set<std::string> type_headers = BlueprintTypeHeaders(*asset);
	if (asset->GenerateCode(source, header, generated_header, generated_source) != 0) {
		RiseFail(rise_, msg, "code generation failed", guid);
		return false;
	}

	const fs::path generated_dir = AssetFile(root_, asset->GetSpace(), asset->GetName()).parent_path()
		/ FromUtf8(asset->GetName());
	std::vector<std::pair<fs::path, fs::path>> dependency_artifacts;
	for (const auto& type_header : type_headers) {
		const fs::path relative_header = FromUtf8(type_header);
		if (relative_header.is_absolute())
			continue;
		bool has_parent_traversal = false;
		for (const auto& part : relative_header) {
			if (part == "..") {
				has_parent_traversal = true;
				break;
			}
		}
		if (has_parent_traversal)
			continue;

		// Core stores the stable public type header as <space>/<name>.h. Engine
		// keeps each blueprint's generated implementation in a private directory
		// beside that public header.
		const fs::path dependency_dir = (root_ / relative_header.parent_path()
			/ FromUtf8(relative_header.stem().string())).lexically_normal();
		std::error_code dependency_ec;
		if (!fs::is_directory(dependency_dir, dependency_ec)
			|| !fs::exists(dependency_dir / "CMakeLists.txt", dependency_ec))
			continue;
		if (dependency_dir == generated_dir.lexically_normal())
			continue;
		const auto artifacts = LatestBlueprintLinkArtifacts(dependency_dir, CLASSGRAPH_BUILD_CONFIG);
		if (artifacts.first.empty() || artifacts.second.empty()) {
			RiseFail(rise_, msg, "blueprint dependency must be compiled before compiling this blueprint", type_header);
			return false;
		}
		dependency_artifacts.push_back(artifacts);
	}
	const fs::path build_dir = generated_dir / "build" / FromUtf8(GenerateGUID());
	std::error_code ec;
	fs::create_directories(build_dir, ec);
	if (ec || !WriteText(generated_dir / FromUtf8(asset->GetName() + ".cpp"), source)
		|| !WriteText(generated_dir / FromUtf8(asset->GetName() + ".h"), header)
		|| !WriteText(generated_dir / FromUtf8(asset->GetName() + ".gen.h"), generated_header)
		|| !WriteText(generated_dir / FromUtf8(asset->GetName() + ".gen.cpp"), generated_source)) {
		RiseFail(rise_, msg, "write generated source failed", guid);
		return false;
	}
	const fs::path public_header = (root_ / FromUtf8(asset->GetSpace())
		/ FromUtf8(asset->GetName() + ".h")).lexically_normal();
	const std::string forwarding_header = "#pragma once\n#include \""
		+ asset->GetName() + "/" + asset->GetName() + ".h\"\n";
	if (!WriteText(public_header, forwarding_header)) {
		RiseFail(rise_, msg, "write public blueprint header failed", guid);
		return false;
	}

	auto cmake_path = [](std::string path) {
		std::replace(path.begin(), path.end(), '\\', '/');
		return path;
	};
	const std::string name = asset->GetName();
	const std::string runtime_name = "BlueprintRuntime_" + asset->GetGuid();
	std::string cmake_project =
		"cmake_minimum_required(VERSION 3.20)\n"
		"project(ClassGraphBlueprint LANGUAGES CXX)\n"
		"set(CMAKE_CXX_STANDARD 20)\n"
		"set(CMAKE_CXX_STANDARD_REQUIRED ON)\n"
		"add_library(BlueprintRuntime MODULE \"" + name + ".cpp\" \"" + name + ".gen.cpp\")\n"
		"set_target_properties(BlueprintRuntime PROPERTIES PREFIX \"\" OUTPUT_NAME \"" + runtime_name
		+ "\" WINDOWS_EXPORT_ALL_SYMBOLS ON)\n"
		"target_include_directories(BlueprintRuntime PRIVATE \"" + cmake_path(CLASSGRAPH_CORE_INCLUDE) + "\" \""
		+ cmake_path(CLASSGRAPH_SHARE_INCLUDE) + "\" \"" + cmake_path(ToUtf8(root_))
		+ "\" \"${CMAKE_CURRENT_SOURCE_DIR}\")\n"
		"target_link_libraries(BlueprintRuntime PRIVATE \"" + cmake_path(CLASSGRAPH_CORE_LIB) + "\"";
	for (const auto& [dependency_dll, dependency_lib] : dependency_artifacts) {
		cmake_project += " \"" + cmake_path(ToUtf8(dependency_lib)) + "\"";
	}
	cmake_project += ")\n";
	for (const auto& [dependency_dll, dependency_lib] : dependency_artifacts) {
		(void)dependency_lib;
		cmake_project += "add_custom_command(TARGET BlueprintRuntime POST_BUILD COMMAND \"${CMAKE_COMMAND}\" -E copy_if_different \""
			+ cmake_path(ToUtf8(dependency_dll)) + "\" \"$<TARGET_FILE_DIR:BlueprintRuntime>\")\n";
	}
	if (!WriteText(generated_dir / "CMakeLists.txt", cmake_project)) {
		RiseFail(rise_, msg, "write generated build project failed", guid);
		return false;
	}

#ifdef _WIN32
	const fs::path cmake = FromUtf8(CLASSGRAPH_CMAKE_COMMAND);
	const fs::path configure_log = build_dir / "configure.log";
	const fs::path build_log = build_dir / "build.log";
	std::vector<std::wstring> configure_args{
		L"-S", generated_dir.native(), L"-B", build_dir.native(), L"-G", FromUtf8(CLASSGRAPH_CMAKE_GENERATOR).native()
	};
	const std::wstring platform = FromUtf8(CLASSGRAPH_CMAKE_PLATFORM).native();
	if (!platform.empty()) { configure_args.push_back(L"-A"); configure_args.push_back(platform); }
	DWORD exit_code = 0;
	if (!RunProcess(cmake, configure_args, generated_dir, configure_log, exit_code) || exit_code != 0) {
		std::string details = ReadText(configure_log);
		if (details.size() > 16000) details.erase(0, details.size() - 16000);
		Message reply = msg;
		reply.code = 1;
		reply.data = json{{g_key_error, "CMake configure failed"}, {g_key_guid, guid}, {g_key_text, details}}
			.dump(-1, ' ', false, json::error_handler_t::replace);
		rise_(reply);
		return false;
	}
	std::vector<std::wstring> build_args{L"--build", build_dir.native(), L"--config", FromUtf8(CLASSGRAPH_BUILD_CONFIG).native()};
	if (!RunProcess(cmake, build_args, generated_dir, build_log, exit_code) || exit_code != 0) {
		std::string details = ReadText(build_log);
		if (details.size() > 16000) details.erase(0, details.size() - 16000);
		Message reply = msg;
		reply.code = 1;
		reply.data = json{{g_key_error, "C++ build failed"}, {g_key_guid, guid}, {g_key_text, details}}
			.dump(-1, ' ', false, json::error_handler_t::replace);
		rise_(reply);
		return false;
	}
	const fs::path dll = build_dir / FromUtf8(CLASSGRAPH_BUILD_CONFIG) / FromUtf8(runtime_name + ".dll");
	const std::string type_key = asset->GetSpace() + "::" + asset->GetName();
	CG::Core::VSystem::Get().UnloadDll(type_key);
	if (!CG::Core::VSystem::Get().LoadDll(ToUtf8(dll))) {
		RiseFail(rise_, msg, "build succeeded but loading generated DLL failed", guid);
		return false;
	}
#else
	RiseFail(rise_, msg, "blueprint DLL compilation is only supported on Windows", guid);
	return false;
#endif

	PublishTypes();
	PublishFuncs();
	Message reply{};
	reply.act = msg.act;
	reply.code = 0;
	reply.token = msg.token;
	reply.data = json{{g_key_guid, guid}, {g_key_asset, guid},
		{g_key_path, ToUtf8(generated_dir)}, {g_key_dll, ToUtf8(dll)}}.dump();
	RiseOutput(rise_, msg, "Blueprint compiled and loaded: " + ToUtf8(dll), "Engine");
	rise_(reply);
	return true;
}

bool Engine::Impl::RunFunc(const Message& msg)
{
	if (!rise_) return false;
	const json root = json::parse(msg.data, nullptr, false);
	if (!root.is_object()) { RiseFail(rise_, msg, "invalid run request"); return false; }
	const std::string asset_guid = root.value(g_key_asset, std::string{});
	const std::string flow_guid = root.value(g_key_flow, std::string{});
	auto asset_it = FindAsset(assets_, asset_guid);
	if (asset_it == assets_.end() || !asset_it->second) {
		RiseFail(rise_, msg, "blueprint not found", asset_guid);
		return false;
	}
	auto flow = asset_it->second->FindFlow(flow_guid);
	if (!flow || !flow->GetGraph() || !flow->GetGraph()->GetEntry()) {
		RiseFail(rise_, msg, "flow or entry node not found", flow_guid);
		return false;
	}

	auto send_runtime = [&](const std::string& phase, const std::string& node_guid = {},
		const std::string& status = {}, json pins = json::array(), int code = 0,
		json vars = json::array()) {
		Message event{};
		event.act = Action::RuntimeSync;
		event.code = code;
		event.token = msg.token;
		json body = {
			{g_key_asset, asset_guid},
			{g_key_flow, flow_guid},
			{g_key_phase, phase},
			{g_key_guid, node_guid},
			{g_key_pins, std::move(pins)},
		};
		if (phase == "finished") {
			body[g_key_flow_status] = status;
			body[g_key_vars] = std::move(vars);
		} else if (phase == "node")
			body[g_key_node_status] = status;
		event.data = body.dump();
		rise_(event);
	};

	send_runtime("started");
	auto graph = flow->GetGraph();
	if (!graph) { RiseFail(rise_, msg, "flow graph is invalid", flow_guid); return false; }
	auto observer = [&, graph](const std::shared_ptr<CG::Core::ANode>& base, const ::Result& result) {
		auto node = std::dynamic_pointer_cast<CG::Core::VNode>(base);
		if (!node || graph->FindNode(node->GetGuid()) != node) return;
		json pins = json::array();
		auto append_pin = [&](const std::shared_ptr<CG::Core::APin>& pin) {
			if (!pin || pin->GetPinMeta().kind != CG::Core::PinKind::Data) return;
			pins.push_back({{g_key_guid, pin->GetGuid()},
				{g_key_value, CG::Core::ToString(pin->GetFrame())}});
		};
		for (const auto& pin : node->GetInputPins()) append_pin(pin);
		for (const auto& pin : node->GetOutputPins()) append_pin(pin);
		send_runtime("node", node->GetGuid(), result.code == 0 ? "Ok" : "Error", std::move(pins), result.code);
	};
	std::vector<std::shared_ptr<CG::Core::VNode>> observed_nodes;
	for (const auto& [guid, node] : graph->GetNodes()) {
		(void)guid;
		if (auto vnode = std::dynamic_pointer_cast<CG::Core::VNode>(node)) {
			vnode->SetWorkObserver(observer);
			observed_nodes.push_back(std::move(vnode));
		}
	}
	struct ObserverReset {
		std::vector<std::shared_ptr<CG::Core::VNode>>& nodes;
		~ObserverReset() {
			for (const auto& node : nodes) if (node) node->SetWorkObserver({});
		}
	} observer_reset{observed_nodes};
	const int result = flow->Run();
	Message reply = msg;
	reply.code = result == 0 ? 0 : 1;
	reply.data = json{{g_key_asset, asset_guid}, {g_key_flow, flow_guid},
		{g_key_flow_status, result == 0 ? "Completed" : "Failed"},
		{g_key_error, result == 0 ? std::string{} : std::string("flow execution failed")}}.dump();
	if (result == 0)
		RiseOutput(rise_, msg, "Flow execution completed.", "Engine");
	rise_(reply);
	Persist(asset_it->second);
	send_runtime("finished", {}, result == 0 ? "Completed" : "Failed", json::array(), result,
		VariableRows(*asset_it->second));
	return result == 0;
}

}
