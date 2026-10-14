#pragma  once
#include "Abstract/AVariant.h"
#include <unordered_map>

namespace CG::Core {

	class VVariant : public AVariant {
	public:
		VVariant(const VType* type);
		const VType* GetType() const override;
		std::shared_ptr<AFrame> Getter() const override;
		void Setter(std::shared_ptr<AFrame> var) override;
		void Reset(const VType* type);
	private:
		std::shared_ptr<AFrame> frame_;
	};

}