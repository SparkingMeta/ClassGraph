#include "Realistic/VMethod.h"
#include "Base/VClass.h"
#include "Realistic/VFrame.h"
#include "Base/VRegister.h"

#include <format>

using namespace CG::Core;

std::string CG::Core::VMethod::GetKey() const
{
	std::string key{};
	std::string args_key{};
	size_t count = args_.size() - (index_return_ < 0 ? 0 : 1);
	for (size_t index = 0; index < count; index++) {
		args_key += args_[index].type->desc_;
		if(index != count - 1)
			args_key += ",";
	}
	key = std::format("{}({})",GetName(), args_key);
	return key;
}

std::shared_ptr<AContext> VMethod::MakeContext() const
{
	std::vector<const VType*> types;
	types.reserve(args_.size());
	for(size_t i = 0; i < args_.size(); i++)
		types.push_back(args_[i].type);

	return std::make_shared<VContext>(types);
}

int CG::Core::VMethod::Call(AContext* ctx)
{
	return Invoke(object, ctx);
}

CallMeta CG::Core::VMethod::GetMeta() const
{
	CallMeta meta;
	meta.name = GetName();
	meta.pure = is_pure_;
	meta.args = args_;
	return meta;
}

std::vector<AFrame*> CG::Core::VMethod::GetInputs(AContext* ctx) const
{
	std::vector<AFrame*> frames{};
	if(!ctx) return frames;
	for (const auto& e : args_) {
		if(e.is_output) continue;
		VFrame* frame = new VFrame(e.type);
		frame->SetName(e.name);
		frames.push_back(frame);
	}
	
	return frames;
}

std::vector<AFrame*> CG::Core::VMethod::GetOutputs(AContext* ctx) const
{
	std::vector<AFrame*> frames;
	if(!ctx) return frames;
	for (const auto& e : args_) {
		if (!e.is_output) continue;
		VFrame* frame = new VFrame(e.type);
		frame->SetName(e.name);
		frames.push_back(frame);
	}

	return frames;
}

const VClass* CG::Core::VMethod::StaticClass()
{
	static VClass* c = CreateClass<VMethod, AMethod>("CG::Core", "VMethod");
	return c;
}

const VClass* CG::Core::VMethod::GetClass() const
{
	return StaticClass();
}

int CG::Core::VMethod::Invoke(VObject* object, AContext* ctx)
{
	if(invoker_)
		invoker_(object, this, ctx);
	return 0;
}

VClass* CG::Core::FindClass(const std::string& name)
{
	return VClassRegister::Get().Find(name);
}
