#include "Realistic/VController.h"
#include "Abstract/AFrame.h"
#include "Abstract/ANode.h"
#include "Realistic/VContext.h"
#include "Realistic/VFrame.h"
#include "Base/VRegister.h"

using namespace CG::Core;

namespace {

	const std::string g_key_cond = "cond";
	const std::string g_key_first = "first";
	const std::string g_key_last = "last";
	const std::string g_key_index = "index";
	const std::string g_key_true = "true";
	const std::string g_key_false = "false";
	const std::string g_key_completed = "completed";
	const std::string g_key_body = "body";
	const std::string g_key_default = "default";

	int OutputIndex(const CallMeta& meta, const std::string& name)
	{
		int index = 0;
		for (const auto& arg : meta.args) {
			if (!arg.is_output)
				continue;
			if (arg.name == name)
				return index;
			++index;
		}
		return -1;
	}

	bool ReadBool(const std::shared_ptr<AFrame>& frame, bool& out)
	{
		if (!frame || !frame->GetType())
			return false;
		return frame->GetValue(frame->GetType(), &out);
	}

	bool ReadInt(const std::shared_ptr<AFrame>& frame, int& out)
	{
		if (!frame || !frame->GetType())
			return false;
		return frame->GetValue(frame->GetType(), &out);
	}

} // namespace

CG::Core::FSwitchInt::FSwitchInt()
{
	meta_.pure = IsPure();
	meta_.name = "SwitchInt";

	meta_.args.emplace_back(ArgMeta{{{}, g_key_cond},false, true, FindType(typeid(int))});
	meta_.args.emplace_back(ArgMeta{{{},"case_0"},true, false, FindType(typeid(int))});
	meta_.args.emplace_back(ArgMeta{{{}, g_key_default},true, false, nullptr});
}

ExecMeta CG::Core::FSwitchInt::GetExecMeta() const
{
	return ExecMeta{ {}, true, false };
}

std::string CG::Core::FSwitchInt::AddCase()
{
	const VType* type = FindType(typeid(int));
	if (!type)
		return {};

	int n = 0;
	for (;;) {
		const std::string name = "case_" + std::to_string(n);
		bool used = false;
		for (const auto& arg : meta_.args) {
			if (arg.name == name) {
				used = true;
				break;
			}
		}
		if (!used) {
			auto it = meta_.args.end();
			for (auto i = meta_.args.begin(); i != meta_.args.end(); ++i) {
				if (i->is_output && i->name == g_key_default) {
					it = i;
					break;
				}
			}
			meta_.args.insert(it, ArgMeta{ {{}, name}, true, false, type });
			return name;
		}
		++n;
	}
}

bool CG::Core::FSwitchInt::DelCase(const std::string& name)
{
	if (name.empty() || name == g_key_default || name == g_key_cond)
		return false;
	for (auto it = meta_.args.begin(); it != meta_.args.end(); ++it) {
		if (it->is_output && it->name == name) {
			meta_.args.erase(it);
			return true;
		}
	}
	return false;
}

Status CG::Core::FSwitchInt::Init(AContext* ctx)
{
	return {};
}

void CG::Core::FSwitchInt::Invoke(AContext* ctx, Status& status)
{
	int code = -1;
	do {
		auto inputs = GetInputs(ctx, meta_);
		auto outputs = GetOutputs(ctx, meta_);
		if (inputs.size() != 1 || outputs.size() < 1)
			break;

		int cond = 0;
		if (!ReadInt(inputs[0], cond))
			break;

		int index = -1;
		const int last = static_cast<int>(outputs.size()) - 1;
		for (int i = 0; i < last; ++i) {
			int value = 0;
			if (ReadInt(outputs[i], value) && value == cond)
				index = i;
		}
		if (index < 0)
			index = last;

		status.index = index;
		code = 0;
	} while (0);

	status.code = code;
}

CG::Core::FBranch::FBranch()
{
	meta_.pure = IsPure();
	meta_.name = "Branch";

	meta_.args.emplace_back(ArgMeta{ {{}, g_key_cond}, false, true, FindType(typeid(bool)) });
	meta_.args.emplace_back(ArgMeta{ {{}, g_key_true}, true, false, nullptr });
	meta_.args.emplace_back(ArgMeta{ {{}, g_key_false}, true, false, nullptr });
}

ExecMeta CG::Core::FBranch::GetExecMeta() const
{
	return ExecMeta{ {}, true, false };
}

Status CG::Core::FBranch::Init(AContext* ctx)
{
	return {};
}

void CG::Core::FBranch::Invoke(AContext* ctx, Status& status)
{
	int code = -1;
	do {
		auto inputs = GetInputs(ctx, meta_);
		if (inputs.size() != 1)
			break;

		bool cond = false;
		if (!ReadBool(inputs[0], cond))
			break;

		status.index = cond ? OutputIndex(meta_, g_key_true) : OutputIndex(meta_, g_key_false);
		if (status.index < 0)
			break;
		code = 0;
	} while (0);

	status.code = code;
}


