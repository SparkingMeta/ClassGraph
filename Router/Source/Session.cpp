#include "Session.h"

#include "Base/VClass.h"
#include "Base/VSystem.h"
#include "Base/VType.h"
#include "Realistic/VAsset.h"
#include "Realistic/VFlow.h"
#include "Realistic/VGraph.h"
#include "Realistic/VNode.h"
#include "Realistic/VOperator.h"
#include "Realistic/VVariant.h"
#include "Realistic/VVarFunction.h"
#include "Realistic/VLine.h"
#include "Realistic/VController.h"
#include "Abstract/AContext.h"
#include "Abstract/ACaller.h"
#include "Abstract/AFlow.h"
#include "Abstract/AFrame.h"
#include "Abstract/AFunction.h"
#include "Abstract/AScript.h"
#include "Abstract/APin.h"
#include "Abstract/ALine.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <functional>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <unordered_set>

#ifdef _WIN32
#include <windows.h>
#endif

namespace CG::Router {
using namespace CG::Core;
namespace fs = std::filesystem;

	std::string Trim(std::string s)
	{
		while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
			s.erase(s.begin());
		while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
			s.pop_back();
		return s;
	}

	void TryRemoveFile(const fs::path& path)
	{
		std::error_code ec;
		for (int i = 0; i < 10; ++i) {
			ec.clear();
			if (!fs::exists(path, ec))
				return;
			ec.clear();
			if (fs::remove(path, ec) && !ec)
				return;
#ifdef _WIN32
			Sleep(80);
#endif
		}
	}


	std::string KindLabel(int kind)
	{
		switch (kind) {
		case CK_Controller: return "Flow Control";
		case CK_Operator: return "Utilities";
		case CK_Script: return "Scripts";
		default: return "Functions";
		}
	}

	DtoEditKind EditKindOf(const VType* type)
	{
		if (!type)
			return DtoEditKind::None;
		const auto& name = type->name_;
		if (name == "bool") return DtoEditKind::Bool;
		if (name == "int") return DtoEditKind::Int;
		if (name == "int64") return DtoEditKind::Int64;
		if (name == "float") return DtoEditKind::Float;
		if (name == "double") return DtoEditKind::Double;
		if (name == "std::string") return DtoEditKind::String;
		return DtoEditKind::None;
	}

	std::string FormatFloatText(double v, int precision)
	{
		std::ostringstream oss;
		oss << std::setprecision(precision) << v;
		std::string text = oss.str();
		if (text.find('.') == std::string::npos && text.find('e') == std::string::npos
			&& text.find('E') == std::string::npos)
			text += ".0";
		return text;
	}

	std::string ReadFrameText(const std::shared_ptr<AFrame>& frame)
	{
		if (!frame || !frame->GetType())
			return {};
		const VType* type = frame->GetType();
		switch (EditKindOf(type)) {
		case DtoEditKind::Int: {
			int v = 0;
			if (frame->GetValue(type, &v))
				return std::to_string(v);
			break;
		}
		case DtoEditKind::Int64: {
			int64 v = 0;
			if (frame->GetValue(type, &v))
				return std::to_string(v);
			break;
		}
		case DtoEditKind::Float: {
			float v = 0.f;
			if (frame->GetValue(type, &v))
				return FormatFloatText(v, 9);
			break;
		}
		case DtoEditKind::Double: {
			double v = 0.0;
			if (frame->GetValue(type, &v))
				return FormatFloatText(v, 17);
			break;
		}
		case DtoEditKind::String: {
			std::string v;
			if (frame->GetValue(type, &v))
				return v;
			break;
		}
		case DtoEditKind::Bool: {
			bool v = false;
			if (frame->GetValue(type, &v))
				return v ? "true" : "false";
			break;
		}
		default:
			break;
		}
		if (type->serialize_ && frame->GetBuffer()) {
			std::string text;
			if (type->serialize_(frame->GetBuffer(), text))
				return text;
		}
		return {};
	}

	bool WriteFrameText(const std::shared_ptr<AFrame>& frame, const std::string& text)
	{
		if (!frame || !frame->GetType())
			return false;
		const VType* type = frame->GetType();
		const std::string value = Trim(text);
		try {
			switch (EditKindOf(type)) {
			case DtoEditKind::Int: {
				int v = std::stoi(value);
				return frame->SetValue(type, &v);
			}
			case DtoEditKind::Int64: {
				int64 v = static_cast<int64>(std::stoll(value));
				return frame->SetValue(type, &v);
			}
			case DtoEditKind::Float: {
				float v = std::stof(value);
				return frame->SetValue(type, &v);
			}
			case DtoEditKind::Double: {
				double v = std::stod(value);
				return frame->SetValue(type, &v);
			}
			case DtoEditKind::String: {
				std::string v = text;
				return frame->SetValue(type, &v);
			}
			case DtoEditKind::Bool: {
				bool v = false;
				if (value == "1" || value == "true" || value == "True" || value == "TRUE")
					v = true;
				else if (value == "0" || value == "false" || value == "False" || value == "FALSE")
					v = false;
				else
					return false;
				return frame->SetValue(type, &v);
			}
			default:
				if (type->deserialize_ && frame->GetBuffer())
					return type->deserialize_(frame->GetBuffer(), value);
				return false;
			}
		} catch (...) {
			return false;
		}
	}

	bool PinConnected(const std::shared_ptr<APin>& pin)
	{
		return pin && !pin->GetLines().empty();
	}

	bool PinEditable(const std::shared_ptr<APin>& pin)
	{
		if (!pin || pin->GetDirect() != EDirect::In)
			return false;
		if (pin->GetPinMeta().kind != PinKind::Data)
			return false;
		if (PinConnected(pin))
			return false;
		auto frame = pin->GetFrame();
		const VType* type = frame ? frame->GetType() : pin->GetPinMeta().type;
		return frame && EditKindOf(type) != DtoEditKind::None;
	}

	bool IsCaseLiteralPin(const std::shared_ptr<APin>& pin)
	{
		if (!pin || pin->GetDirect() != EDirect::Out)
			return false;
		const auto& meta = pin->GetPinMeta();
		return meta.kind == PinKind::Exec && meta.type != nullptr;
	}

	std::shared_ptr<AFrame> PinValueFrame(const std::shared_ptr<APin>& pin)
	{
		if (!pin)
			return {};
		if (auto frame = pin->GetFrame())
			return frame;
		auto node = pin->GetOwner();
		if (!node || !node->GetContext())
			return {};
		return node->GetContext()->GetFrame(pin->GetPinMeta().name);
	}

	PinDto BuildPinDto(const std::shared_ptr<APin>& pin)
	{
		PinDto dto;
		if (!pin)
			return dto;
		const auto& meta = pin->GetPinMeta();
		dto.id = pin->GetGuid();
		dto.name = meta.name;
		dto.kind = meta.kind == PinKind::Exec ? DtoPinKind::Exec : DtoPinKind::Data;
		dto.direction = pin->GetDirect() == EDirect::Out ? DtoPinDir::Out : DtoPinDir::In;
		dto.type_name = meta.type ? meta.type->name_ : "";
		dto.connected = PinConnected(pin);
		auto frame = PinValueFrame(pin);
		const VType* type = frame ? frame->GetType() : meta.type;
		dto.editable = PinEditable(pin) || IsCaseLiteralPin(pin);
		dto.edit_kind = EditKindOf(type);
		dto.value_text = ReadFrameText(frame);
		return dto;
	}

	nlohmann::json ToJson(const PinDto& p)
	{
		return nlohmann::json{
			{"id", p.id},
			{"name", p.name},
			{"kind", static_cast<int>(p.kind)},
			{"direction", static_cast<int>(p.direction)},
			{"type_name", p.type_name},
			{"connected", p.connected},
			{"editable", p.editable},
			{"edit_kind", static_cast<int>(p.edit_kind)},
			{"value_text", p.value_text},
		};
	}

	nlohmann::json ToJson(const NodeDto& n)
	{
		nlohmann::json pins = nlohmann::json::array();
		for (const auto& p : n.pins)
			pins.push_back(ToJson(p));
		return nlohmann::json{
			{"guid", n.guid},
			{"name", n.name},
			{"title", n.title},
			{"caller_name", n.caller_name},
			{"caller_kind", n.caller_kind},
			{"is_pure", n.is_pure},
			{"is_entry", n.is_entry},
			{"is_exit", n.is_exit},
			{"x", n.x},
			{"y", n.y},
			{"pins", std::move(pins)},
		};
	}

	nlohmann::json ToJson(const LineDto& l)
	{
		return nlohmann::json{
			{"id", l.id},
			{"from_pin_id", l.from_pin_id},
			{"to_pin_id", l.to_pin_id},
			{"is_exec", l.is_exec},
		};
	}

	nlohmann::json ToJson(const GraphDto& g)
	{
		nlohmann::json nodes = nlohmann::json::array();
		nlohmann::json lines = nlohmann::json::array();
		for (const auto& n : g.nodes)
			nodes.push_back(ToJson(n));
		for (const auto& l : g.lines)
			lines.push_back(ToJson(l));
		return nlohmann::json{
			{"flow_guid", g.flow_guid},
			{"entry_node_id", g.entry_node_id},
			{"exit_node_id", g.exit_node_id},
			{"has_exit", g.has_exit},
			{"nodes", std::move(nodes)},
			{"lines", std::move(lines)},
		};
	}

