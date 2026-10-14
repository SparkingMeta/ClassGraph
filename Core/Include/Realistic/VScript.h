#pragma once
#include "Abstract/AScript.h"
#include "Base/VMeta.h"
#include <memory>
#include <string>
#include <vector>

namespace CG::Core {
	class AFlow;
	struct VType;

	class VScript : public AScript {
	public:
		VScript(const std::shared_ptr<AFlow>& flow);

		std::shared_ptr<AFlow> GetFlow() const { return flow_.lock(); }

		std::string GetKey() const override;
		bool IsPure() const override;
		int GetKind() const override;
		Result Call(VObject*, AContext* ctx) override;
		CallMeta GetCallMeta() const override;
		ExecMeta GetExecMeta() const override;
	private:
		std::vector<const VType*> GetInputTypes() const;
		std::vector<const VType*> GetOutputTypes() const;
	private:
		std::weak_ptr<AFlow> flow_;

	};

}
