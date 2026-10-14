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

int TestBranch()
{
	const char* suite = "TestBranch";
	auto& system = VSystem::Get();

	auto blueprint = std::make_shared<VBlueprint>();
	blueprint->SetSpace("user");
	blueprint->SetName("VBranchTest");

	auto flow = blueprint->FindFlow(blueprint->AddFlow("Select"));
	auto greater_fn = system.FindCaller("Greater(int)");
	if (!flow || !greater_fn) return 1;

	auto graph = flow->GetGraph();
	auto entry = MakeEntryNode({ {"a", "int"}, {"b", "int"} });
	auto gt = MakeNode(greater_fn);
	auto branch = MakeNode(std::make_shared<FBranch>());
	auto exit = MakeExitNode({ {"value", "int"} });
	if (!entry || !gt || !branch || !exit) return 1;

	graph->AddNode(entry);
	graph->AddNode(gt);
	graph->AddNode(branch);
	graph->AddNode(exit);

	Connect(entry->GetOutputPin(0), branch->GetInputPin(0));
	ConnectOutArg(gt, "a", entry, "a");
	ConnectOutArg(gt, "b", entry, "b");
	ConnectOutArg(branch, "cond", gt, "return");
	Connect(FindOutPin(branch, "true"), exit->GetInputPin(0));
	ConnectOutArg(exit, "value", entry, "a");
	std::static_pointer_cast<VGraph>(graph)->SetEntry(entry);
	std::static_pointer_cast<VGraph>(graph)->SetExit(exit);

	GeneratedCode code;
	if (!GenerateAll(suite, blueprint, code)) return 1;

	const std::vector<Check> checks = {
		{ "signature", "int Select(int a, int b);", code.header },
		{ "if else codegen", "if (", code.source },
		{ "if else codegen", "} else {", code.source },
		{ "return value", "return a;", code.source },
		{ "method register", "fn->SetName(\"Select\")", code.gen_source },
	};
	if (!RunChecks(suite, checks)) return 1;

	std::cout << suite << " PASSED\n";
	return 0;
}
