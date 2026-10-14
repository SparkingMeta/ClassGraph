#include "VApi.h"
#include "VRouter.h"
#include "Session.h"
#include "IRouter.h"
#include "Base/VSystem.h"
#include "classgraph_core_paths.h"

#include <memory>
#include <mutex>
#include <stdexcept>

namespace CG::Router {
	using namespace CG::Core;
	std::mutex g_mutex;
	std::unique_ptr<VRouter> g_router;
	std::shared_ptr<Session> g_session;
	bool g_inited = false;

	HostConfig MakeCoreHostConfig()
	{
		HostConfig cfg;
		cfg.generated_root = CLASSGRAPH_GENERATED_ROOT;
		cfg.core_include = CLASSGRAPH_CORE_INCLUDE;
		cfg.core_lib = CLASSGRAPH_CORE_LIB;
		cfg.cmake_command = CLASSGRAPH_CMAKE_COMMAND;
		cfg.cmake_generator = CLASSGRAPH_CMAKE_GENERATOR;
		cfg.cmake_platform = CLASSGRAPH_CMAKE_PLATFORM;
		cfg.build_config = CLASSGRAPH_BUILD_CONFIG;
		return cfg;
	}

	void InitHost(const HostConfig& config)
	{
		std::lock_guard lock(g_mutex);
		if (g_inited)
			return;
		VSystem::Get().Init();
		g_session = std::make_shared<Session>(config);
		g_router = std::make_unique<VRouter>(g_session);
		g_session->SetRise([](const Message& msg) {
			if (g_router)
				g_router->Rise(msg);
		});
		VSystem::Get().SetPrinter([](const std::string& text) {
			if (g_router)
				g_router->Rise(MakeMessage(Action::Print, {}, text));
		});
		g_inited = true;
	}

	void ShutdownHost()
	{
		std::lock_guard lock(g_mutex);
		if (!g_inited)
			return;
		VSystem::Get().SetPrinter(nullptr);
		if (g_session)
			g_session->SetRise(nullptr);
		g_router.reset();
		g_session.reset();
		g_inited = false;
	}

	IRouter& GetRouter()
	{
		if (!g_router)
			throw std::runtime_error("GetRouter: host not initialized");
		return *g_router;
	}
}