#include "SessionInternal.h"

namespace CG::Router {
using namespace CG::Core;

void Session::RegisterGraph()
{
	handlers_.Subscribe(Action::AddNode, Handler<Message>(std::bind(&Session::AddNode, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::DelNode, Handler<Message>(std::bind(&Session::DelNode, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::MoveNode, Handler<Message>(std::bind(&Session::MoveNode, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::SelectNode, Handler<Message>(std::bind(&Session::SelectNode, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::AddExitNode, Handler<Message>(std::bind(&Session::AddExit, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::ConnectPin, Handler<Message>(std::bind(&Session::ConnectPin, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::DisconnectPin, Handler<Message>(std::bind(&Session::DisconnectPin, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::DisconnectLine, Handler<Message>(std::bind(&Session::DisconnectLine, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::SetPinValue, Handler<Message>(std::bind(&Session::SetPinValue, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::AddParam, Handler<Message>(std::bind(&Session::AddParam, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::DelParam, Handler<Message>(std::bind(&Session::DelParam, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::RenameParam, Handler<Message>(std::bind(&Session::RenameParam, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::SetParamType, Handler<Message>(std::bind(&Session::SetParamType, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::GetDetails, Handler<Message>(std::bind(&Session::GetDetails, this, std::placeholders::_1)));
}

void Session::AddNode(const Message& msg)
{
	int code = -1;
	std::string desc{};
	std::string data{};
	do
	{
		const auto body = Body(msg);
		std::string key = Trim(JsonStr(body, "key"));
		if (key.empty())
			key = Trim(JsonStr(body, "guid"));
		if (key.empty()) {
			desc = "AddNode: empty key";
			break;
		}
		const float x = static_cast<float>(JsonNum(body, "x", 120));
		const float y = static_cast<float>(JsonNum(body, "y", 120));
		if (!PlaceKey(key, x, y)) {
			desc = "AddNode failed";
			break;
		}
		auto node = FindNode(selected_node_guid_);
		if (!node) {
			desc = "AddNode failed";
			break;
		}

		code = 0;
		data = DumpNode(MakeNode(node), current_flow_guid_);
	} while (0);

	Print(desc);
	if(code == 0) Reply(msg.act, msg.token, data);
}

void Session::DelNode(const Message& msg)
{
	int code = -1;
	std::string desc{};
	std::string data{};
	do
	{
		auto graph = std::dynamic_pointer_cast<VGraph>(FindGraph());
		if (!graph) {
			desc = "DelNode: no graph";
			break;
		}
		const std::string guid = JsonStr(Body(msg), "guid");
		auto node = FindNode(guid);
		if (!node) {
			desc = "DelNode: node not found";
			break;
		}
		if (graph->GetEntry() == node) {
			desc = "Cannot delete Entry node";
			break;
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

		code = 0;
		data = nlohmann::json({{"guid", guid}}).dump();
	} while (0);

	Print(desc);
	if(code == 0) Reply(msg.act, msg.token, data);
}

void Session::MoveNode(const Message& msg)
{
	int code = -1;
	std::string desc{};
	std::string data{};
	do
	{
		const auto body = Body(msg);
		auto node = FindNode(JsonStr(body, "guid"));
		if (!node) {
			desc = "MoveNode: node not found";
			break;
		}
		node->SetCoord({
			static_cast<float>(JsonNum(body, "x")),
			static_cast<float>(JsonNum(body, "y")),
		});
		code = 0;
	} while (0);

	Print(desc);
	if(code == 0) Reply(msg.act, msg.token, data);
}

void Session::SelectNode(const Message& msg)
{
	int code = -1;
	std::string desc{};
	std::string data{};
	do
	{
		const std::string guid = JsonStr(Body(msg), "guid");
		selected_node_guid_ = guid;
		if (guid.empty()) {
			code = 0;
			break;
		}
		if (FindNode(guid)) {
			code = 0;
			data = nlohmann::json({{"guid", guid}}).dump();
			break;
		}
		if (blueprint_ && blueprint_->FindVar(guid)) {
			code = 0;
			data = nlohmann::json({{"guid", guid}}).dump();
			break;
		}
		selected_node_guid_.clear();
		desc = "SelectNode: not found";
	} while (0);

	Print(desc);
	if(code == 0) Reply(msg.act, msg.token, data);
}

void Session::AddExit(const Message& msg)
{
	int code = -1;
	std::string desc{};
	std::string data{};
	do
	{
		const auto body = Body(msg);
		const float x = static_cast<float>(JsonNum(body, "x", 420));
		const float y = static_cast<float>(JsonNum(body, "y", 180));
		if (!PlaceKey("exit", x, y)) {
			desc = "AddExit failed";
			break;
		}
		auto node = FindNode(selected_node_guid_);
		if (!node) {
			desc = "AddExit failed";
			break;
		}
		code = 0;
		data = DumpNode(MakeNode(node), current_flow_guid_);
	} while (0);

	Print(desc);
	if(code == 0) Reply(msg.act, msg.token, data);
}

void Session::ConnectPin(const Message& msg)
{
	int code = -1;
	std::string desc{};
	std::string data{};
	do
	{
		const auto body = Body(msg);
		auto from = FindPin(JsonStr(body, "from"));
		auto to = FindPin(JsonStr(body, "to"));
		if (!from || !to) {
			desc = "ConnectPin: pin not found";
			break;
		}
		if (!ConnectPins(from, to)) {
			desc = "ConnectPin failed";
			break;
		}

		std::shared_ptr<ALine> found;
		auto match = [&](const std::shared_ptr<APin>& pin) {
			if (!pin || found)
				return;
			for (const auto& line : pin->GetLines()) {
				if (!line)
					continue;
				auto a = line->GetFrom();
				auto b = line->GetTo();
				if ((a == from && b == to) || (a == to && b == from)) {
					found = line;
					return;
				}
			}
		};
		match(from);
		match(to);
		if (!found) {
			desc = "ConnectPin failed";
			break;
		}

		Line l;
		l.id = found->GetGuid();
		if (auto a = found->GetFrom())
			l.from_pin_id = a->GetGuid();
		if (auto b = found->GetTo())
			l.to_pin_id = b->GetGuid();
		if (auto out = found->GetFrom())
			l.is_exec = out->GetPinMeta().kind == CG::Core::PinKind::Exec;

		code = 0;
		data = DumpLine(l, current_flow_guid_);
	} while (0);

	Print(desc);
	if(code == 0) Reply(msg.act, msg.token, data);
}

void Session::DisconnectPin(const Message& msg)
{
	int code = -1;
	std::string desc{};
	std::string data{};
	do
	{
		auto pin = FindPin(JsonStr(Body(msg), "guid"));
		if (!pin) {
			desc = "DisconnectPin: pin not found";
			break;
		}
		CG::Core::DisconnectPin(pin);

		code = 0;
		data = nlohmann::json({{"guid", pin->GetGuid()}}).dump();
	} while (0);

	Print(desc);
	if(code == 0) Reply(msg.act, msg.token, data);
}

void Session::DisconnectLine(const Message& msg)
{
	int code = -1;
	std::string desc{};
	std::string data{};
	do
	{
		const std::string line_id = JsonStr(Body(msg), "guid");
		if (line_id.empty()) {
			desc = "DisconnectLine: empty guid";
			break;
		}
		auto graph = std::dynamic_pointer_cast<VGraph>(FindGraph());
		if (!graph) {
			desc = "DisconnectLine: no graph";
			break;
		}
		bool found = false;
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
						found = true;
						break;
					}
				}
				if (found)
					break;
			}
			if (found)
				break;
		}
		if (!found) {
			desc = "DisconnectLine: line not found";
			break;
		}

		code = 0;
		data = nlohmann::json({{"guid", line_id}}).dump();
	} while (0);

	Print(desc);
	if(code == 0) Reply(msg.act, msg.token, data);
}

void Session::SetPinValue(const Message& msg)
{
	int code = -1;
	std::string desc{};
	std::string data{};
	do
	{
		const auto body = Body(msg);
		auto pin = FindPin(JsonStr(body, "guid"));
		if (!pin) {
			desc = "SetPinValue: pin not found";
			break;
		}
		auto frame = PinValueFrame(pin);
		if (!WriteFrameText(frame, JsonStr(body, "value"))) {
			desc = "SetPinValue failed";
			break;
		}

		code = 0;
		data = nlohmann::json({{"guid", pin->GetGuid()}}).dump();
	} while (0);

	Print(desc);
	if(code == 0) Reply(msg.act, msg.token, data);
}

void Session::AddParam(const Message& msg)
{
	int code = -1;
	std::string desc{};
	std::string data{};
	do
	{
		const auto body = Body(msg);
		auto node = FindNode(JsonStr(body, "guid"));
		if (!node || !node->GetCaller()) {
			desc = "AddParam: node not found";
			break;
		}

		if (auto* sw = dynamic_cast<FSwitchInt*>(node->GetCaller().get())) {
			const std::string name = sw->AddCase();
			if (name.empty()) {
				desc = "AddCase failed";
				break;
			}
			node->ApplyMeta();
			InitSwitchCaseValue(node, name);
			selected_node_guid_ = node->GetGuid();
			code = 0;
			desc = "Added case " + name;
			data = nlohmann::json({{"guid", node->GetGuid()}, {"name", name}}).dump();
			break;
		}

		std::string name = Trim(JsonStr(body, "name"));
		const std::string type_name = Trim(JsonStr(body, "type"));
		if (name.empty())
			name = UniqueParamName(node->GetCaller()->GetCallMeta(), "NewParam");
		if (!IsIdent(name)) {
			desc = "Param name must be a valid C++ identifier";
			break;
		}
		const VType* type = VSystem::Get().FindType(type_name);
		if (!type) {
			desc = "Unknown type: " + type_name;
			break;
		}
		auto* entry = dynamic_cast<FEntry*>(node->GetCaller().get());
		auto* exit = dynamic_cast<FExit*>(node->GetCaller().get());
		const bool ok = entry ? entry->AddParam(name, type)
			: exit ? exit->AddParam(name, type) : false;
		if (!ok) {
			desc = "AddParam failed (duplicate name?)";
			break;
		}
		node->ApplyMeta();
		SyncScriptCallers(blueprint_, FindFlow());
		selected_node_guid_ = node->GetGuid();

		code = 0;
		desc = "Added param " + name + " (" + type_name + ")";
		data = nlohmann::json({{"guid", node->GetGuid()}, {"name", name}, {"type", type_name}}).dump();
	} while (0);

	Print(desc);
	if(code == 0) Reply(msg.act, msg.token, data);
}

void Session::DelParam(const Message& msg)
{
	int code = -1;
	std::string desc{};
	std::string data{};
	do
	{
		const auto body = Body(msg);
		auto node = FindNode(JsonStr(body, "guid"));
		if (!node || !node->GetCaller()) {
			desc = "DelParam: node not found";
			break;
		}
		const std::string name = Trim(JsonStr(body, "name"));
		if (name.empty()) {
			desc = "DelParam: empty name";
			break;
		}
		auto* entry = dynamic_cast<FEntry*>(node->GetCaller().get());
		auto* exit = dynamic_cast<FExit*>(node->GetCaller().get());
		auto* sw = dynamic_cast<FSwitchInt*>(node->GetCaller().get());
		const bool ok = entry ? entry->DelParam(name)
			: exit ? exit->DelParam(name)
			: sw ? sw->DelCase(name) : false;
		if (!ok) {
			desc = "DelParam failed";
			break;
		}
		node->ApplyMeta();
		if (!sw)
			SyncScriptCallers(blueprint_, FindFlow());
		selected_node_guid_ = node->GetGuid();

		code = 0;
		desc = "Deleted param " + name;
		data = nlohmann::json({{"guid", node->GetGuid()}, {"name", name}}).dump();
	} while (0);

	Print(desc);
	if(code == 0) Reply(msg.act, msg.token, data);
}

void Session::RenameParam(const Message& msg)
{
	int code = -1;
	std::string desc{};
	std::string data{};
	do
	{
		const auto body = Body(msg);
		auto node = FindNode(JsonStr(body, "guid"));
		if (!node || !node->GetCaller()) {
			desc = "RenameParam: node not found";
			break;
		}
		const std::string old_name = Trim(JsonStr(body, "old"));
		const std::string new_name = Trim(JsonStr(body, "name"));
		if (!IsIdent(new_name)) {
			desc = "Param name must be a valid C++ identifier";
			break;
		}
		auto* entry = dynamic_cast<FEntry*>(node->GetCaller().get());
		auto* exit = dynamic_cast<FExit*>(node->GetCaller().get());
		const bool ok = entry ? entry->RenameParam(old_name, new_name)
			: exit ? exit->RenameParam(old_name, new_name) : false;
		if (!ok) {
			desc = "RenameParam failed";
			break;
		}
		node->ApplyMeta(old_name, new_name);
		SyncScriptCallers(blueprint_, FindFlow(), old_name, new_name);
		selected_node_guid_ = node->GetGuid();

		code = 0;
		desc = "Param " + old_name + " renamed to " + new_name;
		data = nlohmann::json({{"guid", node->GetGuid()}, {"old", old_name}, {"name", new_name}}).dump();
	} while (0);

	Print(desc);
	if(code == 0) Reply(msg.act, msg.token, data);
}

void Session::SetParamType(const Message& msg)
{
	int code = -1;
	std::string desc{};
	std::string data{};
	do
	{
		const auto body = Body(msg);
		auto node = FindNode(JsonStr(body, "guid"));
		if (!node || !node->GetCaller()) {
			desc = "SetParamType: node not found";
			break;
		}
		const std::string name = Trim(JsonStr(body, "name"));
		const std::string type_name = Trim(JsonStr(body, "type"));
		const VType* type = VSystem::Get().FindType(type_name);
		if (!type) {
			desc = "Unknown type: " + type_name;
			break;
		}
		auto* entry = dynamic_cast<FEntry*>(node->GetCaller().get());
		auto* exit = dynamic_cast<FExit*>(node->GetCaller().get());
		const bool ok = entry ? entry->SetParamType(name, type)
			: exit ? exit->SetParamType(name, type) : false;
		if (!ok) {
			desc = "SetParamType failed";
			break;
		}
		node->ApplyMeta();
		SyncScriptCallers(blueprint_, FindFlow());
		selected_node_guid_ = node->GetGuid();

		code = 0;
		desc = "Param " + name + " type -> " + type_name;
		data = nlohmann::json({{"guid", node->GetGuid()}, {"name", name}, {"type", type_name}}).dump();
	} while (0);

	Print(desc);
	if(code == 0) Reply(msg.act, msg.token, data);
}


bool Session::PlaceNode(const std::shared_ptr<VNode>& node, float x, float y)
{
	auto graph = std::dynamic_pointer_cast<VGraph>(FindGraph());
	if (!graph || !node)
		return false;
	node->SetCoord({ x, y });
	graph->AddNode(node);
	selected_node_guid_ = node->GetGuid();
	return true;
}

bool Session::PlaceKey(const std::string& key, float x, float y)
{
	auto graph = std::dynamic_pointer_cast<VGraph>(FindGraph());
	if (!graph)
		return false;

	if (key == "exit") {
		if (graph->GetExit()) {
			Print("This graph already has a Return node");
			return false;
		}
		auto exit_fn = std::make_shared<FExit>();
		exit_fn->SetName("Exit");
		auto node = VNode::Make(exit_fn);
		node->SetName("Return");
		node->SetCoord({ x, y });
		graph->AddNode(node);
		graph->SetExit(node);
		selected_node_guid_ = node->GetGuid();
		Print("Added Return node");
		return true;
	}

	if (key.rfind("flow:", 0) == 0) {
		const std::string guid = key.substr(5);
		if (guid.empty() || guid == current_flow_guid_) {
			Print("Cannot call the current function from itself");
			return false;
		}
		if (!blueprint_)
			return false;
		auto flow = blueprint_->FindFlow(guid);
		if (!flow || !flow->GetScript()) {
			Print("Function not found");
			return false;
		}
		auto script = std::static_pointer_cast<ACaller>(flow->GetScript());
		script->SetName(flow->GetName());
		auto node = VNode::Make(script);
		node->SetName(script->GetName());
		if (!PlaceNode(node, x, y))
			return false;
		Print("Place node: " + script->GetName());
		return true;
	}

	if (key.rfind("method:", 0) == 0) {
		const std::string rest = key.substr(7);
		const auto slash = rest.rfind('/');
		if (slash == std::string::npos || slash == 0)
			return false;
		const std::string class_key = rest.substr(0, slash);
		const std::string method_name = rest.substr(slash + 1);
		auto* cls = VSystem::Get().FindClass(class_key);
		if (!cls)
			return false;
		auto method = cls->GetMethod(method_name);
		if (!method)
			return false;
		auto caller = std::static_pointer_cast<ACaller>(method);
		auto node = VNode::Make(caller);
		node->SetName(caller->GetName());
		if (!PlaceNode(node, x, y))
			return false;
		Print("Place node: " + caller->GetName());
		return true;
	}

	auto caller = VSystem::Get().FindCaller(key);
	if (!caller)
		return false;
	if (dynamic_cast<FSwitchInt*>(caller.get())) {
		auto fresh = std::make_shared<FSwitchInt>();
		fresh->SetName(caller->GetName());
		caller = std::move(fresh);
	}
	auto node = VNode::Make(caller);
	node->SetName(caller->GetName());
	if (!PlaceNode(node, x, y))
		return false;
	Print("Place node: " + caller->GetName());
	return true;
}

std::string Session::MakeDetailsJson() const
{
	if (!selected_node_guid_.empty()) {
		if (auto node = FindNode(selected_node_guid_))
			return ToJson(MakeNodeDetails(node)).dump();
		if (blueprint_) {
			auto var = blueprint_->FindVar(selected_node_guid_);
			if (var) {
				VarDetails dto;
				dto.guid = selected_node_guid_;
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
				return ToJson(dto).dump();
			}
		}
	}
	nlohmann::json j{{"kind", "blueprint"}};
	if (blueprint_) {
		j["guid"] = blueprint_->GetGuid();
		j["name"] = blueprint_->GetName();
		j["space"] = blueprint_->GetSpace();
		j["self_type_name"] = SelfType();
	}
	return j.dump();
}

void Session::GetDetails(const Message& msg)
{
	Reply(msg.act, msg.token, MakeDetailsJson());
}

Node Session::MakeNode(const std::shared_ptr<VNode>& vnode) const
{
	Node n;
	if (!vnode)
		return n;
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
	return n;
}

NodeDetails Session::MakeNodeDetails(const std::shared_ptr<VNode>& node) const
{
	NodeDetails dto;
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
				Param p;
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


} // namespace CG::Router
