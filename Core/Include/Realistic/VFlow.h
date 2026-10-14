#pragma once
#include "Abstract/AFlow.h"

namespace CG::Core {
	class VFlow : public AFlow {
	public:
		VFlow();
		~VFlow() = default;

		std::shared_ptr<AGraph> GetGraph() const override;
		std::shared_ptr<AScript> GetScript() const override;
		const FlowMeta& GetFlowMeta() const override;
		int Run(const std::string& token) override;

		void BindScript(const std::shared_ptr<AScript>& script);

	private:
		void Init();

	private:
		std::shared_ptr<AGraph> graph_;
		std::shared_ptr<AScript> script_;
		mutable FlowMeta meta_;
	};
}
