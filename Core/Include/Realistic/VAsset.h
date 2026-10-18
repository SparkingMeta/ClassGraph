#pragma once
#include "Abstract/AAsset.h"
#include "Realistic/VBlueprintObject.h"
#include <unordered_map>
#include <set>

namespace CG::Core {
	class AFlow;
	class AVariant;
	struct VType;
	class VSystem;

	class VBlueprint : public AAsset {
	public:
		void Load(const std::string& info) override;
		int GenerateCode(std::string& source, std::string& header, std::string& gen_header, std::string& meta);

		std::string AddFlow(const std::string& name);
		std::string AddFlow(const std::string& name, const std::string& guid);
		void DelFlow(const std::string& guid);
		std::shared_ptr<AFlow> FindFlow(const std::string& guid) const;
		std::shared_ptr<AFlow> FindFlowByName(const std::string& name) const;
		void ClearFlows();

		std::string AddVar(const std::string& name, const std::string& type_name);
		std::string AddVar(const std::string& name, const std::string& type_name, const std::string& guid);
		void DelVar(const std::string& guid);
		std::shared_ptr<AVariant> FindVar(const std::string& guid);

		const std::unordered_map<std::string, std::shared_ptr<AFlow>>& GetFlows() const { return flows_; }
		const std::unordered_map<std::string, std::shared_ptr<AVariant>>& GetVars() const { return vars_; }

		void SetSpace(const std::string& space) { space_ = space; }
		std::string GetSpace() const { return space_; }

		void SetParentClass(const std::string& parent_class) { parent_class_ = parent_class; }
		std::string GetParentClass() const { return parent_class_; }

		void SetParentHeader(const std::string& header) { parent_header_ = header; }
		std::string GetParentHeader() const { return parent_header_; }

	private:
		uint16 GetID() const;
	private:
		std::string space_;
		std::string parent_class_{ kBlueprintDefaultParent };
		std::string parent_header_{ kBlueprintDefaultParentHeader };
		std::unordered_map<std::string, std::shared_ptr<AFlow>> flows_;
		std::unordered_map<std::string, std::shared_ptr<AVariant>> vars_;
	};
}
