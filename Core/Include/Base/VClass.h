#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include "VCore.h"
#include "VObject.h"

using namespace std;

namespace CG::Core {

	class AFunction;
	class AProperty;

	using AMethod = AFunction;
	using ObjectCreator = VObject*(*)(const std::string&);

	class CORE_API VClass : public VObject{
	friend class VObject;

	template<typename T, typename Super>
	friend VClass* CreateClass(const std::string&, const std::string&);

	public:
		VClass(const std::string& space, const std::string& name, const std::string& desc);

		~VClass() = default;

		std::string GetSpace() const;

		std::string GetDesc() const;

		const VClass* GetParent() const;

		bool IsChildOf(const VClass* target) const;

		void AddMethod(const std::shared_ptr<AMethod>& method);

		std::shared_ptr<AMethod> GetMethod(const std::string& name) const;

		std::vector<std::shared_ptr<AMethod>> GetMethods() const;

		void AddProperty(const std::shared_ptr<AProperty>& property);

		std::shared_ptr<AProperty> GetProperty(const std::string& name) const;

		std::vector<std::shared_ptr<AProperty>> GetProperties() const;

		void SetCreator(ObjectCreator creator);

		VObject* CreateObject(const std::string& name) const;

	private:
		void SetParent(const VClass* parent);

	private:
		std::string space_{};
		std::string desc_{};
		const VClass* parent_{};
		std::once_flag flag_{};
		std::unordered_map<std::string, std::shared_ptr<AMethod>> methods_{};
		std::unordered_map<std::string, std::shared_ptr<AProperty>> pros_{};
		std::vector<std::shared_ptr<AProperty>> prop_order_{};
		ObjectCreator creator_{};
	};
	
	template<typename T, typename Super>
	static VClass* CreateClass(const std::string& space, const std::string& name) {
		static_assert(std::is_base_of_v<Super, T>, "T must inherit from Super!");
		static VClass c(space, name, typeid(T).name());
		c.SetParent(Super::StaticClass());
		return &c;
	}

}