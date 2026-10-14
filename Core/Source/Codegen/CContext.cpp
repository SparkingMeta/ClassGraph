#include "CodeGen/CContext.h"

#include "Abstract/AFrame.h"
#include "Base/VMeta.h"
#include "Base/VType.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

using namespace CG::Core;
using namespace CG::Core::cg;

namespace {

	struct ClassifiedFlow {
		std::vector<ArgMeta> inputs;
		std::vector<ArgMeta> outputs;
	};

	ClassifiedFlow ClassifyFlow(const FlowMeta& flow_meta)
	{
		ClassifiedFlow out;
		for (const auto& param : flow_meta.params) {
			if (param.name.empty()) continue;
			if (param.is_output)
				out.outputs.push_back(param);
			else if (param.type)
				out.inputs.push_back(param);
		}
		return out;
	}

	std::string UniqueCppName(std::vector<std::string>& taken, const std::string& name)
	{
		auto has = [&](const std::string& n) {
			return std::find(taken.begin(), taken.end(), n) != taken.end();
		};
		if (!has(name)) {
			taken.push_back(name);
			return name;
		}
		std::string cand = "out_" + name;
		if (!has(cand)) {
			taken.push_back(cand);
			return cand;
		}
		for (int i = 1; i < 1000; ++i) {
			cand = "out_" + name + "_" + std::to_string(i);
			if (!has(cand)) {
				taken.push_back(cand);
				return cand;
			}
		}
		taken.push_back(name + "_x");
		return name + "_x";
	}

} // namespace

void CG::Core::cg::ApplyEmitIncludes(CBuildContext& ctx, const EmitMeta& emit)
{
	for (const auto& inc : emit.includes)
		ctx.includes.insert(inc);
}

std::unique_ptr<CExpr> CG::Core::cg::BuildExprFromEmit(const EmitMeta& emit, const std::string& fallback_name,
	std::vector<std::unique_ptr<CExpr>> inputs)
{
	switch (emit.emit_kind) {
	case EmitKind::EmitBinary:
		if (inputs.size() >= 2)
			return MakeBinary(emit.op, std::move(inputs[0]), std::move(inputs[1]));
		break;
	case EmitKind::EmitUnary:
		if (!inputs.empty())
			return MakeUnary(emit.op, std::move(inputs[0]));
		break;
	case EmitKind::EmitSymbol:
		if (!inputs.empty())
			return MakeStreamInsert(emit.callee, emit.suffix, std::move(inputs[0]));
		break;
	case EmitKind::EmitCall:
	default:
		break;
	}

	const std::string callee = emit.callee.empty() ? fallback_name : emit.callee;
	return MakeCall(callee, std::move(inputs));
}

std::string CG::Core::cg::CastType(const VType* type)
{
	if (!type) return "void";
	return type->name_.empty() ? "void" : type->name_;
}

void CG::Core::cg::CollectTypeHeader(std::set<std::string>& includes, const VType* type)
{
	if (!type || type->header_.empty()) return;
	includes.insert(type->header_);
}

void CG::Core::cg::FillMethodFromFlowMeta(CMethod& method, const FlowMeta& flow_meta,
	std::set<std::string>& header_includes)
{
	const auto classified = ClassifyFlow(flow_meta);
	std::vector<std::string> taken;
	for (const auto& param : classified.inputs) {
		method.params.push_back({ CastType(param.type), param.name });
		taken.push_back(param.name);
		CollectTypeHeader(header_includes, param.type);
	}
	if (classified.outputs.empty())
		return;

	for (size_t i = 0; i + 1 < classified.outputs.size(); ++i) {
		const auto& param = classified.outputs[i];
		if (!param.type) continue;
		const std::string cpp_name = UniqueCppName(taken, param.name);
		method.params.push_back({ CastType(param.type) + "&", cpp_name });
		CollectTypeHeader(header_includes, param.type);
	}

	const auto& ret = classified.outputs.back();
	if (ret.type)
		method.return_type = CastType(ret.type);
	CollectTypeHeader(header_includes, ret.type);
}

void CG::Core::cg::CollectFlowMethodParams(CBuildContext& ctx, const FlowMeta& flow_meta)
{
	ctx.method_params.clear();
	ctx.return_pin.clear();
	ctx.extra_outs.clear();

	const auto classified = ClassifyFlow(flow_meta);
	std::vector<std::string> taken;
	for (const auto& param : classified.inputs) {
		ctx.method_params.insert(param.name);
		taken.push_back(param.name);
	}
	if (classified.outputs.empty())
		return;

	for (size_t i = 0; i + 1 < classified.outputs.size(); ++i) {
		const auto& param = classified.outputs[i];
		ctx.extra_outs.emplace_back(param.name, UniqueCppName(taken, param.name));
	}
	ctx.return_pin = classified.outputs.back().name;
}

std::string CG::Core::cg::ToLiteral(const std::shared_ptr<AFrame>& frame)
{
	if (!frame) return {};
	const VType* type = frame->GetType();
	if (!type) return {};

	auto quote = [](const std::string& text) {
		std::string out = "\"";
		for (char c : text) {
			switch (c) {
			case '\\': out += "\\\\"; break;
			case '"': out += "\\\""; break;
			case '\n': out += "\\n"; break;
			case '\r': out += "\\r"; break;
			case '\t': out += "\\t"; break;
			default: out += c; break;
			}
		}
		out += '"';
		return out;
	};

	if (type->id_ == typeid(int) || type->id_ == typeid(int32)) {
		int value = 0;
		if (frame->GetValue(type, &value))
			return std::to_string(value);
	}
	if (type->id_ == typeid(int64) || type->id_ == typeid(long long)) {
		int64 value = 0;
		if (frame->GetValue(type, &value))
			return std::to_string(value) + "LL";
	}
	if (type->id_ == typeid(bool)) {
		bool value = false;
		if (frame->GetValue(type, &value))
			return value ? "true" : "false";
	}
	if (type->id_ == typeid(float)) {
		float value = 0.f;
		if (frame->GetValue(type, &value)) {
			std::ostringstream os;
			os << std::setprecision(9) << value;
			std::string text = os.str();
			if (text.find('.') == std::string::npos && text.find('e') == std::string::npos
				&& text.find('E') == std::string::npos)
				text += ".0";
			return text + "f";
		}
	}
	if (type->id_ == typeid(double)) {
		double value = 0.0;
		if (frame->GetValue(type, &value)) {
			std::ostringstream os;
			os << std::setprecision(17) << value;
			std::string text = os.str();
			if (text.find('.') == std::string::npos && text.find('e') == std::string::npos
				&& text.find('E') == std::string::npos)
				text += ".0";
			return text;
		}
	}
	if (type->id_ == typeid(std::string)) {
		std::string value;
		if (frame->GetValue(type, &value))
			return quote(value);
	}
	return {};
}
