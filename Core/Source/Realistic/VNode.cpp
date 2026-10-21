#include "Realistic/VNode.h"
#include "Abstract/ACaller.h"
#include "Abstract/APin.h"
#include "Abstract/ALine.h"
#include "Realistic/VPin.h"
#include "Realistic/VLine.h"
#include "Abstract/AContext.h"
#include "Abstract/AFrame.h"
#include "Realistic/VContext.h"
#include "Realistic/VFrame.h"
#include "Base/VMeta.h"
#include "Base/VConvert.h"

#include <string>
#include <unordered_map>
#include <unordered_set>

using namespace CG::Core;


CG::Core::VNode::VNode(const std::shared_ptr<ACaller>& caller) : caller_(caller)
{
	Init();
}

std::shared_ptr<VNode> CG::Core::VNode::Make(const std::shared_ptr<ACaller>& caller)
{
	auto node = std::make_shared<VNode>(caller);
	node->InitPins();
	return node;
}

Result CG::Core::VNode::Work()
{
	int code = -1, index = -1;
	bool loop = false;
	do {
		if (!caller_) break;

		Update();

		auto result = caller_->Call(this, context_.get());

		code = result.code;
		index = result.index;
		loop = result.flag;

	}while(loop);
	
	const Result result{ {}, code, index };
	if (work_observer_)
		work_observer_(std::static_pointer_cast<ANode>(shared_from_this()), result);
	return result;
}

void CG::Core::VNode::Update()
{
	for (const auto& e : in_pins_) {
		auto pin = e.second;
		if (!pin || pin->GetPinMeta().kind != PinKind::Data)
			continue;
		auto frame = pin->GetFrame();
		const auto& meta = pin->GetPinMeta();
		if (!frame || !meta.type || meta.value.empty())
			continue;
		if (auto value = FromString(meta.value, meta.type))
			frame->Update(value);
	}
	for (const auto& e : in_pins_) {
		if (e.second)
			e.second->Update();
	}
}

std::shared_ptr<ANode> CG::Core::VNode::Next(int index) const
{
	std::shared_ptr<ANode> node{};

	do {
		std::shared_ptr<APin> pin{};
		if(auto it = out_pins_.find(index); it != out_pins_.end())
			pin = it->second;

		if(!pin) break;

		std::shared_ptr<ALine> line = pin->GetLine();
		if(!line) break;

		pin = line->GetTo();
		if(!pin) break;

		node = pin->GetOwner();

	}while(0);

	return node;
}

std::shared_ptr<APin> CG::Core::VNode::GetInputPin(uint16 id) const
{
	if (auto it = in_pins_.find(id); it != in_pins_.end())
		return it->second;
	return std::shared_ptr<APin>();
}

std::shared_ptr<APin> CG::Core::VNode::GetOutputPin(uint16 id) const
{
	if (auto it = out_pins_.find(id); it != out_pins_.end())
		return it->second;
	return std::shared_ptr<APin>();
}

std::shared_ptr<APin> CG::Core::VNode::GetPin(const std::string& guid) const
{
	if (guid.empty())
		return {};
	for (const auto& [id, pin] : in_pins_) {
		(void)id;
		if (pin && pin->GetGuid() == guid)
			return pin;
	}
	for (const auto& [id, pin] : out_pins_) {
		(void)id;
		if (pin && pin->GetGuid() == guid)
			return pin;
	}
	return {};
}

std::shared_ptr<APin> CG::Core::VNode::AddPin(int kind)
{
	return std::shared_ptr<APin>();
}

std::optional<bool> CG::Core::VNode::IsPure() const
{
	std::optional<bool> res;
	if(caller_) res = caller_->IsPure();
	return res;
}

void CG::Core::VNode::Init()
{
	if (!caller_) return;

	auto call_meta = caller_->GetCallMeta();

	context_ = caller_->MakeContext(call_meta);
}

void CG::Core::VNode::InitPins()
{
	if (!caller_ || !context_) return;

	auto call_meta = caller_->GetCallMeta();
	auto exec_meta = caller_->GetExecMeta();
	auto layout = BuildLayout(exec_meta, call_meta);

	auto self = std::static_pointer_cast<ANode>(shared_from_this());

	auto make_pin = [&](const PinMeta& pin_meta, EDirect direct) -> std::shared_ptr<APin> {
		std::shared_ptr<APin> pin;
		if (pin_meta.kind == PinKind::Data) {
			auto data_pin = std::make_shared<VDataPin>(self, direct);
			data_pin->SetFrame(context_->GetFrame(pin_meta.name));
			pin = data_pin;
		} else {
			pin = std::make_shared<VExecPin>(self, direct);
		}
		pin->SetPinMeta(pin_meta);
		if (pin_meta.kind == PinKind::Data && direct == EDirect::In) {
			if (auto frame = pin->GetFrame()) {
				auto meta = pin->GetPinMeta();
				if (meta.value.empty())
					meta.value = ToString(frame);
				pin->SetPinMeta(meta);
			}
		}
		return pin;
	};

	uint16 index = 0;
	for (const auto& pin_meta : layout.inputs)
		in_pins_[index++] = make_pin(pin_meta, EDirect::In);

	index = 0;
	for (const auto& pin_meta : layout.outputs)
		out_pins_[index++] = make_pin(pin_meta, EDirect::Out);
}

std::vector<std::shared_ptr<APin>> CG::Core::VNode::GetInputPins() const
{
	std::vector<std::shared_ptr<APin>> pins;
	for (uint16 i = 0; i < static_cast<uint16>(in_pins_.size()); ++i) {
		if (auto it = in_pins_.find(i); it != in_pins_.end())
			pins.push_back(it->second);
	}
	return pins;
}

