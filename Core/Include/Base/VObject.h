#pragma once
#include <string>
#include <iostream>
#include <memory>
#include "VCore.h"
	
namespace CG::Core {
	class VClass;
	class AContext;
	class ACaller;

	enum class EAccessLevel : uint16_t{
		Public,
		Protected,
		Private
	};


	struct VObject : std::enable_shared_from_this<VObject>
	{
	public:
		virtual ~VObject() = default;

		virtual const VClass* GetClass() const;

		virtual bool Serialize(std::string& text) const;

		virtual bool Deserialize(const std::string& text);

		bool IsA(const VClass* target) const;

		void SetName(const std::string& name);

		std::string GetName() const;

		void SetGuid(const std::string& guid);

		std::string GetGuid() const;

		static const VClass* StaticClass();

		template<typename T>
		const T* Cast() const {
			if (IsA(T::StaticClass()))
				return static_cast<const T*>(this);
			return nullptr;
		}

		template<typename T>
		T* Cast() {
			return const_cast<T*>(static_cast<const VObject*>(this)->Cast<T>());
		}

		void ProcessEvent(ACaller* caller, AContext* ctx);

	protected:
		VObject();
		VObject(const std::string& name);

	private:
		std::string name_{};
		std::string guid_{};
	};

}

template<class T, class U>
std::shared_ptr<T> CastPointer(const std::shared_ptr<U>& src) {
    if (src && src->IsA(T::StaticClass())) {
        return std::shared_ptr<T>(src, static_cast<T*>(src.get()));
    }
    return {nullptr};
}


