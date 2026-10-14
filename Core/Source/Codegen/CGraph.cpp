#include "CodeGen/CGraph.h"

#include "Abstract/ACaller.h"
#include "Abstract/ALine.h"
#include "Abstract/APin.h"
#include "Realistic/VController.h"
#include "Realistic/VOperator.h"

using namespace CG::Core;
using namespace CG::Core::cg;

int CG::Core::cg::FindInPinIndex(const LayoutMeta& layout, const std::string& arg_name)
{
	for (size_t i = 0; i < layout.inputs.size(); ++i) {
		if (layout.inputs[i].name == arg_name)
			return static_cast<int>(i);
	}
	return -1;
}

int CG::Core::cg::FindOutPinIndex(const LayoutMeta& layout, const std::string& arg_name)
{
	for (size_t i = 0; i < layout.outputs.size(); ++i) {
		if (layout.outputs[i].name == arg_name)
			return static_cast<int>(i);
	}
	return -1;
}

LayoutMeta CG::Core::cg::ResolveLayout(const std::shared_ptr<ANode>& node)
{
	if (!node) return {};
	auto caller = node->GetCaller();
	if (!caller) return {};
	return BuildLayout(caller->GetExecMeta(), caller->GetCallMeta());
}

std::shared_ptr<APin> CG::Core::cg::FindInPin(const std::shared_ptr<ANode>& node, const std::string& arg_name)
{
	if (!node) return {};
	auto layout = ResolveLayout(node);
	int index = FindInPinIndex(layout, arg_name);
	if (index < 0) return {};
	return node->GetInputPin(static_cast<uint16>(index));
}

std::shared_ptr<APin> CG::Core::cg::FindOutPin(const std::shared_ptr<ANode>& node, const std::string& arg_name)
{
	if (!node) return {};
	auto layout = ResolveLayout(node);
	int index = FindOutPinIndex(layout, arg_name);
	if (index < 0) return {};
	return node->GetOutputPin(static_cast<uint16>(index));
}

std::shared_ptr<ANode> CG::Core::cg::NextExec(const std::shared_ptr<ANode>& node, int output_index)
{
	if (!node) return {};
	return node->Next(output_index);
}

std::shared_ptr<ANode> CG::Core::cg::NextExec(const std::shared_ptr<ANode>& node, const std::string& flow_name)
{
	if (!node) return {};
	auto layout = ResolveLayout(node);
	int index = FindOutPinIndex(layout, flow_name);
	if (index < 0) return {};
	return node->Next(index);
}

std::shared_ptr<ANode> CG::Core::cg::NextExec(const std::shared_ptr<ANode>& node, PinKind kind, int ordinal)
{
	if (!node) return {};
	auto layout = ResolveLayout(node);
	int IsMatched = 0;
	for (size_t i = 0; i < layout.outputs.size(); ++i) {
		if (layout.outputs[i].kind != kind) continue;
		if (IsMatched == ordinal)
			return node->Next(static_cast<int>(i));
		++IsMatched;
	}
	return {};
}

std::shared_ptr<ANode> CG::Core::cg::GetFromNode(const std::shared_ptr<ANode>& node, const std::string& arg_name)
{
	auto pin = FindInPin(node, arg_name);
	if (!pin) return {};

	auto line = pin->GetLine();
	if (!line) return {};

	auto from = line->GetFrom();
	if (!from) return {};

	return from->GetOwner();
}

std::string CG::Core::cg::GetFromOutArg(const std::shared_ptr<ANode>& node, const std::string& in_arg)
{
	auto in_pin = FindInPin(node, in_arg);
	if (!in_pin) return {};

	auto line = in_pin->GetLine();
	if (!line) return {};

	auto from_pin = line->GetFrom();
	if (!from_pin) return {};

	auto upstream = from_pin->GetOwner();
	if (!upstream) return {};

	auto layout = ResolveLayout(upstream);
	for (size_t i = 0; i < layout.outputs.size(); ++i) {
		if (upstream->GetOutputPin(static_cast<uint16>(i)) == from_pin)
			return layout.outputs[i].name;
	}
	return {};
}

bool CG::Core::cg::IsController(const ACaller* caller)
{
	if (!caller) return false;
	return dynamic_cast<const FBranch*>(caller)
		|| dynamic_cast<const FWhile*>(caller)
		|| dynamic_cast<const FLoop*>(caller)
		|| dynamic_cast<const FSwitchInt*>(caller)
		|| dynamic_cast<const FSequence*>(caller)
		|| dynamic_cast<const FExit*>(caller);
}