	nlohmann::json ToJson(const BlueprintDto& b)
	{
		nlohmann::json flows = nlohmann::json::array();
		for (const auto& f : b.flows)
			flows.push_back({{"guid", f.guid}, {"name", f.name}});
		nlohmann::json vars = nlohmann::json::array();
		for (const auto& v : b.vars)
			vars.push_back({{"guid", v.guid}, {"name", v.name}, {"type_name", v.type_name}});
		nlohmann::json types = nlohmann::json::array();
		for (const auto& t : b.types)
			types.push_back({{"name", t.name}});
		return nlohmann::json{
			{"guid", b.guid},
			{"space", b.space},
			{"name", b.name},
			{"current_flow_guid", b.current_flow_guid},
			{"current_flow_name", b.current_flow_name},
			{"self_type_name", b.self_type_name},
			{"flows", std::move(flows)},
			{"vars", std::move(vars)},
			{"types", std::move(types)},
		};
	}

	nlohmann::json ToJson(const PaletteDto& p)
	{
		nlohmann::json items = nlohmann::json::array();
		for (const auto& c : p.items) {
			items.push_back({
				{"key", c.key},
				{"name", c.name},
				{"group", c.group},
				{"kind", c.kind},
				{"is_pure", c.is_pure},
			});
		}
		return nlohmann::json{{"items", std::move(items)}};
	}

	nlohmann::json ToJson(const NodeDetailsDto& d)
	{
		nlohmann::json params = nlohmann::json::array();
		for (const auto& p : d.params)
			params.push_back({{"name", p.name}, {"type_name", p.type_name}});
		nlohmann::json types = nlohmann::json::array();
		for (const auto& t : d.type_names)
			types.push_back(t);
		nlohmann::json pins = nlohmann::json::array();
		for (const auto& p : d.pins)
			pins.push_back(ToJson(p));
		return nlohmann::json{
			{"kind", "node"},
			{"node_guid", d.node_guid},
			{"title", d.title},
			{"kind_label", d.kind_label},
			{"caller_name", d.caller_name},
			{"is_entry", d.is_entry},
			{"is_exit", d.is_exit},
			{"can_edit_params", d.can_edit_params},
			{"params", std::move(params)},
			{"pins", std::move(pins)},
			{"type_names", std::move(types)},
		};
	}

	nlohmann::json ToJson(const VarDetailsDto& d)
	{
		nlohmann::json types = nlohmann::json::array();
		for (const auto& t : d.type_names)
			types.push_back(t);
		return nlohmann::json{
			{"kind", "var"},
			{"guid", d.guid},
			{"name", d.name},
			{"type_name", d.type_name},
			{"value_text", d.value_text},
			{"edit_kind", static_cast<int>(d.edit_kind)},
			{"type_names", std::move(types)},
		};
	}

	std::string QuoteArg(const std::string& s)
	{
		if (s.find_first_of(" \t\"") == std::string::npos)
			return s;
		std::string out = "\"";
		for (char c : s) {
			if (c == '"')
				out += "\\\"";
			else
				out.push_back(c);
		}
		out.push_back('"');
		return out;
	}

	bool RunProcess(const std::string& exe, const std::vector<std::string>& args,
		std::string& output, int& exit_code)
	{
		std::ostringstream cmd;
		cmd << QuoteArg(exe);
		for (const auto& a : args)
			cmd << ' ' << QuoteArg(a);
		const std::string cmdline = cmd.str();

#ifdef _WIN32
		SECURITY_ATTRIBUTES sa{};
		sa.nLength = sizeof(sa);
		sa.bInheritHandle = TRUE;
		HANDLE read_pipe = nullptr;
		HANDLE write_pipe = nullptr;
		if (!CreatePipe(&read_pipe, &write_pipe, &sa, 0))
			return false;
		SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0);

		STARTUPINFOA si{};
		si.cb = sizeof(si);
		si.dwFlags = STARTF_USESTDHANDLES;
		si.hStdOutput = write_pipe;
		si.hStdError = write_pipe;
		si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

		PROCESS_INFORMATION pi{};
		std::vector<char> mutable_cmd(cmdline.begin(), cmdline.end());
		mutable_cmd.push_back('\0');
		const BOOL ok = CreateProcessA(
			nullptr,
			mutable_cmd.data(),
			nullptr,
			nullptr,
			TRUE,
			CREATE_NO_WINDOW,
			nullptr,
			nullptr,
			&si,
			&pi);
		CloseHandle(write_pipe);
		if (!ok) {
			CloseHandle(read_pipe);
			return false;
		}

		char buf[4096];
		DWORD read = 0;
		while (ReadFile(read_pipe, buf, sizeof(buf), &read, nullptr) && read > 0)
			output.append(buf, buf + read);
		CloseHandle(read_pipe);

		WaitForSingleObject(pi.hProcess, 180000);
		DWORD code = 1;
		GetExitCodeProcess(pi.hProcess, &code);
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
		exit_code = static_cast<int>(code);
		return true;
#else
		FILE* pipe = popen(cmdline.c_str(), "r");
		if (!pipe)
			return false;
		char buf[4096];
		while (fgets(buf, sizeof(buf), pipe))
			output += buf;
		exit_code = pclose(pipe);
		return true;
#endif
	}

	std::string UniqueParamName(const CallMeta& meta, const std::string& base)
	{
		auto taken = [&](const std::string& name) {
			for (const auto& arg : meta.args) {
				if (arg.name == name)
					return true;
			}
			return false;
		};
		if (!taken(base))
			return base;
		for (int i = 1; i < 1000; ++i) {
			const std::string name = base + "_" + std::to_string(i);
			if (!taken(name))
				return name;
		}
		return base + "_x";
	}

	void InitSwitchCaseValue(const std::shared_ptr<VNode>& node, const std::string& new_name)
	{
		if (!node || !node->GetContext() || !node->GetCaller())
			return;
		int next = 0;
		for (const auto& arg : node->GetCaller()->GetCallMeta().args) {
			if (!arg.is_output || !arg.type || arg.name == new_name || arg.name == "default")
				continue;
			const std::string text = ReadFrameText(node->GetContext()->GetFrame(arg.name));
			if (text.empty())
				continue;
			try {
				const int v = std::stoi(text) + 1;
				if (v > next)
					next = v;
			} catch (...) {
			}
		}
		WriteFrameText(node->GetContext()->GetFrame(new_name), std::to_string(next));
	}

	void SyncScriptCallers(const std::shared_ptr<VBlueprint>& bp, const std::shared_ptr<AFlow>& callee,
		const std::string& renamed_from = {}, const std::string& renamed_to = {})
	{
		if (!bp || !callee)
			return;
		auto script = callee->GetScript();
		if (!script)
			return;
		for (const auto& [guid, flow] : bp->GetFlows()) {
			(void)guid;
			if (!flow)
				continue;
			auto graph = std::dynamic_pointer_cast<VGraph>(flow->GetGraph());
			if (!graph)
				continue;
			for (const auto& [ng, anode] : graph->GetNodes()) {
				(void)ng;
				auto vnode = std::dynamic_pointer_cast<VNode>(anode);
				if (!vnode || vnode->GetCaller().get() != script.get())
					continue;
				vnode->ApplyMeta(renamed_from, renamed_to);
			}
		}
	}

	nlohmann::json Body(const Message& msg)
	{
		return MsgBody(msg);
	}

Session::Session(HostConfig config)
	: config_(std::move(config))
{
}

Session::~Session() = default;

void Session::SetRise(std::function<void(const Message&)> rise)
{
	rise_ = std::move(rise);
}

void Session::Rise(Action action, std::string token, std::string data)
{
	if (!rise_)
		return;
	rise_(MakeMessage(action, std::move(token), std::move(data)));
}

void Session::Reply(const Message& req, nlohmann::json body)
{
	if (!rise_)
		return;
	rise_(MakeReply(req, std::move(body)));
}

