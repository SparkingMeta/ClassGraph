#pragma once
#include <typeindex>

#include "VCore.h"
#include "VObject.h"
#include "VTraits.h"

namespace CG::Core {

	struct VType {
		VType(std::type_index id) : id_(id){}

		/*
		void Construct(void* ptr) const { if (constructor_) constructor_(ptr); }
		void Destruct(void* ptr) const { if (destructor_) destructor_(ptr); }
		void Copy(void* dst, const void* src) const { if (copyer_) copyer_(dst, src); }
		void Move(void* dst, void* src) const { if (mover_) mover_(dst, src); }
		*/

		std::string name_;
		std::string desc_;
		std::string header_;
		size_t size_{0};
		size_t align_{0};
		std::type_index id_;

		Construct construct_{};
		Destruct destruct_{};
		CopyConstruct copy_construct_{};
		MoveConstruct move_construct_{};
		CopyAssign copy_assign_{};
		MoveAssign move_assign_{};

		Serialize serialize_{};
		Deserialize deserialize_{};
	};

	template<typename T>
	struct VTypeImpl : public VType {
		VTypeImpl(const std::string& name, const std::string& header = {}) : VType(typeid(T)) {
			name_ = name;
			header_ = header;
			desc_ = typeid(T).name();
			size_ = sizeof(T);
			align_ = alignof(T);

			construct_ = TypeTraits<T>::Construct();
			destruct_ = TypeTraits<T>::Destruct();

			copy_construct_ = TypeTraits<T>::CopyConstruct();
			move_construct_ = TypeTraits<T>::MoveConstruct();

			copy_assign_ = TypeTraits<T>::CopyAssign();
			move_assign_ = TypeTraits<T>::MoveAssign();

			serialize_ = TypeTraits<T>::Serialize();
			deserialize_ = TypeTraits<T>::Deserialize();
		}

	};

	using VBool = VTypeImpl<bool>;
	using VInt32 = VTypeImpl<int32>;
	using VInt64 = VTypeImpl<int64>;
	using VUint32 = VTypeImpl<uint32>;
	using VUint64 = VTypeImpl<uint64>;
	using VFloat = VTypeImpl<float>;
	using VDouble = VTypeImpl<double>;
	using VString = VTypeImpl<std::string>;

	struct VField : VObject {
		const VType* type_{ nullptr };
		size_t offset_{ 0 };
	};

}
