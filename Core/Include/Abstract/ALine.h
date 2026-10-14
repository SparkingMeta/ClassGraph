#pragma once
#include "Base/VObject.h"

namespace CG::Core {
	class APin;
	class ALine : public VObject {
	public:
		virtual std::shared_ptr<APin> GetFrom() const = 0;
		virtual std::shared_ptr<APin> GetTo() const = 0;
		virtual void SetFrom(std::shared_ptr<APin>) = 0;
		virtual void SetTo(std::shared_ptr<APin>) = 0;
	};
}