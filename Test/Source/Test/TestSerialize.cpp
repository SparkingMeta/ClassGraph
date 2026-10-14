#include "Test/TestCases.h"
#include "Test/TestUtil.h"

#include "Base/VConvert.h"
#include "Base/VRegister.h"
#include "Base/VSystem.h"
#include "Base/VType.h"
#include "Realistic/VFrame.h"

#include <iostream>
#include <string>

#if __has_include("user/VTest.h")
#include "user/VTest.h"

int TestSerialize()
{
	using namespace CG::Core;

	Z_Construct_VClass_VTest();

	const VType* tint = FindType("int");
	const VType* tbool = FindType("bool");
	const VType* tstr = FindType("std::string");
	const VType* tcls = FindType("user::VTest");
	if (!tint || !tbool || !tstr || !tcls) {
		std::cerr << "missing registered types\n";
		return 1;
	}

	{
		auto frame = std::make_shared<VFrame>(tint);
		int32 v = 42;
		if (!frame->SetValue(tint, &v)) return 1;
		const std::string text = ToString(frame);
		if (text != "42") {
			std::cerr << "int dump expected 42 got " << text << '\n';
			return 1;
		}
		auto back = FromString(text, tint);
		int32 out = 0;
		if (!back || !back->GetValue(tint, &out) || out != 42) {
			std::cerr << "int load failed\n";
			return 1;
		}
	}

	{
		auto frame = std::make_shared<VFrame>(tbool);
		bool v = true;
		frame->SetValue(tbool, &v);
		if (ToString(frame) != "true") return 1;
		auto back = FromString("false", tbool);
		bool out = true;
		if (!back || !back->GetValue(tbool, &out) || out) return 1;
	}

	{
		auto frame = std::make_shared<VFrame>(tstr);
		std::string v = "hello\"x";
		frame->SetValue(tstr, &v);
		const std::string text = ToString(frame);
		auto back = FromString(text, tstr);
		std::string out;
		if (!back || !back->GetValue(tstr, &out) || out != v) {
			std::cerr << "string roundtrip failed: " << text << '\n';
			return 1;
		}
	}

	{
		user::VTest obj;
		obj.score = 7;
		std::string text;
		if (!obj.Serialize(text)) {
			std::cerr << "VTest serialize failed\n";
			return 1;
		}
		user::VTest other;
		other.score = 0;
		if (!other.Deserialize(text) || other.score != 7) {
			std::cerr << "VTest deserialize failed: " << text << '\n';
			return 1;
		}

		auto frame = std::make_shared<VFrame>(tcls);
		if (!frame->SetValue(tcls, &obj)) return 1;
		auto back = FromString(ToString(frame), tcls);
		user::VTest loaded;
		if (!back || !back->GetValue(tcls, &loaded) || loaded.score != 7) {
			std::cerr << "VTest frame roundtrip failed\n";
			return 1;
		}
	}

	std::cout << "TestSerialize OK\n";
	return 0;
}

#else

int TestSerialize()
{
	std::cerr << "TestSerialize SKIPPED: Core/Include/user/VTest.h missing (run TestVTest codegen first)\n";
	return 0;
}

#endif
