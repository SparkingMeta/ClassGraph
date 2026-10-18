#include "Realistic/VGraph.h"
#include "Abstract/ANode.h"
#include "Abstract/AFlow.h"
#include "Abstract/ALine.h"

using namespace CG::Core;

CG::Core::VGraph::VGraph()
{
}

std::shared_ptr<AFlow> CG::Core::VGraph::GetFlow() const
{
	return flow_.lock();
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
	flow_ = flow;
}

void CG::Core::VGraph::AddLine(const std::shared_ptr<ALine>& line)
{
	if (line && !line->GetGuid().empty())
		lines_[line->GetGuid()] = line;
}

void CG::Core::VGraph::DelLine(const std::string& guid)
{
	lines_.erase(guid);
}

std::shared_ptr<ALine> CG::Core::VGraph::FindLine(const std::string& guid) const
{
	if (auto it = lines_.find(guid); it != lines_.end()) return it->second;
	return {};
}
