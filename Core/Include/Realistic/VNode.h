#pragma once
#include "Abstract/ANode.h"
#include "Base/VCore.h"
#include "Base/VMeta.h"

#include <unordered_map>
#include <vector>

namespace CG::Core {

	class VNode : public ANode {
	public:
		VNode(const std::shared_ptr<ACaller>& caller);
		virtual ~VNode() = default;

		static std::shared_ptr<VNode> Make(const std::shared_ptr<ACaller>& caller);

		Result Work() override;
		std::shared_ptr<ANode> Next(int index = 0) const override;
		std::shared_ptr<APin> GetInputPin(uint16 id) const override;
		std::shared_ptr<APin> GetOutputPin(uint16 id) const override;
		std::shared_ptr<APin> GetPin(const std::string& guid) const override;
		std::shared_ptr<APin> AddPin(int kind) override;

		std::shared_ptr<ACaller> GetCaller() const override{ return caller_; }
		std::optional<bool> IsPure() const override;

		AContext* GetContext() const override { return context_.get(); }

		Vec2f GetCoord() const { return coord_; }
		void SetCoord(const Vec2f& coord) { coord_ = coord; }

		std::vector<std::shared_ptr<APin>> GetInputPins() const;
		std::vector<std::shared_ptr<APin>> GetOutputPins() const;
		void ApplyMeta(const std::string& renamed_from = {}, const std::string& renamed_to = {});

		/*static const VClass* StaticClass() {
			static VClass* c = CreateClass<VNode, ANode>("CG::Core", "VNode");
			return c;
		}

		const VClass* GetClass() const override {
			return StaticClass();
		}*/

	private:
	 void Init();
	 void Update();
	 void InitPins();

	private:
		Vec2f coord_{};
		std::unordered_map<uint16, std::shared_ptr<APin>> in_pins_{};
		std::unordered_map<uint16, std::shared_ptr<APin>> out_pins_{};
		std::shared_ptr<AContext> context_{ nullptr };
		std::shared_ptr<ACaller> caller_{ nullptr };
	};

}