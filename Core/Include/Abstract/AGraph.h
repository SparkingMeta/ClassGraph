#pragma once

#include "Base/VObject.h"
#include <unordered_map>

namespace CG::Core {
	class ALine;
	class ANode;
	class AFlow;

	class AGraph : public VObject {
	public:
		virtual ~AGraph() = default;

		virtual std::shared_ptr<AFlow> GetFlow() const = 0;
		virtual void SetFlow(const std::shared_ptr<AFlow>& flow) = 0;

		virtual std::shared_ptr<ANode> GetEntry() const = 0;
		virtual std::shared_ptr<ANode> GetExit() const = 0;

		virtual void AddNode(const std::shared_ptr<ANode>& node) = 0;
		virtual void DelNode(const std::string& guid) = 0;
		virtual std::shared_ptr<ANode> FindNode(const std::string& guid) const = 0;

		virtual void AddLine(const std::shared_ptr<ALine>& line) = 0;
		virtual void DelLine(const std::string& guid) = 0;
		virtual std::shared_ptr<ALine> FindLine(const std::string& guid) const = 0;

		virtual const std::unordered_map<std::string, std::shared_ptr<ANode>>& GetNodes() const = 0;
		virtual const std::unordered_map<std::string, std::shared_ptr<ALine>>& GetLines() const = 0;
	};

} // namespace CG::Core
