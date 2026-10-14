#include "Test/TestUtil.h"

#include "Realistic/VController.h"
#include "Realistic/VFlow.h"
#include "Realistic/VGraph.h"
#include "Realistic/VNode.h"
#include "Realistic/VOperator.h"
#include "Realistic/VVariant.h"
#include "Realistic/VVarFunction.h"
#include "Base/VSystem.h"
#include "CodeGen/CGraph.h"

#include <iostream>
#include <memory>

using namespace CG::Core;
using namespace CG::Core::cg;
using namespace test;

namespace {

bool RunSetVarLiteral(const char* suite)
{
	auto blueprint = std::make_shared<VBlueprint>();
	auto var_guid = blueprint->AddVar("value", "int");
	auto var = std::static_pointer_cast<VVariant>(blueprint->FindVar(var_guid));
	auto flow = blueprint->FindFlow(blueprint->AddFlow("Run"));
	if (!var || !flow) return false;

	auto graph = flow->GetGraph();
	auto entry = MakeEntryNode();
	auto setter = MakeSetVarNode(var);
	if (!entry || !setter) return false;

	if (!SetNodeInt(setter, "value", 42)) return false;

	graph->AddNode(entry);
	graph->AddNode(setter);
	Connect(entry->GetOutputPin(0), setter->GetInputPin(0));
	std::static_pointer_cast<VGraph>(graph)->SetEntry(entry);

	if (!RunFlowOk(suite, "SetVarLiteral", flow)) return false;

	int out = 0;
	if (!GetVarInt(var, out) || out != 42) {
		std::cerr << suite << " SetVarLiteral: expected 42, got " << out << '\n';
		return false;
	}
	std::cout << suite << " OK: SetVarLiteral\n";
	return true;
}

bool RunPureAdd(const char* suite, VSystem& system)
{
	auto blueprint = std::make_shared<VBlueprint>();
	auto var_guid = blueprint->AddVar("sum", "int");
	auto var = std::static_pointer_cast<VVariant>(blueprint->FindVar(var_guid));
	auto flow = blueprint->FindFlow(blueprint->AddFlow("Run"));
	auto add_fn = system.FindCaller("Add(int)");
	if (!var || !flow || !add_fn) return false;

	auto graph = flow->GetGraph();
	auto entry = MakeEntryNode({ {"a", "int"}, {"b", "int"} });
	auto add_node = MakeNode(add_fn);
	auto setter = MakeSetVarNode(var);
	if (!entry || !add_node || !setter) return false;

	graph->AddNode(entry);
	graph->AddNode(add_node);
	graph->AddNode(setter);
	Connect(entry->GetOutputPin(0), setter->GetInputPin(0));
	ConnectOutArg(add_node, "a", entry, "a");
	ConnectOutArg(add_node, "b", entry, "b");
	ConnectOutArg(setter, "value", add_node, "return");
	std::static_pointer_cast<VGraph>(graph)->SetEntry(entry);

	if (!SetNodeInt(entry, "a", 10) || !SetNodeInt(entry, "b", 32)) return false;
	if (!RunFlowOk(suite, "PureAdd", flow)) return false;

	int out = 0;
	if (!GetVarInt(var, out) || out != 42) {
		std::cerr << suite << " PureAdd: expected 42, got " << out << '\n';
		return false;
	}
	std::cout << suite << " OK: PureAdd\n";
	return true;
}

bool RunBranch(const char* suite, VSystem& system)
{
	auto blueprint = std::make_shared<VBlueprint>();
	auto var_guid = blueprint->AddVar("result", "int");
	auto var = std::static_pointer_cast<VVariant>(blueprint->FindVar(var_guid));
	auto flow = blueprint->FindFlow(blueprint->AddFlow("Run"));
	auto greater = system.FindCaller("Greater(int)");
	if (!var || !flow || !greater) return false;

	auto graph = flow->GetGraph();
	auto entry = MakeEntryNode({ {"a", "int"}, {"b", "int"} });
	auto gt = MakeNode(greater);
	auto branch = MakeNode(std::make_shared<FBranch>());
	auto set_true = MakeSetVarNode(var);
	auto set_false = MakeSetVarNode(var);
	if (!entry || !gt || !branch || !set_true || !set_false) return false;

	if (!SetNodeInt(set_true, "value", 1) || !SetNodeInt(set_false, "value", 2)) return false;

	graph->AddNode(entry);
	graph->AddNode(gt);
	graph->AddNode(branch);
	graph->AddNode(set_true);
	graph->AddNode(set_false);
	Connect(entry->GetOutputPin(0), branch->GetInputPin(0));
	ConnectOutArg(gt, "a", entry, "a");
	ConnectOutArg(gt, "b", entry, "b");
	ConnectOutArg(branch, "cond", gt, "return");
	Connect(FindOutPin(branch, "true"), set_true->GetInputPin(0));
	Connect(FindOutPin(branch, "false"), set_false->GetInputPin(0));
	std::static_pointer_cast<VGraph>(graph)->SetEntry(entry);

	if (!SetNodeInt(entry, "a", 5) || !SetNodeInt(entry, "b", 3)) return false;
	if (!RunFlowOk(suite, "BranchTrue", flow, "branch-true")) return false;

	int out = 0;
	if (!GetVarInt(var, out) || out != 1) {
		std::cerr << suite << " BranchTrue: expected 1, got " << out << '\n';
		return false;
	}

	if (!SetNodeInt(entry, "a", 1) || !SetNodeInt(entry, "b", 9)) return false;
	if (!RunFlowOk(suite, "BranchFalse", flow, "branch-false")) return false;
	if (!GetVarInt(var, out) || out != 2) {
		std::cerr << suite << " BranchFalse: expected 2, got " << out << '\n';
		return false;
	}
	std::cout << suite << " OK: Branch\n";
	return true;
}

bool RunSequence(const char* suite)
{
	auto blueprint = std::make_shared<VBlueprint>();
	auto var_guid = blueprint->AddVar("step", "int");
	auto var = std::static_pointer_cast<VVariant>(blueprint->FindVar(var_guid));
	auto flow = blueprint->FindFlow(blueprint->AddFlow("Run"));
	if (!var || !flow) return false;

	auto graph = flow->GetGraph();
	auto entry = MakeEntryNode();
	auto seq = MakeNode(std::make_shared<FSequence>());
	auto set_first = MakeSetVarNode(var);
	auto set_second = MakeSetVarNode(var);
	if (!entry || !seq || !set_first || !set_second) return false;

	if (!SetNodeInt(set_first, "value", 1) || !SetNodeInt(set_second, "value", 2)) return false;

	graph->AddNode(entry);
	graph->AddNode(seq);
	graph->AddNode(set_first);
	graph->AddNode(set_second);
	Connect(entry->GetOutputPin(0), seq->GetInputPin(0));
	Connect(FindOutPin(seq, "then_0"), set_first->GetInputPin(0));
	Connect(FindOutPin(seq, "then_1"), set_second->GetInputPin(0));
	std::static_pointer_cast<VGraph>(graph)->SetEntry(entry);

	if (!RunFlowOk(suite, "Sequence", flow)) return false;

	int out = 0;
	if (!GetVarInt(var, out) || out != 2) {
		std::cerr << suite << " Sequence: expected 2, got " << out << '\n';
		return false;
	}
	std::cout << suite << " OK: Sequence\n";
	return true;
}

bool RunWhileSkipBody(const char* suite, VSystem& system)
{
	auto blueprint = std::make_shared<VBlueprint>();
	auto var_guid = blueprint->AddVar("hits", "int");
	auto var = std::static_pointer_cast<VVariant>(blueprint->FindVar(var_guid));
	auto flow = blueprint->FindFlow(blueprint->AddFlow("Run"));
	auto less = system.FindCaller("Less(int)");
	if (!var || !flow || !less) return false;

	auto graph = flow->GetGraph();
	auto entry = MakeEntryNode({ {"a", "int"}, {"b", "int"} });
	auto cmp = MakeNode(less);
	auto while_node = MakeNode(std::make_shared<FWhile>());
	auto bump = MakeSetVarNode(var);
	auto done = MakeSetVarNode(var);
	if (!entry || !cmp || !while_node || !bump || !done) return false;

	if (!SetNodeInt(bump, "value", 99) || !SetNodeInt(done, "value", 7)) return false;

	graph->AddNode(entry);
	graph->AddNode(cmp);
	graph->AddNode(while_node);
	graph->AddNode(bump);
	graph->AddNode(done);
	Connect(entry->GetOutputPin(0), while_node->GetInputPin(0));
	ConnectOutArg(cmp, "a", entry, "a");
	ConnectOutArg(cmp, "b", entry, "b");
	ConnectOutArg(while_node, "cond", cmp, "return");
	Connect(FindOutPin(while_node, "body"), bump->GetInputPin(0));
	Connect(FindOutPin(while_node, "completed"), done->GetInputPin(0));
	std::static_pointer_cast<VGraph>(graph)->SetEntry(entry);

	if (!SetNodeInt(entry, "a", 5) || !SetNodeInt(entry, "b", 3)) return false;
	if (!RunFlowOk(suite, "WhileSkipBody", flow)) return false;

	int out = 0;
	if (!GetVarInt(var, out) || out != 7) {
		std::cerr << suite << " WhileSkipBody: expected 7, got " << out << '\n';
		return false;
	}
	std::cout << suite << " OK: WhileSkipBody\n";
	return true;
}

bool RunWhileBump(const char* suite, VSystem& system)
{
	auto blueprint = std::make_shared<VBlueprint>();
	auto var_guid = blueprint->AddVar("a", "int");
	auto var = std::static_pointer_cast<VVariant>(blueprint->FindVar(var_guid));
	auto flow = blueprint->FindFlow(blueprint->AddFlow("Run"));
	auto less = system.FindCaller("Less(int)");
	auto add_fn = system.FindCaller("Add(int)");
	if (!var || !flow || !less || !add_fn) return false;
	if (!SetVarInt(var, 0)) return false;

	auto graph = flow->GetGraph();
	auto entry = MakeEntryNode();
	auto getter_cond = MakeGetVarNode(var);
	auto cmp = MakeNode(less);
	auto while_node = MakeNode(std::make_shared<FWhile>());
	auto getter_add = MakeGetVarNode(var);
	auto add_node = MakeNode(add_fn);
	auto setter = MakeSetVarNode(var);
	if (!entry || !getter_cond || !cmp || !while_node || !getter_add || !add_node || !setter)
		return false;
	if (!SetNodeInt(cmp, "b", 5) || !SetNodeInt(add_node, "b", 1))
		return false;

	graph->AddNode(entry);
	graph->AddNode(getter_cond);
	graph->AddNode(cmp);
	graph->AddNode(while_node);
	graph->AddNode(getter_add);
	graph->AddNode(add_node);
	graph->AddNode(setter);
	Connect(entry->GetOutputPin(0), while_node->GetInputPin(0));
	ConnectOutArg(cmp, "a", getter_cond, "return");
	ConnectOutArg(while_node, "cond", cmp, "return");
	Connect(FindOutPin(while_node, "body"), setter->GetInputPin(0));
	ConnectOutArg(add_node, "a", getter_add, "return");
	ConnectOutArg(setter, "value", add_node, "return");
	std::static_pointer_cast<VGraph>(graph)->SetEntry(entry);

	if (!RunFlowOk(suite, "WhileBump", flow)) return false;

	int out = 0;
	if (!GetVarInt(var, out) || out != 5) {
		std::cerr << suite << " WhileBump: expected a == 5, got " << out << '\n';
		return false;
	}
	std::cout << suite << " OK: WhileBump\n";
	return true;
}

bool RunWhilePrint(const char* suite, VSystem& system)
{
	auto blueprint = std::make_shared<VBlueprint>();
	auto var_guid = blueprint->AddVar("a", "int");
	auto var = std::static_pointer_cast<VVariant>(blueprint->FindVar(var_guid));
	auto flow = blueprint->FindFlow(blueprint->AddFlow("Run"));
	auto less = system.FindCaller("Less(int)");
	auto add_fn = system.FindCaller("Add(int)");
	auto print_fn = system.FindCaller("Print");
	if (!var || !flow || !less || !add_fn || !print_fn) return false;
	if (!SetVarInt(var, 0)) return false;

	auto graph = flow->GetGraph();
	auto entry = MakeEntryNode();
	auto getter_cond = MakeGetVarNode(var);
	auto cmp = MakeNode(less);
	auto while_node = MakeNode(std::make_shared<FWhile>());
	auto getter_add = MakeGetVarNode(var);
	auto add_node = MakeNode(add_fn);
	auto setter = MakeSetVarNode(var);
	auto print_node = MakeNode(print_fn);
	if (!entry || !getter_cond || !cmp || !while_node || !getter_add || !add_node || !setter || !print_node)
		return false;
	if (!SetNodeInt(cmp, "b", 5) || !SetNodeInt(add_node, "b", 1))
		return false;

	graph->AddNode(entry);
	graph->AddNode(getter_cond);
	graph->AddNode(cmp);
	graph->AddNode(while_node);
	graph->AddNode(getter_add);
	graph->AddNode(add_node);
	graph->AddNode(setter);
	graph->AddNode(print_node);
	Connect(entry->GetOutputPin(0), while_node->GetInputPin(0));
	ConnectOutArg(cmp, "a", getter_cond, "return");
	ConnectOutArg(while_node, "cond", cmp, "return");
	Connect(FindOutPin(while_node, "body"), setter->GetInputPin(0));
	Connect(setter->GetOutputPin(0), print_node->GetInputPin(0));
	ConnectOutArg(add_node, "a", getter_add, "return");
	ConnectOutArg(setter, "value", add_node, "return");
	ConnectOutArg(print_node, "value", getter_add, "return");
	std::static_pointer_cast<VGraph>(graph)->SetEntry(entry);

	std::string printed;
	{
		CoutCapture capture;
		if (!RunFlowOk(suite, "WhilePrint", flow)) return false;
		printed = capture.Str();
	}

	int out = 0;
	if (!GetVarInt(var, out) || out != 5) {
		std::cerr << suite << " WhilePrint: expected a == 5, got " << out << '\n';
		return false;
	}
	if (printed.find("1") == std::string::npos
		|| printed.find("2") == std::string::npos
		|| printed.find("3") == std::string::npos
		|| printed.find("4") == std::string::npos
		|| printed.find("5") == std::string::npos) {
		std::cerr << suite << " WhilePrint: stdout missing 1..5, got [" << printed << "]\n";
		return false;
	}
	std::cout << suite << " OK: WhilePrint\n";
	return true;
}

bool RunLoopCount(const char* suite, VSystem& system)
{
	auto blueprint = std::make_shared<VBlueprint>();
	auto var_guid = blueprint->AddVar("counter", "int");
	auto var = std::static_pointer_cast<VVariant>(blueprint->FindVar(var_guid));
	auto flow = blueprint->FindFlow(blueprint->AddFlow("Run"));
	auto add_fn = system.FindCaller("Add(int)");
	if (!var || !flow || !add_fn) return false;
	if (!SetVarInt(var, 0)) return false;

	auto graph = flow->GetGraph();
	auto entry = MakeEntryNode({ {"run", "bool"}, {"first", "int"}, {"last", "int"} });
	auto loop = MakeNode(std::make_shared<FLoop>());
	auto getter = MakeGetVarNode(var);
	auto add_one = MakeNode(add_fn);
	auto setter = MakeSetVarNode(var);
	auto done = MakeExitNode();
	if (!entry || !loop || !getter || !add_one || !setter || !done) return false;

	if (!SetNodeInt(add_one, "b", 1)) return false;

	graph->AddNode(entry);
	graph->AddNode(loop);
	graph->AddNode(getter);
	graph->AddNode(add_one);
	graph->AddNode(setter);
	graph->AddNode(done);
	Connect(entry->GetOutputPin(0), loop->GetInputPin(0));
	ConnectOutArg(loop, "cond", entry, "run");
	ConnectOutArg(loop, "first", entry, "first");
	ConnectOutArg(loop, "last", entry, "last");
	Connect(FindOutPin(loop, "body"), setter->GetInputPin(0));
	Connect(FindOutPin(loop, "completed"), done->GetInputPin(0));
	ConnectOutArg(add_one, "a", getter, "return");
	ConnectOutArg(setter, "value", add_one, "return");
	std::static_pointer_cast<VGraph>(graph)->SetEntry(entry);

	if (!SetNodeBool(entry, "run", true) || !SetNodeInt(entry, "first", 0) || !SetNodeInt(entry, "last", 3))
		return false;
	if (!RunFlowOk(suite, "LoopCount", flow, "loop-count")) return false;

	int out = 0;
	if (!GetVarInt(var, out) || out != 3) {
		std::cerr << suite << " LoopCount: expected counter == 3, got " << out << '\n';
		return false;
	}
	std::cout << suite << " OK: LoopCount (" << out << " iterations)\n";
	return true;
}

bool RunSwitchCase(const char* suite)
{
	auto blueprint = std::make_shared<VBlueprint>();
	auto var_guid = blueprint->AddVar("picked", "int");
	auto var = std::static_pointer_cast<VVariant>(blueprint->FindVar(var_guid));
	auto flow = blueprint->FindFlow(blueprint->AddFlow("Run"));
	if (!var || !flow) return false;

	auto graph = flow->GetGraph();
	auto entry = MakeEntryNode({ {"sel", "int"} });
	auto sw = MakeNode(std::make_shared<FSwitchInt>());
	auto set_case = MakeSetVarNode(var);
	auto set_default = MakeSetVarNode(var);
	if (!entry || !sw || !set_case || !set_default) return false;

	if (!SetNodeInt(sw, "case_0", 0)) return false;
	if (!SetNodeInt(set_case, "value", 10) || !SetNodeInt(set_default, "value", 99)) return false;

	graph->AddNode(entry);
	graph->AddNode(sw);
	graph->AddNode(set_case);
	graph->AddNode(set_default);
	Connect(entry->GetOutputPin(0), sw->GetInputPin(0));
	ConnectOutArg(sw, "cond", entry, "sel");
	Connect(FindOutPin(sw, "case_0"), set_case->GetInputPin(0));
	Connect(FindOutPin(sw, "default"), set_default->GetInputPin(0));
	std::static_pointer_cast<VGraph>(graph)->SetEntry(entry);

	if (!SetNodeInt(entry, "sel", 0)) return false;
	if (!RunFlowOk(suite, "SwitchCase0", flow, "switch-0")) return false;

	int out = 0;
	if (!GetVarInt(var, out) || out != 10) {
		std::cerr << suite << " SwitchCase0: expected 10, got " << out << '\n';
		return false;
	}

	if (!SetNodeInt(entry, "sel", 5)) return false;
	if (!RunFlowOk(suite, "SwitchDefault", flow, "switch-default")) return false;
	if (!GetVarInt(var, out) || out != 99) {
		std::cerr << suite << " SwitchDefault: expected 99, got " << out << '\n';
		return false;
	}
	std::cout << suite << " OK: Switch\n";
	return true;
}

bool RunSwitchAddDelCase(const char* suite)
{
	auto blueprint = std::make_shared<VBlueprint>();
	auto var_guid = blueprint->AddVar("picked", "int");
	auto var = std::static_pointer_cast<VVariant>(blueprint->FindVar(var_guid));
	auto flow = blueprint->FindFlow(blueprint->AddFlow("Run"));
	if (!var || !flow) return false;

	auto sw_fn = std::make_shared<FSwitchInt>();
	if (sw_fn->AddCase() != "case_1") {
		std::cerr << suite << " SwitchAddDel: AddCase did not yield case_1\n";
		return false;
	}
	if (sw_fn->DelCase("default") || sw_fn->DelCase("cond")) {
		std::cerr << suite << " SwitchAddDel: default/cond must not delete\n";
		return false;
	}

	auto graph = flow->GetGraph();
	auto entry = MakeEntryNode({ {"sel", "int"} });
	auto sw = MakeNode(sw_fn);
	auto set_one = MakeSetVarNode(var);
	auto set_sixteen = MakeSetVarNode(var);
	auto set_default = MakeSetVarNode(var);
	if (!entry || !sw || !set_one || !set_sixteen || !set_default) return false;
	if (!SetNodeInt(sw, "case_0", 1) || !SetNodeInt(sw, "case_1", 16)) return false;
	if (!SetNodeInt(set_one, "value", 10) || !SetNodeInt(set_sixteen, "value", 20)
		|| !SetNodeInt(set_default, "value", 99))
		return false;

	graph->AddNode(entry);
	graph->AddNode(sw);
	graph->AddNode(set_one);
	graph->AddNode(set_sixteen);
	graph->AddNode(set_default);
	Connect(entry->GetOutputPin(0), sw->GetInputPin(0));
	ConnectOutArg(sw, "cond", entry, "sel");
	Connect(FindOutPin(sw, "case_0"), set_one->GetInputPin(0));
	Connect(FindOutPin(sw, "case_1"), set_sixteen->GetInputPin(0));
	Connect(FindOutPin(sw, "default"), set_default->GetInputPin(0));
	std::static_pointer_cast<VGraph>(graph)->SetEntry(entry);

	if (!SetNodeInt(entry, "sel", 1) || !RunFlowOk(suite, "SwitchCase1", flow, "switch-1"))
		return false;
	int out = 0;
	if (!GetVarInt(var, out) || out != 10) {
		std::cerr << suite << " SwitchCase1: expected 10, got " << out << '\n';
		return false;
	}
	if (!SetNodeInt(entry, "sel", 16) || !RunFlowOk(suite, "SwitchCase16", flow, "switch-16"))
		return false;
	if (!GetVarInt(var, out) || out != 20) {
		std::cerr << suite << " SwitchCase16: expected 20, got " << out << '\n';
		return false;
	}

	const auto case1_guid = FindOutPin(sw, "case_1")->GetGuid();
	const auto default_guid = FindOutPin(sw, "default")->GetGuid();
	const auto cond_guid = FindInPin(sw, "cond")->GetGuid();
	if (sw_fn->AddCase() != "case_2") {
		std::cerr << suite << " SwitchAddDel: AddCase did not yield case_2\n";
		return false;
	}
	std::static_pointer_cast<VNode>(sw)->ApplyMeta();
	if (!FindOutPin(sw, "case_2")
		|| FindInPin(sw, "cond")->GetGuid() != cond_guid
		|| FindOutPin(sw, "case_1")->GetGuid() != case1_guid
		|| FindOutPin(sw, "default")->GetGuid() != default_guid) {
		std::cerr << suite << " SwitchAddDel: AddCase rebuilt surviving pins\n";
		return false;
	}

	if (!sw_fn->DelCase("case_0")) {
		std::cerr << suite << " SwitchAddDel: DelCase case_0 failed\n";
		return false;
	}
	std::static_pointer_cast<VNode>(sw)->ApplyMeta();
	if (FindOutPin(sw, "case_0") || !FindOutPin(sw, "case_1") || !FindOutPin(sw, "default")) {
		std::cerr << suite << " SwitchAddDel: pin layout after DelCase is wrong\n";
		return false;
	}
	if (FindOutPin(sw, "case_1")->GetGuid() != case1_guid
		|| FindOutPin(sw, "default")->GetGuid() != default_guid) {
		std::cerr << suite << " SwitchAddDel: DelCase rebuilt surviving pins\n";
		return false;
	}

	if (!SetNodeInt(entry, "sel", 1) || !RunFlowOk(suite, "SwitchAfterDel", flow, "switch-del"))
		return false;
	if (!GetVarInt(var, out) || out != 99) {
		std::cerr << suite << " SwitchAfterDel: expected default 99, got " << out << '\n';
		return false;
	}
	if (!SetNodeInt(entry, "sel", 16) || !RunFlowOk(suite, "SwitchAfterDel16", flow, "switch-del-16"))
		return false;
	if (!GetVarInt(var, out) || out != 20) {
		std::cerr << suite << " SwitchAfterDel16: expected 20, got " << out << '\n';
		return false;
	}
	std::cout << suite << " OK: SwitchAddDelCase\n";
	return true;
}

bool RunPrint(const char* suite, VSystem& system)
{
	auto blueprint = std::make_shared<VBlueprint>();
	auto flow = blueprint->FindFlow(blueprint->AddFlow("Run"));
	auto print = system.FindCaller("Print");
	if (!flow || !print) return false;

	auto graph = flow->GetGraph();
	auto entry = MakeEntryNode();
	auto print_node = MakeNode(print);
	if (!entry || !print_node) return false;

	if (!SetNodeInt(print_node, "value", 123)) return false;

	graph->AddNode(entry);
	graph->AddNode(print_node);
	Connect(entry->GetOutputPin(0), print_node->GetInputPin(0));
	std::static_pointer_cast<VGraph>(graph)->SetEntry(entry);

	CoutCapture capture;
	if (!RunFlowOk(suite, "Print", flow)) return false;
	if (capture.Str().find("123") == std::string::npos) {
		std::cerr << suite << " Print: stdout missing 123\n";
		return false;
	}
	std::cout << suite << " OK: Print\n";
	return true;
}

} // namespace

int TestBlueprintRun()
{
	const char* suite = "TestBlueprintRun";
	auto& system = VSystem::Get();

	if (!RunSetVarLiteral(suite)) return 1;
	if (!RunPureAdd(suite, system)) return 1;
	if (!RunBranch(suite, system)) return 1;
	if (!RunSequence(suite)) return 1;
	if (!RunWhileSkipBody(suite, system)) return 1;
	if (!RunWhileBump(suite, system)) return 1;
	if (!RunWhilePrint(suite, system)) return 1;
	if (!RunLoopCount(suite, system)) return 1;
	if (!RunSwitchCase(suite)) return 1;
	if (!RunSwitchAddDelCase(suite)) return 1;
	if (!RunPrint(suite, system)) return 1;

	std::cout << suite << " PASSED\n";
	return 0;
}
