#pragma once

#include "CodeGen/CNode.h"
#include "Base/VMeta.h"
#include "Realistic/VAsset.h"

#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace CG::Core {
	class ANode;
	class AVariant;
	class AFrame;
	struct VType;
}

namespace CG::Core::cg {

	struct CBuildContext {
		const VBlueprint* blueprint{};
		std::string class_name;
		std::set<std::string> method_params;
		std::string return_pin;
		std::vector<std::pair<std::string, std::string>> extra_outs;
		std::unordered_map<const ANode*, std::unique_ptr<CExpr>> expr_cache;
		std::unordered_map<const ANode*, std::unordered_map<std::string, std::unique_ptr<CExpr>>> output_expr_cache;
		std::unordered_map<const ANode*, std::string> call_result_names;
		std::unordered_map<const ANode*, std::unordered_map<std::string, std::string>> output_expr_names;
		size_t next_codegen_temp{};
		std::set<std::string> includes;
	};

	void ApplyEmitIncludes(CBuildContext& ctx, const EmitMeta& emit);

	std::unique_ptr<CExpr> BuildExprFromEmit(const EmitMeta& emit, const std::string& fallback_name,
		std::vector<std::unique_ptr<CExpr>> inputs);

	std::string CastType(const VType* type);

	void CollectTypeHeader(std::set<std::string>& includes, const VType* type);

	void FillMethodFromFlowMeta(CMethod& method, const FlowMeta& flow_meta,
		std::set<std::string>& header_includes);

	void CollectFlowMethodParams(CBuildContext& ctx, const FlowMeta& flow_meta);

	std::string ToLiteral(const std::shared_ptr<AFrame>& frame);

} // namespace CG::Core::cg
