#include "Realistic/VVariant.h"
#include "Realistic/VFrame.h"

using namespace CG::Core;

CG::Core::VVariant::VVariant(const VType* type) : frame_(std::make_shared<VFrame>(type))
{
}

const VType* CG::Core::VVariant::GetType() const
{
	if (!frame_) return nullptr;
	return frame_->GetType();
}

std::shared_ptr<AFrame> CG::Core::VVariant::Getter() const
{
	return frame_;
}

void CG::Core::VVariant::Setter(std::shared_ptr<AFrame> var)
{
	frame_->Update(var);
}

void CG::Core::VVariant::Reset(const VType* type)
{
	frame_ = std::make_shared<VFrame>(type);
}

