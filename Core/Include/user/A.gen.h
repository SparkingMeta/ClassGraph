#pragma once

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID A_h

namespace CG::Core {
	class VClass;
}

CG::Core::VClass* Z_Construct_VClass_A();

#define A_h_11_GENERATED_BODY \
public: \
	inline static const CG::Core::VClass* StaticClass() { \
		return Z_Construct_VClass_A(); \
	} \
	const CG::Core::VClass* GetClass() const override { \
		return StaticClass(); \
	}
