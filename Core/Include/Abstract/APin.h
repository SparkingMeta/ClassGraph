#pragma once
#include "Base/VObject.h"
#include "Base/VCore.h"
#include "Base/VMeta.h"

#include <vector>

namespace CG::Core {
	class ANode;
	class ALine;
	class AFrame;

	enum class EDirect : uint16 {
		In,
		Out
	};

	class APin : public VObject {
	public:
		APin(const std::shared_ptr<ANode>& node, EDirect direct) : node_(node), direct_(direct){}
		virtual void BindLine(std::shared_ptr<ALine>) = 0;
		virtual void UnbindLine(std::shared_ptr<ALine>) = 0;
		virtual std::shared_ptr<ALine> GetLine() const = 0;
		virtual std::vector<std::shared_ptr<ALine>> GetLines() const
		{
			auto line = GetLine();
			if (line)
				return { line };
			return {};
		}
		virtual void SetFrame(const std::shared_ptr<AFrame>&) = 0;
		virtual std::shared_ptr<AFrame> GetFrame() const = 0;
		virtual void Update() = 0;
		std::shared_ptr<ANode> GetOwner() const { return node_.lock();}
		EDirect GetDirect() const { return direct_;}

		void SetPinMeta(const PinMeta& meta) { pin_meta_ = meta; }
		const PinMeta& GetPinMeta() const { return pin_meta_; }
	private:
		std::weak_ptr<ANode> node_;
		EDirect direct_;
		PinMeta pin_meta_{};
	};
}