bool Session::Handle(const Message& request, Message*)
{
	switch (static_cast<Action>(request.act)) {
	case Action::CreateBP: CreateBP(request); break;
	case Action::DeleteBP: DeleteBP(request); break;
	case Action::RenameBP: RenameBP(request); break;
	case Action::SetSpace: SetSpace(request); break;
	case Action::OpenBP: OpenBP(request); break;
	case Action::ListAssets: ListAssets(request); break;
	case Action::CloseBP: CloseBP(request); break;
	case Action::AddFlow: AddFlow(request); break;
	case Action::DelFlow: DelFlow(request); break;
	case Action::RenameFlow: RenameFlow(request); break;
	case Action::SelectFlow: SelectFlow(request); break;
	case Action::RunFlow: RunFlow(request); break;
	case Action::AddVar: AddVar(request); break;
	case Action::DelVar: DelVar(request); break;
	case Action::RenameVar: RenameVar(request); break;
	case Action::SetVarType: SetVarType(request); break;
	case Action::SetVarValue: SetVarValue(request); break;
	case Action::SelectVar: SelectVar(request); break;
	case Action::PlaceVarGet: PlaceGet(request); break;
	case Action::PlaceVarSet: PlaceSet(request); break;
	case Action::AddNode: AddNode(request); break;
	case Action::DelNode: DelNode(request); break;
	case Action::MoveNode: MoveNode(request); break;
	case Action::SelectNode: SelectNode(request); break;
	case Action::AddExitNode: AddExit(request); break;
	case Action::ConnectPin: ConnectPin(request); break;
	case Action::DisconnectPin: DisconnectPin(request); break;
	case Action::DisconnectLine: DisconnectLine(request); break;
	case Action::SetPinValue: SetPinValue(request); break;
	case Action::AddParam: AddParam(request); break;
	case Action::DelParam: DelParam(request); break;
	case Action::RenameParam: RenameParam(request); break;
	case Action::SetParamType: SetParamType(request); break;
	case Action::GenerateCode: GenerateCode(request); break;
	case Action::CompileType:
		if (Compile()) {
			Reply(request, {{"ok", true}});
			NotifyPalette();
		} else {
			Reply(request, {{"ok", false}});
		}
		break;
	case Action::LoadType: {
		const auto body = Body(request);
		const std::string path = JsonStr(body, "path");
		if (path.empty()) {
			Print("LoadType: empty path");
			Reply(request, {{"ok", false}});
			break;
		}
		auto* cls = VSystem::Get().LoadDll(path);
		if (!cls) {
			Print("LoadType failed: " + path);
			Reply(request, {{"ok", false}, {"path", path}});
			break;
		}
		Print("Loaded type " + cls->GetSpace() + "::" + cls->GetName());
		Reply(request, {{"ok", true}, {"guid", cls->GetSpace() + "::" + cls->GetName()}});
		NotifyPalette();
		break;
	}
	case Action::UnloadType: {
		const auto body = Body(request);
		std::string key = JsonStr(body, "guid");
		if (key.empty())
			key = SelfType();
		if (key.empty()) {
			Print("UnloadType: empty class key");
			Reply(request, {{"ok", false}});
			break;
		}
		VSystem::Get().UnloadDll(key);
		Print("Unloaded type " + key);
		Reply(request, {{"ok", true}, {"guid", key}});
		NotifyPalette();
		break;
	}
	default:
		return false;
	}
	return true;
}

void Session::CreateBP(const Message& msg)
{
	const auto body = Body(msg);
	std::string space = Trim(JsonStr(body, "space"));
	std::string name = Trim(JsonStr(body, "name"));
	if (space.empty())
		space = "user";
	if (name.empty())
		name = "NewBlueprint";
	if (!IsIdent(space) || !IsIdent(name)) {
		Print("CreateBP: space/name must be valid C++ identifiers");
		Reply(msg, {{"ok", false}});
		return;
	}

	MakeAssetDir();
	const std::string path = AssetPath(space, name);
	if (fs::exists(path)) {
		Print("Asset already exists: " + space + "::" + name);
		NotifyAssets();
		Reply(msg, {{"ok", false}, {"path", path}});
		return;
	}
	if (!WriteAsset(space, name)) {
		Print("Failed to create asset file: " + path);
		Reply(msg, {{"ok", false}, {"path", path}});
		return;
	}

	NotifyAssets();
	Print("Created asset " + space + "::" + name);
	Reply(msg, {{"ok", true}, {"path", path}, {"name", name}, {"space", space}});
	OpenBP(MakeMessage(Action::OpenBP, {}, nlohmann::json{{"path", path}}.dump()));
}

void Session::DeleteBP(const Message& msg)
{
	const auto body = Body(msg);
	std::string path = Trim(JsonStr(body, "path"));
	if (path.empty())
		path = Trim(JsonStr(body, "guid"));
	if (path.empty()) {
		Print("DeleteBP: empty path");
		Reply(msg, {{"ok", false}});
		return;
	}

	std::error_code ec;
	if (fs::exists(path))
		fs::remove(path, ec);

	if (!current_asset_path_.empty() && current_asset_path_ == path)
		Clear();

	NotifyAssets();
	Print("Deleted asset " + path);
	Reply(msg, {{"ok", true}, {"path", path}});
}

void Session::OpenBP(const Message& msg)
{
	const auto body = Body(msg);
	std::string path = Trim(JsonStr(body, "path"));
	if (path.empty()) {
		Print("OpenBP: empty path");
		Reply(msg, {{"ok", false}});
		return;
	}

	std::string space;
	std::string name;
	if (!ReadAsset(path, space, name)) {
		Print("OpenBP: failed to read " + path);
		Reply(msg, {{"ok", false}, {"path", path}});
		return;
	}
	if (!IsIdent(space) || !IsIdent(name)) {
		Print("OpenBP: invalid space/name in asset");
		Reply(msg, {{"ok", false}});
		return;
	}

	blueprint_ = std::make_shared<VBlueprint>();
	blueprint_->SetSpace(space);
	blueprint_->SetName(name);
	current_flow_guid_.clear();
	selected_node_guid_.clear();
	current_asset_path_ = path;

	const std::string guid = blueprint_->AddFlow("EventGraph");
	if (auto flow = blueprint_->FindFlow(guid)) {
		MakeEntry(flow);
		current_flow_guid_ = guid;
	}

	NotifyAll();
	Print("Opened Blueprint " + space + "::" + name);
	Reply(msg, {{"ok", true}, {"guid", blueprint_->GetGuid()}, {"name", name}, {"space", space}});
}

void Session::ListAssets(const Message& msg)
{
	MakeAssetDir();
	NotifyAssets();
	Reply(msg, {{"ok", true}});
}

void Session::CloseBP(const Message& msg)
{
	Clear();
	Print("Blueprint closed");
	Reply(msg, {{"ok", true}});
}

void Session::RenameBP(const Message& msg)
{
	if (!blueprint_)
		return;
	const std::string name = Trim(JsonStr(Body(msg), "name"));
	if (name.empty() || name == blueprint_->GetName())
		return;
	if (!IsIdent(name)) {
		Print("Blueprint name must be a valid C++ identifier");
		Reply(msg, {{"ok", false}});
		return;
	}
	blueprint_->SetName(name);
	NotifyBp();
	Print("Blueprint renamed to " + name);
	Reply(msg, {{"ok", true}, {"guid", blueprint_->GetGuid()}, {"name", name}});
}

void Session::SetSpace(const Message& msg)
{
	if (!blueprint_)
		return;
	const std::string space = Trim(JsonStr(Body(msg), "space"));
	if (!IsIdent(space)) {
		Print("Namespace must be a valid C++ identifier");
		Reply(msg, {{"ok", false}});
		return;
	}
	blueprint_->SetSpace(space);
	NotifyBp();
	Print("Namespace set to " + space);
	Reply(msg, {{"ok", true}, {"guid", blueprint_->GetGuid()}, {"space", space}});
}

void Session::AddFlow(const Message& msg)
{
	if (!blueprint_)
		return;
	std::string name = Trim(JsonStr(Body(msg), "name"));
	if (name.empty())
		name = NewFlow("NewFunction");
	else if (blueprint_->FindFlowByName(name))
		name = NewFlow(name);

	const std::string guid = blueprint_->AddFlow(name);
	auto flow = blueprint_->FindFlow(guid);
	if (!flow)
		return;
	MakeEntry(flow);
	current_flow_guid_ = guid;
	selected_node_guid_.clear();
	NotifyPalette();
	NotifyGraph();
	NotifyDetails();
	Print("Added function " + name);
	Reply(msg, {{"ok", true}, {"guid", guid}, {"name", name}});
}

void Session::DelFlow(const Message& msg)
{
	if (!blueprint_)
		return;
	if (blueprint_->GetFlows().size() <= 1) {
		Print("Keep at least one function");
		return;
	}
	const std::string guid = JsonStr(Body(msg), "guid");
	if (guid.empty())
		return;
	if (guid == current_flow_guid_) {
		for (const auto& [id, flow] : blueprint_->GetFlows()) {
			if (id != guid && flow) {
				current_flow_guid_ = id;
				break;
			}
		}
	}
	blueprint_->DelFlow(guid);
	selected_node_guid_.clear();
	NotifyPalette();
	NotifyGraph();
	NotifyDetails();
	Print("Deleted function");
	Reply(msg, {{"ok", true}, {"guid", guid}, {"current", current_flow_guid_}});
}

void Session::RenameFlow(const Message& msg)
{
	if (!blueprint_)
		return;
	const auto body = Body(msg);
	const std::string guid = JsonStr(body, "guid");
	auto flow = blueprint_->FindFlow(guid);
	if (!flow)
		return;
	const std::string new_name = Trim(JsonStr(body, "name"));
	if (new_name.empty())
		return;
	if (auto other = blueprint_->FindFlowByName(new_name); other && other != flow) {
		Print("Function name already exists: " + new_name);
		Reply(msg, {{"ok", false}, {"guid", guid}});
		return;
	}
	flow->SetName(new_name);
	if (auto script = flow->GetScript())
		script->SetName(new_name);
	NotifyPalette();
	Reply(msg, {{"ok", true}, {"guid", guid}, {"name", new_name}});
}

