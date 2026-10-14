#include "VRouter.h"

using namespace CG::Router;

VRouter::VRouter(std::shared_ptr<Service> service)
	: service_(std::move(service))
{
}

VRouter::~VRouter() = default;

void VRouter::DoAction(const Message& msg)
{
	if (service_)
		service_->Handle(msg, nullptr);
}

void VRouter::SetRise(std::function<void(const Message&)> rise)
{
	rise_ = std::move(rise);
}

void VRouter::Rise(const Message& msg)
{
	if (rise_)
		rise_(msg);
}