#pragma once
#include <unordered_map>
#include <string>
#include <memory>

namespace CG::Core {
	class AFrame;
	struct VType;

	class AContext {
	public:
		virtual ~AContext() = default;

		virtual bool SetFrame(const std::string&name, const std::shared_ptr<AFrame>& frame) = 0;
		virtual std::shared_ptr<AFrame> GetFrame(const std::string& name) const = 0;
		virtual std::byte* GetRef(const std::string& name) const = 0;
	};

}