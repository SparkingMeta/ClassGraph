#pragma once
#include "Abstract/ACaller.h"

namespace CG::Core {
	class AFunction : public ACaller {
	public:
		virtual std::string GetKey() const = 0;

	};
}