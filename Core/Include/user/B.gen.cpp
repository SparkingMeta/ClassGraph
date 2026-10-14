#include "B.h"
#include "Realistic/VBlueprintObject.h"
#include "Base/VClass.h"
#include "Base/VSystem.h"
#include "Base/VRegister.h"
#include "Base/VType.h"
#include "Base/VObject.h"
#include "Realistic/VFunction.h"
#include "Realistic/VProperty.h"

CG::Core::VClass* Z_Construct_VClass_B() {
	static CG::Core::VClass* c = []() {
		CG::Core::RegisterClassType<user::B>("user::B", "user/B.h");
		auto* cls = CG::Core::CreateClass<user::B, CG::Core::VBlueprintObject>("user", "B");
		cls->SetCreator([](const std::string& name) -> CG::Core::VObject* {
			auto* obj = new user::B();
			if (!name.empty()) obj->SetName(name);
			return obj;
		});
		cls->AddProperty(CG::Core::LoadProperty(&user::B::a, "a"));
		cls->AddProperty(CG::Core::LoadProperty(&user::B::b, "b"));
		{
			auto fn = std::make_shared<CG::Core::VFunction>();
			fn->SetName("EventGraph");
			auto meta = CG::Core::Load(&user::B::EventGraph);
			meta.owner = cls;
			fn->SetMeta(meta);
			cls->AddMethod(fn);
		}
		CG::Core::VSystem::Get().RegisterCls(cls);
		return cls;
	}();
	return c;
}

#if defined(_WIN32)
#define CLASSGRAPH_TYPE_API __declspec(dllexport)
#else
#define CLASSGRAPH_TYPE_API __attribute__((visibility("default")))
#endif

extern "C" CLASSGRAPH_TYPE_API CG::Core::VClass* ClassGraphLoadType() {
	return Z_Construct_VClass_B();
}
