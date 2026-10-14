#include "CodeGen/CHandler.h"

#include "CodeGen/CGraph.h"
#include "Abstract/AContext.h"
#include "Abstract/AFlow.h"
#include "Abstract/AFrame.h"
#include "Abstract/APin.h"
#include "Abstract/AVariant.h"
#include "Realistic/VController.h"
#include "Realistic/VFunction.h"
#include "Realistic/VVarFunction.h"
#include "Realistic/VOperator.h"
#include "Realistic/VScript.h"
#include "Realistic/VBlueprintObject.h"

using namespace CG::Core;
using namespace CG::Core::cg;

namespace {

	std::unique_ptr<CExpr> ResolveArgExpr(CBuildContext& ctx, const std::shared_ptr<ANode>& node, const std::string& arg_name)
	{
		if (auto upstream = GetFromNode(node, arg_name)) {
			if (dynamic_cast<const FEntry*>(upstream->GetCaller().get())) {
				auto out_name = GetFromOutArg(node, arg_name);
				if (!out_name.empty())
					return MakeLiteral(out_name);
			}
			return BuildExpr(ctx, upstream);
		}
		if (ctx.method_params.count(arg_name))
			return MakeLiteral(arg_name);
		if (auto pin = FindInPin(node, arg_name))
			return MakeLiteral(ToLiteral(pin->GetFrame()));
		return nullptr;
	}

	std::vector<std::unique_ptr<CExpr>> CollectInputExprs(CBuildContext& ctx,
		const std::shared_ptr<ANode>& node, const CallMeta& call_meta, bool through_build_expr)
	{
		std::vector<std::unique_ptr<CExpr>> inputs;
		for (const auto& arg : call_meta.args) {
			if (arg.is_output) continue;
			std::unique_ptr<CExpr> expr = through_build_expr
				? BuildExpr(ctx, node, arg.name)
				: ResolveArgExpr(ctx, node, arg.name);
			if (!expr) return {};
			inputs.push_back(std::move(expr));
		}
		return inputs;
	}

	std::unique_ptr<CExpr> LowerEmitExpr(CBuildContext& ctx, const std::shared_ptr<ANode>& node, const ACaller* caller)
	{
		if (!caller) return nullptr;
		auto inputs = CollectInputExprs(ctx, node, caller->GetCallMeta(), caller->GetKind() == CK_Function);
		if (inputs.empty() && !caller->GetCallMeta().args.empty())
			return nullptr;

		const auto& emit = caller->GetEmitMeta();
		ApplyEmitIncludes(ctx, emit);
		return BuildExprFromEmit(emit, caller->GetName(), std::move(inputs));
	}

	bool HasCodegenEmit(const ACaller* caller)
	{
		if (!caller) return false;
		const auto& emit = caller->GetEmitMeta();
		switch (emit.emit_kind) {
		case EmitKind::EmitBinary:
		case EmitKind::EmitUnary:
			return !emit.op.empty();
		case EmitKind::EmitSymbol:
			return !emit.callee.empty();
		case EmitKind::EmitCall:
			return !emit.callee.empty();
		}
		return false;
	}

	std::unique_ptr<CStmt> EmptyOr(std::unique_ptr<CStmt> s)
	{
		return s ? std::move(s) : MakeCompound({});
	}

	void AppendStmt(std::vector<std::unique_ptr<CStmt>>& statements, std::unique_ptr<CStmt> stmt)
	{
		if (!stmt) return;
		if (stmt->kind == StmtKind::SK_Compound) {
			for (auto& child : stmt->statements)
				statements.push_back(std::move(child));
			return;
		}
		statements.push_back(std::move(stmt));
	}

	void AppendAfterController(CBuildContext& ctx, const std::shared_ptr<ANode>& node,
		const char* completed_pin, std::vector<std::unique_ptr<CStmt>>& statements)
	{
		AppendStmt(statements, BuildStmtChain(ctx, NextExec(node, completed_pin)));
	}

