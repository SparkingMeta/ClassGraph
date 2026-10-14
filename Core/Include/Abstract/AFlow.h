#pragma once
#include "Base/VObject.h"
#include "Base/VMeta.h"

namespace CG::Core {
	class AGraph;
	class AScript;

	class AFlow : public VObject {
	public:
		virtual std::shared_ptr<AGraph> GetGraph() const = 0;
		virtual std::shared_ptr<AScript> GetScript() const = 0;
		virtual const FlowMeta& GetFlowMeta() const = 0;
		virtual int Run() = 0;
	};
}