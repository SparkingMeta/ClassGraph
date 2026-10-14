#include "SessionInternal.h"

#include "Realistic/VGraph.h"
#include "Realistic/VNode.h"

#include <cctype>

namespace CG::Router {
using namespace CG::Core;

Session::Session(HostConfig config)
	: config_(std::move(config))
{
	Init();
}

Session::~Session() = default;

void Session::Init()
{
	RegisterAction();
	MakeAssetDir();
	LoadAssets();
}

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

void Session::Reply(int act, const std::string& token, const std::string& body)
{
	if (!rise_)
		return;
	rise_(MakeReply(static_cast<Action>(act), token, body));
}

bool Session::Handle(const Message& request, Message*)
{
	handlers_.Publish({static_cast<Action>(request.act), request});
	return true;
}

void Session::RegisterAction()
{
	RegisterAsset();
	RegisterFlow();
	RegisterVar();
	RegisterGraph();
	RegisterType();
}

std::shared_ptr<AFlow> Session::FindFlow() const
{
	if (!blueprint_)
		return {};
	return blueprint_->FindFlow(current_flow_guid_);
}

std::shared_ptr<AGraph> Session::FindGraph() const
{
	auto flow = FindFlow();
	return flow ? flow->GetGraph() : std::shared_ptr<AGraph>{};
}

std::shared_ptr<VNode> Session::FindNode(const std::string& guid) const
{
	if (guid.empty())
		return {};
	auto graph = FindGraph();
	if (!graph)
		return {};
	return std::dynamic_pointer_cast<VNode>(graph->FindNode(guid));
}

std::shared_ptr<APin> Session::FindPin(const std::string& guid) const
{
	if (guid.empty())
		return {};
	auto graph = std::dynamic_pointer_cast<VGraph>(FindGraph());
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

void Session::Print(const std::string& text)
{
	if (text.empty())
		return;
	Rise(Action::Print, {}, text);
}

void Session::Clear()
{
	blueprint_.reset();
	current_flow_guid_.clear();
	selected_node_guid_.clear();
	current_asset_guid_.clear();
}

}
