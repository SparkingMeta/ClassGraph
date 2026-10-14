#include "CodeGen/CCompiler.h"

#include "CodeGen/CHandler.h"
#include "CodeGen/CContext.h"
#include "Abstract/AFlow.h"
#include "Abstract/AGraph.h"
#include "Abstract/AVariant.h"
#include "CodeGen/CEmitter.h"

using namespace CG::Core;
using namespace CG::Core::cg;

namespace {

	bool StmtReturns(const CStmt* stmt)
	{
		if (!stmt) return false;
		switch (stmt->kind) {
		case StmtKind::SK_Return:
			return true;
		case StmtKind::SK_Compound:
			for (const auto& child : stmt->statements) {
				if (StmtReturns(child.get()))
					return true;
			}
			return false;
		case StmtKind::SK_If:
			return StmtReturns(stmt->then_br.get()) && StmtReturns(stmt->else_br.get());
		case StmtKind::SK_Switch:
			if (stmt->cases.empty())
				return false;
			for (const auto& c : stmt->cases) {
				if (!StmtReturns(c.body.get()))
					return false;
			}
			return true;
		default:
			return false;
		}
	}

	void EnsureReturn(CMethod& method)
	{
		if (method.return_type == "void" || StmtReturns(method.body.get()))
			return;
		std::vector<std::unique_ptr<CStmt>> stmts;
		if (method.body && method.body->kind == StmtKind::SK_Compound)
			stmts = std::move(method.body->statements);
		else if (method.body)
			stmts.push_back(std::move(method.body));
		stmts.push_back(MakeReturn(MakeLiteral("{}")));
		method.body = MakeCompound(std::move(stmts));
	}

	CMethod CompileFlow(CBuildContext& ctx, const std::shared_ptr<AFlow>& flow, std::set<std::string>& header_includes)
	{
		CMethod method;
		method.name = flow->GetName();

		const FlowMeta& flow_meta = flow->GetFlowMeta();
		FillMethodFromFlowMeta(method, flow_meta, header_includes);
		CollectFlowMethodParams(ctx, flow_meta);

		auto graph = flow->GetGraph();
		if (graph) {
			method.body = BuildStmtChain(ctx, graph->GetEntry());
			if (!method.body)
				method.body = MakeCompound({});
		} else {
			method.body = MakeCompound({});
		}
		EnsureReturn(method);
		return method;
	}

} // namespace

int CCompiler::Compile(const VBlueprint& blueprint, std::string& source, std::string& header,
	std::string& gen_header, std::string& meta)
{
	CUnit unit;
	CClass cls;
	cls.space = blueprint.GetSpace();
	cls.name = blueprint.GetName();
	if (cls.name.empty())
		return -1;

	cls.parent = blueprint.GetParentClass();
	if (cls.parent.empty())
		cls.parent = kBlueprintDefaultParent;

	cls.parent_header = blueprint.GetParentHeader();
	if (cls.parent_header.empty())
		cls.parent_header = kBlueprintDefaultParentHeader;

	cls.source_guid = blueprint.GetGuid();
	cls.includes.insert(cls.parent_header);

	CBuildContext ctx;
	ctx.blueprint = &blueprint;
	ctx.class_name = cls.name;

	for (const auto& [guid, var] : blueprint.GetVars()) {
		if (!var) continue;
		cls.fields.push_back({ CastType(var->GetType()), var->GetName(), ToLiteral(var->Getter()) });
		CollectTypeHeader(cls.includes, var->GetType());
	}

	for (const auto& [guid, flow] : blueprint.GetFlows()) {
		if (!flow) continue;
		cls.methods.push_back(CompileFlow(ctx, flow, cls.includes));
	}

	for (const auto& inc : ctx.includes)
		cls.source_includes.insert(inc);

	unit.classes.push_back(std::move(cls));
	CEmitter::Emit(unit, header, gen_header, source, meta);
	return 0;
}
