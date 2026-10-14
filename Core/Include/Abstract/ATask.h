#pragma once
#include "Base/VObject.h"

namespace CG::Core {
	class AGraph;
	class AScript;

	class ATask : public VObject {
	public:
		virtual Result Run() = 0;
	};
}