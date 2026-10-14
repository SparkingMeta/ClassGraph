#pragma once

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID VTest_h

namespace CG::Core {
	class VClass;
}

CG::Core::VClass* Z_Construct_VClass_VTest();

#define VTest_h_12_GENERATED_BODY \
public: \
	inline static const CG::Core::VClass* StaticClass() { \
		return Z_Construct_VClass_VTest(); \
	} \
	const CG::Core::VClass* GetClass() const override { \
		return StaticClass(); \
	}
