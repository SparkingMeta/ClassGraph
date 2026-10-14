#pragma once
#include "Abstract/ACaller.h"

namespace CG::Core {

	class AOperator : public ACaller {
	public:
		bool IsPure() const override;
		int GetKind() const override;
		CallMeta GetCallMeta() const override;

	protected:
		CallMeta meta_;

		/*static const VClass* StaticClass() {
			static VClass* c = CreateClass<AOperator, ACaller>("CG::Core", "AOperator");
			return c;
		}

		const VClass* GetClass() const override {
			return StaticClass();
		}*/
	};
}