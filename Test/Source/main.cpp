#include "Test/TestCases.h"
#include "Base/VSystem.h"

#include <iostream>

int main()
{
	CG::Core::VSystem::Get().Init();

	struct TestCase {
		const char* name;
		int (*run)();
	};

	const TestCase tests[] = {
		{ "TestFlowMeta", TestFlowMeta },
		{ "TestVTest", TestVTest },
		{ "TestMyBranch", TestMyBranch },
		{ "TestBranch", TestBranch },
		{ "TestNodesCodegen", TestNodesCodegen },
		{ "TestBlueprintRun", TestBlueprintRun },
		{ "TestVTestInterop", TestVTestInterop },
		{ "TestSerialize", TestSerialize },
	};

	for (const auto& test : tests) {
		std::cout << "===== " << test.name << " =====\n";
		if (test.run() != 0) {
			std::cerr << "FAILED: " << test.name << '\n';
			return 1;
		}
	}

	std::cout << "ALL CODEGEN TESTS PASSED\n";
	return 0;
}
