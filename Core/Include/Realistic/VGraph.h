#pragma once
#include "Abstract/AGraph.h"

#include <unordered_map>

namespace CG::Core {

	class VGraph : public AGraph {
	public:
		
		/*
		static const VClass* StaticClass() {
			return CreateClass<VGraph, VObject>("CG::Core", "VGraph");
		}*/

		explicit VGraph(const std::shared_ptr<AFlow>& flow);

		std::shared_ptr<AFlow> GetFlow() const override;

		std::shared_ptr<ANode> GetEntry() const override;
		std::shared_ptr<ANode> GetExit() const override;

		void SetEntry(const std::shared_ptr<ANode>& node);
		void SetExit(const std::shared_ptr<ANode>& node);

		void AddNode(const std::shared_ptr<ANode>& node) override;
		void DelNode(const std::string& guid) override;
		std::shared_ptr<ANode> FindNode(const std::string& guid) const override;

		void SetFlow(const std::shared_ptr<AFlow>& flow);
		const std::unordered_map<std::string, std::shared_ptr<ANode>>& GetNodes() const { return nodes_; }

	private:
		std::weak_ptr<AFlow> task_;
		std::shared_ptr<ANode> entry_;
		std::shared_ptr<ANode> exit_;
		std::unordered_map<std::string, std::shared_ptr<ANode>> nodes_;
		
	};

} // namespace CG::Core
