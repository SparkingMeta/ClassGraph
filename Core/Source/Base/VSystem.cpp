#include "Base/VSystem.h"

#include "Base/VRegister.h"

#include "Base/VType.h"
#include "Base/VMeta.h"
#include "Base/VClass.h"
#include "Abstract/ACaller.h"
#include "Realistic/VFunction.h"
#include "Realistic/VOperator.h"
#include "Realistic/VController.h"

#ifdef _WIN32
#include <windows.h>
#endif

using namespace CG::Core;

namespace {

	VInt32 s_int_type("int");
	VBool s_bool_type("bool");
	VInt64 s_int64_type("int64");
	VFloat s_float_type("float");
	VDouble s_double_type("double");
	VString s_string_type("std::string", "<string>");

	std::string CallerKey(const char* fn, const VType* type)
	{
		return std::string(fn) + "(" + type->name_ + ")";
	}

	template<typename T>
	T Add(T a, T b) { return a + b; }

	template<typename T>
	bool Greater(T a, T b) { return a > b; }

	template<typename T>
	bool Less(T a, T b) { return a < b; }

	template<typename T>
	bool Equal(T a, T b) { return a == b; }

	template<typename T>
	bool NotEqual(T a, T b) { return a != b; }

	template<typename T>
	T Min(T a, T b) { return a < b ? a : b; }

	template<typename T>
	T Max(T a, T b) { return a > b ? a : b; }


	bool And(bool a, bool b) { return a && b; }
	bool Or(bool a, bool b) { return a || b; }
	bool Not(bool a) { return !a; }

	template<typename Fn, typename... Names>
	std::shared_ptr<VFunction> CallerFromFn(const std::string& name, Fn fn, Names... names)
	{
		auto caller = std::make_shared<VFunction>();
		caller->SetName(name);
		caller->SetMeta(Load(fn, names...));
		return caller;
	}

	void BindEmit(const std::shared_ptr<ACaller>& caller, EmitKind kind,
		const std::string& op = {}, const std::string& callee = {},
		const std::string& suffix = {}, std::initializer_list<const char*> includes = {})
	{
		if (!caller) return;

		EmitMeta emit;
		emit.emit_kind = kind;
		emit.op = op;
		emit.callee = callee;
		emit.suffix = suffix;

		for (const char* inc : includes)
			emit.includes.emplace_back(inc);

		caller->SetEmit(emit);
	}

	template<typename T>
	void MathCaller(VSystem& system)
	{
		const VType* type = FindType(typeid(T));
		if (!type) return;

		auto greater = CallerFromFn(CallerKey("Greater", type), &Greater<T>, "a", "b");
		BindEmit(greater, EmitKind::EmitBinary, ">");
		system.RegisterCaller(greater);

		auto add_fn = CallerFromFn(CallerKey("Add", type), &Add<T>, "a", "b");
		BindEmit(add_fn, EmitKind::EmitBinary, "+");
		system.RegisterCaller(add_fn);

		auto less = CallerFromFn(CallerKey("Less", type), &Less<T>, "a", "b");
		BindEmit(less, EmitKind::EmitBinary, "<");
		system.RegisterCaller(less);

		auto equal = CallerFromFn(CallerKey("Equal", type), &Equal<T>, "a", "b");
		BindEmit(equal, EmitKind::EmitBinary, "==");
		system.RegisterCaller(equal);

		auto not_equal = CallerFromFn(CallerKey("NotEqual", type), &NotEqual<T>, "a", "b");
		BindEmit(not_equal, EmitKind::EmitBinary, "!=");
		system.RegisterCaller(not_equal);

		auto min_fn = CallerFromFn(CallerKey("Min", type), &Min<T>, "a", "b");
		BindEmit(min_fn, EmitKind::EmitCall, {}, "std::min", { "<algorithm>" });
		system.RegisterCaller(min_fn);

		auto max_fn = CallerFromFn(CallerKey("Max", type), &Max<T>, "a", "b");
		BindEmit(max_fn, EmitKind::EmitCall, {}, "std::max", { "<algorithm>" });
		system.RegisterCaller(max_fn);
	}

	template<typename... Ts>
	void RegisterMath(VSystem& system)
	{
		(MathCaller<Ts>(system), ...);
	}

	void RegisterTypes(VSystem& system)
	{
		system.RegisterType(&s_int_type);
		system.RegisterType(&s_bool_type);
		system.RegisterType(&s_int64_type);
		system.RegisterType(&s_float_type);
		system.RegisterType(&s_double_type);
		system.RegisterType(&s_string_type);
	}

