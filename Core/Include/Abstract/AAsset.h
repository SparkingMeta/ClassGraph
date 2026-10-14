#pragma once
#include "Base/VObject.h"

namespace CG::Core {
	class AAsset : public VObject {
	public:
		virtual void Load(const std::string&) = 0;
	};
}