	class EntryHandler final : public ICHandler {
	public:
		bool IsMatch(const ACaller* caller) const override {
			return dynamic_cast<const FEntry*>(caller) != nullptr;
		}
		std::optional<std::unique_ptr<CStmt>> BuildStmt(CBuildContext&, const std::shared_ptr<ANode>&) const override {
			return std::nullopt;
		}
		std::optional<std::unique_ptr<CExpr>> BuildExpr(CBuildContext&, const std::shared_ptr<ANode>&) const override {
			return std::nullopt;
		}
	};

	class ExitHandler final : public ICHandler {
	public:
		bool IsMatch(const ACaller* caller) const override {
			return dynamic_cast<const FExit*>(caller) != nullptr;
		}
		std::optional<std::unique_ptr<CStmt>> BuildStmt(CBuildContext& ctx, const std::shared_ptr<ANode>& node) const override {
			if (!node || !node->GetCaller())
				return std::nullopt;

			std::vector<std::unique_ptr<CStmt>> stmts;
			auto resolve = [&](const std::string& pin_name) {
				auto expr = ResolveArgExpr(ctx, node, pin_name);
				return expr ? std::move(expr) : MakeLiteral("{}");
			};

			for (const auto& [pin_name, cpp_name] : ctx.extra_outs)
				stmts.push_back(MakeAssign(MakeLiteral(cpp_name), resolve(pin_name)));

			if (!ctx.return_pin.empty()) {
				stmts.push_back(MakeReturn(resolve(ctx.return_pin)));
			} else {
				for (const auto& arg : node->GetCaller()->GetCallMeta().args) {
					if (arg.is_output || !arg.is_runtime || arg.name.empty())
						continue;
					stmts.push_back(MakeReturn(resolve(arg.name)));
					break;
				}
			}

			if (stmts.empty())
				return std::nullopt;
			if (stmts.size() == 1)
				return std::move(stmts[0]);
			return MakeCompound(std::move(stmts));
		}
		std::optional<std::unique_ptr<CExpr>> BuildExpr(CBuildContext&, const std::shared_ptr<ANode>&) const override {
			return std::nullopt;
		}
	};

	class VarHandler final : public ICHandler {
	public:
		bool IsMatch(const ACaller* caller) const override {
			return dynamic_cast<const FGetVar*>(caller) || dynamic_cast<const FSetVar*>(caller);
		}
		std::optional<std::unique_ptr<CStmt>> BuildStmt(CBuildContext& ctx, const std::shared_ptr<ANode>& node) const override {
			auto caller = dynamic_cast<const FSetVar*>(node->GetCaller().get());
			if (!caller) return std::nullopt;
			auto var = caller->GetVar();
			if (!var) return std::nullopt;

			auto value = ResolveArgExpr(ctx, node, "value");
			if (!value) return std::nullopt;
			return MakeAssign(MakeMember(var->GetName()), std::move(value));
		}
		std::optional<std::unique_ptr<CExpr>> BuildExpr(CBuildContext&, const std::shared_ptr<ANode>& node) const override {
			auto caller = dynamic_cast<const FGetVar*>(node->GetCaller().get());
			if (!caller) return std::nullopt;
			auto var = caller->GetVar();
			if (!var) return std::nullopt;
			return MakeMember(var->GetName());
		}
	};

	bool IsClassMethod(const ACaller* caller)
	{
		auto fn = dynamic_cast<const VFunction*>(caller);
		return fn && fn->GetFuncMeta().owner;
	}

	std::optional<std::unique_ptr<CExpr>> BuildClassMethodCallExpr(CBuildContext& ctx,
		const std::shared_ptr<ANode>& node, const VFunction* fn)
	{
		if (!fn) return std::nullopt;
		auto target = ResolveArgExpr(ctx, node, "self");
		if (!target) return std::nullopt;

		std::vector<std::unique_ptr<CExpr>> args;
		for (const auto& arg : fn->GetCallMeta().args) {
			if (arg.name == "self" || arg.is_output) continue;
			auto expr = ResolveArgExpr(ctx, node, arg.name);
			if (!expr) return std::nullopt;
			args.push_back(std::move(expr));
		}

		auto call = MakeMemberCall(std::move(target), fn->GetName(), std::move(args));
		call->op = ".";
		return call;
	}

