#pragma once
#include "Base/VType.h"
namespace CG::Core {

	enum class EVisiable : uint8_t {
		Public,
		Private
	};

	class AProperty : public VField {
	public:
		virtual void SetAccessLevel(int) = 0;
		virtual int GetAccessLevel() const = 0;
		virtual bool IsConst() const = 0;
		virtual bool IsStatic() const = 0;
	};

}