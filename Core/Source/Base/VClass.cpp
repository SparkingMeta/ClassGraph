#include "Base/VClass.h"
#include "Abstract/AFunction.h"
#include "Abstract/AProperty.h"
#include <iostream>
#include <mutex>
#include <unordered_set>
#include <vector>

using namespace CG::Core;

VClass::VClass(const std::string& space, const std::string& name, const std::string& desc) : VObject(name), space_(space), desc_(desc)
{
}

std::string VClass::GetSpace() const
{
	return space_;
}

std::string VClass::GetDesc() const
{
	return desc_;
}

const VClass* VClass::GetParent() const
{
	return parent_;
}

bool VClass::IsChildOf(const VClass* target) const
{
	const VClass* cls = this;
	while (cls) {
		if (cls == target)
			return true;
		cls = cls->GetParent();
	}
	return false;
}

void VClass::AddMethod(const std::shared_ptr<AMethod>& method)
{
	methods_.insert({ method->GetKey(),method });
}

std::shared_ptr<AMethod> VClass::GetMethod(const std::string& name) const
{
	if (auto it = methods_.find(name); it != methods_.end())
		return it->second;
	if (parent_)
		return parent_->GetMethod(name);
	return nullptr;
}

std::vector<std::shared_ptr<AMethod>> VClass::GetMethods() const
{
	std::vector<std::shared_ptr<AMethod>> out;
	std::unordered_set<std::string> seen;
	for (const VClass* cls = this; cls; cls = cls->GetParent()) {
		for (const auto& [key, method] : cls->methods_) {
			if (!method)
				continue;
			const std::string name = method->GetName();
			if (name.empty() || !seen.insert(name).second)
				continue;
			out.push_back(method);
		}
	}
	return out;
}

void VClass::AddProperty(const std::shared_ptr<AProperty>& property)
{
	if (!property) return;
	const std::string name = property->GetName();
	if (auto it = pros_.find(name); it != pros_.end()) {
		it->second = property;
		for (auto& p : prop_order_) {
			if (p && p->GetName() == name) {
				p = property;
				return;
			}
		}
		prop_order_.push_back(property);
		return;
	}
	pros_.insert({ name, property });
	prop_order_.push_back(property);
}

std::shared_ptr<AProperty> VClass::GetProperty(const std::string& name) const
{
	for (const VClass* cls = this; cls; cls = cls->GetParent()) {
		if (auto it = cls->pros_.find(name); it != cls->pros_.end())
			return it->second;
	}
	return nullptr;
}

std::vector<std::shared_ptr<AProperty>> VClass::GetProperties() const
{
	std::vector<std::shared_ptr<AProperty>> out;
	std::unordered_set<std::string> seen;
	std::vector<const VClass*> chain;
	for (const VClass* cls = this; cls; cls = cls->GetParent())
		chain.push_back(cls);
	for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
		for (const auto& prop : (*it)->prop_order_) {
			if (!prop)
				continue;
			const std::string name = prop->GetName();
			if (name.empty() || !seen.insert(name).second)
				continue;
			out.push_back(prop);
		}
	}
	return out;
}

void CG::Core::VClass::SetCreator(ObjectCreator creator)
{
	creator_ = creator;
}

VObject* CG::Core::VClass::CreateObject(const std::string& name) const
{
	if(creator_)
		return creator_(name);
	return nullptr;
}

void VClass::SetParent(const VClass* parent)
{
	if (!parent) return;
	std::call_once(flag_, [this, parent]() {this->parent_ = parent; });
}

