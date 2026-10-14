#pragma once
#include "Base/VCore.h"
#include "Base/VObject.h"
#include "Base/VSerialize.h"

namespace CG::Core {
	template<typename T>
	struct TypeTraits
	{
	private:

		static void ConstructImpl(void* ptr)
		{
			new(ptr) T();
		}

		static void DestructImpl(void* ptr)
		{
			reinterpret_cast<T*>(ptr)->~T();
		}

		static void CopyConstructImpl(void* dst, const void* src)
		{
			new(dst) T(*reinterpret_cast<const T*>(src));
		}

		static void MoveConstructImpl(void* dst, void* src)
		{
			new(dst) T(std::move(*reinterpret_cast<T*>(src)));
		}

		static void CopyAssignImpl(void* dst, const void* src)
		{
			*reinterpret_cast<T*>(dst)
				= *reinterpret_cast<const T*>(src);
		}

		static void MoveAssignImpl(void* dst, void* src)
		{
			*reinterpret_cast<T*>(dst)
				= std::move(*reinterpret_cast<T*>(src));
		}

		static bool SerializeImpl(const void* ptr, std::string& out)
		{
			if constexpr (std::is_base_of_v<VObject, T>) {
				return ToJson(reinterpret_cast<const VObject*>(ptr), out);
			}
			else {
				return ToJson<T>(*reinterpret_cast<const T*>(ptr), out);
			}
		}

		static bool DeserializeImpl(void* ptr, const std::string& in)
		{
			if constexpr (std::is_base_of_v<VObject, T>) {
				return FromJson(in, reinterpret_cast<VObject*>(ptr));
			}
			else {
				return FromJson<T>(in, *reinterpret_cast<T*>(ptr));
			}
		}

	public:
		static constexpr Construct Construct()
		{
			if constexpr (std::is_default_constructible_v<T>)
				return &ConstructImpl;
			else
				return nullptr;
		}

		static constexpr Destruct Destruct()
		{
			if constexpr (std::is_trivially_destructible_v<T>)
				return nullptr;
			else
				return &DestructImpl;
		}

		static constexpr CopyConstruct CopyConstruct()
		{
			if constexpr (std::is_copy_constructible_v<T>)
				return &CopyConstructImpl;
			else
				return nullptr;
		}

		static constexpr MoveConstruct MoveConstruct()
		{
			if constexpr (std::is_move_constructible_v<T>)
				return &MoveConstructImpl;
			else
				return nullptr;
		}

		static constexpr CopyAssign CopyAssign()
		{
			if constexpr (std::is_copy_assignable_v<T>)
				return &CopyAssignImpl;
			else
				return nullptr;
		}

		static constexpr MoveAssign MoveAssign()
		{
			if constexpr (std::is_move_assignable_v<T>)
				return &MoveAssignImpl;
			else
				return nullptr;
		}

		static constexpr Serialize Serialize()
		{
			return &SerializeImpl;
		}

		static constexpr Deserialize Deserialize()
		{
			return &DeserializeImpl;
		}
	};
}
