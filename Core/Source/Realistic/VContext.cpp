#include "Realistic/VContext.h"
#include "Realistic/VFrame.h"
#include "Base/VMeta.h"
#include "Base/VConvert.h"

using namespace CG::Core;

CG::Core::VContext::VContext(const CallMeta& meta)
{
	Init(meta);
}

CG::Core::VContext::~VContext()
{
}


bool CG::Core::VContext::SetFrame(const std::string& name, const std::shared_ptr<AFrame>& frame) {
	bool ret = false;

	do {
		if(!frame) break;

		auto it = frames_.find(name);
		if (it != frames_.end()) {
			auto tag = it->second;
			if(tag->GetType() != frame->GetType()) break;

			it->second = frame;
		}
		else 
			frames_.insert({name,frame});
		
		ret = true;
	}while(0);

	return ret;
}

std::shared_ptr<AFrame> CG::Core::VContext::GetFrame(const std::string& name) const
{
	if(auto it = frames_.find(name); it != frames_.end())
		return it->second;
	return {};
}

std::byte* CG::Core::VContext::GetRef(const std::string& name) const {
	auto frame = GetFrame(name);
	if(!frame) return {nullptr};
	return frame->GetBuffer();
}

bool CG::Core::VContext::RemoveFrame(const std::string& name)
{
	return frames_.erase(name) > 0;
}

bool CG::Core::VContext::RenameFrame(const std::string& from, const std::string& to)
{
	if (from.empty() || to.empty())
		return false;
	if (from == to)
		return true;
	auto it = frames_.find(from);
	if (it == frames_.end())
		return false;
	if (frames_.find(to) != frames_.end())
		return false;
	auto frame = std::move(it->second);
	frames_.erase(it);
	frames_.emplace(to, std::move(frame));
	return true;
}

std::vector<std::string> CG::Core::VContext::FrameNames() const
{
	std::vector<std::string> names;
	names.reserve(frames_.size());
	for (const auto& [name, frame] : frames_) {
		(void)frame;
		names.push_back(name);
	}
	return names;
}


void CG::Core::VContext::Init(const CallMeta& meta)
{
	for (const auto& e : meta.args) {
		const bool wildcard = e.is_runtime && e.type == nullptr;
		auto frame = std::make_shared<VFrame>(e.type, wildcard);
		if (!e.value.empty()) {
			if (auto value = FromString(e.value, e.type))
				frame->Update(value);
		}
		frames_.insert({ e.name, frame });
	}
}