	class MethodCallHandler final : public ICHandler {
	public:
		bool IsMatch(const ACaller* caller) const override {
			return IsClassMethod(caller);
		}
		std::optional<std::unique_ptr<CStmt>> BuildStmt(CBuildContext& ctx, const std::shared_ptr<ANode>& node) const override {
			auto fn = dynamic_cast<const VFunction*>(node->GetCaller().get());
			if (!fn || fn->IsPure()) return std::nullopt;
			auto expr = BuildClassMethodCallExpr(ctx, node, fn);
			if (!expr) return std::nullopt;
			return MakeExprStmt(std::move(expr.value()));
		}
		std::optional<std::unique_ptr<CExpr>> BuildExpr(CBuildContext& ctx, const std::shared_ptr<ANode>& node) const override {
			auto fn = dynamic_cast<const VFunction*>(node->GetCaller().get());
			if (!fn) return std::nullopt;
			return BuildClassMethodCallExpr(ctx, node, fn);
		}
	};

	class EmitHandler final : public ICHandler {
	public:
		bool IsMatch(const ACaller* caller) const override {
			if (!caller) return false;
			if (IsClassMethod(caller)) return false;
			if (caller->GetKind() == CK_Function)
				return dynamic_cast<const FGetVar*>(caller) == nullptr
					&& dynamic_cast<const FSetVar*>(caller) == nullptr;
			return caller->GetKind() == CK_Operator && HasCodegenEmit(caller);
		}
		std::optional<std::unique_ptr<CStmt>> BuildStmt(CBuildContext& ctx, const std::shared_ptr<ANode>& node) const override {
			auto caller = node->GetCaller().get();
			if (!caller || caller->IsPure()) return std::nullopt;
			auto expr = LowerEmitExpr(ctx, node, caller);
			if (!expr) return std::nullopt;
			return MakeExprStmt(std::move(expr));
		}
		std::optional<std::unique_ptr<CExpr>> BuildExpr(CBuildContext& ctx, const std::shared_ptr<ANode>& node) const override {
			auto expr = LowerEmitExpr(ctx, node, node->GetCaller().get());
			if (!expr) return std::nullopt;
			return expr;
		}
	};

	class ScriptHandler final : public ICHandler {
	public:
		bool IsMatch(const ACaller* caller) const override {
			return dynamic_cast<const VScript*>(caller) != nullptr;
		}
		std::unique_ptr<CExpr> LowerScriptCall(CBuildContext& ctx, const std::shared_ptr<ANode>& node,
			const VScript* script) const
		{
			if (!script) return {};
			auto flow = script->GetFlow();
			if (!flow) return {};
			const std::string& name = flow->GetName();
			if (name.empty()) return {};

			std::vector<std::unique_ptr<CExpr>> args;
			for (const auto& arg : script->GetCallMeta().args) {
				if (arg.is_output || !arg.is_runtime) continue;
				auto expr = ResolveArgExpr(ctx, node, arg.name);
				if (!expr) return {};
				args.push_back(std::move(expr));
			}
			return MakeMemberCall(MakeLiteral(kCodegenSelf), name, std::move(args));
		}

		std::optional<std::unique_ptr<CStmt>> BuildStmt(CBuildContext& ctx, const std::shared_ptr<ANode>& node) const override {
			auto script = dynamic_cast<const VScript*>(node->GetCaller().get());
			auto expr = LowerScriptCall(ctx, node, script);
			if (!expr) return std::nullopt;
			return MakeExprStmt(std::move(expr));
		}
		std::optional<std::unique_ptr<CExpr>> BuildExpr(CBuildContext& ctx, const std::shared_ptr<ANode>& node) const override {
			auto script = dynamic_cast<const VScript*>(node->GetCaller().get());
			if (!script) return std::nullopt;
			bool has_output = false;
			for (const auto& arg : script->GetCallMeta().args) {
				if (arg.is_output && arg.is_runtime) {
					has_output = true;
					break;
				}
			}
			if (!has_output) return std::nullopt;
			auto expr = LowerScriptCall(ctx, node, script);
			if (!expr) return std::nullopt;
			return expr;
		}
	};

