#include "Abstract/ACaller.h"
#include "Realistic/VContext.h"
#include "Realistic/VFrame.h"	

using namespace CG::Core;

std::shared_ptr<AContext> CG::Core::ACaller::MakeContext(const CallMeta& meta) const
{
	return std::make_shared<VContext>(meta);
}

std::shared_ptr<AFrame> CG::Core::FindFrame(AContext* ctx, const std::string& name)
{

	return std::shared_ptr<AFrame>();
}

std::vector<std::shared_ptr<AFrame>> CG::Core::GetInputs(AContext* ctx, const CallMeta& meta)
{
	std::vector<std::shared_ptr<AFrame>> frames{};
	if(!ctx) return frames;
	for (const auto& e : meta.args) {
		if(e.is_output) continue;
		auto frame = ctx->GetFrame(e.name);
		frames.emplace_back(frame);
	}
	
	return frames;
}

std::vector<std::shared_ptr<AFrame>> CG::Core::GetOutputs(AContext* ctx, const CallMeta& meta)
{
	std::vector<std::shared_ptr<AFrame>> frames;
	if(!ctx) return frames;
	for (const auto& e : meta.args) {
		if (!e.is_output) continue;
		auto frame = ctx->GetFrame(e.name);
		frames.emplace_back(frame);
	}

	return frames;
}
