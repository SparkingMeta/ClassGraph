#include "Realistic/VBlueprintObject.h"

#include "Base/VClass.h"
#include "Base/VSystem.h"
#include "Base/VRegister.h"
#include "Realistic/VAsset.h"
#include "Realistic/VFlow.h"

using namespace CG::Core;

const VClass* VBlueprintObject::StaticClass()
{
	static VClass* cls = []() {
		auto* c = CreateClass<VBlueprintObject, VObject>("CG::Core", "VBlueprintObject");
		c->SetCreator([](const std::string& name) -> VObject* {
			auto* obj = new VBlueprintObject();
			if (!name.empty())
				obj->SetName(name);
			return obj;
		});
		CG::Core::VSystem::Get().RegisterCls(c);
		return c;
	}();
	return cls;
}

const VClass* VBlueprintObject::GetClass() const
{
	return StaticClass();
}


void VBlueprintObject::SetBlueprint(const std::shared_ptr<VBlueprint>& blueprint)
{
	blueprint_ = blueprint;
}

std::shared_ptr<VBlueprint> VBlueprintObject::GetBlueprint() const
{
	return blueprint_.lock();
}
