#pragma once

#include "Abstract/AFunction.h"
#include "Base/VType.h"

#include <memory>
#include <string>

namespace CG::Core {
	class AContext;
	class ANode;
	class AVariant;

	class AVarFunction : public AFunction {
	public:
		std::shared_ptr<AVariant> GetVar() const { return var_; }

		std::string GetKey() const override { return meta_.name; }
		int GetKind() const override { return CK_Function; }
		CallMeta GetCallMeta() const override { return meta_; }

	protected:
		AVarFunction(std::shared_ptr<AVariant> var, const char* prefix);

		std::shared_ptr<AVariant> var_;
		CallMeta meta_;
	};

	class FGetVar : public AVarFunction {
	public:
		explicit FGetVar(std::shared_ptr<AVariant> var);

		bool IsPure() const override;
		ExecMeta GetExecMeta() const override;
		Result Call(VObject* object, AContext* ctx) override;
	};

	class FSetVar : public AVarFunction {
	public:
		explicit FSetVar(std::shared_ptr<AVariant> var);

		bool IsPure() const override;
		ExecMeta GetExecMeta() const override;
		Result Call(VObject* object, AContext* ctx) override;
	};

	std::shared_ptr<ANode> MakeGetVarNode(std::shared_ptr<AVariant> var);
	std::shared_ptr<ANode> MakeSetVarNode(std::shared_ptr<AVariant> var);
}
