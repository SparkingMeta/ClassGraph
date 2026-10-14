#pragma once
#include <vector>
#include <unordered_map>
#include "Abstract/AContext.h"
#include "Base/VType.h"

namespace CG::Core {
	struct CallMeta;

	class VContext : public AContext {
	public:
		VContext(const CallMeta& meta);
		virtual ~VContext();

		virtual bool SetFrame(const std::string& name, const std::shared_ptr<AFrame>& frame) override;
		virtual std::shared_ptr<AFrame> GetFrame(const std::string& name) const override;
		virtual std::byte* GetRef(const std::string& name) const override;

		bool RemoveFrame(const std::string& name);
		bool RenameFrame(const std::string& from, const std::string& to);
		std::vector<std::string> FrameNames() const;
	private:
		void Init(const CallMeta& meta);
	private:
		std::unordered_map<std::string, std::shared_ptr<AFrame>> frames_{};
	};

}