void Session::SelectFlow(const Message& msg)
{
	if (!blueprint_)
		return;
	const std::string guid = JsonStr(Body(msg), "guid");
	if (guid.empty() || !blueprint_->FindFlow(guid))
		return;
	current_flow_guid_ = guid;
	selected_node_guid_.clear();
	NotifyPalette();
	NotifyGraph();
	NotifyDetails();
	const auto flow = blueprint_->FindFlow(guid);
	Reply(msg, {{"ok", true}, {"guid", guid}, {"name", flow ? flow->GetName() : ""}});
}

void Session::RunFlow(const Message& msg)
{
	auto flow = Flow();
	if (!flow) {
		Print("Select a graph first");
		Reply(msg, {{"ok", false}});
		return;
	}
	const int code = flow->Run(msg.token);
	if (code == 0)
		Print("Play " + flow->GetName() + ": succeeded");
	else
		Print("Play " + flow->GetName() + ": failed (code=" + std::to_string(code) + ")");
	Reply(msg, {{"ok", code == 0}, {"guid", flow->GetGuid()}});
}

void Session::AddVar(const Message& msg)
{
	if (!blueprint_)
		return;
	const auto body = Body(msg);
	std::string type_name = Trim(JsonStr(body, "type"));
	std::string name = Trim(JsonStr(body, "name"));
	if (type_name.empty())
		type_name = "int";
	if (name.empty())
		name = NewVar("NewVar");
	else
		name = NewVar(name);

	const std::string guid = blueprint_->AddVar(name, type_name);
	if (guid.empty()) {
		if (type_name == SelfType())
			Print("Cannot add member of self type " + type_name);
		else
			Print("AddVar failed, check type: " + type_name);
		Reply(msg, {{"ok", false}});
		return;
	}
	Print("Added variable " + name + " (" + type_name + ")");
	Reply(msg, {{"ok", true}, {"guid", guid}, {"name", name}, {"type", type_name}});
}

void Session::DelVar(const Message& msg)
{
	if (!blueprint_)
		return;
	const std::string guid = JsonStr(Body(msg), "guid");
	if (guid.empty())
		return;
	blueprint_->DelVar(guid);
	if (selected_node_guid_ == guid)
		selected_node_guid_.clear();
	NotifyDetails();
	Print("Deleted variable");
	Reply(msg, {{"ok", true}, {"guid", guid}});
}

void Session::RenameVar(const Message& msg)
{
	if (!blueprint_)
		return;
	const auto body = Body(msg);
	const std::string guid = JsonStr(body, "guid");
	auto var = blueprint_->FindVar(guid);
	if (!var)
		return;
	const std::string new_name = Trim(JsonStr(body, "name"));
	if (new_name.empty())
		return;
	for (const auto& [id, other] : blueprint_->GetVars()) {
		if (other && other != var && other->GetName() == new_name) {
			Print("Variable name already exists: " + new_name);
			Reply(msg, {{"ok", false}, {"guid", guid}});
			return;
		}
	}
	var->SetName(new_name);
	Reply(msg, {{"ok", true}, {"guid", guid}, {"name", new_name}});
}

void Session::SetVarType(const Message& msg)
{
	if (!blueprint_)
		return;
	const auto body = Body(msg);
	const std::string guid = JsonStr(body, "guid");
	auto var = blueprint_->FindVar(guid);
	if (!var)
		return;
	const std::string type_name = Trim(JsonStr(body, "type"));
	if (type_name.empty())
		return;
	if (type_name == SelfType()) {
		Print("Cannot change variable to self type " + type_name);
		Reply(msg, {{"ok", false}, {"guid", guid}});
		return;
	}
	const VType* type = VSystem::Get().FindType(type_name);
	if (!type) {
		Print("Unknown type: " + type_name);
		Reply(msg, {{"ok", false}, {"guid", guid}});
		return;
	}
	if (var->GetType() == type)
		return;
	auto vvar = std::dynamic_pointer_cast<VVariant>(var);
	if (!vvar)
		return;
	vvar->Reset(type);
	NotifyVar(guid);
	NotifyGraph();
	Print("Variable " + var->GetName() + " type -> " + type_name);
	Reply(msg, {{"ok", true}, {"guid", guid}, {"type", type_name}});
}

void Session::SetVarValue(const Message& msg)
{
	if (!blueprint_)
		return;
	const auto body = Body(msg);
	const std::string guid = JsonStr(body, "guid");
	auto var = blueprint_->FindVar(guid);
	if (!var) {
		Print("SetVarValue: variable not found");
		Reply(msg, {{"ok", false}});
		return;
	}
	if (!WriteFrameText(var->Getter(), JsonStr(body, "value"))) {
		Print("SetVarValue failed");
		NotifyVar(guid);
		Reply(msg, {{"ok", false}, {"guid", guid}});
		return;
	}
	Reply(msg, {{"ok", true}, {"guid", guid}});
}

void Session::SelectVar(const Message& msg)
{
	if (!blueprint_)
		return;
	const std::string guid = JsonStr(Body(msg), "guid");
	if (guid.empty() || !blueprint_->FindVar(guid))
		return;
	selected_node_guid_.clear();
	NotifyVar(guid);
	Reply(msg, {{"ok", true}, {"guid", guid}});
}

void Session::PlaceGet(const Message& msg)
{
	if (!blueprint_)
		return;
	const auto body = Body(msg);
	auto var = blueprint_->FindVar(JsonStr(body, "guid"));
	if (!var) {
		Print("Variable not found");
		Reply(msg, {{"ok", false}});
		return;
	}
	const float x = static_cast<float>(JsonNum(body, "x", 120));
	const float y = static_cast<float>(JsonNum(body, "y", 120));
	auto node = std::dynamic_pointer_cast<VNode>(MakeGetVarNode(var));
	if (!node) {
		Print("Cannot create Get node");
		Reply(msg, {{"ok", false}});
		return;
	}
	PlaceNode(node, x, y);
	Print("Placed Get " + var->GetName());
	Reply(msg, {{"ok", true}, {"guid", node->GetGuid()}});
}

void Session::PlaceSet(const Message& msg)
{
	if (!blueprint_)
		return;
	const auto body = Body(msg);
	auto var = blueprint_->FindVar(JsonStr(body, "guid"));
	if (!var) {
		Print("Variable not found");
		Reply(msg, {{"ok", false}});
		return;
	}
	const float x = static_cast<float>(JsonNum(body, "x", 120));
	const float y = static_cast<float>(JsonNum(body, "y", 120));
	auto node = std::dynamic_pointer_cast<VNode>(MakeSetVarNode(var));
	if (!node) {
		Print("Cannot create Set node");
		Reply(msg, {{"ok", false}});
		return;
	}
	PlaceNode(node, x, y);
	Print("Placed Set " + var->GetName());
	Reply(msg, {{"ok", true}, {"guid", node->GetGuid()}});
}

void Session::AddNode(const Message& msg)
{
	const auto body = Body(msg);
	std::string key = Trim(JsonStr(body, "key"));
	if (key.empty())
		key = Trim(JsonStr(body, "guid"));
	if (key.empty())
		return;
	const float x = static_cast<float>(JsonNum(body, "x", 120));
	const float y = static_cast<float>(JsonNum(body, "y", 120));
	PlaceKey(key, x, y);
	Reply(msg, {{"ok", true}});
}

void Session::DelNode(const Message& msg)
{
	auto graph = std::dynamic_pointer_cast<VGraph>(Graph());
	if (!graph)
		return;
	const std::string guid = JsonStr(Body(msg), "guid");
	auto node = FindNode(guid);
	if (!node)
		return;
	if (graph->GetEntry() == node) {
		Print("Cannot delete Entry node");
		return;
	}
	if (graph->GetExit() == node)
		graph->SetExit({});
	for (const auto& pin : node->GetInputPins())
		CG::Core::DisconnectPin(pin);
	for (const auto& pin : node->GetOutputPins())
		CG::Core::DisconnectPin(pin);
	graph->DelNode(guid);
	if (selected_node_guid_ == guid)
		selected_node_guid_.clear();
	NotifyGraph();
	NotifyDetails();
	Reply(msg, {{"ok", true}, {"guid", guid}});
}

void Session::MoveNode(const Message& msg)
{
	const auto body = Body(msg);
	auto node = FindNode(JsonStr(body, "guid"));
	if (!node)
		return;
	node->SetCoord({
		static_cast<float>(JsonNum(body, "x")),
		static_cast<float>(JsonNum(body, "y")),
	});
}

void Session::SelectNode(const Message& msg)
{
	const std::string guid = JsonStr(Body(msg), "guid");
	selected_node_guid_ = guid;
	if (guid.empty()) {
		NotifyDetails();
		Reply(msg, {{"ok", true}});
		return;
	}
	if (FindNode(guid)) {
		NotifyNode(guid);
		Reply(msg, {{"ok", true}, {"guid", guid}});
		return;
	}
	if (blueprint_ && blueprint_->FindVar(guid)) {
		NotifyVar(guid);
		Reply(msg, {{"ok", true}, {"guid", guid}});
		return;
	}
	selected_node_guid_.clear();
	NotifyDetails();
	Reply(msg, {{"ok", false}});
}

