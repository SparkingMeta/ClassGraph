#pragma once

#include "Base/VClass.h"
#include "Realistic/VBlueprintObject.h"
#include "A.gen.h"

namespace user {

class A : public CG::Core::VBlueprintObject {
public:
	GENERATED_BODY()
	int a = 2;

	void EventGraph();
};

} // namespace user
