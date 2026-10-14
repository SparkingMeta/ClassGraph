#pragma once

#include "CodeGen/CContext.h"
#include "CodeGen/CNode.h"

#include <memory>
#include <optional>
#include <vector>

namespace CG::Core {
	class ACaller;
	class ANode;
}

namespace CG::Core::cg {

	class ICHandler {
	public:
		virtual ~ICHandler() = default;

		virtual bool IsMatch(const ACaller* caller) const = 0;

		virtual std::optional<std::unique_ptr<CStmt>> BuildStmt(CBuildContext& ctx, const std::shared_ptr<ANode>& node) const = 0;

		virtual std::optional<std::unique_ptr<CExpr>> BuildExpr(CBuildContext& ctx, const std::shared_ptr<ANode>& node) const = 0;
	};

	class CHandlerRegistry {
	public:
		void Register(std::unique_ptr<ICHandler> handler);
		const ICHandler* Find(const ACaller* caller) const;

	private:
		std::vector<std::unique_ptr<ICHandler>> handlers_;
	};

	CHandlerRegistry& DefaultHandlers();

	std::unique_ptr<CExpr> BuildExpr(CBuildContext& ctx, const std::shared_ptr<ANode>& node, const std::string& arg_name);

	std::unique_ptr<CExpr> BuildExpr(CBuildContext& ctx, const std::shared_ptr<ANode>& node);

	std::unique_ptr<CStmt> BuildStmtChain(CBuildContext& ctx, const std::shared_ptr<ANode>& entry);

} // namespace CG::Core::cg