void Session::AddExit(const Message& msg)
{
	const auto body = Body(msg);
	const float x = static_cast<float>(JsonNum(body, "x", 420));
	const float y = static_cast<float>(JsonNum(body, "y", 180));
	PlaceKey("exit", x, y);
	Reply(msg, {{"ok", true}});
}

void Session::ConnectPin(const Message& msg)
{
	const auto body = Body(msg);
	auto from = FindPin(JsonStr(body, "from"));
	auto to = FindPin(JsonStr(body, "to"));
	if (!from || !to) {
		Print("ConnectPin: pin not found");
		return;
	}
	if (!ConnectPins(from, to)) {
		Print("ConnectPin failed");
		return;
	}
	NotifyGraph();
	if (!selected_node_guid_.empty())
		NotifyNode(selected_node_guid_);
	Reply(msg, {{"ok", true}, {"from", from->GetGuid()}, {"to", to->GetGuid()}});
}

void Session::DisconnectPin(const Message& msg)
{
	auto pin = FindPin(JsonStr(Body(msg), "guid"));
	if (!pin)
		return;
	CG::Core::DisconnectPin(pin);
	NotifyGraph();
	if (!selected_node_guid_.empty())
		NotifyNode(selected_node_guid_);
	Reply(msg, {{"ok", true}, {"guid", pin->GetGuid()}});
}

void Session::DisconnectLine(const Message& msg)
{
	const std::string line_id = JsonStr(Body(msg), "guid");
	if (line_id.empty())
		return;
	auto graph = std::dynamic_pointer_cast<VGraph>(Graph());
	if (!graph)
		return;
	for (const auto& [guid, anode] : graph->GetNodes()) {
		auto vnode = std::dynamic_pointer_cast<VNode>(anode);
		if (!vnode)
			continue;
		for (const auto& pin : vnode->GetOutputPins()) {
			if (!pin)
				continue;
			for (const auto& line : pin->GetLines()) {
				if (line && line->GetGuid() == line_id) {
					CG::Core::DisconnectLine(line);
					NotifyGraph();
					Reply(msg, {{"ok", true}, {"guid", line_id}});
					return;
				}
			}
		}
	}
	Reply(msg, {{"ok", false}, {"guid", line_id}});
}

void Session::SetPinValue(const Message& msg)
{
	const auto body = Body(msg);
	auto pin = FindPin(JsonStr(body, "guid"));
	if (!pin) {
		Print("SetPinValue: pin not found");
		Reply(msg, {{"ok", false}});
		return;
	}
	auto frame = PinValueFrame(pin);
	if (!WriteFrameText(frame, JsonStr(body, "value"))) {
		Print("SetPinValue failed");
		Reply(msg, {{"ok", false}, {"guid", pin->GetGuid()}});
		return;
	}
	// Do not NotifyGraph: rebuilding destroys QGraphicsProxyWidget editors mid-event (crash).
	if (!selected_node_guid_.empty())
		NotifyNode(selected_node_guid_);
	Reply(msg, {{"ok", true}, {"guid", pin->GetGuid()}});
}

void Session::AddParam(const Message& msg)
{
	const auto body = Body(msg);
	auto node = FindNode(JsonStr(body, "guid"));
	if (!node || !node->GetCaller()) {
		Reply(msg, {{"ok", false}});
		return;
	}

	if (auto* sw = dynamic_cast<FSwitchInt*>(node->GetCaller().get())) {
		const std::string name = sw->AddCase();
		if (name.empty()) {
			Print("AddCase failed");
			Reply(msg, {{"ok", false}, {"guid", node->GetGuid()}});
			return;
		}
		node->ApplyMeta();
		InitSwitchCaseValue(node, name);
		selected_node_guid_ = node->GetGuid();
		NotifyGraph();
		NotifyNode(selected_node_guid_);
		Print("Added case " + name);
		Reply(msg, {{"ok", true}, {"guid", node->GetGuid()}, {"name", name}});
		return;
	}
	std::string name = Trim(JsonStr(body, "name"));
	const std::string type_name = Trim(JsonStr(body, "type"));
	if (name.empty())
		name = UniqueParamName(node->GetCaller()->GetCallMeta(), "NewParam");
	if (!IsIdent(name)) {
		Print("Param name must be a valid C++ identifier");
		Reply(msg, {{"ok", false}, {"guid", node->GetGuid()}});
		return;
	}
	const VType* type = VSystem::Get().FindType(type_name);
	if (!type) {
		Print("Unknown type: " + type_name);
		Reply(msg, {{"ok", false}, {"guid", node->GetGuid()}});
		return;
	}
	auto* entry = dynamic_cast<FEntry*>(node->GetCaller().get());
	auto* exit = dynamic_cast<FExit*>(node->GetCaller().get());
	const bool ok = entry ? entry->AddParam(name, type)
		: exit ? exit->AddParam(name, type) : false;
	if (!ok) {
		Print("AddParam failed (duplicate name?)");
		Reply(msg, {{"ok", false}, {"guid", node->GetGuid()}});
		return;
	}
	node->ApplyMeta();
	SyncScriptCallers(blueprint_, Flow());
	selected_node_guid_ = node->GetGuid();
	NotifyGraph();
	NotifyNode(selected_node_guid_);
	Print("Added param " + name + " (" + type_name + ")");
	Reply(msg, {{"ok", true}, {"guid", node->GetGuid()}, {"name", name}, {"type", type_name}});
}

void Session::DelParam(const Message& msg)
{
	const auto body = Body(msg);
	auto node = FindNode(JsonStr(body, "guid"));
	if (!node || !node->GetCaller()) {
		Reply(msg, {{"ok", false}});
		return;
	}
	const std::string name = Trim(JsonStr(body, "name"));
	if (name.empty()) {
		Reply(msg, {{"ok", false}, {"guid", node->GetGuid()}});
		return;
	}
	auto* entry = dynamic_cast<FEntry*>(node->GetCaller().get());
	auto* exit = dynamic_cast<FExit*>(node->GetCaller().get());
	auto* sw = dynamic_cast<FSwitchInt*>(node->GetCaller().get());
	const bool ok = entry ? entry->DelParam(name)
		: exit ? exit->DelParam(name)
		: sw ? sw->DelCase(name) : false;
	if (!ok) {
		Reply(msg, {{"ok", false}, {"guid", node->GetGuid()}});
		return;
	}
	node->ApplyMeta();
	if (!sw)
		SyncScriptCallers(blueprint_, Flow());
	selected_node_guid_ = node->GetGuid();
	NotifyGraph();
	NotifyNode(selected_node_guid_);
	Print("Deleted param " + name);
	Reply(msg, {{"ok", true}, {"guid", node->GetGuid()}, {"name", name}});
}

void Session::RenameParam(const Message& msg)
{
	const auto body = Body(msg);
	auto node = FindNode(JsonStr(body, "guid"));
	if (!node || !node->GetCaller()) {
		Reply(msg, {{"ok", false}});
		return;
	}
	const std::string old_name = Trim(JsonStr(body, "old"));
	const std::string new_name = Trim(JsonStr(body, "name"));
	if (!IsIdent(new_name)) {
		Print("Param name must be a valid C++ identifier");
		Reply(msg, {{"ok", false}, {"guid", node->GetGuid()}});
		return;
	}
	auto* entry = dynamic_cast<FEntry*>(node->GetCaller().get());
	auto* exit = dynamic_cast<FExit*>(node->GetCaller().get());
	const bool ok = entry ? entry->RenameParam(old_name, new_name)
		: exit ? exit->RenameParam(old_name, new_name) : false;
	if (!ok) {
		Print("RenameParam failed");
		Reply(msg, {{"ok", false}, {"guid", node->GetGuid()}});
		return;
	}
	node->ApplyMeta(old_name, new_name);
	SyncScriptCallers(blueprint_, Flow(), old_name, new_name);
	selected_node_guid_ = node->GetGuid();
	NotifyGraph();
	NotifyNode(selected_node_guid_);
	Print("Param " + old_name + " renamed to " + new_name);
	Reply(msg, {{"ok", true}, {"guid", node->GetGuid()}, {"old", old_name}, {"name", new_name}});
}

void Session::SetParamType(const Message& msg)
{
	const auto body = Body(msg);
	auto node = FindNode(JsonStr(body, "guid"));
	if (!node || !node->GetCaller()) {
		Reply(msg, {{"ok", false}});
		return;
	}
	const std::string name = Trim(JsonStr(body, "name"));
	const std::string type_name = Trim(JsonStr(body, "type"));
	const VType* type = VSystem::Get().FindType(type_name);
	if (!type) {
		Print("Unknown type: " + type_name);
		Reply(msg, {{"ok", false}, {"guid", node->GetGuid()}});
		return;
	}
	auto* entry = dynamic_cast<FEntry*>(node->GetCaller().get());
	auto* exit = dynamic_cast<FExit*>(node->GetCaller().get());
	const bool ok = entry ? entry->SetParamType(name, type)
		: exit ? exit->SetParamType(name, type) : false;
	if (!ok) {
		Reply(msg, {{"ok", false}, {"guid", node->GetGuid()}});
		return;
	}
	node->ApplyMeta();
	SyncScriptCallers(blueprint_, Flow());
	selected_node_guid_ = node->GetGuid();
	NotifyGraph();
	NotifyNode(selected_node_guid_);
	Print("Param " + name + " type -> " + type_name);
	Reply(msg, {{"ok", true}, {"guid", node->GetGuid()}, {"name", name}, {"type", type_name}});
}

