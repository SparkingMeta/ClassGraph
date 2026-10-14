#pragma once
#include "Base/VObject.h"

#include <optional>

namespace CG::Core {
	class ACaller;
	class AContext;
	class APin;

	class ANode : public VObject {
	public:
		virtual ~ANode() = default;
		virtual Result Work() = 0;
		virtual std::shared_ptr<ANode> Next(int index = 0) const = 0;
		virtual std::shared_ptr<APin> GetInputPin(uint16 id) const = 0;
		virtual std::shared_ptr<APin> GetOutputPin(uint16 id) const = 0;
		virtual std::shared_ptr<APin> GetPin(const std::string& guid) const = 0;
		virtual std::shared_ptr<APin> AddPin(int kind) = 0;
		virtual std::shared_ptr<ACaller> GetCaller() const = 0;
		virtual std::optional<bool> IsPure() const = 0;
		virtual AContext* GetContext() const = 0;

		/*static const VClass* StaticClass() {
			static VClass* c = CreateClass<ANode, VObject>("CG::Core", "ANode");
			return c;
		}

		const VClass* GetClass() const override {
			return StaticClass();
		}*/
	};
}