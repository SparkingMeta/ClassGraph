#pragma once

#include "IRouter.h"
#include "service.h"

#include <functional>
#include <memory>

namespace CG::Router {

	class ROUTER_API VRouter : public IRouter {
	public:
		explicit VRouter(std::shared_ptr<Service> service);
		~VRouter() override;

		void DoAction(const Message& msg) override;
		void SetRise(std::function<void(const Message&)> rise) override;

		void Rise(const Message& msg);

	private:
		std::shared_ptr<Service> service_;
		std::function<void(const Message&)> rise_;
	};

}