void Session::GenerateCode(const Message& msg)
{
	if (!blueprint_) {
		Reply(msg, {{"ok", false}});
		return;
	}

	std::string source, header, gen_header, meta;
	if (blueprint_->GenerateCode(source, header, gen_header, meta) != 0) {
		Print("Compile failed: GenerateCode");
		Reply(msg, {{"ok", false}});
		return;
	}

	std::string space = blueprint_->GetSpace();
	if (space.empty())
		space = "user";
	const std::string name = blueprint_->GetName();
	const fs::path src_dir = fs::path(config_.generated_root) / space;
	std::error_code ec;
	fs::create_directories(src_dir, ec);
	if (ec) {
		Print("Cannot create source dir: " + src_dir.string());
		Reply(msg, {{"ok", false}});
		return;
	}

	const fs::path stem = src_dir / name;
	if (!WriteFile((stem.string() + ".h"), header)
		|| !WriteFile((stem.string() + ".gen.h"), gen_header)
		|| !WriteFile((stem.string() + ".cpp"), source)
		|| !WriteFile((stem.string() + ".gen.cpp"), meta)) {
		Print("Failed to write generated sources");
		Reply(msg, {{"ok", false}});
		return;
	}
	Print("Wrote sources: " + stem.string() + "/{h,cpp,gen.h,gen.cpp}");

	if (!Compile()) {
		Reply(msg, {{"ok", false}, {"guid", blueprint_->GetGuid()}});
		return;
	}
	NotifyBp();
	NotifyPalette();
	Reply(msg, {{"ok", true}, {"guid", blueprint_->GetGuid()}});
}

void Session::MakeEntry(const std::shared_ptr<AFlow>& flow)
{
	if (!flow)
		return;
	auto graph = std::dynamic_pointer_cast<VGraph>(flow->GetGraph());
	if (!graph)
		return;
	if (graph->GetEntry())
		return;
	auto entry = std::make_shared<FEntry>();
	entry->SetName("Entry");
	auto node = VNode::Make(entry);
	node->SetName("Entry");
	node->SetCoord({ 80.f, 180.f });
	graph->AddNode(node);
	graph->SetEntry(node);
}

std::shared_ptr<AFlow> Session::Flow() const
{
	if (!blueprint_)
		return {};
	return blueprint_->FindFlow(current_flow_guid_);
}

std::shared_ptr<AGraph> Session::Graph() const
{
	auto flow = Flow();
	return flow ? flow->GetGraph() : std::shared_ptr<AGraph>{};
}

std::shared_ptr<VNode> Session::FindNode(const std::string& guid) const
{
	if (guid.empty())
		return {};
	auto graph = Graph();
	if (!graph)
		return {};
	return std::dynamic_pointer_cast<VNode>(graph->FindNode(guid));
}

std::shared_ptr<APin> Session::FindPin(const std::string& guid) const
{
	if (guid.empty())
		return {};
	auto graph = std::dynamic_pointer_cast<VGraph>(Graph());
	if (!graph)
		return {};
	for (const auto& [node_guid, anode] : graph->GetNodes()) {
		auto vnode = std::dynamic_pointer_cast<VNode>(anode);
		if (!vnode)
			continue;
		if (auto pin = vnode->GetPin(guid))
			return pin;
	}
	return {};
}

std::string Session::NewFlow(const std::string& base) const
{
	if (!blueprint_ || !blueprint_->FindFlowByName(base))
		return base;
	for (int i = 1; i < 1000; ++i) {
		const std::string name = base + "_" + std::to_string(i);
		if (!blueprint_->FindFlowByName(name))
			return name;
	}
	return base + "_x";
}

std::string Session::NewVar(const std::string& base) const
{
	auto exists = [&](const std::string& name) {
		if (!blueprint_)
			return false;
		for (const auto& [guid, var] : blueprint_->GetVars()) {
			if (var && var->GetName() == name)
				return true;
		}
		return false;
	};
	if (!exists(base))
		return base;
	for (int i = 1; i < 1000; ++i) {
		const std::string name = base + "_" + std::to_string(i);
		if (!exists(name))
			return name;
	}
	return base + "_x";
}

std::string Session::SelfType() const
{
	if (!blueprint_)
		return {};
	std::string space = blueprint_->GetSpace();
	if (space.empty())
		space = "user";
	const std::string name = blueprint_->GetName();
	if (name.empty())
		return {};
	return space + "::" + name;
}

bool Session::IsIdent(const std::string& name) const
{
	if (name.empty())
		return false;
	const unsigned char first = static_cast<unsigned char>(name[0]);
	if (!(std::isalpha(first) || name[0] == '_'))
		return false;
	for (char c : name) {
		const unsigned char uc = static_cast<unsigned char>(c);
		if (!(std::isalnum(uc) || c == '_'))
			return false;
	}
	return true;
}

void Session::PlaceNode(const std::shared_ptr<VNode>& node, float x, float y)
{
	auto graph = std::dynamic_pointer_cast<VGraph>(Graph());
	if (!graph || !node)
		return;
	node->SetCoord({ x, y });
	graph->AddNode(node);
	selected_node_guid_ = node->GetGuid();
	NotifyGraph();
	NotifyNode(selected_node_guid_);
}

void Session::PlaceKey(const std::string& key, float x, float y)
{
	auto graph = std::dynamic_pointer_cast<VGraph>(Graph());
	if (!graph)
		return;

	if (key == "exit") {
		if (graph->GetExit()) {
			Print("This graph already has a Return node");
			return;
		}
		auto exit_fn = std::make_shared<FExit>();
		exit_fn->SetName("Exit");
		auto node = VNode::Make(exit_fn);
		node->SetName("Return");
		node->SetCoord({ x, y });
		graph->AddNode(node);
		graph->SetExit(node);
		selected_node_guid_ = node->GetGuid();
		NotifyGraph();
		NotifyNode(selected_node_guid_);
		Print("Added Return node");
		return;
	}

	if (key.rfind("flow:", 0) == 0) {
		const std::string guid = key.substr(5);
		if (guid.empty() || guid == current_flow_guid_) {
			Print("Cannot call the current function from itself");
			return;
		}
		if (!blueprint_)
			return;
		auto flow = blueprint_->FindFlow(guid);
		if (!flow || !flow->GetScript()) {
			Print("Function not found");
			return;
		}
		auto script = std::static_pointer_cast<ACaller>(flow->GetScript());
		script->SetName(flow->GetName());
		auto node = VNode::Make(script);
		node->SetName(script->GetName());
		PlaceNode(node, x, y);
		Print("Place node: " + script->GetName());
		return;
	}

	if (key.rfind("method:", 0) == 0) {
		const std::string rest = key.substr(7);
		const auto slash = rest.rfind('/');
		if (slash == std::string::npos || slash == 0)
			return;
		const std::string class_key = rest.substr(0, slash);
		const std::string method_name = rest.substr(slash + 1);
		auto* cls = VSystem::Get().FindClass(class_key);
		if (!cls)
			return;
		auto method = cls->GetMethod(method_name);
		if (!method)
			return;
		auto caller = std::static_pointer_cast<ACaller>(method);
		auto node = VNode::Make(caller);
		node->SetName(caller->GetName());
		PlaceNode(node, x, y);
		Print("Place node: " + caller->GetName());
		return;
	}

	auto caller = VSystem::Get().FindCaller(key);
	if (!caller)
		return;
	if (dynamic_cast<FSwitchInt*>(caller.get())) {
		auto fresh = std::make_shared<FSwitchInt>();
		fresh->SetName(caller->GetName());
		caller = std::move(fresh);
	}
	auto node = VNode::Make(caller);
	node->SetName(caller->GetName());
	PlaceNode(node, x, y);
	Print("Place node: " + caller->GetName());
}

void Session::Print(const std::string& text)
{
	Rise(Action::Print, {}, text);
}

void Session::NotifyBp()
{
	const auto dto = MakeBp();
	Rise(Action::RefreshMyBlueprint, {}, ToJson(dto).dump());
}

void Session::NotifyPalette()
{
	const auto dto = MakePalette();
	Rise(Action::RefreshPalette, {}, ToJson(dto).dump());
}

void Session::NotifyGraph()
{
	const auto dto = MakeGraph();
	Rise(Action::RefreshGraph, {}, ToJson(dto).dump());
}

void Session::NotifyNode(const std::string& node_guid)
{
	auto node = FindNode(node_guid);
	if (!node) {
		NotifyDetails();
		return;
	}
	const auto dto = MakeNodeDetails(node);
	Rise(Action::RefreshDetails, {}, ToJson(dto).dump());
}

void Session::NotifyDetails()
{
	nlohmann::json j{{"kind", "blueprint"}};
	if (blueprint_) {
		j["guid"] = blueprint_->GetGuid();
		j["name"] = blueprint_->GetName();
		j["space"] = blueprint_->GetSpace();
		j["self_type_name"] = SelfType();
	}
	Rise(Action::RefreshDetails, {}, j.dump());
}