std::vector<std::shared_ptr<APin>> CG::Core::VNode::GetOutputPins() const
{
	std::vector<std::shared_ptr<APin>> pins;
	for (uint16 i = 0; i < static_cast<uint16>(out_pins_.size()); ++i) {
		if (auto it = out_pins_.find(i); it != out_pins_.end())
			pins.push_back(it->second);
	}
	return pins;
}

void CG::Core::VNode::ApplyMeta(const std::string& renamed_from, const std::string& renamed_to)
{
	if (!caller_ || !context_) return;

	auto call_meta = caller_->GetCallMeta();
	auto layout = BuildLayout(caller_->GetExecMeta(), call_meta);
	auto* ctx = dynamic_cast<VContext*>(context_.get());

	if (ctx && !renamed_from.empty() && renamed_from != renamed_to)
		ctx->RenameFrame(renamed_from, renamed_to);

	std::unordered_set<std::string> keep;
	for (const auto& arg : call_meta.args) {
		keep.insert(arg.name);
		auto frame = context_->GetFrame(arg.name);
		if (!frame) {
			const bool wildcard = arg.is_runtime && arg.type == nullptr;
			context_->SetFrame(arg.name, std::make_shared<VFrame>(arg.type, wildcard));
			frame = context_->GetFrame(arg.name);
		} else if (arg.type && frame->GetType() && arg.type != frame->GetType()) {
			if (auto vf = std::dynamic_pointer_cast<VFrame>(frame))
				vf->Reset(arg.type);
		}
		if (frame && !arg.value.empty())
			if (auto value = FromString(arg.value, arg.type)) frame->Update(value);
	}
	if (ctx) {
		for (const auto& name : ctx->FrameNames()) {
			if (!keep.count(name))
				ctx->RemoveFrame(name);
		}
	}

	auto pin_key = [](EDirect direct, const PinMeta& meta) {
		return std::to_string(static_cast<int>(direct)) + "/"
			+ std::to_string(static_cast<int>(meta.kind)) + "/" + meta.name;
	};

	std::unordered_map<std::string, std::shared_ptr<APin>> old;
	auto take_old = [&](const auto& pins, EDirect direct) {
		for (const auto& [id, pin] : pins) {
			(void)id;
			if (!pin) continue;
			auto meta = pin->GetPinMeta();
			if (!renamed_from.empty() && meta.name == renamed_from)
				meta.name = renamed_to;
			old[pin_key(direct, meta)] = pin;
		}
	};
	take_old(in_pins_, EDirect::In);
	take_old(out_pins_, EDirect::Out);

	auto self = std::static_pointer_cast<ANode>(shared_from_this());
	auto make_pin = [&](const PinMeta& pin_meta, EDirect direct) -> std::shared_ptr<APin> {
		std::shared_ptr<APin> pin;
		if (pin_meta.kind == PinKind::Data) {
			auto data_pin = std::make_shared<VDataPin>(self, direct);
			data_pin->SetFrame(context_->GetFrame(pin_meta.name));
			pin = data_pin;
		} else {
			pin = std::make_shared<VExecPin>(self, direct);
		}
		pin->SetPinMeta(pin_meta);
		if (pin_meta.kind == PinKind::Data && direct == EDirect::In && pin_meta.value.empty()) {
			if (auto frame = pin->GetFrame()) {
				auto meta = pin->GetPinMeta();
				meta.value = ToString(frame);
				pin->SetPinMeta(meta);
			}
		}
		return pin;
	};

	std::unordered_set<APin*> kept;
	auto fill = [&](const std::vector<PinMeta>& metas, EDirect direct)
		-> std::unordered_map<uint16, std::shared_ptr<APin>> {
		std::unordered_map<uint16, std::shared_ptr<APin>> next;
		uint16 index = 0;
		for (const auto& pin_meta : metas) {
			std::shared_ptr<APin> pin;
			if (auto it = old.find(pin_key(direct, pin_meta)); it != old.end() && it->second) {
				pin = it->second;
				const auto old_meta = pin->GetPinMeta();
				const auto* old_type = old_meta.type;
				auto next_meta = pin_meta;
				if (pin_meta.kind == PinKind::Data && direct == EDirect::In
					&& old_type == pin_meta.type && next_meta.value.empty())
					next_meta.value = old_meta.value;
				pin->SetPinMeta(next_meta);
				if (pin_meta.kind == PinKind::Data)
					pin->SetFrame(context_->GetFrame(pin_meta.name));
				if (old_type && pin_meta.type && old_type != pin_meta.type)
					DisconnectPin(pin);
				if (pin_meta.kind == PinKind::Data && direct == EDirect::In && next_meta.value.empty()) {
					if (auto frame = pin->GetFrame()) {
						next_meta.value = ToString(frame);
						pin->SetPinMeta(next_meta);
					}
				}
			} else {
				pin = make_pin(pin_meta, direct);
			}
			if (pin)
				kept.insert(pin.get());
			next[index++] = pin;
		}
		return next;
	};

	auto next_in = fill(layout.inputs, EDirect::In);
	auto next_out = fill(layout.outputs, EDirect::Out);

	for (const auto& [key, pin] : old) {
		(void)key;
		if (pin && !kept.count(pin.get()))
			DisconnectPin(pin);
	}

	in_pins_ = std::move(next_in);
	out_pins_ = std::move(next_out);
}
