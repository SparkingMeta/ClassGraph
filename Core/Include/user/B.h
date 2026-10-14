#pragma once

#include "Base/VClass.h"
#include "Realistic/VBlueprintObject.h"
#include "user/A.h"
#include "B.gen.h"

namespace user {

class B : public CG::Core::VBlueprintObject {
public:
	GENERATED_BODY()
	int a = 0;
	user::A b;

	void EventGraph();
};

} // namespace user
