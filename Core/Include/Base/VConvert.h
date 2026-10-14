#pragma once
#include <string>
#include <memory>

#include "Abstract/AFrame.h"
#include "Base/VType.h"
#include "Realistic/VFrame.h"

namespace CG::Core {

	inline std::string ToString(const std::shared_ptr<AFrame>& frame)
	{
		if (!frame || !frame->IsValid())
			return {};

		const VType* type = frame->GetType();
		if (!type || !type->serialize_)
			return {};

		std::string out;
		if (!type->serialize_(frame->GetBuffer(), out))
			return {};
		return out;
	}

	inline std::shared_ptr<AFrame> FromString(const std::string& text, const VType* type)
	{
		if (!type || !type->deserialize_ || !type->construct_)
			return {};

		auto frame = std::make_shared<VFrame>(type);
		if (!frame->IsValid())
			return {};

		if (!type->deserialize_(frame->GetBuffer(), text))
			return {};
		return frame;
	}

}
