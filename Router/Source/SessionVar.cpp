#include "SessionInternal.h"

namespace CG::Router {
using namespace CG::Core;

void Session::RegisterVar()
{
	handlers_.Subscribe(Action::AddVar, Handler<Message>(std::bind(&Session::AddVar, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::DelVar, Handler<Message>(std::bind(&Session::DelVar, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::RenameVar, Handler<Message>(std::bind(&Session::RenameVar, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::SetVarType, Handler<Message>(std::bind(&Session::SetVarType, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::SetVarValue, Handler<Message>(std::bind(&Session::SetVarValue, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::SelectVar, Handler<Message>(std::bind(&Session::SelectVar, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::PlaceVarGet, Handler<Message>(std::bind(&Session::PlaceGet, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::PlaceVarSet, Handler<Message>(std::bind(&Session::PlaceSet, this, std::placeholders::_1)));
}

void Session::AddVar(const Message& msg)
{
	int code = -1;
	std::string desc{};
	std::string data{};
	do
	{
		if (!blueprint_) {
			desc = "AddVar: no blueprint";
			break;
		}
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
				desc = "Cannot add member of self type " + type_name;
			else
				desc = "AddVar failed, check type: " + type_name;
			break;
		}

		code = 0;
		desc = "Added variable " + name + " (" + type_name + ")";
		data = nlohmann::json({{"guid", guid}, {"name", name}, {"type", type_name}}).dump();
	} while (0);

	Print(desc);
	if(code == 0) Reply(msg.act, msg.token, data);
}

void Session::DelVar(const Message& msg)
{
	int code = -1;
	std::string desc{};
	std::string data{};
	do
	{
		if (!blueprint_) {
			desc = "DelVar: no blueprint";
			break;
		}
		const std::string guid = JsonStr(Body(msg), "guid");
		if (guid.empty()) {
			desc = "DelVar: empty guid";
			break;
		}
		blueprint_->DelVar(guid);
		if (selected_node_guid_ == guid)
			selected_node_guid_.clear();

		code = 0;
		desc = "Deleted variable";
		data = nlohmann::json({{"guid", guid}}).dump();
	} while (0);

	Print(desc);
	if(code == 0) Reply(msg.act, msg.token, data);
}

void Session::RenameVar(const Message& msg)
{
	int code = -1;
	std::string desc{};
	std::string data{};
	do
	{
		if (!blueprint_) {
			desc = "RenameVar: no blueprint";
			break;
		}
		const auto body = Body(msg);
		const std::string guid = JsonStr(body, "guid");
		auto var = blueprint_->FindVar(guid);
		if (!var) {
			desc = "RenameVar: variable not found";
			break;
		}
		const std::string new_name = Trim(JsonStr(body, "name"));
		if (new_name.empty()) {
			desc = "RenameVar: empty name";
			break;
		}
		bool dup = false;
		for (const auto& [id, other] : blueprint_->GetVars()) {
			if (other && other != var && other->GetName() == new_name) {
				dup = true;
				break;
			}
		}
		if (dup) {
			desc = "Variable name already exists: " + new_name;
			break;
		}
		var->SetName(new_name);

		code = 0;
		desc = "Renamed variable to " + new_name;
		data = nlohmann::json({{"guid", guid}, {"name", new_name}}).dump();
	} while (0);

	Print(desc);
	if(code == 0) Reply(msg.act, msg.token, data);
}

void Session::SetVarType(const Message& msg)
{
	int code = -1;
	std::string desc{};
	std::string data{};
	do
	{
		if (!blueprint_) {
			desc = "SetVarType: no blueprint";
			break;
		}
		const auto body = Body(msg);
		const std::string guid = JsonStr(body, "guid");
		auto var = blueprint_->FindVar(guid);
		if (!var) {
			desc = "SetVarType: variable not found";
			break;
		}
		const std::string type_name = Trim(JsonStr(body, "type"));
		if (type_name.empty()) {
			desc = "SetVarType: empty type";
			break;
		}
		if (type_name == SelfType()) {
			desc = "Cannot change variable to self type " + type_name;
			break;
		}
		const VType* type = VSystem::Get().FindType(type_name);
		if (!type) {
			desc = "Unknown type: " + type_name;
			break;
		}
		if (var->GetType() == type)
			break;
		auto vvar = std::dynamic_pointer_cast<VVariant>(var);
		if (!vvar) {
			desc = "SetVarType: invalid variable";
			break;
		}
		vvar->Reset(type);

		code = 0;
		desc = "Variable " + var->GetName() + " type -> " + type_name;
		data = nlohmann::json({{"guid", guid}, {"type", type_name}}).dump();
	} while (0);

	Print(desc);
	if(code == 0) Reply(msg.act, msg.token, data);
}

void Session::SetVarValue(const Message& msg)
{
	int code = -1;
	std::string desc{};
	std::string data{};
	do
	{
		if (!blueprint_) {
			desc = "SetVarValue: no blueprint";
			break;
		}
		const auto body = Body(msg);
		const std::string guid = JsonStr(body, "guid");
		auto var = blueprint_->FindVar(guid);
		if (!var) {
			desc = "SetVarValue: variable not found";
			break;
		}
		if (!WriteFrameText(var->Getter(), JsonStr(body, "value"))) {
			desc = "SetVarValue failed";
			break;
		}

		code = 0;
		data = nlohmann::json({{"guid", guid}}).dump();
	} while (0);

	Print(desc);
	if(code == 0) Reply(msg.act, msg.token, data);
}

void Session::SelectVar(const Message& msg)
{
	int code = -1;
	std::string desc{};
	std::string data{};
	do
	{
		if (!blueprint_) {
			desc = "SelectVar: no blueprint";
			break;
		}
		const std::string guid = JsonStr(Body(msg), "guid");
		if (guid.empty() || !blueprint_->FindVar(guid)) {
			desc = "SelectVar: variable not found";
			break;
		}
		selected_node_guid_ = guid;

		code = 0;
		data = nlohmann::json({{"guid", guid}}).dump();
	} while (0);

	Print(desc);
	if(code == 0) Reply(msg.act, msg.token, data);
}

void Session::PlaceGet(const Message& msg)
{
	int code = -1;
	std::string desc{};
	std::string data{};
	do
	{
		if (!blueprint_) {
			desc = "PlaceGet: no blueprint";
			break;
		}
		const auto body = Body(msg);
		auto var = blueprint_->FindVar(JsonStr(body, "guid"));
		if (!var) {
			desc = "Variable not found";
			break;
		}
		const float x = static_cast<float>(JsonNum(body, "x", 120));
		const float y = static_cast<float>(JsonNum(body, "y", 120));
		auto node = std::dynamic_pointer_cast<VNode>(MakeGetVarNode(var));
		if (!node) {
			desc = "Cannot create Get node";
			break;
		}
		if (!PlaceNode(node, x, y)) {
			desc = "PlaceGet failed";
			break;
		}

		code = 0;
		desc = "Placed Get " + var->GetName();
		data = DumpNode(MakeNode(node), current_flow_guid_);
	} while (0);

	Print(desc);
	if(code == 0) Reply(msg.act, msg.token, data);
}

void Session::PlaceSet(const Message& msg)
{
	int code = -1;
	std::string desc{};
	std::string data{};
	do
	{
		if (!blueprint_) {
			desc = "PlaceSet: no blueprint";
			break;
		}
		const auto body = Body(msg);
		auto var = blueprint_->FindVar(JsonStr(body, "guid"));
		if (!var) {
			desc = "Variable not found";
			break;
		}
		const float x = static_cast<float>(JsonNum(body, "x", 120));
		const float y = static_cast<float>(JsonNum(body, "y", 120));
		auto node = std::dynamic_pointer_cast<VNode>(MakeSetVarNode(var));
		if (!node) {
			desc = "Cannot create Set node";
			break;
		}
		if (!PlaceNode(node, x, y)) {
			desc = "PlaceSet failed";
			break;
		}

		code = 0;
		desc = "Placed Set " + var->GetName();
		data = DumpNode(MakeNode(node), current_flow_guid_);
	} while (0);

	Print(desc);
	if(code == 0) Reply(msg.act, msg.token, data);
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


} // namespace CG::Router
