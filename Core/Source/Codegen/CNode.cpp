#include "CodeGen/CNode.h"

using namespace CG::Core::cg;

std::unique_ptr<CExpr> CG::Core::cg::MakeLiteral(std::string value)
{
	auto e = std::make_unique<CExpr>();
	e->kind = ExprKind::EK_Literal;
	e->text = std::move(value);
	return e;
}

std::unique_ptr<CExpr> CG::Core::cg::MakeMember(std::string name)
{
	auto e = std::make_unique<CExpr>();
	e->kind = ExprKind::EK_Member;
	e->text = std::move(name);
	return e;
}

std::unique_ptr<CExpr> CG::Core::cg::MakeBinary(std::string op, std::unique_ptr<CExpr> lhs, std::unique_ptr<CExpr> rhs)
{
	auto e = std::make_unique<CExpr>();
	e->kind = ExprKind::EK_Binary;
	e->op = std::move(op);
	e->lhs = std::move(lhs);
	e->rhs = std::move(rhs);
	return e;
}

std::unique_ptr<CExpr> CG::Core::cg::MakeUnary(std::string op, std::unique_ptr<CExpr> operand)
{
	auto e = std::make_unique<CExpr>();
	e->kind = ExprKind::EK_Unary;
	e->op = std::move(op);
	e->lhs = std::move(operand);
	return e;
}

std::unique_ptr<CExpr> CG::Core::cg::MakeStreamInsert(std::string stream, std::string suffix, std::unique_ptr<CExpr> value)
{
	auto e = std::make_unique<CExpr>();
	e->kind = ExprKind::EK_StreamInsert;
	e->text = std::move(stream);
	e->op = std::move(suffix);
	e->args.push_back(std::move(value));
	return e;
}

std::unique_ptr<CExpr> CG::Core::cg::MakeMemberCall(std::unique_ptr<CExpr> object, std::string method,
	std::vector<std::unique_ptr<CExpr>> args)
{
	auto e = std::make_unique<CExpr>();
	e->kind = ExprKind::EK_MemberCall;
	e->lhs = std::move(object);
	e->text = std::move(method);
	e->args = std::move(args);
	return e;
}

std::unique_ptr<CExpr> CG::Core::cg::MakeCall(std::string callee, std::vector<std::unique_ptr<CExpr>> args)
{
	auto e = std::make_unique<CExpr>();
	e->kind = ExprKind::EK_Caller;
	e->text = std::move(callee);
	e->args = std::move(args);
	return e;
}

std::unique_ptr<CStmt> CG::Core::cg::MakeCompound(std::vector<std::unique_ptr<CStmt>> statements)
{
	auto s = std::make_unique<CStmt>();
	s->kind = StmtKind::SK_Compound;
	s->statements = std::move(statements);
	return s;
}

std::unique_ptr<CStmt> CG::Core::cg::MakeDeclare(std::string type, std::string name, std::unique_ptr<CExpr> value)
{
	auto s = std::make_unique<CStmt>();
	s->kind = StmtKind::SK_Declare;
	s->declaration_type = std::move(type);
	s->declaration_name = std::move(name);
	s->value = std::move(value);
	return s;
}

std::unique_ptr<CStmt> CG::Core::cg::MakeAssign(std::unique_ptr<CExpr> target, std::unique_ptr<CExpr> value)
{
	auto s = std::make_unique<CStmt>();
	s->kind = StmtKind::SK_Assign;
	s->target = std::move(target);
	s->value = std::move(value);
	return s;
}

std::unique_ptr<CStmt> CG::Core::cg::MakeExprStmt(std::unique_ptr<CExpr> expr)
{
	auto s = std::make_unique<CStmt>();
	s->kind = StmtKind::SK_ExprStmt;
	s->expr = std::move(expr);
	return s;
}

std::unique_ptr<CStmt> CG::Core::cg::MakeReturn(std::unique_ptr<CExpr> value)
{
	auto s = std::make_unique<CStmt>();
	s->kind = StmtKind::SK_Return;
	s->value = std::move(value);
	return s;
}

std::unique_ptr<CStmt> CG::Core::cg::MakeIf(std::unique_ptr<CExpr> cond, std::unique_ptr<CStmt> then_br, std::unique_ptr<CStmt> else_br)
{
	auto s = std::make_unique<CStmt>();
	s->kind = StmtKind::SK_If;
	s->expr = std::move(cond);
	s->then_br = std::move(then_br);
	s->else_br = std::move(else_br);
	return s;
}

std::unique_ptr<CStmt> CG::Core::cg::MakeWhile(std::unique_ptr<CExpr> cond, std::unique_ptr<CStmt> body)
{
	auto s = std::make_unique<CStmt>();
	s->kind = StmtKind::SK_While;
	s->expr = std::move(cond);
	s->body = std::move(body);
	return s;
}

std::unique_ptr<CStmt> CG::Core::cg::MakeFor(std::string index_name, std::unique_ptr<CExpr> first,
	std::unique_ptr<CExpr> last, std::unique_ptr<CStmt> body, std::string result_name,
	std::string result_type)
{
	auto s = std::make_unique<CStmt>();
	s->kind = StmtKind::SK_For;
	s->index_name = std::move(index_name);
	s->result_name = std::move(result_name);
	s->result_type = std::move(result_type);
	s->first = std::move(first);
	s->last = std::move(last);
	s->body = std::move(body);
	return s;
}

std::unique_ptr<CStmt> CG::Core::cg::MakeSwitch(std::unique_ptr<CExpr> cond, std::vector<CCase> cases)
{
	auto s = std::make_unique<CStmt>();
	s->kind = StmtKind::SK_Switch;
	s->expr = std::move(cond);
	s->cases = std::move(cases);
	return s;
}

std::unique_ptr<CExpr> CG::Core::cg::CloneExpr(const CExpr& expr)
{
	auto e = std::make_unique<CExpr>();
	e->kind = expr.kind;
	e->text = expr.text;
	e->op = expr.op;
	if (expr.lhs) e->lhs = CloneExpr(*expr.lhs);
	if (expr.rhs) e->rhs = CloneExpr(*expr.rhs);
	e->args.reserve(expr.args.size());
	for (const auto& arg : expr.args)
		e->args.push_back(CloneExpr(*arg));
	return e;
}