CG::Core::FWhile::FWhile()
{
	meta_.pure = IsPure();
	meta_.name = "While";

	meta_.args.emplace_back(ArgMeta{ {{}, g_key_cond}, false, true, FindType(typeid(bool)) });
	meta_.args.emplace_back(ArgMeta{ {{}, g_key_completed}, true, false, nullptr });
	meta_.args.emplace_back(ArgMeta{ {{}, g_key_body}, true, false, nullptr });
}

ExecMeta CG::Core::FWhile::GetExecMeta() const
{
	return ExecMeta{ {}, true, false };
}

Status CG::Core::FWhile::Init(AContext* ctx)
{
	Status status;
	auto inputs = GetInputs(ctx, meta_);
	status.code = (inputs.size() == 1 && inputs[0]) ? 0 : -1;
	return status;
}

void CG::Core::FWhile::Invoke(AContext* ctx, Status& status)
{
	int code = -1;
	do {
		auto inputs = GetInputs(ctx, meta_);
		if (inputs.size() != 1)
			break;

		bool cond = false;
		if (!ReadBool(inputs[0], cond))
			break;

		status.flag = cond;
		status.index = cond ? OutputIndex(meta_, g_key_body) : OutputIndex(meta_, g_key_completed);
		if (status.index < 0)
			break;

		code = 0;
	} while (0);

	status.code = code;
}

CG::Core::FLoop::FLoop()
{
	meta_.pure = IsPure();
	meta_.name = "Loop";

	meta_.args.emplace_back(ArgMeta{ {{}, g_key_cond}, false, true, FindType(typeid(bool)) });
	meta_.args.emplace_back(ArgMeta{ {{}, g_key_first}, false, true, FindType(typeid(int)) });
	meta_.args.emplace_back(ArgMeta{ {{}, g_key_last}, false, true, FindType(typeid(int)) });

	meta_.args.emplace_back(ArgMeta{ {{}, g_key_completed}, true, false, nullptr });
	meta_.args.emplace_back(ArgMeta{ {{}, g_key_body}, true, false, nullptr });
	meta_.args.emplace_back(ArgMeta{ {{}, g_key_index}, true, true, FindType(typeid(int)) });
}

ExecMeta CG::Core::FLoop::GetExecMeta() const
{
	return ExecMeta{ {}, true, false };
}

Status CG::Core::FLoop::Init(AContext* ctx)
{
	Status status;
	int code = -1;
	do {
		auto inputs = GetInputs(ctx, meta_);
		auto outputs = GetOutputs(ctx, meta_);
		if (inputs.size() != 3 || outputs.size() != 3)
			break;

		bool cond = false;
		int first = 0;
		int last = 0;
		if (!ReadBool(inputs[0], cond) || !ReadInt(inputs[1], first) || !ReadInt(inputs[2], last))
			break;

		status.cond = cond;
		status.first = first;
		status.last = last;
		status.index = first;
		code = 0;
	} while (0);

	status.code = code;
	return status;
}

void CG::Core::FLoop::Invoke(AContext* ctx, Status& status)
{
	//input:[cond(bool), first(int), last(int)]
	//output:[completed(false), loop body(true), index(int) ]
	int code = -1;
	do {
		auto frame = ctx->GetFrame(g_key_index);

		bool loop = false;
		if (status.cond && status.out < status.last) {
			if (frame) frame->SetValue(frame->GetType(), &status.out);
			status.out++;
			loop = true;
		}

		status.loop = loop;
		status.index = loop ? OutputIndex(meta_, g_key_body) : OutputIndex(meta_, g_key_completed);
		if (status.index < 0)
			break;

		code = 0;
	} while (0);

	status.code = code;
}

CG::Core::FSequence::FSequence()
{
	meta_.pure = IsPure();
	meta_.name = "Sequence";

	// default two then pins; more can be added later via meta_
	meta_.args.emplace_back(ArgMeta{ {{},"then_0"}, true, false, nullptr });
	meta_.args.emplace_back(ArgMeta{ {{},"then_1"}, true, false, nullptr });
}

ExecMeta CG::Core::FSequence::GetExecMeta() const
{
	return ExecMeta{ {}, true, false };
}

Status CG::Core::FSequence::Init(AContext* ctx)
{
	Status status;
	int code = -1;
	do {
		auto outputs = GetOutputs(ctx, meta_);

		if(outputs.size() < 1)
			break;

		status.index = 0;
		code = 0;
	} while (0);

	status.code = code;
	return status;
}

void CG::Core::FSequence::Invoke(AContext* ctx, Status& status)
{
	int code = -1;
	do {
		auto outputs = GetOutputs(ctx, meta_);

		bool loop = false;
		if(status.index++ < outputs.size() - 1)
			loop = true;

		status.loop = loop;
		code = 0;
	} while (0);

	status.code = code;
}

