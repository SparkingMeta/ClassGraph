#include "Test/TestUtil.h"

#include "Abstract/ACaller.h"
#include "Abstract/AScript.h"
#include "Realistic/VContext.h"
#include "Realistic/VFlow.h"
#include "Realistic/VFrame.h"
#include "Realistic/VGraph.h"
#include "Realistic/VVariant.h"
#include "Realistic/VVarFunction.h"
#include "Base/VSystem.h"

#include <iostream>
#include <memory>
#include <vector>

using namespace CG::Core;
using namespace test;

int TestMyBranch()
{
	const char* suite = "TestMyBranch";
	auto& system = VSystem::Get();

	auto blueprint = std::make_shared<VBlueprint>();
	blueprint->SetSpace("user");
	blueprint->SetName("MyBranch");

	auto a_guid = blueprint->AddVar("a", "int");
	auto b_guid = blueprint->AddVar("b", "int");
	if (a_guid.empty() || b_guid.empty()) return 1;

	auto var_a = std::static_pointer_cast<VVariant>(blueprint->FindVar(a_guid));
	auto var_b = std::static_pointer_cast<VVariant>(blueprint->FindVar(b_guid));

	auto print_flow = blueprint->FindFlow(blueprint->AddFlow("PrintValue"));
	auto echo_flow = blueprint->FindFlow(blueprint->AddFlow("Echo"));
	auto run_flow = blueprint->FindFlow(blueprint->AddFlow("Run"));
	if (!print_flow || !echo_flow || !run_flow) return 1;

	{
		auto graph = print_flow->GetGraph();
		auto entry = MakeEntryNode({ {"value", "int"} });
		auto print_node = MakeNode(system.FindCaller("Print"));
		if (!entry || !print_node) return 1;
		graph->AddNode(entry);
		graph->AddNode(print_node);
		Connect(entry->GetOutputPin(0), print_node->GetInputPin(0));
		ConnectOutArg(print_node, "value", entry, "value");
		std::static_pointer_cast<VGraph>(graph)->SetEntry(entry);
	}

	{
		auto graph = echo_flow->GetGraph();
		auto entry = MakeEntryNode({ {"x", "int"}, {"tag", "std::string"} });
		if (!entry) return 1;
		graph->AddNode(entry);
		std::static_pointer_cast<VGraph>(graph)->SetEntry(entry);
	}

	{
		auto graph = run_flow->GetGraph();
		auto setter_a = MakeSetVarNode(var_a);
		auto setter_b = MakeSetVarNode(var_b);
		auto getter_a = MakeGetVarNode(var_a);
		auto print_script = std::static_pointer_cast<ACaller>(print_flow->GetScript());
		auto call_print = MakeNode(print_script);
		auto entry = MakeEntryNode();
		if (!setter_a || !setter_b || !getter_a || !call_print || !entry) return 1;

		int a_value = 20;
		int b_value = 7;
		setter_a->GetContext()->GetFrame("value")->SetValue(var_a->GetType(), &a_value);
		setter_b->GetContext()->GetFrame("value")->SetValue(var_b->GetType(), &b_value);

		graph->AddNode(entry);
		graph->AddNode(setter_a);
		graph->AddNode(setter_b);
		graph->AddNode(getter_a);
		graph->AddNode(call_print);
		Connect(entry->GetOutputPin(0), setter_a->GetInputPin(0));
		Connect(setter_a->GetOutputPin(0), setter_b->GetInputPin(0));
		Connect(setter_b->GetOutputPin(0), call_print->GetInputPin(0));
		ConnectArg(call_print, "value", getter_a->GetOutputPin(0));
		std::static_pointer_cast<VGraph>(graph)->SetEntry(entry);

		run_flow->Run();
	}

	GeneratedCode code;
	if (!GenerateAll(suite, blueprint, code)) return 1;
	if (!VerifyGeneratedBodyMacro(suite, "MyBranch", code.header, code.gen_header)) return 1;

	const std::vector<Check> checks = {
		{ "member a", "int a;", code.header },
		{ "member b", "int b;", code.header },
		{ "PrintValue signature", "void PrintValue(int value);", code.header },
		{ "Echo signature", "void Echo(int x, std::string tag);", code.header },
		{ "Run signature", "void Run();", code.header },
		{ "string include", "#include <string>", code.header },
		{ "no iostream in header", "#include <iostream>", code.header, false },
		{ "member assign a", "a = 20;", code.source },
		{ "member assign b", "b = 7;", code.source },
		{ "subflow call", "this->PrintValue(a);", code.source },
		{ "property a", "LoadProperty(&user::MyBranch::a", code.gen_source },
		{ "property b", "LoadProperty(&user::MyBranch::b", code.gen_source },
		{ "method Run", "fn->SetName(\"Run\")", code.gen_source },
		{ "method PrintValue", "fn->SetName(\"PrintValue\")", code.gen_source },
		{ "method Echo", "fn->SetName(\"Echo\")", code.gen_source },
	};
	if (!RunChecks(suite, checks)) return 1;

	std::cout << suite << " PASSED\n";
	return 0;
}
