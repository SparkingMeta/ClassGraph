#include "Base/VObject.h"
#include "Base/VClass.h"
#include "Base/VGuid.h"
#include "Base/VSerialize.h"
#include "Abstract/ACaller.h"

using namespace CG::Core;

const VClass* VObject::GetClass() const
{
	return StaticClass();
}

bool CG::Core::VObject::Serialize(std::string& text) const
{
	return ToJson(this, text);
}

bool CG::Core::VObject::Deserialize(const std::string& text)
{
	return FromJson(text, this);
}

bool VObject::IsA(const VClass* target) const
{
	return GetClass()->IsChildOf(target);
}

void VObject::SetName(const std::string& name)
{
	name_ = name;
}

std::string VObject::GetName() const
{
	return name_;
}

void VObject::SetGuid(const std::string& guid)
{
	guid_ = guid;
}

std::string VObject::GetGuid() const
{
	return guid_;
}

inline const VClass* CG::Core::VObject::StaticClass()
{
	static VClass c("CG::Core", typeid(VObject).name(), "VObject");
	return &c;
}

void CG::Core::VObject::ProcessEvent(ACaller* caller, AContext* ctx)
{
	if (!caller) return;

	caller->Call(this, ctx);
}

VObject::VObject()
{
	guid_ = GenerateGUID();
}

CG::Core::VObject::VObject(const std::string& name) : name_(name)
{
}


