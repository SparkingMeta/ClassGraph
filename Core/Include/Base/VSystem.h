#pragma once

#include "VCore.h"

#include <functional>
#include <memory>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace CG::Core {
	using Printer = std::function<void(const std::string&)>;

	struct VType;
	class VClass;
	class ACaller;

	// System registry: types, reflection classes, global ACaller nodes
	class CORE_API VSystem {
	public:
		static VSystem& Get();

		// Register base types, reflection root class, global ACaller nodes
		void Init();

		// --- types ---
		bool RegisterType(const VType* type);
		void UnregisterType(const std::type_index& id);
		void UnregisterType(const std::string& name);
		const VType* FindType(const std::string& name) const;
		const VType* FindType(const std::type_index& id) const;
		std::vector<const VType*> GetTypes() const;

		// --- reflection classes (key = space::name) ---
		bool RegisterCls(VClass* cls);
		void UnregisterCls(const std::string& key);
		VClass* FindClass(const std::string& key) const;
		std::vector<VClass*> GetClasses() const;

		// --- global ACaller (cross-blueprint nodes, indexed by GetName()) ---
		bool RegisterCaller(const std::shared_ptr<ACaller>& caller);
		void UnregisterCaller(const std::string& name);
		std::shared_ptr<ACaller> FindCaller(const std::string& name) const;
		std::vector<std::shared_ptr<ACaller>> ListCallers() const;

		// Optional host sink for debug/print output (set by app/editor layer).
		
		void SetPrinter(Printer handler);
		void Print(const std::string& text) const;

		void UnloadDll(const std::string& class_key);
		VClass* LoadDll(const std::string& path);

	private:
		VSystem();
		~VSystem();
		Printer printer_;
		std::unordered_map<std::string, void*> type_modules_;
	};

} // namespace CG::Core
