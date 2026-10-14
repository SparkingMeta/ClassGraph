#include "SessionInternal.h"

namespace CG::Router {
using namespace CG::Core;

void Session::RegisterFlow()
{
	handlers_.Subscribe(Action::AddFlow, Handler<Message>(std::bind(&Session::AddFlow, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::DelFlow, Handler<Message>(std::bind(&Session::DelFlow, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::RenameFlow, Handler<Message>(std::bind(&Session::RenameFlow, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::SelectFlow, Handler<Message>(std::bind(&Session::SelectFlow, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::RunFlow, Handler<Message>(std::bind(&Session::RunFlow, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::GetGraph, Handler<Message>(std::bind(&Session::GetGraph, this, std::placeholders::_1)));
}

void Session::AddFlow(const Message& msg)
{
	int code = -1;
	std::string desc{};
	std::string data{};
	do
	{
		if (!blueprint_) {
			desc = "AddFlow: no blueprint";
			break;
		}
		std::string name = Trim(JsonStr(Body(msg), "name"));
		if (name.empty())
			name = NewFlow("NewFunction");
		else if (blueprint_->FindFlowByName(name))
			name = NewFlow(name);

		const std::string guid = blueprint_->AddFlow(name);
		auto flow = blueprint_->FindFlow(guid);
		if (!flow) {
			desc = "AddFlow: failed to create function";
			break;
		}
		MakeEntry(flow);
		current_flow_guid_ = guid;
		selected_node_guid_.clear();

		code = 0;
		desc = "Added function " + name;
		data = nlohmann::json({{"guid", guid}, {"name", name}}).dump();
	} while (0);

	Print(desc);
	if(code == 0) Reply(msg.act, msg.token, data);
}

void Session::DelFlow(const Message& msg)
{
	int code = -1;
	std::string desc{};
	std::string data{};
	do
	{
		if (!blueprint_) {
			desc = "DelFlow: no blueprint";
			break;
		}
		if (blueprint_->GetFlows().size() <= 1) {
			desc = "Keep at least one function";
			break;
		}
		const std::string guid = JsonStr(Body(msg), "guid");
		if (guid.empty()) {
			desc = "DelFlow: empty guid";
			break;
		}
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

		code = 0;
		desc = "Deleted function";
		data = nlohmann::json({{"guid", guid}, {"current", current_flow_guid_}}).dump();
	} while (0);

	Print(desc);
	if(code == 0) Reply(msg.act, msg.token, data);
}

void Session::RenameFlow(const Message& msg)
{
	int code = -1;
	std::string desc{};
	std::string data{};
	do
	{
		if (!blueprint_) {
			desc = "RenameFlow: no blueprint";
			break;
		}
		const auto body = Body(msg);
		const std::string guid = JsonStr(body, "guid");
		auto flow = blueprint_->FindFlow(guid);
		if (!flow) {
			desc = "RenameFlow: function not found";
			break;
		}
		const std::string new_name = Trim(JsonStr(body, "name"));
		if (new_name.empty()) {
			desc = "RenameFlow: empty name";
			break;
		}
		if (auto other = blueprint_->FindFlowByName(new_name); other && other != flow) {
			desc = "Function name already exists: " + new_name;
			break;
		}
		flow->SetName(new_name);
		if (auto script = flow->GetScript())
			script->SetName(new_name);

		code = 0;
		desc = "Renamed function to " + new_name;
		data = nlohmann::json({{"guid", guid}, {"name", new_name}}).dump();
	} while (0);

	Print(desc);
	if(code == 0) Reply(msg.act, msg.token, data);
}

void Session::SelectFlow(const Message& msg)
{
	int code = -1;
	std::string desc{};
	std::string data{};
	do
	{
		if (!blueprint_) {
			desc = "SelectFlow: no blueprint";
			break;
		}
		const std::string guid = JsonStr(Body(msg), "guid");
		auto flow = blueprint_->FindFlow(guid);
		if (guid.empty() || !flow) {
			desc = "SelectFlow: function not found";
			break;
		}
		current_flow_guid_ = guid;
		selected_node_guid_.clear();

		code = 0;
		data = nlohmann::json({{"guid", guid}, {"name", flow->GetName()}}).dump();
	} while (0);

	Print(desc);
	if(code == 0) Reply(msg.act, msg.token, data);
}

void Session::RunFlow(const Message& msg)
{
	int code = -1;
	std::string desc{};
	std::string data{};
	do
	{
		auto flow = FindFlow();
		if (!flow) {
			desc = "Select a graph first";
			break;
		}
		const int run_code = flow->Run();
		if (run_code != 0) {
			desc = "Play " + flow->GetName() + ": failed (code=" + std::to_string(run_code) + ")";
			break;
		}

		code = 0;
		desc = "Play " + flow->GetName() + ": succeeded";
		data = nlohmann::json({{"guid", flow->GetGuid()}}).dump();
	} while (0);

	Print(desc);
	if(code == 0) Reply(msg.act, msg.token, data);
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

void Session::GetGraph(const Message& msg)
{
	Reply(msg.act, msg.token, ToJson(MakeGraph()).dump());
}

Graph Session::MakeGraph() const
{
	Graph dto;
	dto.flow_guid = current_flow_guid_;
	auto graph = std::dynamic_pointer_cast<VGraph>(FindGraph());
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
		dto.nodes.push_back(MakeNode(vnode));

		for (const auto& pin : vnode->GetOutputPins()) {
			if (!pin)
				continue;
			for (const auto& line : pin->GetLines()) {
				if (!line)
					continue;
				const std::string lid = line->GetGuid();
				if (!seen_lines.insert(lid).second)
					continue;
				Line l;
				l.id = lid;
				l.from_pin_id = pin->GetGuid();
				if (auto to = line->GetTo())
					l.to_pin_id = to->GetGuid();
				l.is_exec = pin->GetPinMeta().kind == CG::Core::PinKind::Exec;
				dto.lines.push_back(std::move(l));
			}
		}
	}
	return dto;
}


} // namespace CG::Router
