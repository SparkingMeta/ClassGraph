#include "Realistic/VController.h"
#include "Abstract/AFrame.h"
#include "Abstract/ANode.h"
#include "Realistic/VContext.h"
#include "Realistic/VFrame.h"
#include "Base/VRegister.h"

#include <algorithm>
#include <charconv>
#include <limits>
#include <unordered_set>

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
	meta_.args.emplace_back(ArgMeta{{{}, "case_0"}, true, false, FindType(typeid(int)), "0"});
	meta_.args.emplace_back(ArgMeta{{{}, g_key_default},true, false, nullptr});
}

ExecMeta CG::Core::FSwitchInt::GetExecMeta() const
{
	return ExecMeta{ {}, true, false };
}

std::string CG::Core::FSwitchInt::AddCase(const std::string& requested_name)
{
	const VType* type = FindType(typeid(int));
	if (!type)
		return {};

	std::string name = requested_name;
	if (name.empty()) {
		for (int n = 0;; ++n) {
			name = "case_" + std::to_string(n);
			if (std::none_of(meta_.args.begin(), meta_.args.end(), [&](const auto& arg) { return arg.name == name; }))
				break;
		}
	}
	if (name == g_key_cond || name == g_key_default
		|| std::any_of(meta_.args.begin(), meta_.args.end(), [&](const auto& arg) { return arg.name == name; }))
		return {};
	auto it = std::find_if(meta_.args.begin(), meta_.args.end(), [](const auto& arg) {
		return arg.is_output && arg.name == g_key_default;
	});
	std::unordered_set<int> used_values;
	for (const auto& arg : meta_.args) {
		if (!arg.is_output || arg.is_runtime || arg.name.rfind("case_", 0) != 0)
			continue;
		const auto& case_text = arg.value;
		int value = 0;
		const auto parsed = std::from_chars(case_text.data(), case_text.data() + case_text.size(), value);
		if (!case_text.empty() && parsed.ec == std::errc{} && parsed.ptr == case_text.data() + case_text.size())
			used_values.insert(value);
	}
	int next_value = 0;
	while (used_values.contains(next_value) && next_value < (std::numeric_limits<int>::max)()) ++next_value;
	if (used_values.contains(next_value)) return {};
	meta_.args.insert(it, ArgMeta{{{}, name}, true, false, type, std::to_string(next_value)});
	return name;
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

bool CG::Core::FSwitchInt::SetCaseValue(const std::string& name, const std::string& value)
{
	int parsed_value = 0;
	const auto parsed = std::from_chars(value.data(), value.data() + value.size(), parsed_value);
	if (value.empty() || parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size())
		return false;
	auto target = std::find_if(meta_.args.begin(), meta_.args.end(), [&](const auto& arg) {
		return arg.is_output && !arg.is_runtime && arg.name == name && name.rfind("case_", 0) == 0;
	});
	if (target == meta_.args.end()) return false;
	for (const auto& arg : meta_.args) {
		if (!arg.is_output || arg.is_runtime || arg.name.rfind("case_", 0) != 0 || arg.name == name)
			continue;
		const auto& other_text = arg.value;
		int other_value = 0;
		const auto other_parsed = std::from_chars(other_text.data(), other_text.data() + other_text.size(), other_value);
		if (!other_text.empty() && other_parsed.ec == std::errc{}
			&& other_parsed.ptr == other_text.data() + other_text.size() && other_value == parsed_value)
			return false;
	}
	target->value = value;
	return true;
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
		status.out = first;
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

std::string CG::Core::FSequence::AddThen(const std::string& requested_name)
{
	std::string name = requested_name;
	if (name.empty()) {
		for (int n = 0;; ++n) {
			name = "then_" + std::to_string(n);
			if (std::none_of(meta_.args.begin(), meta_.args.end(), [&](const auto& arg) { return arg.name == name; }))
				break;
		}
	}
	if (name.rfind("then_", 0) != 0
		|| std::any_of(meta_.args.begin(), meta_.args.end(), [&](const auto& arg) { return arg.name == name; }))
		return {};
	meta_.args.emplace_back(ArgMeta{{{}, name}, true, false, nullptr});
	return name;
}

bool CG::Core::FSequence::DelThen(const std::string& name)
{
	const auto count = std::count_if(meta_.args.begin(), meta_.args.end(), [](const auto& arg) {
		return arg.is_output && !arg.is_runtime && arg.name.rfind("then_", 0) == 0;
	});
	if (count <= 1)
		return false;
	for (auto it = meta_.args.begin(); it != meta_.args.end(); ++it) {
		if (it->is_output && !it->is_runtime && it->name == name && name.rfind("then_", 0) == 0) {
			meta_.args.erase(it);
			return true;
		}
	}
	return false;
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
		status.out = 0;
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

		if (outputs.empty() || status.out < 0 || status.out >= static_cast<int>(outputs.size()))
			break;

		status.index = status.out++;
		status.loop = status.out < static_cast<int>(outputs.size());
		code = 0;
	} while (0);

	status.code = code;
}

