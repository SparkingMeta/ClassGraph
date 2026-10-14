#include "Realistic/VAsset.h"
#include "Realistic/VFlow.h"
#include "Realistic/VGraph.h"
#include "Realistic/VVariant.h"
#include "Realistic/VScript.h"
#include "Base/VGuid.h"
#include "Base/VRegister.h"
#include "Base/VSystem.h"
#include "CodeGen/CCompiler.h"
#include <format>

using namespace CG::Core;

void CG::Core::VBlueprint::Load(const std::string& info)
{
}

int CG::Core::VBlueprint::GenerateCode(std::string& source, std::string& header, std::string& gen_header, std::string& meta)
{
	return CG::Core::cg::CCompiler::Compile(*this, source, header, gen_header, meta);
}

std::string CG::Core::VBlueprint::AddFlow(const std::string& name)
{
	auto flow = std::make_shared<VFlow>();

	std::string guid = GenerateGUID();

	flow->SetGuid(guid);
	flow->SetName(name);
	auto script = std::make_shared<VScript>(flow);
	script->SetName(name);
	flow->BindScript(script);
	if (auto graph = std::dynamic_pointer_cast<VGraph>(flow->GetGraph()))
		graph->SetFlow(flow);
	flows_.insert({flow->GetGuid(), flow});
	return guid;
}

void CG::Core::VBlueprint::DelFlow(const std::string& guid)
{
	flows_.erase(guid);
}

std::shared_ptr<AFlow> CG::Core::VBlueprint::FindFlow(const std::string& guid) const
{
	if(auto it = flows_.find(guid); it != flows_.end())
		return it->second;
	return nullptr;
}

std::shared_ptr<AFlow> CG::Core::VBlueprint::FindFlowByName(const std::string& name) const
{
	for (const auto& [guid, flow] : flows_) {
		if (flow && flow->GetName() == name)
			return flow;
	}
	return nullptr;
}

std::string CG::Core::VBlueprint::AddVar(const std::string& name, const std::string& type_name)
{
	const VType* type = VTypeRegister::Find(type_name);
	if (!type) return {};

	if(type_name == std::format("{}::{}",space_, GetName()))
		return {};

	auto var = std::make_shared<VVariant>(type);

	std::string guid = GenerateGUID();
	
	var->SetGuid(guid);
	var->SetName(name);
	var->SetID(GetID());

	vars_.insert({var->GetGuid(),var});

	return var->GetGuid();
}

void CG::Core::VBlueprint::DelVar(const std::string& guid)
{
	vars_.erase(guid);
}

std::shared_ptr<AVariant> CG::Core::VBlueprint::FindVar(const std::string& guid)
{
	if(auto it = vars_.find(guid); it != vars_.end())
		return it->second;
	return {};
}

uint16 CG::Core::VBlueprint::GetID() const
{
	uint16 id = 1;

	do 
	{
		std::set<uint16> ids;
		for (const auto& e : vars_) 
			ids.insert(e.second->GetID());

		if(ids.empty()) break;

		auto it = ids.begin();
		uint16 begin_id = *it;
		uint16 end_id = *ids.rbegin();
		
		uint16 diff = *ids.rbegin() - begin_id;
		if ((diff + 1) == ids.size()) {
			if(id == begin_id) id = end_id + 1;
		}
		else {
			uint16 pre_id = begin_id;
			while (++it != ids.end()) {
				if ((*it - pre_id) != 1) {
					id = pre_id + 1;
					break;
				}
				pre_id = *it;
			}
		}

	} while (0);
	

	return id;
}
