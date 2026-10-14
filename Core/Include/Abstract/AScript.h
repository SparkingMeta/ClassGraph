#pragma once

#include "Abstract/ACaller.h"

namespace CG::Core {
	class AFrame;
	class AContext;

	class AScript : public ACaller {
	public:
		virtual std::string GetKey() const = 0;
	};
}