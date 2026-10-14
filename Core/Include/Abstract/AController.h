#pragma once
#include "Abstract/ACaller.h"

namespace CG::Core {
	struct Status : Result {
		bool loop{};
		bool cond{};
		int first{};
		int last{};
		int out{};
	};

	class AController : public ACaller {
	public:
		bool IsPure() const override;
		int GetKind() const override;
		CallMeta GetCallMeta() const override;
		Result Call(VObject* object, AContext* ctx) override;

	private:
		virtual Status Init(AContext* ctx) = 0;
		virtual void Invoke(AContext* ctx, Status& status) = 0;
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