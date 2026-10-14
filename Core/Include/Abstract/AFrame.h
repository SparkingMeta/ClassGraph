#pragma once
#include "Base/VObject.h"
//#include "Base/VClass.h"

namespace CG::Core {
	struct VType;

	class AFrame : public VObject {
	public:
		virtual ~AFrame() = default;
		virtual bool IsValid() const = 0;
		virtual bool Update(const std::shared_ptr<AFrame>& frame) = 0;
		virtual bool GetValue(const VType* type, void* value) const = 0;
		virtual bool SetValue(const VType* type, void* value) = 0;
		//virtual void SetType(const VType*) = 0;
		//virtual void SetBuffer(std::byte*) = 0;
		virtual const VType* GetType() const = 0;
		virtual std::byte* GetBuffer() const = 0;
		virtual bool IsWildcard() const = 0;

		/*static const VClass* StaticClass() {
			static VClass* c = CreateClass<AFrame, VObject>("CG::Core", "AFrame");
			return c;
		};

		const VClass* GetClass() const override {
			return StaticClass();
		}*/
	};
}