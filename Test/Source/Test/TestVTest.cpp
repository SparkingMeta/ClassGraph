#include "Test/TestUtil.h"

#include "Realistic/VFlow.h"
#include "Realistic/VGraph.h"
#include "Base/VSystem.h"

#include <iostream>
#include <memory>
#include <vector>

using namespace CG::Core;
using namespace test;

int TestVTest()
{
	const char* suite = "TestVTest";
	auto& system = VSystem::Get();

	auto blueprint = std::make_shared<VBlueprint>();
	blueprint->SetGuid("{A3F2E1D0-4B5C-6D7E-8F9A-0B1C2D3E4F5A}");
	blueprint->SetSpace("user");
	blueprint->SetName("VTest");

	if (blueprint->AddVar("score", "int").empty()) return 1;

	auto add_int_flow = blueprint->FindFlow(blueprint->AddFlow("AddInt"));
	auto print_flow = blueprint->FindFlow(blueprint->AddFlow("Print"));
	auto add_fn = system.FindCaller("Add(int)");
	auto print_caller = system.FindCaller("Print");
	if (!add_int_flow || !print_flow || !add_fn || !print_caller) return 1;

	{
		auto graph = add_int_flow->GetGraph();
		auto entry = MakeEntryNode({ {"a", "int"}, {"b", "int"} });
		auto add_node = MakeNode(add_fn);
		auto exit_node = MakeExitNode({ {"value", "int"} });
		graph->AddNode(entry);
		graph->AddNode(add_node);
		graph->AddNode(exit_node);
		Connect(entry->GetOutputPin(0), exit_node->GetInputPin(0));
		ConnectOutArg(add_node, "a", entry, "a");
		ConnectOutArg(add_node, "b", entry, "b");
		ConnectOutArg(exit_node, "value", add_node, "return");
		std::static_pointer_cast<VGraph>(graph)->SetEntry(entry);
		std::static_pointer_cast<VGraph>(graph)->SetExit(exit_node);
	}

	{
		auto graph = print_flow->GetGraph();
		auto entry = MakeEntryNode({ {"value", "std::string"} });
		auto print_node = MakeNode(print_caller);
		graph->AddNode(entry);
		graph->AddNode(print_node);
		Connect(entry->GetOutputPin(0), print_node->GetInputPin(0));
		ConnectOutArg(print_node, "value", entry, "value");
		std::static_pointer_cast<VGraph>(graph)->SetEntry(entry);
	}

	GeneratedCode code;
	if (!GenerateAll(suite, blueprint, code)) return 1;
	if (!VerifyGeneratedBodyMacro(suite, "VTest", code.header, code.gen_header)) return 1;

	const std::vector<Check> checks = {
		{ "class decl", "class VTest : public CG::Core::VBlueprintObject", code.header },
		{ "namespace", "namespace user", code.header },
		{ "GENERATED_BODY", "GENERATED_BODY()", code.header },
		{ "member field", "int score;", code.header },
		{ "AddInt signature", "int AddInt(int a, int b);", code.header },
		{ "Print signature", "void Print(std::string value);", code.header },
		{ "string include in header", "#include <string>", code.header },
		{ "no iostream in header", "#include <iostream>", code.header, false },
		{ "iostream in cpp", "#include <iostream>", code.source },
		{ "AddInt body", "return (a + b);", code.source },
		{ "Print body", "std::cout << value << std::endl;", code.source },
		{ "gen.h construct decl", "CG::Core::VClass* Z_Construct_VClass_VTest();", code.gen_header },
		{ "property register", "cls->AddProperty(CG::Core::LoadProperty(&user::VTest::score", code.gen_source },
		{ "AddInt method register", "fn->SetName(\"AddInt\")", code.gen_source },
		{ "Print method register", "fn->SetName(\"Print\")", code.gen_source },
		{ "RegisterCls", "CG::Core::VSystem::Get().RegisterCls(cls);", code.gen_source },
	};
	if (!RunChecks(suite, checks)) return 1;
	if (!WriteAllOutputs(blueprint, code)) {
		std::cerr << suite << ": write files failed\n";
		return 1;
	}

	std::cout << suite << " PASSED\n";
	return 0;
}
