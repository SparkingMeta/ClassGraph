#pragma once

#include "VCore.h"

#include <functional>
#include <memory>
#include <string>
#include <typeindex>
#include <type_traits>
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
		template<typename T>
		requires (!std::is_same_v<T, std::string>)
		void Print(const T& value) const {
			Print(FindType(typeid(T)), static_cast<const void*>(&value));
		}

		void UnloadDll(const std::string& class_key);
		void UnloadAllDlls();
		VClass* LoadDll(const std::string& path);

	private:
		VSystem();
		~VSystem();
		void Print(const VType* type, const void* value) const;
		Printer printer_;
		std::unordered_map<std::string, void*> type_modules_;
	};

} // namespace CG::Core
