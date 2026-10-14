#pragma once

#include "VDto.h"

#include <string>

namespace CG::Router {

	ROUTER_API bool ParseGraphDto(const std::string& json, GraphDto& out);
	ROUTER_API bool ParseBlueprintDto(const std::string& json, BlueprintDto& out);
	ROUTER_API bool ParsePaletteDto(const std::string& json, PaletteDto& out);
	ROUTER_API bool ParseNodeDetailsDto(const std::string& json, NodeDetailsDto& out);
	ROUTER_API bool ParseVarDetailsDto(const std::string& json, VarDetailsDto& out);
	ROUTER_API bool ParseAssetListDto(const std::string& json, AssetListDto& out);

} // namespace sys
