#include "SessionInternal.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
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

	EditKind EditKindOf(const VType* type)
	{
		if (!type)
			return EditKind::None;
		const auto& name = type->name_;
		if (name == "bool") return EditKind::Bool;
		if (name == "int") return EditKind::Int;
		if (name == "int64") return EditKind::Int64;
		if (name == "float") return EditKind::Float;
		if (name == "double") return EditKind::Double;
		if (name == "std::string") return EditKind::String;
		return EditKind::None;
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
		case EditKind::Int: {
			int v = 0;
			if (frame->GetValue(type, &v))
				return std::to_string(v);
			break;
		}
		case EditKind::Int64: {
			int64 v = 0;
			if (frame->GetValue(type, &v))
				return std::to_string(v);
			break;
		}
		case EditKind::Float: {
			float v = 0.f;
			if (frame->GetValue(type, &v))
				return FormatFloatText(v, 9);
			break;
		}
		case EditKind::Double: {
			double v = 0.0;
			if (frame->GetValue(type, &v))
				return FormatFloatText(v, 17);
			break;
		}
		case EditKind::String: {
			std::string v;
			if (frame->GetValue(type, &v))
				return v;
			break;
		}
		case EditKind::Bool: {
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
			case EditKind::Int: {
				int v = std::stoi(value);
				return frame->SetValue(type, &v);
			}
			case EditKind::Int64: {
				int64 v = static_cast<int64>(std::stoll(value));
				return frame->SetValue(type, &v);
			}
			case EditKind::Float: {
				float v = std::stof(value);
				return frame->SetValue(type, &v);
			}
			case EditKind::Double: {
				double v = std::stod(value);
				return frame->SetValue(type, &v);
			}
			case EditKind::String: {
				std::string v = text;
				return frame->SetValue(type, &v);
			}
			case EditKind::Bool: {
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
		if (pin->GetPinMeta().kind != CG::Core::PinKind::Data)
			return false;
		if (PinConnected(pin))
			return false;
		auto frame = pin->GetFrame();
		const VType* type = frame ? frame->GetType() : pin->GetPinMeta().type;
		return frame && EditKindOf(type) != EditKind::None;
	}

	bool IsCaseLiteralPin(const std::shared_ptr<APin>& pin)
	{
		if (!pin || pin->GetDirect() != EDirect::Out)
			return false;
		const auto& meta = pin->GetPinMeta();
		return meta.kind == CG::Core::PinKind::Exec && meta.type != nullptr;
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

	Pin BuildPinDto(const std::shared_ptr<APin>& pin)
	{
		Pin dto;
		if (!pin)
			return dto;
		const auto& meta = pin->GetPinMeta();
		dto.id = pin->GetGuid();
		dto.name = meta.name;
		dto.kind = meta.kind == CG::Core::PinKind::Exec ? PinKind::Exec : PinKind::Data;
		dto.direction = pin->GetDirect() == EDirect::Out ? PinDir::Out : PinDir::In;
		dto.type_name = meta.type ? meta.type->name_ : "";
		dto.connected = PinConnected(pin);
		auto frame = PinValueFrame(pin);
		const VType* type = frame ? frame->GetType() : meta.type;
		dto.editable = PinEditable(pin) || IsCaseLiteralPin(pin);
		dto.edit_kind = EditKindOf(type);
		dto.value_text = ReadFrameText(frame);
		return dto;
	}

	nlohmann::json ToJson(const Pin& p)
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

	nlohmann::json ToJson(const Node& n)
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

	nlohmann::json ToJson(const Line& l)
	{
		return nlohmann::json{
			{"id", l.id},
			{"from_pin_id", l.from_pin_id},
			{"to_pin_id", l.to_pin_id},
			{"is_exec", l.is_exec},
		};
	}

	std::string DumpNode(const Node& n, const std::string& flow)
	{
		auto j = ToJson(n);
		j["flow_guid"] = flow;
		return j.dump();
	}

	std::string DumpLine(const Line& l, const std::string& flow)
	{
		auto j = ToJson(l);
		j["flow_guid"] = flow;
		return j.dump();
	}

	nlohmann::json ToJson(const Graph& g)
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

	nlohmann::json ToJson(const Blueprint& b)
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

	nlohmann::json ToJson(const Palette& p)
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

	nlohmann::json ToJson(const NodeDetails& d)
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

	nlohmann::json ToJson(const VarDetails& d)
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
		const std::string& renamed_from, const std::string& renamed_to)
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

} // namespace CG::Router
