#pragma  once
#include "Abstract/AFunction.h"
#include "Realistic/VContext.h"
#include "Realistic/VFrame.h"
#include "Base/VType.h"
#include "Base/VRegister.h"
#include "Base/VMeta.h"
#include <cassert>
#include <string_view>

namespace CG::Core {
	using FuncPtr = std::function<void(AContext*)>;

	struct FuncMeta : CallMeta
	{
		int index;
		FuncPtr invoker;
		VClass* owner;
		EmitMeta emit;
	};

	template<typename T>
	T FetchArg(AContext* ctx, std::string name) {
		std::shared_ptr<AFrame> frame{};

		if constexpr (std::is_reference_v<T>) {
			assert(ctx != nullptr);

			frame = ctx->GetFrame(name);
			auto type = frame->GetType();
			assert(std::type_index(typeid(T)) == type->id_);

			auto ptr = frame->GetBuffer();
			assert(ptr != nullptr);

			using Raw = std::remove_reference_t<T>;
			Raw* raw_ptr = std::launder(reinterpret_cast<Raw*>(ptr));
			return static_cast<T>(*raw_ptr);
		}
		else {
			if (!ctx) return{};
			frame = ctx->GetFrame(name);
			auto type = frame->GetType();
			if (std::type_index(typeid(T)) != type->id_)
				return {};
			T value{};
			frame->GetValue(type, &value);
			return value;
		}
	}

	template<typename ArgsTuple, size_t... I>
	auto FetchArgs(AContext* ctx, CallMeta& meta, std::index_sequence<I...>) {
		return std::tuple<std::tuple_element_t<I, ArgsTuple>...>{
			FetchArg<std::tuple_element_t<I, ArgsTuple>>(ctx, meta.args[I].name)...
		};
	}


	template<typename Fn>
	void ExecThunk(AContext* ctx, CallMeta meta, Fn fn) {
		using T = Traits<Fn>;
		using R = typename T::ReturnType;
		using ArgsTuple = typename T::ArgsTuple;

		constexpr bool has_self = has_class_type_v<T>;

		using Args = tuple_t<typename SelfTuple<T, has_self>::type, ArgsTuple>;

		constexpr size_t size = std::tuple_size_v<Args>;
		auto args = FetchArgs<Args>(ctx, meta, std::make_index_sequence<size>{});

		if constexpr (!T::has_return) {
			std::apply(fn, args);
		}
		else {
			R result = std::apply(fn, args);
			if(auto frame = ctx->GetFrame("return"))
				frame->SetValue(frame->GetType(), &result);
		}
	}

	template<typename ArgType>
	void ParseArg(bool output, const char* name, CallMeta& meta)
	{
		ArgMeta arg;
		arg.name = name;
		arg.is_output = output;
		arg.is_runtime = true;
		arg.type = FindType(typeid(ArgType));
		meta.args.push_back(arg);
	}

	template<typename ArgsTuple, typename NameTuple, size_t... I>
	void ForEachArgImpl(NameTuple&& names, std::index_sequence<I...>, int index, CallMeta& meta)
	{
		(
			[&] {
				using ArgType = std::tuple_element_t<I, ArgsTuple>;
				bool output = index == I;
				const char* arg_name = std::get<I>(names);
				if constexpr (std::is_reference_v<ArgType> && !std::is_const_v<ArgType>) {
					if (!(arg_name && std::string_view(arg_name) == "self"))
						output = true;
				}

				ParseArg<ArgType>(output, arg_name, meta);
			}()
				, ...
		);
	}

	template<typename ArgsTuple, typename NameTuple>
	CallMeta ForEachArg(NameTuple&& names, int index)
	{
		constexpr size_t N = std::tuple_size_v<ArgsTuple>;
		CallMeta meta;
		meta.args.reserve(N);
		ForEachArgImpl<ArgsTuple>(std::forward<NameTuple>(names), std::make_index_sequence<N>{}, index, meta);
		return meta;
	}

	template<typename Fn, typename... Names>
	FuncMeta Load(Fn fn, Names... names) {
		using T = Traits<Fn>;
		using R = typename T::ReturnType;
		using ArgsTuple = typename T::ArgsTuple;

		static_assert(sizeof...(names) == std::tuple_size_v<ArgsTuple>, "Parameter name count not IsMatch");

		constexpr bool has_self = has_class_type_v<T>;

		using Args = tuple_t<typename SelfTuple<T, has_self>::type, ArgsTuple>;

		using Pack = std::conditional_t<std::is_void_v<R>, Args, tuple_t<Args, std::tuple<R>>>;

		auto names_tuple = std::forward_as_tuple(names...);

		auto pack_names = [&]() {
			if constexpr (has_self) {
				if constexpr (!std::is_void_v<R>)
					return std::tuple_cat(std::make_tuple("self"), names_tuple, std::make_tuple("return"));
				else
					return std::tuple_cat(std::make_tuple("self"), names_tuple);
			}
			else {
				if constexpr (!std::is_void_v<R>)
					return std::tuple_cat(names_tuple, std::make_tuple("return"));
				else
					return names_tuple;
			}
			}();

		int index = -1;
		if constexpr (T::has_return) index = std::tuple_size_v<Pack> - 1;

		auto meta = ForEachArg<Pack>(pack_names, index);

		FuncMeta func_meta;
		func_meta.pure = T::is_static ? true : T::is_const;
		func_meta.index = index;
		func_meta.invoker = [=](AContext* ctx) {ExecThunk(ctx, meta, fn);};
		if constexpr (has_self)
			func_meta.owner = FindClass(typeid(typename T::ClassType).name());
		else
			func_meta.owner = nullptr;
		func_meta.args = std::move(meta.args);

		return func_meta;
	}

	

	class VFunction : public AFunction {
	public:
		std::string GetKey() const override;

		bool IsPure() const override;

		int GetKind() const override;

		CallMeta GetCallMeta() const override;

		ExecMeta GetExecMeta() const override;

		Result Call(VObject*, AContext* ctx) override;

		void SetMeta(const FuncMeta& meta);
		const FuncMeta& GetFuncMeta() const;
		void SetEmit(const EmitMeta& meta) override;
		EmitMeta GetEmitMeta() const override;

	private:
		FuncMeta meta_;
	};
}