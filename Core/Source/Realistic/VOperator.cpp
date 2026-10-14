#include "Realistic/VOperator.h"
#include "Abstract/AContext.h"
#include "Abstract/AFrame.h"
#include "Base/VConvert.h"
#include "Base/VRegister.h"
#include "Base/VSystem.h"

#include <iostream>

using namespace CG::Core;

namespace {

	bool RenameArg(CallMeta& meta, const std::string& old_name, const std::string& new_name)
	{
		if (old_name.empty() || new_name.empty())
			return false;
		if (old_name == new_name)
			return true;
		for (const auto& arg : meta.args) {
			if (arg.name == new_name)
				return false;
		}
		for (auto& arg : meta.args) {
			if (arg.name == old_name) {
				arg.name = new_name;
				return true;
			}
		}
		return false;
	}

	bool SetArgType(CallMeta& meta, const std::string& name, const VType* type)
	{
		if (name.empty() || !type)
			return false;
		for (auto& arg : meta.args) {
			if (arg.name == name) {
				arg.type = type;
				return true;
			}
		}
		return false;
	}

	std::string FormatFrame(const std::shared_ptr<AFrame>& frame)
	{
		if (!frame || !frame->IsValid())
			return {};
		std::string text = ToString(frame);
		if (!text.empty())
			return text;
		const VType* type = frame->GetType();
		return type ? type->name_ : std::string{};
	}

} // namespace

CG::Core::FPrint::FPrint()
{
	meta_.pure = IsPure();
	meta_.name = "Print";

	meta_.args.emplace_back(ArgMeta{ {{},"value"}, false, true, nullptr });
}

ExecMeta CG::Core::FPrint::GetExecMeta() const
{
	return ExecMeta{ {}, true, true };
}

Result CG::Core::FPrint::Call(VObject* object, AContext* ctx)
{
	Result status{};
	int code = -1, index = -1;
	do {
		auto inputs = GetInputs(ctx, meta_);
		if (inputs.size() != 1) break;

		const std::string text = FormatFrame(inputs[0]);
		std::cout << text << std::endl;
		VSystem::Get().Print(text);

		index = 0;
		code = 0;
	} while (0);
		
	status.code = code;
	status.index = index;

	return status;
}

CG::Core::FEntry::FEntry()
{
	meta_.pure = IsPure();
	meta_.name = "Entry";
}

bool CG::Core::FEntry::AddParam(const std::string& name, const VType* type)
{
	if (name.empty() || !type) return false;
	for (const auto& arg : meta_.args) {
		if (arg.name == name)
			return false;
	}
	meta_.args.push_back(ArgMeta{ {{}, name}, true, true, type });
	return true;
}

bool CG::Core::FEntry::DelParam(const std::string& name)
{
	for (auto it = meta_.args.begin(); it != meta_.args.end(); ++it) {
		if (it->name == name) {
			meta_.args.erase(it);
			return true;
		}
	}
	return false;
}

bool CG::Core::FEntry::RenameParam(const std::string& old_name, const std::string& new_name)
{
	return RenameArg(meta_, old_name, new_name);
}

bool CG::Core::FEntry::SetParamType(const std::string& name, const VType* type)
{
	return SetArgType(meta_, name, type);
}

ExecMeta CG::Core::FEntry::GetExecMeta() const
{
	return ExecMeta{ {}, false, true };
}

Result CG::Core::FEntry::Call(VObject* object, AContext* ctx)
{
	return {};
}

CG::Core::FExit::FExit()
{
	meta_.pure = IsPure();
	meta_.name = "Exit";
}

bool CG::Core::FExit::AddParam(const std::string& name, const VType* type)
{
	if (name.empty() || !type) return false;
	for (const auto& arg : meta_.args) {
		if (arg.name == name)
			return false;
	}
	meta_.args.push_back(ArgMeta{ {{}, name}, false, true, type });
	return true;
}

bool CG::Core::FExit::DelParam(const std::string& name)
{
	for (auto it = meta_.args.begin(); it != meta_.args.end(); ++it) {
		if (it->name == name) {
			meta_.args.erase(it);
			return true;
		}
	}
	return false;
}

bool CG::Core::FExit::RenameParam(const std::string& old_name, const std::string& new_name)
{
	return RenameArg(meta_, old_name, new_name);
}

bool CG::Core::FExit::SetParamType(const std::string& name, const VType* type)
{
	return SetArgType(meta_, name, type);
}

ExecMeta CG::Core::FExit::GetExecMeta() const
{
	return ExecMeta{ {}, true, false };
}

Result CG::Core::FExit::Call(VObject* object, AContext* ctx)
{
	Result status{};
	status.code = 0;
	status.index = 0;
	return status;
}