	void RegisterCallers(VSystem& system)
	{
		auto print = std::make_shared<FPrint>();
		print->SetName("Print");
		BindEmit(print, EmitKind::EmitSymbol, {}, "std::cout", " << std::endl", { "<iostream>" });
		system.RegisterCaller(print);

		auto branch = std::make_shared<FBranch>();
		branch->SetName("Branch");
		system.RegisterCaller(branch);

		auto while_fn = std::make_shared<FWhile>();
		while_fn->SetName("While");
		system.RegisterCaller(while_fn);

		auto loop = std::make_shared<FLoop>();
		loop->SetName("Loop");
		system.RegisterCaller(loop);

		auto sequence = std::make_shared<FSequence>();
		sequence->SetName("Sequence");
		system.RegisterCaller(sequence);

		auto switch_int = std::make_shared<FSwitchInt>();
		switch_int->SetName("SwitchInt");
		system.RegisterCaller(switch_int);

		RegisterMath<int, int64, float, double>(system);

		const VType* tbool = FindType(typeid(bool));
		if (tbool) {
			auto and_fn = CallerFromFn(CallerKey("And", tbool), &And, "a", "b");
			BindEmit(and_fn, EmitKind::EmitBinary, "&&");
			system.RegisterCaller(and_fn);

			auto or_fn = CallerFromFn(CallerKey("Or", tbool), &Or, "a", "b");
			BindEmit(or_fn, EmitKind::EmitBinary, "||");
			system.RegisterCaller(or_fn);

			auto not_fn = CallerFromFn(CallerKey("Not", tbool), &Not, "a");
			BindEmit(not_fn, EmitKind::EmitUnary, "!");
			system.RegisterCaller(not_fn);
		}
	}
}

VSystem& VSystem::Get()
{
	static VSystem instance;
	return instance;
}

VSystem::VSystem() = default;

VSystem::~VSystem() = default;

void VSystem::SetPrinter(Printer handler)
{
	printer_ = std::move(handler);
}

void VSystem::Print(const std::string& text) const
{
	if (printer_)
		printer_(text);
}

void VSystem::Init()
{
	static bool initialized = false;
	if (initialized) return;
	initialized = true;

	RegisterTypes(*this);
	RegisterCallers(*this);
}

bool VSystem::RegisterType(const VType* type)
{
	if (!type) return false;
	VTypeRegister::Register(type);
	return true;
}

void VSystem::UnregisterType(const std::type_index& id)
{
	VTypeRegister::Unregister(id);
}

void VSystem::UnregisterType(const std::string& name)
{
	VTypeRegister::Unregister(name);
}

const VType* VSystem::FindType(const std::string& name) const
{
	return VTypeRegister::Find(name);
}

const VType* VSystem::FindType(const std::type_index& id) const
{
	return VTypeRegister::Find(id);
}

bool VSystem::RegisterCls(VClass* cls)
{
	return VClassRegister::Get().Register(cls);
}

void VSystem::UnregisterCls(const std::string& key)
{
	VClassRegister::Get().Unregister(key);
}

VClass* VSystem::FindClass(const std::string& key) const
{
	return VClassRegister::Get().Find(key);
}

std::vector<VClass*> VSystem::GetClasses() const
{
	return VClassRegister::Get().List();
}

bool VSystem::RegisterCaller(const std::shared_ptr<ACaller>& caller)
{
	return ACallerRegister::Get().Register(caller);
}

void VSystem::UnregisterCaller(const std::string& name)
{
	ACallerRegister::Get().Unregister(name);
}

std::shared_ptr<ACaller> VSystem::FindCaller(const std::string& name) const
{
	return ACallerRegister::Get().Find(name);
}

std::vector<const VType*> VSystem::GetTypes() const
{
	return VTypeRegister::List();
}

std::vector<std::shared_ptr<ACaller>> VSystem::ListCallers() const
{
	return ACallerRegister::Get().List();
}

void VSystem::UnloadDll(const std::string& class_key)
{
	auto it = type_modules_.find(class_key);
	if (it == type_modules_.end())
		return;
	UnregisterCls(class_key);
	UnregisterType(class_key);
#ifdef _WIN32
	if (it->second)
		FreeLibrary(static_cast<HMODULE>(it->second));
#endif
	type_modules_.erase(it);
}

VClass* VSystem::LoadDll(const std::string& path)
{
#ifndef _WIN32
	(void)path;
	return nullptr;
#else
	HMODULE mod = LoadLibraryA(path.c_str());
	if (!mod)
		return nullptr;

	using LoadFn = VClass* (*)();
	auto fn = reinterpret_cast<LoadFn>(GetProcAddress(mod, "ClassGraphLoadType"));
	if (!fn) {
		FreeLibrary(mod);
		return nullptr;
	}

	VClass* cls = fn();
	if (!cls) {
		FreeLibrary(mod);
		return nullptr;
	}

	const std::string key = cls->GetSpace() + "::" + cls->GetName();
	auto it = type_modules_.find(key);
	if (it != type_modules_.end() && it->second != mod) {
		UnregisterCls(key);
		UnregisterType(key);
		FreeLibrary(static_cast<HMODULE>(it->second));
	}
	type_modules_[key] = mod;
	return cls;
#endif
}

