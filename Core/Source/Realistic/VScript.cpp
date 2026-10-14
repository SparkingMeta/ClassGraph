#include "Realistic/VScript.h"
#include "Realistic/VFlow.h"
#include "Abstract/ANode.h"
#include "Abstract/AGraph.h"
#include "Abstract/AContext.h"
#include "Abstract/AFrame.h"
#include "Base/VGuid.h"
#include "Realistic/VContext.h"

using namespace CG::Core;

CG::Core::VScript::VScript(const std::shared_ptr<AFlow>& flow) : flow_(flow)
{
}

std::string CG::Core::VScript::GetKey() const
{
	auto flow = flow_.lock();
	if (!flow) return {};
	return flow->GetName();
}

bool CG::Core::VScript::IsPure() const
{
	return false;
}

int CG::Core::VScript::GetKind() const
{
	return CK_Script;
}

CallMeta CG::Core::VScript::GetCallMeta() const
{
	CallMeta meta;
	auto flow = flow_.lock();
	if (!flow) return meta;

	const auto& flow_meta = flow->GetFlowMeta();
	meta.name = flow->GetName();
	meta.pure = false;
	meta.args = flow_meta.params;
	return meta;
}

Result CG::Core::VScript::Call(VObject* object, AContext* ctx)
{
	Result status{};
	int code = -1;
	const int index = 0;

	do {
		auto flow = flow_.lock();
		if (!flow) break;

		auto graph = flow->GetGraph();
		do{
			if(!graph || !ctx) break;

			auto entry = graph->GetEntry();
			if(!entry) break;

			auto* entry_ctx = entry->GetContext();
			if(!entry_ctx) break;

			for (const auto& arg : GetCallMeta().args) {
				if (arg.is_output || !arg.is_runtime || arg.name.empty())
					continue;
				auto src = ctx->GetFrame(arg.name);
				auto dst = entry_ctx->GetFrame(arg.name);
				if (src && dst) dst->Update(src);
			}
		}while(0);

		std::string token = "";
		if (auto* node = dynamic_cast<ANode*>(object))
			token = node->GetToken();

		code = flow->Run(token);

		do{
			if(!graph || !ctx) break;

			auto exit = graph->GetExit();
			if(!exit) break;

			auto* exit_ctx = exit->GetContext();
			if(!exit_ctx) break;

			for (const auto& arg : GetCallMeta().args) {
				if (!arg.is_output || !arg.is_runtime || arg.name.empty())
					continue;
				auto src = exit_ctx->GetFrame(arg.name);
				auto dst = ctx->GetFrame(arg.name);
				if (src && dst) dst->Update(src);
			}
		}while(0);
		
	} while (0);

	status.code = code;
	status.index = index;
	return status;
}

ExecMeta CG::Core::VScript::GetExecMeta() const
{
	return ExecMeta{ {}, !IsPure(), !IsPure() };
}

std::vector<const VType*> CG::Core::VScript::GetInputTypes() const
{
	std::vector<const VType*> types;
	auto flow = flow_.lock();
	if (!flow) return types;

	for (const auto& param : flow->GetFlowMeta().params) {
		if (!param.is_output)
			types.push_back(param.type);
	}
	return types;
}

std::vector<const VType*> CG::Core::VScript::GetOutputTypes() const
{
	std::vector<const VType*> types;
	auto flow = flow_.lock();
	if (!flow) return types;

	for (const auto& param : flow->GetFlowMeta().params) {
		if (param.is_output)
			types.push_back(param.type);
	}
	return types;
}
