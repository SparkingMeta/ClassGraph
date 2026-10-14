#pragma once

#include "Abstract/ACaller.h"
#include "Abstract/ANode.h"
#include "Abstract/APin.h"
#include "Realistic/VAsset.h"
#include "Realistic/VVariant.h"

#include <filesystem>
#include <initializer_list>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace CG::Core {
	class AFlow;
	class VClass;
}

namespace test {

struct Check {
	const char* name;
	const char* needle;
	const std::string& text;
	bool should_contain{ true };
};

struct GeneratedCode {
	std::string header;
	std::string gen_header;
	std::string source;
	std::string gen_source;
};

class CoutCapture {
public:
	CoutCapture();
	~CoutCapture();
	std::string Str() const;

private:
	std::stringstream buf_;
	std::streambuf* old_{ nullptr };
};

std::shared_ptr<CG::Core::ANode> MakeNode(const std::shared_ptr<CG::Core::ACaller>& caller);

void Connect(const std::shared_ptr<CG::Core::APin>& out, const std::shared_ptr<CG::Core::APin>& in);

void ConnectArg(const std::shared_ptr<CG::Core::ANode>& consumer, const std::string& arg_name,
	const std::shared_ptr<CG::Core::APin>& producer_out);

void ConnectOutArg(const std::shared_ptr<CG::Core::ANode>& consumer, const std::string& in_arg,
	const std::shared_ptr<CG::Core::ANode>& producer, const std::string& out_arg);

std::shared_ptr<CG::Core::ANode> MakeEntryNode(
	std::initializer_list<std::pair<const char*, const char*>> params = {});

std::shared_ptr<CG::Core::ANode> MakeExitNode(
	std::initializer_list<std::pair<const char*, const char*>> params = {});

bool Contains(const std::string& text, const std::string& needle);

bool RunChecks(const char* suite, const std::vector<Check>& checks);

bool VerifyGeneratedBodyMacro(const char* suite, const std::string& class_name,
	const std::string& header, const std::string& gen_header);

bool GenerateAll(const char* suite, const std::shared_ptr<CG::Core::VBlueprint>& blueprint, GeneratedCode& out);

bool WriteAllOutputs(const std::shared_ptr<CG::Core::VBlueprint>& blueprint, const GeneratedCode& code);

bool SetNodeInt(const std::shared_ptr<CG::Core::ANode>& node, const std::string& frame_name, int value);

bool SetNodeBool(const std::shared_ptr<CG::Core::ANode>& node, const std::string& frame_name, bool value);

bool GetVarInt(const std::shared_ptr<CG::Core::VVariant>& var, int& out);

bool SetVarInt(const std::shared_ptr<CG::Core::VVariant>& var, int value);

bool RunFlowOk(const char* suite, const char* label, const std::shared_ptr<CG::Core::AFlow>& flow,
	const std::string& token = "test");

CG::Core::VClass* LoadCompiledVTest();

std::shared_ptr<CG::Core::ANode> MakeMethodNode(CG::Core::VClass* cls, const char* method_name);

} // namespace test
