#include "Test/TestUtil.h"

#include "Realistic/VController.h"
#include "Realistic/VFlow.h"
#include "Realistic/VGraph.h"
#include "Base/VSystem.h"
#include "CodeGen/CGraph.h"

#include <iostream>
#include <memory>
#include <vector>

using namespace CG::Core;
using namespace CG::Core::cg;
using namespace test;

namespace {

void WireReturnIntOp(const std::shared_ptr<AFlow>& flow, const std::shared_ptr<ACaller>& op,
	const char* a_name, const char* b_name)
{
	auto graph = flow->GetGraph();
	auto entry = MakeEntryNode({ {a_name, "int"}, {b_name, "int"} });
	auto op_node = MakeNode(op);
	auto exit = MakeExitNode({ {"value", "int"} });
	if (!entry || !op_node || !exit) return;

	graph->AddNode(entry);
	graph->AddNode(op_node);
	graph->AddNode(exit);
	Connect(entry->GetOutputPin(0), exit->GetInputPin(0));
	ConnectOutArg(op_node, "a", entry, a_name);
	ConnectOutArg(op_node, "b", entry, b_name);
	ConnectOutArg(exit, "value", op_node, "return");
	std::static_pointer_cast<VGraph>(graph)->SetEntry(entry);
	std::static_pointer_cast<VGraph>(graph)->SetExit(exit);
}

void WireReturnBoolOp(const std::shared_ptr<AFlow>& flow, const std::shared_ptr<ACaller>& op,
	const char* a_name, const char* b_name)
{
	auto graph = flow->GetGraph();
	auto entry = MakeEntryNode({ {a_name, "int"}, {b_name, "int"} });
	auto op_node = MakeNode(op);
	auto exit = MakeExitNode({ {"value", "bool"} });
	if (!entry || !op_node || !exit) return;

	graph->AddNode(entry);
	graph->AddNode(op_node);
	graph->AddNode(exit);
	Connect(entry->GetOutputPin(0), exit->GetInputPin(0));
	ConnectOutArg(op_node, "a", entry, a_name);
	ConnectOutArg(op_node, "b", entry, b_name);
	ConnectOutArg(exit, "value", op_node, "return");
	std::static_pointer_cast<VGraph>(graph)->SetEntry(entry);
	std::static_pointer_cast<VGraph>(graph)->SetExit(exit);
}

void BuildWhileFlow(const std::shared_ptr<VBlueprint>& bp, VSystem& system)
{
	auto flow = bp->FindFlow(bp->AddFlow("WhileWait"));
	auto graph = flow->GetGraph();
	auto entry = MakeEntryNode({ {"a", "int"}, {"b", "int"} });
	auto less = MakeNode(system.FindCaller("Less(int)"));
	auto while_node = MakeNode(std::make_shared<FWhile>());
	auto print = MakeNode(system.FindCaller("Print"));
	if (!entry || !less || !while_node || !print) return;

	graph->AddNode(entry);
	graph->AddNode(less);
	graph->AddNode(while_node);
	graph->AddNode(print);
	Connect(entry->GetOutputPin(0), while_node->GetInputPin(0));
	ConnectOutArg(less, "a", entry, "a");
	ConnectOutArg(less, "b", entry, "b");
	ConnectOutArg(while_node, "cond", less, "return");
	Connect(FindOutPin(while_node, "body"), print->GetInputPin(0));
	ConnectOutArg(print, "value", entry, "a");
	std::static_pointer_cast<VGraph>(graph)->SetEntry(entry);
}

void BuildLoopFlow(const std::shared_ptr<VBlueprint>& bp, VSystem&)
{
	auto flow = bp->FindFlow(bp->AddFlow("ForIndex"));
	auto graph = flow->GetGraph();
	auto entry = MakeEntryNode({ {"run", "bool"}, {"first", "int"}, {"last", "int"} });
	auto loop = MakeNode(std::make_shared<FLoop>());
	auto exit = MakeExitNode({ {"value", "int"} });
	if (!entry || !loop || !exit) return;

	graph->AddNode(entry);
	graph->AddNode(loop);
	graph->AddNode(exit);
	Connect(entry->GetOutputPin(0), loop->GetInputPin(0));
	ConnectOutArg(loop, "cond", entry, "run");
	ConnectOutArg(loop, "first", entry, "first");
	ConnectOutArg(loop, "last", entry, "last");
	Connect(FindOutPin(loop, "completed"), exit->GetInputPin(0));
	ConnectOutArg(exit, "value", entry, "last");
	std::static_pointer_cast<VGraph>(graph)->SetEntry(entry);
	std::static_pointer_cast<VGraph>(graph)->SetExit(exit);
}

void BuildSequenceFlow(const std::shared_ptr<VBlueprint>& bp, VSystem& system)
{
	auto flow = bp->FindFlow(bp->AddFlow("SequenceStep"));
	auto graph = flow->GetGraph();
	auto entry = MakeEntryNode({ {"a", "int"}, {"b", "int"} });
	auto seq = MakeNode(std::make_shared<FSequence>());
	auto print_a = MakeNode(system.FindCaller("Print"));
	auto print_b = MakeNode(system.FindCaller("Print"));
	if (!entry || !seq || !print_a || !print_b) return;

	graph->AddNode(entry);
	graph->AddNode(seq);
	graph->AddNode(print_a);
	graph->AddNode(print_b);
	Connect(entry->GetOutputPin(0), seq->GetInputPin(0));
	Connect(FindOutPin(seq, "then_0"), print_a->GetInputPin(0));
	Connect(FindOutPin(seq, "then_1"), print_b->GetInputPin(0));
	ConnectOutArg(print_a, "value", entry, "a");
	ConnectOutArg(print_b, "value", entry, "b");
	std::static_pointer_cast<VGraph>(graph)->SetEntry(entry);
}

void BuildSwitchFlow(const std::shared_ptr<VBlueprint>& bp, VSystem&)
{
	auto flow = bp->FindFlow(bp->AddFlow("SwitchPick"));
	auto graph = flow->GetGraph();
	auto entry = MakeEntryNode({ {"sel", "int"} });
	auto sw = MakeNode(std::make_shared<FSwitchInt>());
	auto exit_case = MakeExitNode({ {"value", "int"} });
	auto exit_default = MakeExitNode({ {"value", "int"} });
	if (!entry || !sw || !exit_case || !exit_default) return;

	SetNodeInt(sw, "case_0", 0);

	graph->AddNode(entry);
	graph->AddNode(sw);
	graph->AddNode(exit_case);
	graph->AddNode(exit_default);
	Connect(entry->GetOutputPin(0), sw->GetInputPin(0));
	ConnectOutArg(sw, "cond", entry, "sel");
	Connect(FindOutPin(sw, "case_0"), exit_case->GetInputPin(0));
	Connect(FindOutPin(sw, "default"), exit_default->GetInputPin(0));
	ConnectOutArg(exit_case, "value", entry, "sel");
	int fallback = -1;
	SetNodeInt(exit_default, "value", fallback);
	std::static_pointer_cast<VGraph>(graph)->SetEntry(entry);
	std::static_pointer_cast<VGraph>(graph)->SetExit(exit_case);
}

void BuildEmitFlows(const std::shared_ptr<VBlueprint>& bp, VSystem& system)
{
	WireReturnBoolOp(bp->FindFlow(bp->AddFlow("OpLess")), system.FindCaller("Less(int)"), "a", "b");
	WireReturnBoolOp(bp->FindFlow(bp->AddFlow("OpEqual")), system.FindCaller("Equal(int)"), "a", "b");
	WireReturnBoolOp(bp->FindFlow(bp->AddFlow("OpNotEqual")), system.FindCaller("NotEqual(int)"), "a", "b");
	WireReturnIntOp(bp->FindFlow(bp->AddFlow("OpMin")), system.FindCaller("Min(int)"), "a", "b");
	WireReturnIntOp(bp->FindFlow(bp->AddFlow("OpMax")), system.FindCaller("Max(int)"), "a", "b");

	{
		auto flow = bp->FindFlow(bp->AddFlow("OpNot"));
		auto graph = flow->GetGraph();
		auto entry = MakeEntryNode({ {"a", "bool"} });
		auto not_fn = MakeNode(system.FindCaller("Not(bool)"));
		auto exit = MakeExitNode({ {"value", "bool"} });
		graph->AddNode(entry);
		graph->AddNode(not_fn);
		graph->AddNode(exit);
		Connect(entry->GetOutputPin(0), exit->GetInputPin(0));
		ConnectOutArg(not_fn, "a", entry, "a");
		ConnectOutArg(exit, "value", not_fn, "return");
		std::static_pointer_cast<VGraph>(graph)->SetEntry(entry);
		std::static_pointer_cast<VGraph>(graph)->SetExit(exit);
	}

	{
		auto flow = bp->FindFlow(bp->AddFlow("OpAnd"));
		auto graph = flow->GetGraph();
		auto entry = MakeEntryNode({ {"a", "bool"}, {"b", "bool"} });
		auto and_fn = MakeNode(system.FindCaller("And(bool)"));
		auto exit = MakeExitNode({ {"value", "bool"} });
		graph->AddNode(entry);
		graph->AddNode(and_fn);
		graph->AddNode(exit);
		Connect(entry->GetOutputPin(0), exit->GetInputPin(0));
		ConnectOutArg(and_fn, "a", entry, "a");
		ConnectOutArg(and_fn, "b", entry, "b");
		ConnectOutArg(exit, "value", and_fn, "return");
		std::static_pointer_cast<VGraph>(graph)->SetEntry(entry);
		std::static_pointer_cast<VGraph>(graph)->SetExit(exit);
	}

	{
		auto flow = bp->FindFlow(bp->AddFlow("OpOr"));
		auto graph = flow->GetGraph();
		auto entry = MakeEntryNode({ {"a", "bool"}, {"b", "bool"} });
		auto or_fn = MakeNode(system.FindCaller("Or(bool)"));
		auto exit = MakeExitNode({ {"value", "bool"} });
		graph->AddNode(entry);
		graph->AddNode(or_fn);
		graph->AddNode(exit);
		Connect(entry->GetOutputPin(0), exit->GetInputPin(0));
		ConnectOutArg(or_fn, "a", entry, "a");
		ConnectOutArg(or_fn, "b", entry, "b");
		ConnectOutArg(exit, "value", or_fn, "return");
		std::static_pointer_cast<VGraph>(graph)->SetEntry(entry);
		std::static_pointer_cast<VGraph>(graph)->SetExit(exit);
	}
}

} // namespace

