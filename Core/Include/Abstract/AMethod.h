#pragma once
#include "ACaller.h"

namespace CG::Core {
	class AMethod : public ACaller {
	public:
		virtual ~AMethod() = default;

		virtual std::string GetKey() const = 0;

		bool IsPure() const override { return is_pure_; }

		bool IsStatic() const { return is_static_; }

		const VClass* GetOwner() const { return owner_; }

		/*static const VClass* StaticClass() {
			static VClass* c = CreateClass<AMethod, ACaller>("CG::Core", "AMethod");
			return c;
		}

		const VClass* GetClass() const override {
			return StaticClass();
		}*/

		
	protected:
		bool is_pure_{};
		bool is_static_{};
		VClass* owner_{};
	};
}