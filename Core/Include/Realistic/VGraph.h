#pragma once
#include "Abstract/AGraph.h"

namespace CG::Core {
	class ALine;

	class VGraph : public AGraph {
	public:

		/*
		static const VClass* StaticClass() {
			return CreateClass<VGraph, VObject>("CG::Core", "VGraph");
		}*/

		explicit VGraph();

		std::shared_ptr<AFlow> GetFlow() const override;
		void SetFlow(const std::shared_ptr<AFlow>& flow) override;

		std::shared_ptr<ANode> GetEntry() const override;
		std::shared_ptr<ANode> GetExit() const override;

		void SetEntry(const std::shared_ptr<ANode>& node);
		void SetExit(const std::shared_ptr<ANode>& node);

		void AddNode(const std::shared_ptr<ANode>& node) override;
		void DelNode(const std::string& guid) override;
		std::shared_ptr<ANode> FindNode(const std::string& guid) const override;
		
		void AddLine(const std::shared_ptr<ALine>& line) override;
		void DelLine(const std::string& guid) override;
		std::shared_ptr<ALine> FindLine(const std::string& guid) const override;
		const std::unordered_map<std::string, std::shared_ptr<ALine>>& GetLines() const override{ return lines_; }
		const std::unordered_map<std::string, std::shared_ptr<ANode>>& GetNodes() const override{ return nodes_; }

	private:
		std::weak_ptr<AFlow> flow_;
		std::shared_ptr<ANode> entry_;
		std::shared_ptr<ANode> exit_;
		std::unordered_map<std::string, std::shared_ptr<ANode>> nodes_;
		std::unordered_map<std::string, std::shared_ptr<ALine>> lines_;
		
	};

} // namespace CG::Core
