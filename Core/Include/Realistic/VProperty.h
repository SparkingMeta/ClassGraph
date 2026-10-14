#pragma once

#include "Abstract/AProperty.h"
#include "Base/VRegister.h"

#include <cstddef>
#include <memory>
#include <string>

namespace CG::Core {

	class VProperty : public AProperty {
	public:
		void SetAccessLevel(int level) override { access_ = level; }
		int GetAccessLevel() const override { return access_; }
		bool IsConst() const override { return is_const_; }
		bool IsStatic() const override { return is_static_; }

		void SetConst(bool value) { is_const_ = value; }
		void SetStatic(bool value) { is_static_ = value; }

		const VType* GetType() const { return type_; }
		size_t GetOffset() const { return offset_; }

	private:
		int access_{ static_cast<int>(EVisiable::Public) };
		bool is_const_{ false };
		bool is_static_{ false };
	};

	template<typename C, typename T>
	std::shared_ptr<VProperty> LoadProperty(T C::* member, const char* name)
	{
		auto prop = std::make_shared<VProperty>();
		prop->SetName(name);
		prop->type_ = FindType(typeid(T));
		prop->offset_ = reinterpret_cast<size_t>(&(static_cast<C*>(nullptr)->*member));
		return prop;
	}

}
