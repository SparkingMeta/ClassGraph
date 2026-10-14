#pragma once

#include "Base/VObject.h"

namespace CG::Core {
	class ANode;
	class AFlow;

	class AGraph : public VObject {
	public:
		virtual ~AGraph() = default;

		virtual std::shared_ptr<AFlow> GetFlow() const = 0;

		virtual std::shared_ptr<ANode> GetEntry() const = 0;
		virtual std::shared_ptr<ANode> GetExit() const = 0;

		virtual void AddNode(const std::shared_ptr<ANode>& node) = 0;
		virtual void DelNode(const std::string& guid) = 0;
		virtual std::shared_ptr<ANode> FindNode(const std::string& guid) const = 0;


	};

} // namespace CG::Core
