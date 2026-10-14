#pragma once

#include "Base/VObject.h"

#include <memory>

namespace CG::Core {
	class VBlueprint;

	class ABlueprintObject : public VObject {
	public:
		virtual void SetBlueprint(const std::shared_ptr<VBlueprint>& blueprint) = 0;
		virtual std::shared_ptr<VBlueprint> GetBlueprint() const = 0;
	};
}
