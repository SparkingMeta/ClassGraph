#pragma once
#include <list>
#include <string_view>
#include <memory>
#include <cassert>
#include "Base/VType.h"
#include "Abstract/AMethod.h"
#include "Realistic/VContext.h"
#include "Realistic/VFrame.h"

namespace CG::Core {
	VClass* FindClass(const std::string& name);

	class VMethod;
	using MethodPtr = std::function<void(VObject*, AMethod*, AContext*)>;

	class VMethod : public AMethod {

	public:
		std::string GetKey() const;

		std::shared_ptr<AContext> MakeContext() const override;

		std::vector<AFrame*> GetInputs(AContext* ctx) const override;

		std::vector<AFrame*> GetOutputs(AContext* ctx) const override;

		int Call(AContext*) override;

		CallMeta GetMeta() const override;

		static const VClass* StaticClass();

		const VClass* GetClass() const override;


		/*template<typename T>
		void SetArg(AContext* ctx, const char* name, T&& value) {
			do {
				auto f = FindField(name);
				if (!f) break;

				if (strcmp(typeid(T).name(), f->type_->name_))
					break;

				if (!ctx) break;

				ctx->SetValue(f->offset_, f->type_->size_, &value);
			} while (0);
		}

		template<typename T>
		T GetArg(AContext* ctx, const char* name, bool& ok) {
			ok = false;
			T value{};
			do {
				auto f = FindField(name);
				if (!f) break;

				if (strcmp(typeid(T).name(), f->type_->name_))
					break;

				if (!ctx) break;

				ok = ctx->GetValue(f->offset_, f->type_->size_, &value);
			} while (0);

			return value;
		}

		template<typename T>
		T GetReturn(AContext* ctx, bool& ok) {
			ok = false;
			if (!HasReturn()) return {};
			if (ctx) {
				ok = true;
				T value{};
				ctx->GetValue(0, sizeof(T), &value);
				return value;
			}
			return {};
		}
		*/

		template<typename T>
		void SetReturn(AContext* ctx, T&& value) {
			if (index_return_ < 0) return;
			if (!ctx) return;
			auto frame = ctx->GetFrame(index_return_);
			frame->SetValue(frame->GetType(), &value);
		}

		template<typename Fn, typename... Names>
		requires ((std::convertible_to<Names, std::string_view>) && ...)
		void Load(Fn method, Names... names) {
			using T = Traits<Fn>;
			using ArgsTuple = typename T::ArgsTuple;
			using R = typename T::ReturnType;
			using Args = std::conditional_t<std::is_void_v<R>, ArgsTuple, typename Tuple<R, ArgsTuple>::ArgsTuple>;

			static_assert(sizeof...(names) == std::tuple_size_v<ArgsTuple>, "Parameter name count not match");

			auto count = std::tuple_size_v<Args>;

			auto fn = [=](VObject* object, AMethod* func, AContext* ctx) {
				ExecThunk(object, func, ctx, method);
			};
			this->invoker_ = fn;

			
			if constexpr (!T::is_static)
				owner_ = FindClass(typeid(T::ClassType).name());

			if constexpr(T::has_return)
				index_return_ = count - 1;

			is_pure_ = T::is_const;
			is_static_ = T::is_static;

			args_.reserve(count);

			auto names_tuple = std::forward_as_tuple(names...);

			ForEachArg<Args>(names_tuple);
		}

		template<typename ArgsTuple, size_t... I>
		auto FetchArgs(AContext* ctx, std::index_sequence<I...>) {
			return std::tuple<std::tuple_element_t<I, ArgsTuple>...>{
				FetchArg<std::tuple_element_t<I, ArgsTuple>>(ctx, I)...
			};
		}

	private:
		int Invoke(VObject* object, AContext* ctx);

		template<typename ArgsTuple, typename NameTuple, size_t... I>
		void ForEachArgImpl(NameTuple&& names, std::index_sequence<I...>)
		{
			(
				ParseArg<std::tuple_element_t<I, ArgsTuple>>(I, std::get<I>(names)),
				...
				);
		}

		template<typename ArgsTuple, typename NameTuple>
		void ForEachArg(NameTuple&& names)
		{
			constexpr size_t N = std::tuple_size_v<ArgsTuple>;
			ForEachArgImpl<ArgsTuple>(std::forward<NameTuple>(names), std::make_index_sequence<N>{});
		}

		template<typename ArgType>
		void ParseArg(size_t index, const char* name)
		{
			bool output = false;
			if constexpr (std::is_reference_v<ArgType> && !std::is_const_v<ArgType>)
				output = true;
			if (index == index_return_)
				output = true;
			ParseArgImpl(output, index, typeid(ArgType), name);
		}

		void ParseArgImpl(bool output, size_t index, const std::type_index& id, const char* name);

		template<typename T>
		T FetchArg(AContext* ctx, size_t index) {
			AFrame* frame = nullptr;

			if constexpr (std::is_reference_v<T>) {
				assert(ctx != nullptr);
				
				frame = ctx->GetFrame(index);
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
				frame = ctx->GetFrame(index);
				auto type = frame->GetType();
				if (std::type_index(typeid(T)) != type->id_)
					return {};
				T value{};
				frame->GetValue(type, &value);
				return value;
			}
		}


	public:
		MethodPtr invoker_{};
		int index_return_{-1};
		std::vector<ArgMeta> args_{};
		//std::unordered_map<std::string_view, uint16> indexs_{};
	};

}
