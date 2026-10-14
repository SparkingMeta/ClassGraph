#pragma once

#include "CodeGen/CNode.h"

#include <string>

namespace CG::Core::cg {

	struct COutputs {
		std::string header;
		std::string gen_header;
		std::string source;
		std::string gen_source;
	};

	class CEmitter {
	public:
		static void Emit(const CUnit& unit, COutputs& out);
		static void Emit(const CUnit& unit, std::string& header, std::string& gen_header,std::string& source, std::string& gen_source);
	};

} // namespace CG::Core::cg