void Session::NotifyVar(const std::string& var_guid)
{
	if (!blueprint_)
		return;
	auto var = blueprint_->FindVar(var_guid);
	if (!var) {
		NotifyDetails();
		return;
	}
	VarDetailsDto dto;
	dto.guid = var_guid;
	dto.name = var->GetName();
	dto.type_name = var->GetType() ? var->GetType()->name_ : "?";
	dto.value_text = ReadFrameText(var->Getter());
	dto.edit_kind = EditKindOf(var->GetType());
	const std::string self = SelfType();
	for (const auto* t : VSystem::Get().GetTypes()) {
		if (!t || t->name_.empty())
			continue;
		if (!self.empty() && t->name_ == self)
			continue;
		dto.type_names.push_back(t->name_);
	}
	Rise(Action::RefreshDetails, {}, ToJson(dto).dump());
}

void Session::NotifyAll()
{
	NotifyBp();
	NotifyPalette();
	NotifyGraph();
	if (!selected_node_guid_.empty() && FindNode(selected_node_guid_))
		NotifyNode(selected_node_guid_);
	else if (!selected_node_guid_.empty() && blueprint_ && blueprint_->FindVar(selected_node_guid_))
		NotifyVar(selected_node_guid_);
	else
		NotifyDetails();
}

void Session::NotifyAssets()
{
	const auto dto = MakeAssets();
	nlohmann::json j;
	j["asset_root"] = dto.asset_root;
	j["items"] = nlohmann::json::array();
	for (const auto& item : dto.items) {
		j["items"].push_back({
			{"path", item.path},
			{"space", item.space},
			{"name", item.name},
			{"key", item.key},
		});
	}
	Rise(Action::RefreshAssets, {}, j.dump());
}

void Session::Clear()
{
	blueprint_.reset();
	current_flow_guid_.clear();
	selected_node_guid_.clear();
	current_asset_path_.clear();
	Rise(Action::RefreshMyBlueprint, {}, "{}");
	Rise(Action::RefreshPalette, {}, "{\"items\":[]}");
	Rise(Action::RefreshGraph, {}, "{}");
	Rise(Action::RefreshDetails, {}, "{\"kind\":\"none\"}");
}

AssetListDto Session::MakeAssets() const
{
	AssetListDto dto;
	dto.asset_root = config_.asset_root;
	if (dto.asset_root.empty())
		return dto;

	std::error_code ec;
	if (!fs::exists(dto.asset_root, ec))
		return dto;

	for (auto it = fs::recursive_directory_iterator(dto.asset_root, ec);
		it != fs::recursive_directory_iterator(); ++it) {
		if (ec)
			break;
		if (!it->is_regular_file(ec))
			continue;
		const auto& path = it->path();
		if (path.extension() != ".vbp")
			continue;
		std::string space;
		std::string name;
		if (!ReadAsset(path.string(), space, name))
			continue;
		AssetItemDto item;
		item.path = path.string();
		item.space = space;
		item.name = name;
		item.key = space + "::" + name;
		dto.items.push_back(std::move(item));
	}
	std::sort(dto.items.begin(), dto.items.end(),
		[](const AssetItemDto& a, const AssetItemDto& b) { return a.key < b.key; });
	return dto;
}

void Session::MakeAssetDir() const
{
	if (config_.asset_root.empty())
		return;
	std::error_code ec;
	fs::create_directories(config_.asset_root, ec);
}

std::string Session::AssetPath(const std::string& space, const std::string& name) const
{
	fs::path root = config_.asset_root.empty() ? fs::path("Content/Blueprints") : fs::path(config_.asset_root);
	return (root / space / (name + ".vbp")).string();
}

bool Session::WriteAsset(const std::string& space, const std::string& name) const
{
	const std::string path = AssetPath(space, name);
	std::error_code ec;
	fs::create_directories(fs::path(path).parent_path(), ec);
	nlohmann::json j{{"space", space}, {"name", name}};
	return WriteFile(path, j.dump(2));
}

bool Session::ReadAsset(const std::string& path, std::string& space, std::string& name) const
{
	std::ifstream in(path, std::ios::binary);
	if (!in)
		return false;
	std::ostringstream ss;
	ss << in.rdbuf();
	try {
		const auto j = nlohmann::json::parse(ss.str());
		space = j.value("space", "");
		name = j.value("name", "");
		if (space.empty() || name.empty()) {
			const fs::path p(path);
			if (name.empty())
				name = p.stem().string();
			if (space.empty() && p.has_parent_path())
				space = p.parent_path().filename().string();
		}
		return !space.empty() && !name.empty();
	} catch (...) {
		return false;
	}
}

GraphDto Session::MakeGraph() const
{
	GraphDto dto;
	dto.flow_guid = current_flow_guid_;
	auto graph = std::dynamic_pointer_cast<VGraph>(Graph());
	if (!graph)
		return dto;

	if (auto entry = graph->GetEntry())
		dto.entry_node_id = entry->GetGuid();
	if (auto exit = graph->GetExit()) {
		dto.exit_node_id = exit->GetGuid();
		dto.has_exit = true;
	}

	std::unordered_set<std::string> seen_lines;
	for (const auto& [guid, anode] : graph->GetNodes()) {
		auto vnode = std::dynamic_pointer_cast<VNode>(anode);
		if (!vnode)
			continue;

		NodeDto n;
		n.guid = vnode->GetGuid();
		n.name = vnode->GetName();
		n.title = vnode->GetName();
		const auto coord = vnode->GetCoord();
		n.x = coord.x;
		n.y = coord.y;
		if (auto caller = vnode->GetCaller()) {
			n.caller_name = caller->GetName();
			n.caller_kind = caller->GetKind();
			n.is_pure = caller->IsPure();
			n.is_entry = dynamic_cast<FEntry*>(caller.get()) != nullptr;
			n.is_exit = dynamic_cast<FExit*>(caller.get()) != nullptr;
			if (n.title.empty())
				n.title = caller->GetName();
		}
		for (const auto& pin : vnode->GetInputPins())
			n.pins.push_back(BuildPinDto(pin));
		for (const auto& pin : vnode->GetOutputPins())
			n.pins.push_back(BuildPinDto(pin));
		dto.nodes.push_back(std::move(n));

		for (const auto& pin : vnode->GetOutputPins()) {
			if (!pin)
				continue;
			for (const auto& line : pin->GetLines()) {
				if (!line)
					continue;
				const std::string lid = line->GetGuid();
				if (!seen_lines.insert(lid).second)
					continue;
				LineDto l;
				l.id = lid;
				l.from_pin_id = pin->GetGuid();
				if (auto to = line->GetTo())
					l.to_pin_id = to->GetGuid();
				l.is_exec = pin->GetPinMeta().kind == PinKind::Exec;
				dto.lines.push_back(std::move(l));
			}
		}
	}
	return dto;
}

BlueprintDto Session::MakeBp() const
{
	BlueprintDto dto;
	if (!blueprint_)
		return dto;
	dto.guid = blueprint_->GetGuid();
	dto.space = blueprint_->GetSpace();
	dto.name = blueprint_->GetName();
	dto.current_flow_guid = current_flow_guid_;
	dto.self_type_name = SelfType();
	if (auto flow = blueprint_->FindFlow(current_flow_guid_))
		dto.current_flow_name = flow->GetName();

	for (const auto& [guid, flow] : blueprint_->GetFlows()) {
		if (!flow)
			continue;
		FlowItemDto item;
		item.guid = guid;
		item.name = flow->GetName();
		dto.flows.push_back(std::move(item));
	}
	for (const auto& [guid, var] : blueprint_->GetVars()) {
		if (!var)
			continue;
		VarItemDto item;
		item.guid = guid;
		item.name = var->GetName();
		item.type_name = var->GetType() ? var->GetType()->name_ : "?";
		dto.vars.push_back(std::move(item));
	}
	const std::string self = dto.self_type_name;
	for (const auto* t : VSystem::Get().GetTypes()) {
		if (!t || t->name_.empty())
			continue;
		if (!self.empty() && t->name_ == self)
			continue;
		dto.types.push_back(TypeItemDto{ t->name_ });
	}
	return dto;
}

PaletteDto Session::MakePalette() const
{
	PaletteDto dto;
	for (const auto& caller : VSystem::Get().ListCallers()) {
		if (!caller)
			continue;
		CallerItemDto item;
		item.key = caller->GetName();
		item.name = caller->GetName();
		item.group = KindLabel(caller->GetKind());
		item.kind = caller->GetKind();
		item.is_pure = caller->IsPure();
		dto.items.push_back(std::move(item));
	}

	{
		CallerItemDto item;
		item.key = "exit";
		item.name = "Return";
		item.group = "Flow Control";
		item.kind = CK_Controller;
		dto.items.push_back(std::move(item));
	}

	if (blueprint_) {
		for (const auto& [guid, flow] : blueprint_->GetFlows()) {
			if (!flow || guid == current_flow_guid_)
				continue;
			CallerItemDto item;
			item.key = "flow:" + guid;
			item.name = flow->GetName();
			item.group = "My Blueprint";
			item.kind = CK_Script;
			dto.items.push_back(std::move(item));
		}
	}

	for (VClass* cls : VSystem::Get().GetClasses()) {
		if (!cls)
			continue;
		auto methods = cls->GetMethods();
		if (methods.empty())
			continue;
		const std::string class_key = cls->GetSpace() + "::" + cls->GetName();
		for (const auto& method : methods) {
			if (!method)
				continue;
			CallerItemDto item;
			item.key = "method:" + class_key + "/" + method->GetName();
			item.name = method->GetName();
			item.group = class_key;
			item.kind = method->GetKind();
			item.is_pure = method->IsPure();
			dto.items.push_back(std::move(item));
		}
	}
	return dto;
}

