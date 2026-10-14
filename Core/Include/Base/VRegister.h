#pragma once
#include <memory>
#include <shared_mutex>
#include <unordered_map>
#include <typeindex>
#include <vector>

#include "VType.h"

namespace CG::Core {

	class VTypeRegister {
	public:
		static void Register(const VType* type);
		static void Unregister(const std::type_index& id);
		static void Unregister(const std::string& name);
		static const VType* Find(const std::type_index& id);
		static const VType* Find(const std::string& name);
		static std::vector<const VType*> List();
	private:
		static std::unordered_map<std::type_index, const VType*> index_types_;
		static std::unordered_map<std::string, const VType*> name_types;
	};

	class VClass;
	class VClassRegister {
	public:
		static VClassRegister& Get();

		bool Register(VClass* cls);

		void Unregister(const std::string& name);

		VClass* Find(const std::string& name) const;

		std::vector<VClass*> List() const;

	private:
		mutable std::shared_mutex locker_{};
		std::unordered_map<std::string, VClass*> cls_{};
	};

	template<typename T>
	struct HClassRegister {
		HClassRegister(VClass* cls) {
			VClassRegister::Get().Register(cls);
		}
	};

	template <typename T>
	inline static bool Register() {
		static bool is_regist = []() {
			static CG::Core::HClassRegister<T> {T::StaticClass()};
			return true;
		}();
		return is_regist;
	}

	class ACaller;
	class ACallerRegister {
	public:
		static ACallerRegister& Get();

		bool Register(const std::shared_ptr<ACaller>& caller);

		void Unregister(const std::string& name);

		std::shared_ptr<ACaller> Find(const std::string& name) const;
		std::vector<std::shared_ptr<ACaller>> List() const;

	private:
		mutable std::shared_mutex locker_{};
		std::unordered_map<std::string, std::shared_ptr<ACaller>> callers_;
	};

	inline VClass* FindClass(const std::string& name)
	{
		return VClassRegister::Get().Find(name);
	}

	inline const VType* FindType(const std::type_index& id)
	{
		return VTypeRegister::Find(id);
	}

	inline const VType* FindType(const std::string& name)
	{
		return VTypeRegister::Find(name);
	}

	inline std::shared_ptr<ACaller> FindCaller(const std::string& name)
	{
		return ACallerRegister::Get().Find(name);
	}

	template<typename T>
	const VType* RegisterClassType(const char* name, const char* header = "")
	{
		static VTypeImpl<T> type(name, header);
		VTypeRegister::Register(&type);
		return &type;
	}

}