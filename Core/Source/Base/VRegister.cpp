#include "Base/VRegister.h"
#include "Base/VClass.h"
#include "Base/VType.h"
#include "Abstract/ACaller.h"

using namespace CG::Core;

std::unordered_map<std::type_index, const VType*> VTypeRegister::index_types_{};
std::unordered_map<std::string, const VType*> VTypeRegister::name_types{};

void CG::Core::VTypeRegister::Register(const VType* type)
{
	if (!type) return;
	index_types_.insert({ type->id_, type });
	if (!type->name_.empty())
		name_types[type->name_] = type;
}

void CG::Core::VTypeRegister::Unregister(const std::type_index& id)
{
	if (auto it = index_types_.find(id); it != index_types_.end()) {
		if (it->second && !it->second->name_.empty())
			name_types.erase(it->second->name_);
		index_types_.erase(it);
	}
}

void CG::Core::VTypeRegister::Unregister(const std::string& name)
{
	if (auto it = name_types.find(name); it != name_types.end()) {
		if (it->second)
			index_types_.erase(it->second->id_);
		name_types.erase(it);
	}
}

const VType* CG::Core::VTypeRegister::Find(const std::type_index& id)
{
	if (auto it = index_types_.find(id); it != index_types_.end())
		return it->second;
	return nullptr;
}

const VType* CG::Core::VTypeRegister::Find(const std::string& name)
{
	if (auto it = name_types.find(name); it != name_types.end())
		return it->second;
	return nullptr;
}

std::vector<const VType*> CG::Core::VTypeRegister::List()
{
	std::vector<const VType*> out;
	out.reserve(name_types.size());
	for (const auto& [name, type] : name_types)
		out.push_back(type);
	return out;
}

VClassRegister& VClassRegister::Get()
{
	static VClassRegister instance;
	return instance;
}

bool VClassRegister::Register(VClass* cls){
	if(!cls) return false;
	std::string key = cls->GetSpace() + "::" + cls->GetName();

	std::unique_lock locker(locker_);
	auto it = cls_.find(key);
	if (it == cls_.end()) {
		cls_[key] = cls;
		return true;
	}
		
	if(it->second != cls)
		return false;

	return true;
}

void CG::Core::VClassRegister::Unregister(const std::string& name)
{
	std::unique_lock locker(locker_);
	if(auto it = cls_.find(name); it != cls_.end()) {
		cls_.erase(it);
	}
}

VClass* VClassRegister::Find(const std::string& name) const
{
	std::shared_lock locker(locker_);
	auto it = cls_.find(name);
	return it != cls_.end() ? it->second : nullptr;
}

std::vector<VClass*> VClassRegister::List() const
{
	std::shared_lock locker(locker_);
	std::vector<VClass*> out;
	out.reserve(cls_.size());
	for (const auto& [key, cls] : cls_) {
		if (cls)
			out.push_back(cls);
	}
	return out;
}

ACallerRegister& ACallerRegister::Get()
{
	static ACallerRegister instance;
	return instance;
}

bool CG::Core::ACallerRegister::Register(const std::shared_ptr<ACaller>& caller)
{
	if (!caller) return false;

	const std::string name = caller->GetName();
	if (name.empty()) return false;

	std::unique_lock locker(locker_);
	auto it = callers_.find(name);
	if (it != callers_.end())
		return it->second == caller;

	callers_[name] = caller;
	return true;
}

void CG::Core::ACallerRegister::Unregister(const std::string& name)
{
	std::unique_lock locker(locker_);
	callers_.erase(name);
}

std::shared_ptr<ACaller> ACallerRegister::Find(const std::string& name) const
{
	std::shared_lock locker(locker_);
	if (auto it = callers_.find(name); it != callers_.end())
		return it->second;
	return {};
}

std::vector<std::shared_ptr<ACaller>> ACallerRegister::List() const
{
	std::shared_lock locker(locker_);
	std::vector<std::shared_ptr<ACaller>> out;
	out.reserve(callers_.size());
	for (const auto& [name, caller] : callers_)
		out.push_back(caller);
	return out;
}