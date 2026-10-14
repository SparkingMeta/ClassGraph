#pragma once

#include <vector>
#include <functional>

#include "Base/VObject.h"
#include "Base/VMeta.h"

namespace CG::Core {
	class AFrame;
	class AContext;

	enum CallKind {
		CK_Function,
		CK_Script,
		CK_Controller,
		CK_Operator
	};

	std::shared_ptr<AFrame> FindFrame(AContext* ctx,const std::string& name);
	std::vector<std::shared_ptr<AFrame>> GetInputs(AContext* ctx, const CallMeta& meta);
	std::vector<std::shared_ptr<AFrame>> GetOutputs(AContext* ctx, const CallMeta& meta);

	class ACaller : public VObject {
	public:
		virtual bool IsPure() const = 0;
		virtual int GetKind() const = 0;
		virtual CallMeta GetCallMeta() const = 0;
		virtual ExecMeta GetExecMeta() const = 0;
		virtual Result Call(VObject*, AContext*) = 0;

		virtual EmitMeta GetEmitMeta() const { return emit_; }
		virtual void SetEmit(const EmitMeta& meta) { emit_ = meta; }

		std::shared_ptr<AContext> MakeContext(const CallMeta& meta) const;
		

	protected:
		EmitMeta emit_{};
	};


	template<typename...>
	struct Tuple;

	template<typename... Ts>
	struct Tuple<std::tuple<Ts...>>
	{
		using type = std::tuple<Ts...>;
	};

	template<typename... A, typename... B>
	struct Tuple<std::tuple<A...>, std::tuple<B...>>
	{
		using type = std::tuple<A..., B...>;
	};

	template<typename... Tuples>
	using tuple_t = typename Tuple<Tuples...>::type;


	template<typename T, bool HasSelf>
	struct SelfTuple { using type = std::tuple<>; };

	template<typename T>
	struct SelfTuple<T, true> { using type = std::tuple<typename T::ClassType&>; };

	template<typename T>
	struct Traits;

	template<typename R, typename... Args>
	struct Traits<R(Args...)> {
		using ReturnType = R;
		using ArgsTuple = std::tuple<Args...>;
		static constexpr bool has_return = !std::is_void_v<R>;
		static constexpr size_t arity = sizeof...(Args);
		static constexpr bool is_const = false;
		static constexpr bool is_static = true;
	};

	// function pointer
	template<typename R, typename... Args>
	struct Traits<R(*)(Args...)> : Traits<R(Args...)> {};

	// std::function
	template<typename R, typename... Args>
	struct Traits<std::function<R(Args...)>> : Traits<R(Args...)> {};


	// member function pointer (non-const)
	template<typename TClass, typename R, typename... Args>
	struct Traits<R(TClass::*)(Args...)> : Traits<R(Args...)> {
		using ClassType = TClass;
		static constexpr bool is_static = false;
	};

	// member function pointer (const)
	template<typename TClass, typename R, typename... Args>
	struct Traits<R(TClass::*)(Args...) const> :  Traits<R(TClass::*)(Args...)> {
		static constexpr bool is_const = true;
	};

	// lambda / functor
	template<typename C>
	struct Traits : Traits<decltype(&C::operator())> {
		using ClassType = C;
		static constexpr bool is_static = false;
	};

	template<class T>
	class Loader {
	public:
		
	private:
		static const int index = -1;
	};

	template<typename, typename = void>
	struct has_class_type : std::false_type {};

	template<typename T>
	struct has_class_type<T, std::void_t<typename T::ClassType>> : std::true_type {};

	template<typename T>
	inline constexpr bool has_class_type_v = has_class_type<T>::value;
}

