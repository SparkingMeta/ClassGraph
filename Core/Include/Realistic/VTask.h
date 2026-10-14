#pragma once
#include "Abstract/ATask.h"

namespace CG::Core {
	
	class VTask : public ATask {
	public:
		VTask();
		~VTask() = default;

		Result Run() override;
	};
}