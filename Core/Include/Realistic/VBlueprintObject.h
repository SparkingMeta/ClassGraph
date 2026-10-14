#pragma once

#include "Abstract/ABlueprintObject.h"

namespace CG::Core {
	class VClass;

	inline constexpr const char* kBlueprintDefaultParent = "CG::Core::VBlueprintObject";
	inline constexpr const char* kBlueprintDefaultParentHeader = "Realistic/VBlueprintObject.h";
	inline constexpr const char* kCodegenSelf = "this";

	inline std::string ClsKey(const std::string& space, const std::string& name)
	{
		if (space.empty()) return name;
		return space + "::" + name;
	}

	class VBlueprintObject : public ABlueprintObject {
	public:
		static const VClass* StaticClass();
		const VClass* GetClass() const override;
		void SetBlueprint(const std::shared_ptr<VBlueprint>& blueprint);
		std::shared_ptr<VBlueprint> GetBlueprint() const;

	protected:
		std::weak_ptr<VBlueprint> blueprint_;
	};
}
