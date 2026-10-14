#pragma once

#include "VDto.h"

#include <string>

namespace CG::Router {

	ROUTER_API bool ParseGraph(const std::string& json, Graph& out);
	ROUTER_API bool ParseNodeDto(const std::string& json, Node& out);
	ROUTER_API bool ParseLineDto(const std::string& json, Line& out);
	ROUTER_API bool ParseBlueprint(const std::string& json, Blueprint& out);
	ROUTER_API bool ParsePalette(const std::string& json, Palette& out);
	ROUTER_API bool ParseNode(const std::string& json, NodeDetails& out);
	ROUTER_API bool ParseVar(const std::string& json, VarDetails& out);
	ROUTER_API bool ParseAssets(const std::string& json, AssetList& out);

} // namespace sys
