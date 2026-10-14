#pragma once
#include "Base/VObject.h"
#include "Base/VCore.h"
#include <memory>
#include <set>

namespace CG::Core {
	struct VType;
	class AFrame;

	class AVariant : public VObject {
	public:
		virtual const VType* GetType() const = 0;
		virtual std::shared_ptr<AFrame> Getter() const = 0;
		virtual void Setter(std::shared_ptr<AFrame> var) = 0;

		uint16 GetID() const { return id_; }
		void SetID(uint16 id) { id_ = id; }
	private:
		uint16 id_{};
	};
}