	class BranchHandler final : public ICHandler {
	public:
		bool IsMatch(const ACaller* caller) const override {
			return dynamic_cast<const FBranch*>(caller) != nullptr;
		}
		std::optional<std::unique_ptr<CStmt>> BuildStmt(CBuildContext& ctx, const std::shared_ptr<ANode>& node) const override {
			auto cond = ResolveArgExpr(ctx, node, "cond");
			if (!cond) return std::nullopt;
			return MakeIf(std::move(cond),
				EmptyOr(BuildStmtChain(ctx, NextExec(node, "true"))),
				EmptyOr(BuildStmtChain(ctx, NextExec(node, "false"))));
		}
		std::optional<std::unique_ptr<CExpr>> BuildExpr(CBuildContext&, const std::shared_ptr<ANode>&) const override {
			return std::nullopt;
		}
	};

	class WhileHandler final : public ICHandler {
	public:
		bool IsMatch(const ACaller* caller) const override {
			return dynamic_cast<const FWhile*>(caller) != nullptr;
		}
		std::optional<std::unique_ptr<CStmt>> BuildStmt(CBuildContext& ctx, const std::shared_ptr<ANode>& node) const override {
			auto cond = ResolveArgExpr(ctx, node, "cond");
			if (!cond) return std::nullopt;

			std::vector<std::unique_ptr<CStmt>> statements;
			statements.push_back(MakeWhile(std::move(cond), EmptyOr(BuildStmtChain(ctx, NextExec(node, "body")))));
			AppendAfterController(ctx, node, "completed", statements);
			return MakeCompound(std::move(statements));
		}
		std::optional<std::unique_ptr<CExpr>> BuildExpr(CBuildContext&, const std::shared_ptr<ANode>&) const override {
			return std::nullopt;
		}
	};

	class LoopHandler final : public ICHandler {
	public:
		bool IsMatch(const ACaller* caller) const override {
			return dynamic_cast<const FLoop*>(caller) != nullptr;
		}
		std::optional<std::unique_ptr<CStmt>> BuildStmt(CBuildContext& ctx, const std::shared_ptr<ANode>& node) const override {
			auto first = ResolveArgExpr(ctx, node, "first");
			auto last = ResolveArgExpr(ctx, node, "last");
			if (!first || !last) return std::nullopt;

			std::vector<std::unique_ptr<CStmt>> statements;
			statements.push_back(MakeFor("index", std::move(first), std::move(last),
				EmptyOr(BuildStmtChain(ctx, NextExec(node, "body")))));
			AppendAfterController(ctx, node, "completed", statements);
			return MakeCompound(std::move(statements));
		}
		std::optional<std::unique_ptr<CExpr>> BuildExpr(CBuildContext&, const std::shared_ptr<ANode>&) const override {
			return std::nullopt;
		}
	};

	class SequenceHandler final : public ICHandler {
	public:
		bool IsMatch(const ACaller* caller) const override {
			return dynamic_cast<const FSequence*>(caller) != nullptr;
		}
		std::optional<std::unique_ptr<CStmt>> BuildStmt(CBuildContext& ctx, const std::shared_ptr<ANode>& node) const override {
			auto layout = ResolveLayout(node);
			std::vector<std::unique_ptr<CStmt>> statements;
			for (size_t i = 0; i < layout.outputs.size(); ++i) {
				if (layout.outputs[i].kind != PinKind::Exec || layout.outputs[i].name.empty())
					continue;
				AppendStmt(statements, BuildStmtChain(ctx, node->Next(static_cast<int>(i))));
			}
			return MakeCompound(std::move(statements));
		}
		std::optional<std::unique_ptr<CExpr>> BuildExpr(CBuildContext&, const std::shared_ptr<ANode>&) const override {
			return std::nullopt;
		}
	};

