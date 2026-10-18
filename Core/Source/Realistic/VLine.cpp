#include "Realistic/VLine.h"
#include "Abstract/APin.h"

#include <memory>
#include <utility>

using namespace CG::Core;

std::shared_ptr<APin> CG::Core::VLine::GetFrom() const
{
	return from_.lock();
}

std::shared_ptr<APin> CG::Core::VLine::GetTo() const
{
	return to_.lock();
}

void CG::Core::VLine::SetFrom(std::shared_ptr<APin> from)
{
	from_ = from;
}

void CG::Core::VLine::SetTo(std::shared_ptr<APin> to)
{
	to_ = to;
}

void CG::Core::DisconnectLine(const std::shared_ptr<ALine>& line)
{
	if (!line) return;
	if (auto from = line->GetFrom())
		from->UnbindLine(line);
	if (auto to = line->GetTo())
		to->UnbindLine(line);
}

void CG::Core::DisconnectPin(const std::shared_ptr<APin>& pin)
{
	if (!pin) return;
	auto lines = pin->GetLines();
	for (const auto& line : lines)
		DisconnectLine(line);
}

std::shared_ptr<ALine> CG::Core::MakeLine(const std::shared_ptr<APin>& from_in, const std::shared_ptr<APin>& to_in)
{
	auto from = from_in;
	auto to = to_in;
	if (!from || !to || from == to)
		return {};

	if (from->GetDirect() == EDirect::In && to->GetDirect() == EDirect::Out)
		std::swap(from, to);

	if (from->GetDirect() != EDirect::Out || to->GetDirect() != EDirect::In)
		return {};
	if (from->GetOwner() && to->GetOwner() && from->GetOwner() == to->GetOwner())
		return {};

	const auto& from_meta = from->GetPinMeta();
	const auto& to_meta = to->GetPinMeta();
	if (from_meta.kind != to_meta.kind)
		return {};
	if (from_meta.kind == PinKind::Data && from_meta.type && to_meta.type && from_meta.type != to_meta.type)
		return {};

	if (from_meta.kind == PinKind::Exec)
		DisconnectPin(from);
	DisconnectPin(to);

	auto line = std::make_shared<VLine>();
	line->SetFrom(from);
	line->SetTo(to);
	from->BindLine(line);
	to->BindLine(line);
	return line->GetFrom() && line->GetTo() ? line : std::shared_ptr<ALine>{};
}

bool CG::Core::ConnectPins(const std::shared_ptr<APin>& from, const std::shared_ptr<APin>& to)
{
	return static_cast<bool>(MakeLine(from, to));
}
