#pragma once

#include "IRouter.h"

#include <string>

namespace CG::Router {

	struct ROUTER_API HostConfig {
		std::string generated_root;
		std::string core_include;
		std::string core_lib;
		std::string type_out_dir;
		std::string asset_root;
		std::string cmake_command;
		std::string cmake_generator;
		std::string cmake_platform;
		std::string build_config;
	};

	// Fills core-owned build paths. Host fields (type_out_dir, asset_root) remain empty.
	ROUTER_API HostConfig MakeCoreHostConfig();

	// Host entry for the editor business layer.
	ROUTER_API void InitHost(const HostConfig& config);
	ROUTER_API void ShutdownHost();
	ROUTER_API IRouter& GetRouter();

} // namespace sys