	class SwitchHandler final : public ICHandler {
	public:
		bool IsMatch(const ACaller* caller) const override {
			return dynamic_cast<const FSwitchInt*>(caller) != nullptr;
		}
		std::optional<std::unique_ptr<CStmt>> BuildStmt(CBuildContext& ctx, const std::shared_ptr<ANode>& node) const override {
			auto cond = ResolveArgExpr(ctx, node, "cond");
			if (!cond) return std::nullopt;

			auto layout = ResolveLayout(node);
			std::vector<CCase> cases;
			for (size_t i = 0; i < layout.outputs.size(); ++i) {
				const auto& pin = layout.outputs[i];
				if (pin.kind != PinKind::Exec || pin.name.empty())
					continue;

				CCase c;
				if (pin.name == "default" || pin.name.empty()) {
					c.label = "default";
				} else {
					auto frame = node->GetContext() ? node->GetContext()->GetFrame(pin.name) : nullptr;
					c.label = ToLiteral(frame);
				}
				c.body = EmptyOr(BuildStmtChain(ctx, node->Next(static_cast<int>(i))));
				cases.push_back(std::move(c));
			}
			return MakeSwitch(std::move(cond), std::move(cases));
		}
		std::optional<std::unique_ptr<CExpr>> BuildExpr(CBuildContext&, const std::shared_ptr<ANode>&) const override {
			return std::nullopt;
		}
	};

} // namespace

void CHandlerRegistry::Register(std::unique_ptr<ICHandler> handler)
{
	handlers_.push_back(std::move(handler));
}

const ICHandler* CHandlerRegistry::Find(const ACaller* caller) const
{
	if (!caller) return nullptr;
	for (const auto& handler : handlers_) {
		if (handler->IsMatch(caller))
			return handler.get();
	}
	return nullptr;
}

CHandlerRegistry& CG::Core::cg::DefaultHandlers()
{
	static CHandlerRegistry registry;
	static bool initialized = false;
	if (!initialized) {
		registry.Register(std::make_unique<EntryHandler>());
		registry.Register(std::make_unique<ExitHandler>());
		registry.Register(std::make_unique<VarHandler>());
		registry.Register(std::make_unique<MethodCallHandler>());
		registry.Register(std::make_unique<EmitHandler>());
		registry.Register(std::make_unique<ScriptHandler>());
		registry.Register(std::make_unique<BranchHandler>());
		registry.Register(std::make_unique<WhileHandler>());
		registry.Register(std::make_unique<LoopHandler>());
		registry.Register(std::make_unique<SequenceHandler>());
		registry.Register(std::make_unique<SwitchHandler>());
		initialized = true;
	}
	return registry;
}

std::unique_ptr<CExpr> CG::Core::cg::BuildExpr(CBuildContext& ctx, const std::shared_ptr<ANode>& node)
{
	if (!node) return nullptr;

	if (auto it = ctx.expr_cache.find(node.get()); it != ctx.expr_cache.end())
		return CloneExpr(*it->second);

	const ICHandler* handler = DefaultHandlers().Find(node->GetCaller().get());
	if (!handler) return nullptr;

	auto result = handler->BuildExpr(ctx, node);
	if (!result) return nullptr;

	ctx.expr_cache.emplace(node.get(), CloneExpr(*result.value()));
	return std::move(result.value());
}

std::unique_ptr<CExpr> CG::Core::cg::BuildExpr(CBuildContext& ctx, const std::shared_ptr<ANode>& node, const std::string& arg_name)
{
	if (auto upstream = GetFromNode(node, arg_name)) {
		if (dynamic_cast<const FEntry*>(upstream->GetCaller().get())) {
			auto out_name = GetFromOutArg(node, arg_name);
			if (!out_name.empty())
				return MakeLiteral(out_name);
		}
		return BuildExpr(ctx, upstream);
	}
	if (ctx.method_params.count(arg_name))
		return MakeLiteral(arg_name);

	auto pin = FindInPin(node, arg_name);
	if (!pin) return nullptr;
	return MakeLiteral(ToLiteral(pin->GetFrame()));
}

std::unique_ptr<CStmt> CG::Core::cg::BuildStmtChain(CBuildContext& ctx, const std::shared_ptr<ANode>& entry)
{
	std::vector<std::unique_ptr<CStmt>> statements;
	auto node = entry;
	while (node) {
		auto caller = node->GetCaller();
		if (dynamic_cast<const FEntry*>(caller.get())) {
			node = NextExec(node, 0);
			continue;
		}

		const ICHandler* handler = DefaultHandlers().Find(caller.get());
		if (!handler) break;

		if (auto stmt = handler->BuildStmt(ctx, node))
			statements.push_back(std::move(stmt.value()));

		if (IsController(caller.get()))
			break;

		node = NextExec(node, 0);
	}

	if (statements.empty()) return nullptr;
	return MakeCompound(std::move(statements));
}
