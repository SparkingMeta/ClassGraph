#include "Realistic/VGraph.h"
#include "Abstract/ANode.h"
#include "Abstract/AFlow.h"

using namespace CG::Core;

CG::Core::VGraph::VGraph(const std::shared_ptr<AFlow>& flow) : task_(flow)
{
}

std::shared_ptr<AFlow> CG::Core::VGraph::GetFlow() const
{
	return task_.lock();
}

std::shared_ptr<ANode> CG::Core::VGraph::GetEntry() const
{
	return entry_;
}

std::shared_ptr<ANode> CG::Core::VGraph::GetExit() const
{
	return exit_;
}

void CG::Core::VGraph::SetEntry(const std::shared_ptr<ANode>& node)
{
	entry_ = node;
}

void CG::Core::VGraph::SetExit(const std::shared_ptr<ANode>& node)
{
	exit_ = node;
}

void CG::Core::VGraph::AddNode(const std::shared_ptr<ANode>& node)
{
	if (node) {
		nodes_.insert({node->GetGuid(), node});
	}
}

void CG::Core::VGraph::DelNode(const std::string& guid)
{
	nodes_.erase(guid);
}

std::shared_ptr<ANode> CG::Core::VGraph::FindNode(const std::string& guid) const
{
	if(auto it = nodes_.find(guid); it != nodes_.end())
		return it->second;
	return {nullptr};
}

void CG::Core::VGraph::SetFlow(const std::shared_ptr<AFlow>& flow)
{
	task_ = flow;
}