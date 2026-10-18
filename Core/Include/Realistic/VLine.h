#pragma once
#include "Abstract/ALine.h"

namespace CG::Core {
	class APin;
	class VLine : public ALine {
	public:
		std::shared_ptr<APin> GetFrom() const override;
		std::shared_ptr<APin> GetTo() const override;
		void SetFrom(std::shared_ptr<APin> from) override;
		void SetTo(std::shared_ptr<APin> to) override;
	private:
		std::weak_ptr<APin> from_;
		std::weak_ptr<APin> to_;
	};

	bool ConnectPins(const std::shared_ptr<APin>& from, const std::shared_ptr<APin>& to);
	std::shared_ptr<ALine> MakeLine(const std::shared_ptr<APin>& from, const std::shared_ptr<APin>& to);
	void DisconnectPin(const std::shared_ptr<APin>& pin);
	void DisconnectLine(const std::shared_ptr<ALine>& line);
}
