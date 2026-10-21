#include "Realistic/VPin.h"
#include "Abstract/ALine.h"
#include "Abstract/ANode.h"

using namespace CG::Core;

CG::Core::VExecPin::VExecPin(const std::shared_ptr<ANode>& node, EDirect direct) : APin(node,direct)
{
}

void CG::Core::VExecPin::BindLine(std::shared_ptr<ALine> line)
{
	if (!line) return;
	line_ = line;

	auto pin = static_pointer_cast<APin>(shared_from_this());
	GetDirect() == EDirect::Out ? line->SetFrom(pin) : line->SetTo(pin);
}

void CG::Core::VExecPin::UnbindLine(std::shared_ptr<ALine> line)
{
	if (!line || line_ != line) return;
	if (GetDirect() == EDirect::In)
		line->SetTo({});
	else
		line->SetFrom({});
	line_.reset();
}

std::shared_ptr<ALine> CG::Core::VExecPin::GetLine() const
{
	return line_;
}

void CG::Core::VExecPin::SetFrame(const std::shared_ptr<AFrame>& frame)
{
}

std::shared_ptr<AFrame> CG::Core::VExecPin::GetFrame() const
{
	return nullptr;
}

void CG::Core::VExecPin::Update()
{
}

CG::Core::VDataPin::VDataPin(const std::shared_ptr<ANode>& node, EDirect direct) : APin(node,direct)
{
}

void CG::Core::VDataPin::BindLine(std::shared_ptr<ALine> line)
{
	if(!line) return;

	auto direct = GetDirect();
	if(direct == EDirect::In && lines_.size() != 0)
		return;

	auto pin = static_pointer_cast<APin>(shared_from_this());
	direct == EDirect::In ? line->SetTo(pin) : line->SetFrom(pin);
	lines_.insert(line);
}

void CG::Core::VDataPin::UnbindLine(std::shared_ptr<ALine> line)
{
	if(!line) return;
	if (auto it = lines_.find(line); it != lines_.end()) {
		auto direct = GetDirect();
		direct == EDirect::In ? line->SetTo({}) : line->SetFrom({});
		lines_.erase(it);
	}

}

std::shared_ptr<ALine> CG::Core::VDataPin::GetLine() const
{
	std::shared_ptr<ALine> line = nullptr;
	if(GetDirect() == EDirect::In && lines_.size() == 1)
		line = *lines_.begin();
	return line;
}

std::vector<std::shared_ptr<ALine>> CG::Core::VDataPin::GetLines() const
{
	return { lines_.begin(), lines_.end() };
}

void CG::Core::VDataPin::SetFrame(const std::shared_ptr<AFrame>& frame)
{
	data_ = frame;
}

std::shared_ptr<AFrame> CG::Core::VDataPin::GetFrame() const
{
	return data_.lock();
}

void CG::Core::VDataPin::Update()
{
	do 
	{
		if (GetDirect() != EDirect::In)
			break;

		auto line = GetLine();
		if(!line) break;

		auto from_pin = line->GetFrom();
		if(!from_pin) break;

		auto from_node = from_pin->GetOwner();
		if(!from_node) break;

		auto pure = from_node->IsPure();
		if(!pure.has_value()) break;
		if(pure.value()) from_node->Work();

		auto from_frame = from_pin->GetFrame();
		if(!from_frame) break;

		auto frame = GetFrame();
		if(!frame) break;

		frame->Update(from_frame);
	} while (0);
}

//void CG::Core::VDataExecPin::SetFrame(AFrame* frame)
//{
//	data_ = frame;
//}
//
//AFrame* CG::Core::VDataExecPin::GetFrame() const
//{
//	return data_;
//}
//
//void CG::Core::VDataExecPin::Update()
//{
//	do
//	{
//		if (GetDirect() != EDirect::In)
//			break;
//		auto line = GetLine();
//		if (!line) break;
//
//		auto from_pin = line->GetFrom();
//		if (!from_pin) break;
//
//		auto from_frame = from_pin->GetFrame();
//		if (!from_frame) break;
//
//		auto frame = GetFrame();
//		if (!frame) break;
//
//		frame->Update(from_frame);
//	} while (0);
//}
