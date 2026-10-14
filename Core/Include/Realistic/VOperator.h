#pragma once
#include "Abstract/AOperator.h"
#include "Base/VType.h"
#include <memory>
#include <string>
#include <vector>

namespace CG::Core {
	class AContext;
	class AFrame;
	class ANode;
	class VContext;

	class FPrint : public AOperator {
	public:
		FPrint();
		ExecMeta GetExecMeta() const override;
		Result Call(VObject* object, AContext* ctx) override;
	};
	
	class FEntry : public AOperator {
	public:
		FEntry();
		bool AddParam(const std::string& name, const VType* type);
		bool DelParam(const std::string& name);
		bool RenameParam(const std::string& old_name, const std::string& new_name);
		bool SetParamType(const std::string& name, const VType* type);
		ExecMeta GetExecMeta() const override;
		Result Call(VObject* object, AContext* ctx) override;
	};

	class FExit : public AOperator {
	public:
		FExit();
		bool AddParam(const std::string& name, const VType* type);
		bool DelParam(const std::string& name);
		bool RenameParam(const std::string& old_name, const std::string& new_name);
		bool SetParamType(const std::string& name, const VType* type);
		ExecMeta GetExecMeta() const override;
		Result Call(VObject* object, AContext* ctx) override;
	};
}
