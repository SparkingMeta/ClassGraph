#include "SessionInternal.h"

namespace CG::Router {
using namespace CG::Core;

void Session::RegisterType()
{
	handlers_.Subscribe(Action::GenerateCode, Handler<Message>(std::bind(&Session::GenerateCode, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::CompileType, Handler<Message>(std::bind(&Session::CompileType, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::LoadType, Handler<Message>(std::bind(&Session::LoadType, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::UnloadType, Handler<Message>(std::bind(&Session::UnloadType, this, std::placeholders::_1)));
	handlers_.Subscribe(Action::GetPalette, Handler<Message>(std::bind(&Session::GetPalette, this, std::placeholders::_1)));
}

void Session::GenerateCode(const Message& msg)
{
	int code = -1;
	std::string desc{};
	std::string data{};
	do
	{
		if (!blueprint_) {
			desc = "GenerateCode: no blueprint";
			break;
		}

		std::string source, header, gen_header, meta;
		if (blueprint_->GenerateCode(source, header, gen_header, meta) != 0) {
			desc = "Compile failed: GenerateCode";
			break;
		}

		std::string space = blueprint_->GetSpace();
		if (space.empty())
			space = "user";
		const std::string name = blueprint_->GetName();
		const fs::path src_dir = fs::path(config_.generated_root) / space;
		std::error_code ec;
		fs::create_directories(src_dir, ec);
		if (ec) {
			desc = "Cannot create source dir: " + src_dir.string();
			break;
		}

		const fs::path stem = src_dir / name;
		if (!WriteFile((stem.string() + ".h"), header)
			|| !WriteFile((stem.string() + ".gen.h"), gen_header)
			|| !WriteFile((stem.string() + ".cpp"), source)
			|| !WriteFile((stem.string() + ".gen.cpp"), meta)) {
			desc = "Failed to write generated sources";
			break;
		}
		Print("Wrote sources: " + stem.string() + "/{h,cpp,gen.h,gen.cpp}");

		if (!Compile()) {
			desc = "GenerateCode: compile failed";
			break;
		}

		code = 0;
		desc = "GenerateCode succeeded";
		data = nlohmann::json({{"guid", blueprint_->GetGuid()}}).dump();
	} while (0);

	Print(desc);
	if(code == 0) Reply(msg.act, msg.token, data);
}

void Session::CompileType(const Message& msg)
{
	int code = -1;
	std::string desc{};
	std::string data{};
	do
	{
		if (!Compile()) {
			desc = "CompileType failed";
			break;
		}
		code = 0;
		desc = "CompileType succeeded";
		if (blueprint_)
			data = nlohmann::json({{"guid", blueprint_->GetGuid()}}).dump();
	} while (0);

	Print(desc);
	if(code == 0) Reply(msg.act, msg.token, data);
}

void Session::LoadType(const Message& msg)
{
	int code = -1;
	std::string desc{};
	std::string data{};
	do
	{
		const auto body = Body(msg);
		const std::string path = JsonStr(body, "path");
		if (path.empty()) {
			desc = "LoadType: empty path";
			break;
		}
		auto* cls = VSystem::Get().LoadDll(path);
		if (!cls) {
			desc = "LoadType failed: " + path;
			break;
		}

		code = 0;
		desc = "Loaded type " + cls->GetSpace() + "::" + cls->GetName();
		data = nlohmann::json({{"guid", cls->GetSpace() + "::" + cls->GetName()}}).dump();
	} while (0);

	Print(desc);
	if(code == 0) Reply(msg.act, msg.token, data);
}

void Session::UnloadType(const Message& msg)
{
	int code = -1;
	std::string desc{};
	std::string data{};
	do
	{
		std::string key = JsonStr(Body(msg), "guid");
		if (key.empty())
			key = SelfType();
		if (key.empty()) {
			desc = "UnloadType: empty class key";
			break;
		}
		VSystem::Get().UnloadDll(key);

		code = 0;
		desc = "Unloaded type " + key;
		data = nlohmann::json({{"guid", key}}).dump();
	} while (0);

	Print(desc);
	if(code == 0) Reply(msg.act, msg.token, data);
}

void Session::GetPalette(const Message& msg)
{
	Reply(msg.act, msg.token, ToJson(MakePalette()).dump());
}

Palette Session::MakePalette() const
{
	Palette dto;
	for (const auto& caller : VSystem::Get().ListCallers()) {
		if (!caller)
			continue;
		CallerItem item;
		item.key = caller->GetName();
		item.name = caller->GetName();
		item.group = KindLabel(caller->GetKind());
		item.kind = caller->GetKind();
		item.is_pure = caller->IsPure();
		dto.items.push_back(std::move(item));
	}

	{
		CallerItem item;
		item.key = "exit";
		item.name = "Return";
		item.group = "Flow Control";
		item.kind = CK_Controller;
		dto.items.push_back(std::move(item));
	}

	if (blueprint_) {
		for (const auto& [guid, flow] : blueprint_->GetFlows()) {
			if (!flow || guid == current_flow_guid_)
				continue;
			CallerItem item;
			item.key = "flow:" + guid;
			item.name = flow->GetName();
			item.group = "My Blueprint";
			item.kind = CK_Script;
			dto.items.push_back(std::move(item));
		}
	}

	for (VClass* cls : VSystem::Get().GetClasses()) {
		if (!cls)
			continue;
		auto methods = cls->GetMethods();
		if (methods.empty())
			continue;
		const std::string class_key = cls->GetSpace() + "::" + cls->GetName();
		for (const auto& method : methods) {
			if (!method)
				continue;
			CallerItem item;
			item.key = "method:" + class_key + "/" + method->GetName();
			item.name = method->GetName();
			item.group = class_key;
			item.kind = method->GetKind();
			item.is_pure = method->IsPure();
			dto.items.push_back(std::move(item));
		}
	}
	return dto;
}

bool Session::Compile()
{
	if (!blueprint_)
		return false;

	std::string space = blueprint_->GetSpace();
	if (space.empty())
		space = "user";
	const std::string name = blueprint_->GetName();
	const std::string class_key = space + "::" + name;

	const fs::path src_dir = fs::path(config_.generated_root) / space;
	const fs::path type_out = fs::path(config_.type_out_dir);
	const fs::path work = type_out / name;
	const fs::path build_dir = work / "build";
	const fs::path dll_path = type_out / (name + ".dll");

	VSystem::Get().UnloadDll(class_key);
#ifdef _WIN32
	Sleep(80);
#endif
	TryRemoveFile(dll_path);
	TryRemoveFile(type_out / (name + ".pdb"));
	TryRemoveFile(type_out / (name + ".ilk"));
	TryRemoveFile(type_out / (name + ".exp"));
	std::error_code ec;
	fs::create_directories(work, ec);
	fs::create_directories(type_out, ec);
	if (ec) {
		Print("Cannot create type output dir: " + type_out.string());
		return false;
	}

	const std::string src_dir_s = src_dir.generic_string();
	const std::string core_include = fs::path(config_.core_include).generic_string();
	const std::string core_lib = fs::path(config_.core_lib).generic_string();
	const std::string type_out_s = type_out.generic_string();
	const std::string self = SelfType();

	std::unordered_set<std::string> dep_keys;
	auto add_dep = [&](const VType* type) {
		if (!type || type->name_.empty() || type->name_ == self)
			return;
		if (!VSystem::Get().FindClass(type->name_))
			return;
		dep_keys.insert(type->name_);
	};
	for (const auto& [guid, var] : blueprint_->GetVars()) {
		if (var)
			add_dep(var->GetType());
	}
	for (const auto& [guid, flow] : blueprint_->GetFlows()) {
		if (!flow)
			continue;
		for (const auto& param : flow->GetFlowMeta().params)
			add_dep(param.type);
	}

	std::ostringstream link_libs;
	link_libs << "\"" << core_lib << "\"";
	for (const auto& key : dep_keys) {
		const auto pos = key.rfind("::");
		const std::string dep_name = (pos == std::string::npos) ? key : key.substr(pos + 2);
		const fs::path lib = type_out / (dep_name + ".lib");
		if (!fs::exists(lib)) {
			Print("Type import lib not found: " + lib.string());
			return false;
		}
		link_libs << " \"" << lib.generic_string() << "\"";
	}

#ifdef _WIN32
	const fs::path pdb_path = build_dir / (name + "_" + std::to_string(GetTickCount64()) + ".pdb");
#else
	const fs::path pdb_path = build_dir / (name + ".pdb");
#endif

	std::ostringstream cmake;
	cmake
		<< "cmake_minimum_required(VERSION 3.15)\n"
		<< "project(" << name << " LANGUAGES CXX)\n"
		<< "set(CMAKE_CXX_STANDARD 20)\n"
		<< "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n"
		<< "set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)\n"
		<< "add_library(" << name << " SHARED \"" << src_dir_s << "/" << name << ".cpp\" \""
		<< src_dir_s << "/" << name << ".gen.cpp\")\n"
		<< "target_include_directories(" << name << " PRIVATE \"" << core_include << "\" \""
		<< src_dir_s << "\")\n"
		<< "target_link_libraries(" << name << " PRIVATE " << link_libs.str() << ")\n"
		<< "if(MSVC)\n"
		<< "  target_compile_options(" << name << " PRIVATE /utf-8 /EHsc)\n"
		<< "  target_link_options(" << name << " PRIVATE /INCREMENTAL:NO \"/PDB:"
		<< pdb_path.generic_string() << "\")\n"
		<< "endif()\n"
		<< "set_target_properties(" << name << " PROPERTIES\n"
		<< "  RUNTIME_OUTPUT_DIRECTORY \"" << type_out_s << "\"\n"
		<< "  RUNTIME_OUTPUT_DIRECTORY_DEBUG \"" << type_out_s << "\"\n"
		<< "  RUNTIME_OUTPUT_DIRECTORY_RELEASE \"" << type_out_s << "\"\n"
		<< "  RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO \"" << type_out_s << "\"\n"
		<< "  RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL \"" << type_out_s << "\"\n"
		<< "  LIBRARY_OUTPUT_DIRECTORY \"" << type_out_s << "\"\n"
		<< "  LIBRARY_OUTPUT_DIRECTORY_DEBUG \"" << type_out_s << "\"\n"
		<< "  LIBRARY_OUTPUT_DIRECTORY_RELEASE \"" << type_out_s << "\"\n"
		<< "  LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO \"" << type_out_s << "\"\n"
		<< "  LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL \"" << type_out_s << "\"\n"
		<< "  ARCHIVE_OUTPUT_DIRECTORY \"" << type_out_s << "\"\n"
		<< "  ARCHIVE_OUTPUT_DIRECTORY_DEBUG \"" << type_out_s << "\"\n"
		<< "  ARCHIVE_OUTPUT_DIRECTORY_RELEASE \"" << type_out_s << "\"\n"
		<< "  ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO \"" << type_out_s << "\"\n"
		<< "  ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL \"" << type_out_s << "\"\n"
		<< ")\n";

	if (!WriteFile((work / "CMakeLists.txt").string(), cmake.str())) {
		Print("Cannot write type project CMakeLists.txt");
		return false;
	}

#ifdef _WIN32
	if (!config_.cmake_generator.empty())
		_putenv_s("CMAKE_GENERATOR", config_.cmake_generator.c_str());
#endif

	auto run_cmake = [&](const std::vector<std::string>& args) -> bool {
		std::string out;
		int code = 1;
		const std::string exe = config_.cmake_command.empty() ? "cmake" : config_.cmake_command;
		if (!RunProcess(exe, args, out, code)) {
			Print("Cannot start cmake: " + exe);
			return false;
		}
		if (!out.empty())
			Print(out);
		if (code != 0) {
			Print("cmake failed (code=" + std::to_string(code) + ")");
			return false;
		}
		return true;
	};

	std::vector<std::string> configure{
		"-S", work.string(),
		"-B", build_dir.string(),
	};
	if (!config_.cmake_generator.empty()) {
		configure.push_back("-G");
		configure.push_back(config_.cmake_generator);
	}
	if (!config_.cmake_platform.empty()) {
		configure.push_back("-A");
		configure.push_back(config_.cmake_platform);
	}
	if (!run_cmake(configure))
		return false;

	const std::string build_config = config_.build_config.empty() ? "Debug" : config_.build_config;
	std::vector<std::string> build_args{
		"--build", build_dir.string(),
		"--config", build_config,
		"--target", name,
	};
	if (!run_cmake(build_args))
		return false;

	if (!fs::exists(dll_path)) {
		Print("Type DLL not found: " + dll_path.string());
		return false;
	}

	auto* cls = VSystem::Get().LoadDll(dll_path.string());
	if (!cls) {
		Print("LoadDll failed: " + dll_path.string());
		return false;
	}
	Print("Loaded type " + cls->GetSpace() + "::" + cls->GetName());
	return true;
}

bool Session::WriteFile(const std::string& path, const std::string& text) const
{
	std::ofstream out(path, std::ios::binary);
	if (!out)
		return false;
	out << text;
	return out.good();
}


} // namespace CG::Router