NodeDetailsDto Session::MakeNodeDetails(const std::shared_ptr<VNode>& node) const
{
	NodeDetailsDto dto;
	if (!node)
		return dto;
	dto.node_guid = node->GetGuid();
	dto.title = node->GetName();
	if (auto caller = node->GetCaller()) {
		dto.caller_name = caller->GetName();
		dto.kind_label = KindLabel(caller->GetKind());
		dto.is_entry = dynamic_cast<FEntry*>(caller.get()) != nullptr;
		dto.is_exit = dynamic_cast<FExit*>(caller.get()) != nullptr;
		dto.can_edit_params = dto.is_entry || dto.is_exit;
		if (dto.can_edit_params) {
			for (const auto& arg : caller->GetCallMeta().args) {
				ParamDto p;
				p.name = arg.name;
				p.type_name = arg.type ? arg.type->name_ : "";
				dto.params.push_back(std::move(p));
			}
		}
	}
	for (const auto& pin : node->GetInputPins())
		dto.pins.push_back(BuildPinDto(pin));
	for (const auto& pin : node->GetOutputPins())
		dto.pins.push_back(BuildPinDto(pin));
	for (const auto* t : VSystem::Get().GetTypes()) {
		if (t && !t->name_.empty())
			dto.type_names.push_back(t->name_);
	}
	return dto;
}

bool Session::Compile()
{
	if (!blueprint_)
		return false;

	std::string space = blueprint_->GetSpace();
	if (space.empty())
		space = "user";
	const std::string name = blueprint_->GetName();
	const std::string class_key = space + "::" + name;

	const fs::path src_dir = fs::path(config_.generated_root) / space;
	const fs::path type_out = fs::path(config_.type_out_dir);
	const fs::path work = type_out / name;
	const fs::path build_dir = work / "build";
	const fs::path dll_path = type_out / (name + ".dll");

	VSystem::Get().UnloadDll(class_key);
#ifdef _WIN32
	Sleep(80);
#endif
	TryRemoveFile(dll_path);
	TryRemoveFile(type_out / (name + ".pdb"));
	TryRemoveFile(type_out / (name + ".ilk"));
	TryRemoveFile(type_out / (name + ".exp"));
	std::error_code ec;
	fs::create_directories(work, ec);
	fs::create_directories(type_out, ec);
	if (ec) {
		Print("Cannot create type output dir: " + type_out.string());
		return false;
	}

	const std::string src_dir_s = src_dir.generic_string();
	const std::string core_include = fs::path(config_.core_include).generic_string();
	const std::string core_lib = fs::path(config_.core_lib).generic_string();
	const std::string type_out_s = type_out.generic_string();
	const std::string self = SelfType();

	std::unordered_set<std::string> dep_keys;
	auto add_dep = [&](const VType* type) {
		if (!type || type->name_.empty() || type->name_ == self)
			return;
		if (!VSystem::Get().FindClass(type->name_))
			return;
		dep_keys.insert(type->name_);
	};
	for (const auto& [guid, var] : blueprint_->GetVars()) {
		if (var)
			add_dep(var->GetType());
	}
	for (const auto& [guid, flow] : blueprint_->GetFlows()) {
		if (!flow)
			continue;
		for (const auto& param : flow->GetFlowMeta().params)
			add_dep(param.type);
	}

	std::ostringstream link_libs;
	link_libs << "\"" << core_lib << "\"";
	for (const auto& key : dep_keys) {
		const auto pos = key.rfind("::");
		const std::string dep_name = (pos == std::string::npos) ? key : key.substr(pos + 2);
		const fs::path lib = type_out / (dep_name + ".lib");
		if (!fs::exists(lib)) {
			Print("Type import lib not found: " + lib.string());
			return false;
		}
		link_libs << " \"" << lib.generic_string() << "\"";
	}

#ifdef _WIN32
	const fs::path pdb_path = build_dir / (name + "_" + std::to_string(GetTickCount64()) + ".pdb");
#else
	const fs::path pdb_path = build_dir / (name + ".pdb");
#endif

	std::ostringstream cmake;
	cmake
		<< "cmake_minimum_required(VERSION 3.15)\n"
		<< "project(" << name << " LANGUAGES CXX)\n"
		<< "set(CMAKE_CXX_STANDARD 20)\n"
		<< "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n"
		<< "set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)\n"
		<< "add_library(" << name << " SHARED \"" << src_dir_s << "/" << name << ".cpp\" \""
		<< src_dir_s << "/" << name << ".gen.cpp\")\n"
		<< "target_include_directories(" << name << " PRIVATE \"" << core_include << "\" \""
		<< src_dir_s << "\")\n"
		<< "target_link_libraries(" << name << " PRIVATE " << link_libs.str() << ")\n"
		<< "if(MSVC)\n"
		<< "  target_compile_options(" << name << " PRIVATE /utf-8 /EHsc)\n"
		<< "  target_link_options(" << name << " PRIVATE /INCREMENTAL:NO \"/PDB:"
		<< pdb_path.generic_string() << "\")\n"
		<< "endif()\n"
		<< "set_target_properties(" << name << " PROPERTIES\n"
		<< "  RUNTIME_OUTPUT_DIRECTORY \"" << type_out_s << "\"\n"
		<< "  RUNTIME_OUTPUT_DIRECTORY_DEBUG \"" << type_out_s << "\"\n"
		<< "  RUNTIME_OUTPUT_DIRECTORY_RELEASE \"" << type_out_s << "\"\n"
		<< "  RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO \"" << type_out_s << "\"\n"
		<< "  RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL \"" << type_out_s << "\"\n"
		<< "  LIBRARY_OUTPUT_DIRECTORY \"" << type_out_s << "\"\n"
		<< "  LIBRARY_OUTPUT_DIRECTORY_DEBUG \"" << type_out_s << "\"\n"
		<< "  LIBRARY_OUTPUT_DIRECTORY_RELEASE \"" << type_out_s << "\"\n"
		<< "  LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO \"" << type_out_s << "\"\n"
		<< "  LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL \"" << type_out_s << "\"\n"
		<< "  ARCHIVE_OUTPUT_DIRECTORY \"" << type_out_s << "\"\n"
		<< "  ARCHIVE_OUTPUT_DIRECTORY_DEBUG \"" << type_out_s << "\"\n"
		<< "  ARCHIVE_OUTPUT_DIRECTORY_RELEASE \"" << type_out_s << "\"\n"
		<< "  ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO \"" << type_out_s << "\"\n"
		<< "  ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL \"" << type_out_s << "\"\n"
		<< ")\n";

	if (!WriteFile((work / "CMakeLists.txt").string(), cmake.str())) {
		Print("Cannot write type project CMakeLists.txt");
		return false;
	}

#ifdef _WIN32
	if (!config_.cmake_generator.empty())
		_putenv_s("CMAKE_GENERATOR", config_.cmake_generator.c_str());
#endif

	auto run_cmake = [&](const std::vector<std::string>& args) -> bool {
		std::string out;
		int code = 1;
		const std::string exe = config_.cmake_command.empty() ? "cmake" : config_.cmake_command;
		if (!RunProcess(exe, args, out, code)) {
			Print("Cannot start cmake: " + exe);
			return false;
		}
		if (!out.empty())
			Print(out);
		if (code != 0) {
			Print("cmake failed (code=" + std::to_string(code) + ")");
			return false;
		}
		return true;
	};

	std::vector<std::string> configure{
		"-S", work.string(),
		"-B", build_dir.string(),
	};
	if (!config_.cmake_generator.empty()) {
		configure.push_back("-G");
		configure.push_back(config_.cmake_generator);
	}
	if (!config_.cmake_platform.empty()) {
		configure.push_back("-A");
		configure.push_back(config_.cmake_platform);
	}
	if (!run_cmake(configure))
		return false;

	const std::string build_config = config_.build_config.empty() ? "Debug" : config_.build_config;
	std::vector<std::string> build_args{
		"--build", build_dir.string(),
		"--config", build_config,
		"--target", name,
	};
	if (!run_cmake(build_args))
		return false;

	if (!fs::exists(dll_path)) {
		Print("Type DLL not found: " + dll_path.string());
		return false;
	}

	auto* cls = VSystem::Get().LoadDll(dll_path.string());
	if (!cls) {
		Print("LoadDll failed: " + dll_path.string());
		return false;
	}
	Print("Loaded type " + cls->GetSpace() + "::" + cls->GetName());
	return true;
}

bool Session::WriteFile(const std::string& path, const std::string& text) const
{
	std::ofstream out(path, std::ios::binary);
	if (!out)
		return false;
	out << text;
	return out.good();
}

}
