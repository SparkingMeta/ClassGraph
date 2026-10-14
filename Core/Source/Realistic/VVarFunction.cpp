#include "Realistic/VVarFunction.h"

#include "Abstract/AContext.h"
#include "Abstract/AFrame.h"
#include "Realistic/VNode.h"
#include "Realistic/VVariant.h"

using namespace CG::Core;

CG::Core::AVarFunction::AVarFunction(std::shared_ptr<AVariant> var, const char* prefix)
	: var_(std::move(var))
{
	meta_.name = prefix;
	if (var_)
		meta_.name += " " + var_->GetName();
	SetName(meta_.name);
}

CG::Core::FGetVar::FGetVar(std::shared_ptr<AVariant> var)
	: AVarFunction(std::move(var), "Get")
{
	meta_.pure = true;
	const VType* type = var_ ? var_->GetType() : nullptr;
	meta_.args.emplace_back(ArgMeta{ {{}, "return"}, true, true, type });
}

bool CG::Core::FGetVar::IsPure() const
{
	return true;
}

ExecMeta CG::Core::FGetVar::GetExecMeta() const
{
	return ExecMeta{ {}, false, false };
}

Result CG::Core::FGetVar::Call(VObject* object, AContext* ctx)
{
	Result status{};
	do {
		if (!var_ || !ctx) break;

		const VType* type = var_->GetType();
		if (!type) break;

		auto src = var_->Getter();
		auto dst = ctx->GetFrame("return");
		if (!src || !dst) break;
		if (src->GetType() != type || dst->GetType() != type) break;
		if (!dst->Update(src)) break;

		status.code = 0;
	} while (0);

	return status;
}

CG::Core::FSetVar::FSetVar(std::shared_ptr<AVariant> var)
	: AVarFunction(std::move(var), "Set")
{
	meta_.pure = false;
	const VType* type = var_ ? var_->GetType() : nullptr;
	meta_.args.emplace_back(ArgMeta{ {{}, "value"}, false, true, type });
}

bool CG::Core::FSetVar::IsPure() const
{
	return false;
}

ExecMeta CG::Core::FSetVar::GetExecMeta() const
{
	return ExecMeta{ {}, true, true };
}

Result CG::Core::FSetVar::Call(VObject* object, AContext* ctx)
{
	Result status{};
	int code = -1;
	const int index = 0;

	do {
		if (!var_ || !ctx) break;

		const VType* type = var_->GetType();
		if (!type) break;

		auto inputs = GetInputs(ctx, meta_);
		if (inputs.size() != 1) break;
		if (inputs[0]->GetType() != type) break;

		auto frame = var_->Getter();
		if (!frame) break;
		if (!frame->SetValue(type, inputs[0]->GetBuffer())) break;

		code = 0;
	} while (0);

	status.code = code;
	status.index = index;
	return status;
}

std::shared_ptr<ANode> CG::Core::MakeGetVarNode(std::shared_ptr<AVariant> var)
{
	if (!var || !var->GetType()) return {};
	auto caller = std::make_shared<FGetVar>(var);
	auto node = VNode::Make(caller);
	node->SetName(caller->GetName());
	return node;
}

std::shared_ptr<ANode> CG::Core::MakeSetVarNode(std::shared_ptr<AVariant> var)
{
	if (!var || !var->GetType()) return {};
	auto caller = std::make_shared<FSetVar>(var);
	auto node = VNode::Make(caller);
	node->SetName(caller->GetName());
	return node;
}
