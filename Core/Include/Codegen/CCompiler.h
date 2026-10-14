#pragma once

#include "CodeGen/CNode.h"
#include "Realistic/VAsset.h"

#include <string>

namespace CG::Core::cg {

	class CCompiler {
	public:
		static int Compile(const VBlueprint& blueprint, std::string& source, std::string& header,
			std::string& gen_header, std::string& meta);
	};

} // namespace CG::Core::cg
