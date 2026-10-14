#include "Realistic/VFlow.h"
#include "Realistic/VGraph.h"
#include "Realistic/VScript.h"
#include "Realistic/VOperator.h"
#include "Abstract/ANode.h"

using namespace CG::Core;

CG::Core::VFlow::VFlow()
{
	Init();
}

std::shared_ptr<AGraph> CG::Core::VFlow::GetGraph() const
{
	return graph_;
}

std::shared_ptr<AScript> CG::Core::VFlow::GetScript() const
{
	return script_;
}

const FlowMeta& CG::Core::VFlow::GetFlowMeta() const
{
	meta_.params.clear();
	if (!graph_) return meta_;

	if (auto node = graph_->GetEntry()) {
		if (auto entry = node->GetCaller()) {
			for (const auto& arg : entry->GetCallMeta().args) {
				if (!arg.is_output || !arg.is_runtime || !arg.type || arg.name.empty())
					continue;
				meta_.params.push_back(ArgMeta{ {{}, arg.name}, false, true, arg.type });
			}
		}
	}

	if (auto node = graph_->GetExit()) {
		if (auto exit = node->GetCaller()) {
			for (const auto& arg : exit->GetCallMeta().args) {
				if (arg.is_output || !arg.is_runtime || arg.name.empty())
					continue;
				meta_.params.push_back(ArgMeta{ {{}, arg.name}, true, true, arg.type });
			}
		}
	}

	return meta_;
}

void CG::Core::VFlow::BindScript(const std::shared_ptr<AScript>& script)
{
	script_ = script;
}

int CG::Core::VFlow::Run(const std::string& token)
{
	int code = -1;
	if (!graph_) return code;

	std::shared_ptr<ANode> node = graph_->GetEntry();
	while (node) {
		auto result = node->Work(token);
		code = result.code;
		if (0 != code) break;
		node = node->Next(result.index);
	}

	return code;
}

void CG::Core::VFlow::Init()
{
	graph_ = std::make_shared<VGraph>(std::shared_ptr<AFlow>{});
}
