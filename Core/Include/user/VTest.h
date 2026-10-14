#pragma once

#include "Base/VClass.h"
#include <string>
#include "Realistic/VBlueprintObject.h"
#include "VTest.gen.h"

namespace user {

class VTest : public CG::Core::VBlueprintObject {
public:
	GENERATED_BODY()
	int score;

	int AddInt(int a, int b);
	void Print(std::string value);
};

} // namespace user
