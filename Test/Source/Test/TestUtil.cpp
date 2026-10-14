#include "Test/TestUtil.h"

#include "Abstract/AFlow.h"
#include "Abstract/AFrame.h"
#include "Base/VClass.h"
#include "Realistic/VContext.h"
#include "Realistic/VFunction.h"
#include "Realistic/VGraph.h"
#if __has_include("user/VTest.gen.h")
#include "user/VTest.gen.h"
#endif
#include "Realistic/VLine.h"
#include "Realistic/VNode.h"
#include "Realistic/VOperator.h"
#include "Base/VRegister.h"
#include "CodeGen/CGraph.h"

#include <fstream>
#include <iostream>
#include <sstream>

using namespace CG::Core;
using namespace CG::Core::cg;

namespace test {

namespace {

std::filesystem::path ProjectRoot()
{
	// Test/Source/Test/*.cpp -> ClassGraph/
	return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path();
}

bool WriteTextFile(const std::filesystem::path& path, const std::string& content)
{
	std::error_code ec;
	std::filesystem::create_directories(path.parent_path(), ec);
	std::ofstream out(path, std::ios::binary);
	if (!out) return false;
	out << content;
	return out.good();
}

std::filesystem::path BlueprintOutputDir(const std::shared_ptr<VBlueprint>& blueprint)
{
	return ProjectRoot() / "Core" / "Include" / blueprint->GetSpace();
}

const VType* IntType()
{
	return VTypeRegister::Find("int");
}

const VType* BoolType()
{
	return VTypeRegister::Find("bool");
}

} // namespace

CoutCapture::CoutCapture()
{
	old_ = std::cout.rdbuf(buf_.rdbuf());
}

CoutCapture::~CoutCapture()
{
	if (old_)
		std::cout.rdbuf(old_);
}

std::string CoutCapture::Str() const
{
	return buf_.str();
}

std::shared_ptr<ANode> MakeNode(const std::shared_ptr<ACaller>& caller)
{
	if (!caller) return {};
	return VNode::Make(caller);
}

void Connect(const std::shared_ptr<APin>& out, const std::shared_ptr<APin>& in)
{
	if (!out || !in) return;
	auto line = std::make_shared<VLine>();
	out->BindLine(line);
	in->BindLine(line);
}

void ConnectArg(const std::shared_ptr<ANode>& consumer, const std::string& arg_name,
	const std::shared_ptr<APin>& producer_out)
{
	if (!consumer || !producer_out) return;
	auto pin = FindInPin(consumer, arg_name);
	Connect(producer_out, pin);
}

void ConnectOutArg(const std::shared_ptr<ANode>& consumer, const std::string& in_arg,
	const std::shared_ptr<ANode>& producer, const std::string& out_arg)
{
	Connect(FindOutPin(producer, out_arg), FindInPin(consumer, in_arg));
}

std::shared_ptr<ANode> MakeEntryNode(
	std::initializer_list<std::pair<const char*, const char*>> params)
{
	auto entry = std::make_shared<FEntry>();
	for (const auto& [name, type_name] : params) {
		const VType* type = VTypeRegister::Find(type_name);
		if (!type || !entry->AddParam(name, type))
			return {};
	}
	return MakeNode(entry);
}

std::shared_ptr<ANode> MakeExitNode(
	std::initializer_list<std::pair<const char*, const char*>> params)
{
	auto exit = std::make_shared<FExit>();
	for (const auto& [name, type_name] : params) {
		const VType* type = VTypeRegister::Find(type_name);
		if (!type || !exit->AddParam(name, type))
			return {};
	}
	return MakeNode(exit);
}

bool Contains(const std::string& text, const std::string& needle)
{
	return text.find(needle) != std::string::npos;
}

bool RunChecks(const char* suite, const std::vector<Check>& checks)
{
	for (const auto& c : checks) {
		const bool found = Contains(c.text, c.needle);
		if (found != c.should_contain) {
			std::cerr << suite << " VERIFY FAILED: " << c.name;
			if (c.should_contain)
				std::cerr << " - missing \"" << c.needle << "\"\n";
			else
				std::cerr << " - should not contain \"" << c.needle << "\"\n";
			return false;
		}
		std::cout << suite << " OK: " << c.name << '\n';
	}
	return true;
}

bool VerifyGeneratedBodyMacro(const char* suite, const std::string& class_name,
	const std::string& header, const std::string& gen_header)
{
	const std::string file_id = class_name + "_h";
	int line = 0;
	bool found = false;
	std::istringstream in(header);
	std::string row;
	while (std::getline(in, row)) {
		++line;
		if (row.find("GENERATED_BODY()") != std::string::npos) {
			found = true;
			break;
		}
	}
	if (!found) {
		std::cerr << suite << " VERIFY FAILED: GENERATED_BODY() not found in header\n";
		return false;
	}

	const std::string macro = "#define " + file_id + "_" + std::to_string(line) + "_GENERATED_BODY";
	if (!Contains(gen_header, macro)) {
		std::cerr << suite << " VERIFY FAILED: missing macro \"" << macro << "\" in gen.h\n";
		return false;
	}
	std::cout << suite << " OK: GENERATED_BODY line " << line << " matches " << macro << '\n';
	return true;
}

bool GenerateAll(const char* suite, const std::shared_ptr<VBlueprint>& blueprint, GeneratedCode& out)
{
	if (blueprint->GenerateCode(out.source, out.header, out.gen_header, out.gen_source) != 0) {
		std::cerr << suite << ": GenerateCode failed\n";
		return false;
	}
	return true;
}

bool WriteAllOutputs(const std::shared_ptr<VBlueprint>& blueprint, const GeneratedCode& code)
{
	const auto out_dir = BlueprintOutputDir(blueprint);
	const std::string class_name = blueprint->GetName();
	return WriteTextFile(out_dir / (class_name + ".h"), code.header)
		&& WriteTextFile(out_dir / (class_name + ".gen.h"), code.gen_header)
		&& WriteTextFile(out_dir / (class_name + ".cpp"), code.source)
		&& WriteTextFile(out_dir / (class_name + ".gen.cpp"), code.gen_source);
}

bool SetNodeInt(const std::shared_ptr<ANode>& node, const std::string& frame_name, int value)
{
	if (!node || !node->GetContext()) return false;
	auto frame = node->GetContext()->GetFrame(frame_name);
	if (!frame) return false;
	const VType* type = IntType();
	return type && frame->SetValue(type, &value);
}

bool SetNodeBool(const std::shared_ptr<ANode>& node, const std::string& frame_name, bool value)
{
	if (!node || !node->GetContext()) return false;
	auto frame = node->GetContext()->GetFrame(frame_name);
	if (!frame) return false;
	const VType* type = BoolType();
	return type && frame->SetValue(type, &value);
}

bool GetVarInt(const std::shared_ptr<VVariant>& var, int& out)
{
	if (!var) return false;
	auto frame = var->Getter();
	if (!frame) return false;
	const VType* type = IntType();
	return type && frame->GetValue(type, &out);
}

bool SetVarInt(const std::shared_ptr<VVariant>& var, int value)
{
	if (!var) return false;
	auto frame = var->Getter();
	if (!frame) return false;
	const VType* type = IntType();
	return type && frame->SetValue(type, &value);
}

bool RunFlowOk(const char* suite, const char* label, const std::shared_ptr<AFlow>& flow,
	const std::string& token)
{
	if (!flow) {
		std::cerr << suite << " Run failed: " << label << " (null flow)\n";
		return false;
	}
	if (flow->Run(token) != 0) {
		std::cerr << suite << " Run failed: " << label << '\n';
		return false;
	}
	return true;
}

VClass* LoadCompiledVTest()
{
#if __has_include("user/VTest.gen.h")
	return ::Z_Construct_VClass_VTest();
#else
	return nullptr;
#endif
}

std::shared_ptr<ANode> MakeMethodNode(VClass* cls, const char* method_name)
{
	if (!cls || !method_name) return {};
	auto method = cls->GetMethod(method_name);
	if (!method) return {};
	return MakeNode(std::static_pointer_cast<ACaller>(method));
}

} // namespace test
