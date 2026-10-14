#include "Test/TestUtil.h"

#include "Realistic/VContext.h"
#include "Realistic/VFlow.h"
#include "Realistic/VFrame.h"
#include "Realistic/VGraph.h"
#include "Realistic/VVariant.h"
#include "Realistic/VVarFunction.h"
#include "Base/VRegister.h"

#include <iostream>
#include <memory>
#include <vector>

#if __has_include("user/VTest.h")
#include "user/VTest.h"

using namespace CG::Core;
using namespace test;

namespace {

const VType* VTestType()
{
	return VTypeRegister::Find("user::VTest");
}

bool SetNodeVTest(const std::shared_ptr<ANode>& node, const std::string& frame_name, const user::VTest& value)
{
	if (!node || !node->GetContext()) return false;
	auto frame = node->GetContext()->GetFrame(frame_name);
	if (!frame) return false;
	const VType* type = VTestType();
	return type && frame->SetValue(type, const_cast<user::VTest*>(&value));
}

bool GetVarVTest(const std::shared_ptr<VVariant>& var, user::VTest& out)
{
	if (!var) return false;
	auto frame = var->Getter();
	if (!frame) return false;
	const VType* type = VTestType();
	return type && frame->GetValue(type, &out);
}

} // namespace

int TestVTestInterop()
{
	const char* suite = "TestVTestInterop";

	auto* vtest_cls = LoadCompiledVTest();
	if (!vtest_cls) {
		std::cerr << suite << ": failed to load VTest class\n";
		return 1;
	}
	if (!VTestType()) {
		std::cerr << suite << ": user::VTest type not registered\n";
		return 1;
	}
	if (!vtest_cls->GetMethod("AddInt") || !vtest_cls->GetMethod("Print")) {
		std::cerr << suite << ": VTest methods missing\n";
		return 1;
	}

	auto blueprint = std::make_shared<VBlueprint>();
	blueprint->SetSpace("user");
	blueprint->SetName("VConsumer");

	auto holder_guid = blueprint->AddVar("holder", "user::VTest");
	auto result_guid = blueprint->AddVar("result", "int");
	if (holder_guid.empty() || result_guid.empty()) return 1;

	auto holder = std::static_pointer_cast<VVariant>(blueprint->FindVar(holder_guid));
	auto result = std::static_pointer_cast<VVariant>(blueprint->FindVar(result_guid));

	auto bind_flow = blueprint->FindFlow(blueprint->AddFlow("Bind"));
	auto compute_flow = blueprint->FindFlow(blueprint->AddFlow("Compute"));
	auto invoke_flow = blueprint->FindFlow(blueprint->AddFlow("Invoke"));
	if (!bind_flow || !compute_flow || !invoke_flow) return 1;

	{
		auto graph = bind_flow->GetGraph();
		auto entry = MakeEntryNode({ {"input", "user::VTest"} });
		auto setter = MakeSetVarNode(holder);
		if (!entry || !setter) return 1;

		graph->AddNode(entry);
		graph->AddNode(setter);
		Connect(entry->GetOutputPin(0), setter->GetInputPin(0));
		ConnectOutArg(setter, "value", entry, "input");
		std::static_pointer_cast<VGraph>(graph)->SetEntry(entry);
	}

	{
		auto graph = compute_flow->GetGraph();
		auto entry = MakeEntryNode({ {"a", "int"}, {"b", "int"} });
		auto getter = MakeGetVarNode(holder);
		auto add_node = MakeMethodNode(vtest_cls, "AddInt");
		auto exit = MakeExitNode({ {"value", "int"} });
		if (!entry || !getter || !add_node || !exit) return 1;

		graph->AddNode(entry);
		graph->AddNode(getter);
		graph->AddNode(add_node);
		graph->AddNode(exit);
		Connect(entry->GetOutputPin(0), exit->GetInputPin(0));
		ConnectOutArg(add_node, "self", getter, "return");
		ConnectOutArg(add_node, "a", entry, "a");
		ConnectOutArg(add_node, "b", entry, "b");
		ConnectOutArg(exit, "value", add_node, "return");
		std::static_pointer_cast<VGraph>(graph)->SetEntry(entry);
		std::static_pointer_cast<VGraph>(graph)->SetExit(exit);
	}

	{
		auto graph = invoke_flow->GetGraph();
		auto entry = MakeEntryNode({ {"a", "int"}, {"b", "int"} });
		auto getter = MakeGetVarNode(holder);
		auto add_node = MakeMethodNode(vtest_cls, "AddInt");
		auto setter = MakeSetVarNode(result);
		if (!entry || !getter || !add_node || !setter) return 1;

		graph->AddNode(entry);
		graph->AddNode(getter);
		graph->AddNode(add_node);
		graph->AddNode(setter);
		Connect(entry->GetOutputPin(0), setter->GetInputPin(0));
		ConnectOutArg(add_node, "self", getter, "return");
		ConnectOutArg(add_node, "a", entry, "a");
		ConnectOutArg(add_node, "b", entry, "b");
		ConnectOutArg(setter, "value", add_node, "return");
		std::static_pointer_cast<VGraph>(graph)->SetEntry(entry);
	}

	user::VTest source{};
	source.score = 7;
	{
		auto entry = bind_flow->GetGraph()->GetEntry();
		if (!SetNodeVTest(entry, "input", source)) return 1;
		if (!RunFlowOk(suite, "Bind", bind_flow, "bind")) return 1;

		user::VTest bound{};
		if (!GetVarVTest(holder, bound) || bound.score != 7) {
			std::cerr << suite << ": Bind failed, holder.score=" << bound.score << '\n';
			return 1;
		}
		std::cout << suite << " OK: runtime Bind\n";
	}

	{
		auto entry = invoke_flow->GetGraph()->GetEntry();
		if (!SetNodeInt(entry, "a", 10) || !SetNodeInt(entry, "b", 32)) return 1;
		if (!RunFlowOk(suite, "Invoke", invoke_flow, "invoke")) return 1;

		int sum = 0;
		if (!GetVarInt(result, sum) || sum != 42) {
			std::cerr << suite << ": Invoke AddInt expected 42, got " << sum << '\n';
			return 1;
		}
		std::cout << suite << " OK: runtime member AddInt\n";
	}

	GeneratedCode code;
	if (!GenerateAll(suite, blueprint, code)) return 1;

	const std::vector<Check> checks = {
		{ "holder field", "user::VTest holder;", code.header },
		{ "Bind param", "void Bind(user::VTest input);", code.header },
		{ "Compute param", "int Compute(int a, int b);", code.header },
		{ "VTest include", "user/VTest.h", code.header },
		{ "assign input", "holder = input;", code.source },
		{ "member call", "holder.AddInt(a, b)", code.source },
		{ "return add", "return holder.AddInt(a, b);", code.source },
		{ "invoke assign", "result = holder.AddInt(a, b);", code.source },
	};
	if (!RunChecks(suite, checks)) return 1;

	std::cout << suite << " PASSED\n";
	return 0;
}

#else

int TestVTestInterop()
{
	std::cerr << "TestVTestInterop SKIPPED: Core/Include/user/VTest.h missing (run TestVTest codegen first)\n";
	return 0;
}

#endif
