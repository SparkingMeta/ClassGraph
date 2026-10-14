#pragma once

#include "Session.h"

#include "Base/VClass.h"
#include "Base/VSystem.h"
#include "Base/VType.h"
#include "Base/VGuid.h"
#include "Realistic/VAsset.h"
#include "Realistic/VFlow.h"
#include "Realistic/VGraph.h"
#include "Realistic/VNode.h"
#include "Realistic/VOperator.h"
#include "Realistic/VVariant.h"
#include "Realistic/VVarFunction.h"
#include "Realistic/VLine.h"
#include "Realistic/VController.h"
#include "Abstract/AContext.h"
#include "Abstract/ACaller.h"
#include "Abstract/AFlow.h"
#include "Abstract/AFrame.h"
#include "Abstract/AFunction.h"
#include "Abstract/AScript.h"
#include "Abstract/APin.h"
#include "Abstract/ALine.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <format>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace CG::Router {

	using CG::Core::VType;
	using CG::Core::AFrame;
	using CG::Core::APin;
	using CG::Core::VNode;
	using CG::Core::VBlueprint;
	using CG::Core::AFlow;
	using CG::Core::CallMeta;

	namespace fs = std::filesystem;

	std::string Trim(std::string s);
	void TryRemoveFile(const fs::path& path);
	std::string KindLabel(int kind);
	EditKind EditKindOf(const VType* type);
	std::string ReadFrameText(const std::shared_ptr<AFrame>& frame);
	bool WriteFrameText(const std::shared_ptr<AFrame>& frame, const std::string& text);
	bool PinConnected(const std::shared_ptr<APin>& pin);
	bool PinEditable(const std::shared_ptr<APin>& pin);
	bool IsCaseLiteralPin(const std::shared_ptr<APin>& pin);
	std::shared_ptr<AFrame> PinValueFrame(const std::shared_ptr<APin>& pin);
	Pin BuildPinDto(const std::shared_ptr<APin>& pin);
	nlohmann::json ToJson(const Pin& p);
	nlohmann::json ToJson(const Node& n);
	nlohmann::json ToJson(const Line& l);
	nlohmann::json ToJson(const Graph& g);
	nlohmann::json ToJson(const Blueprint& b);
	nlohmann::json ToJson(const Palette& p);
	nlohmann::json ToJson(const NodeDetails& d);
	nlohmann::json ToJson(const VarDetails& d);
	std::string DumpNode(const Node& n, const std::string& flow);
	std::string DumpLine(const Line& l, const std::string& flow);
	std::string QuoteArg(const std::string& s);
	bool RunProcess(const std::string& exe, const std::vector<std::string>& args,
		std::string& output, int& exit_code);
	std::string UniqueParamName(const CallMeta& meta, const std::string& base);
	void InitSwitchCaseValue(const std::shared_ptr<VNode>& node, const std::string& new_name);
	void SyncScriptCallers(const std::shared_ptr<VBlueprint>& bp, const std::shared_ptr<AFlow>& callee,
		const std::string& renamed_from = {}, const std::string& renamed_to = {});
	nlohmann::json Body(const Message& msg);

}