int TestNodesCodegen()
{
	const char* suite = "TestNodesCodegen";
	auto& system = VSystem::Get();

	auto blueprint = std::make_shared<VBlueprint>();
	blueprint->SetSpace("user");
	blueprint->SetName("NodeCover");

	BuildWhileFlow(blueprint, system);
	BuildLoopFlow(blueprint, system);
	BuildSequenceFlow(blueprint, system);
	BuildSwitchFlow(blueprint, system);
	BuildEmitFlows(blueprint, system);

	GeneratedCode code;
	if (!GenerateAll(suite, blueprint, code)) return 1;

	const std::vector<Check> checks = {
		{ "FWhile", "while (", code.source },
		{ "FLoop", "for (int index = ", code.source },
		{ "FSequence print a", "std::cout << a << std::endl;", code.source },
		{ "FSequence print b", "std::cout << b << std::endl;", code.source },
		{ "FSwitchInt", "switch (", code.source },
		{ "Less emit", "(a < b)", code.source },
		{ "Equal emit", "(a == b)", code.source },
		{ "NotEqual emit", "(a != b)", code.source },
		{ "Min emit", "std::min(", code.source },
		{ "Max emit", "std::max(", code.source },
		{ "Not emit", "!", code.source },
		{ "And emit", "&&", code.source },
		{ "Or emit", "||", code.source },
		{ "While method", "void WhileWait(", code.header },
		{ "Loop method", "int ForIndex(", code.header },
		{ "Switch method", "int SwitchPick(", code.header },
	};
	if (!RunChecks(suite, checks)) return 1;

	std::cout << suite << " PASSED\n";
	return 0;
}
