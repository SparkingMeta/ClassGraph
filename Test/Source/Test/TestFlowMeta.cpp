#include "Test/TestUtil.h"

#include "Realistic/VFlow.h"
#include "Realistic/VGraph.h"
#include "Base/VRegister.h"
#include "Base/VSystem.h"
#include "CodeGen/CGraph.h"

#include <iostream>
#include <memory>

using namespace CG::Core;
using namespace CG::Core::cg;
using namespace test;

int TestFlowMeta()
{
	const char* suite = "TestFlowMeta";

	auto blueprint = std::make_shared<VBlueprint>();
	blueprint->SetName("MetaProbe");
	auto flow = blueprint->FindFlow(blueprint->AddFlow("Probe"));
	if (!flow) return 1;

	auto graph = flow->GetGraph();
	auto entry = MakeEntryNode({ {"x", "int"}, {"y", "int"} });
	auto exit = MakeExitNode({ {"value", "bool"} });
	auto gt = MakeNode(VSystem::Get().FindCaller("Greater(int)"));
	if (!entry || !exit || !gt) return 1;

	graph->AddNode(entry);
	graph->AddNode(gt);
	graph->AddNode(exit);
	Connect(entry->GetOutputPin(0), gt->GetInputPin(0));
	ConnectOutArg(gt, "a", entry, "x");
	ConnectOutArg(gt, "b", entry, "y");
	ConnectOutArg(exit, "value", gt, "return");
	std::static_pointer_cast<VGraph>(graph)->SetEntry(entry);
	std::static_pointer_cast<VGraph>(graph)->SetExit(exit);

	const auto& meta = flow->GetFlowMeta();
	if (meta.params.size() != 3) {
		std::cerr << suite << " FAILED: expected 3 params, got " << meta.params.size() << '\n';
		return 1;
	}
	if (meta.params[0].name != "x" || meta.params[0].is_output || meta.params[0].type != VTypeRegister::Find("int")) {
		std::cerr << suite << " FAILED: input param x\n";
		return 1;
	}
	if (meta.params[2].name != "value" || !meta.params[2].is_output || meta.params[2].type != VTypeRegister::Find("bool")) {
		std::cerr << suite << " FAILED: output param value\n";
		return 1;
	}

	std::cout << suite << " PASSED\n";
	return 0;
}
