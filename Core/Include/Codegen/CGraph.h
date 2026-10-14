#pragma once

#include "Abstract/AGraph.h"
#include "Abstract/ANode.h"
#include "Base/VMeta.h"

#include <memory>
#include <string>
#include <vector>

namespace CG::Core {
	class ACaller;
}

namespace CG::Core::cg {

	int FindInPinIndex(const LayoutMeta& layout, const std::string& arg_name);
	int FindOutPinIndex(const LayoutMeta& layout, const std::string& arg_name);

	LayoutMeta ResolveLayout(const std::shared_ptr<ANode>& node);

	std::shared_ptr<APin> FindInPin(const std::shared_ptr<ANode>& node, const std::string& arg_name);
	std::shared_ptr<APin> FindOutPin(const std::shared_ptr<ANode>& node, const std::string& arg_name);

	std::shared_ptr<ANode> NextExec(const std::shared_ptr<ANode>& node, int output_index = 0);
	std::shared_ptr<ANode> NextExec(const std::shared_ptr<ANode>& node, const std::string& flow_name);
	std::shared_ptr<ANode> NextExec(const std::shared_ptr<ANode>& node, PinKind kind, int ordinal = 0);

	std::shared_ptr<ANode> GetFromNode(const std::shared_ptr<ANode>& node, const std::string& arg_name);
	std::string GetFromOutArg(const std::shared_ptr<ANode>& node, const std::string& in_arg);

	bool IsController(const ACaller* caller);

} // namespace CG::Core::cg
