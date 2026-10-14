#include "Realistic/VFunction.h"
#include "Realistic/VContext.h"

using namespace CG::Core;

std::string CG::Core::VFunction::GetKey() const
{
	return GetName();
}

bool CG::Core::VFunction::IsPure() const
{
	return meta_.pure;
}

int CG::Core::VFunction::GetKind() const
{
	return CK_Function;
}

CallMeta CG::Core::VFunction::GetCallMeta() const
{
	return meta_;
}

ExecMeta CG::Core::VFunction::GetExecMeta() const
{

	return ExecMeta{ {}, !IsPure(), !IsPure() };
}

Result CG::Core::VFunction::Call(VObject*, AContext* ctx)
{
	if(meta_.invoker)
		meta_.invoker(ctx);
	return {};
}

void CG::Core::VFunction::SetMeta(const FuncMeta& meta)
{
	meta_ = meta;
}

const FuncMeta& CG::Core::VFunction::GetFuncMeta() const
{
	return meta_;
}

void CG::Core::VFunction::SetEmit(const EmitMeta& meta)
{
	meta_.emit = meta;
}

EmitMeta CG::Core::VFunction::GetEmitMeta() const
{
	return meta_.emit;
}

