#pragma once

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace CG::Core::cg {

	enum class ExprKind {
		EK_Literal,
		EK_Member,
		EK_Binary,
		EK_Unary,
		EK_StreamInsert,
		EK_MemberCall,
		EK_Caller,
	};

	struct CExpr {
		ExprKind kind{ ExprKind::EK_Literal };
		std::string text;
		std::string op;
		std::unique_ptr<CExpr> lhs;
		std::unique_ptr<CExpr> rhs;
		std::vector<std::unique_ptr<CExpr>> args;
	};

	enum class StmtKind {
		SK_Compound,
		SK_Declare,
		SK_Assign,
		SK_ExprStmt,
		SK_Return,
		SK_If,
		SK_While,
		SK_For,
		SK_Switch,
	};

	struct CStmt;

	struct CCase {
		std::string label;
		std::unique_ptr<CStmt> body;
	};

	struct CStmt {
		StmtKind kind{ StmtKind::SK_Compound };
		std::unique_ptr<CExpr> expr;
		std::unique_ptr<CExpr> target;
		std::unique_ptr<CExpr> value;
		std::unique_ptr<CStmt> then_br;
		std::unique_ptr<CStmt> else_br;
		std::unique_ptr<CStmt> body;
		std::vector<std::unique_ptr<CStmt>> statements;
		std::vector<CCase> cases;
		std::string declaration_type;
		std::string declaration_name;
		std::string index_name;
		std::string result_name;
		std::string result_type;
		std::unique_ptr<CExpr> first;
		std::unique_ptr<CExpr> last;
	};

	struct CField {
		std::string type;
		std::string name;
		std::string value;
	};

	struct CMethod {
		std::string name;
		std::string return_type{"void"};
		std::vector<CField> params;
		std::unique_ptr<CStmt> body;

		CMethod() = default;
		CMethod(CMethod&&) = default;
		CMethod& operator=(CMethod&&) = default;
		CMethod(const CMethod&) = delete;
		CMethod& operator=(const CMethod&) = delete;
	};

	struct CClass {
		std::string space;
		std::string name;
		std::string parent;
		std::string parent_header;
		std::string source_guid;
		std::vector<CField> fields;
		std::vector<CMethod> methods;
		std::set<std::string> includes;
		std::set<std::string> source_includes;

		CClass() = default;
		CClass(CClass&&) = default;
		CClass& operator=(CClass&&) = default;
		CClass(const CClass&) = delete;
		CClass& operator=(const CClass&) = delete;
	};

	struct CUnit {
		std::vector<CClass> classes;
	};

	std::unique_ptr<CExpr> MakeLiteral(std::string value);
	std::unique_ptr<CExpr> MakeMember(std::string name);
	std::unique_ptr<CExpr> MakeBinary(std::string op, std::unique_ptr<CExpr> lhs, std::unique_ptr<CExpr> rhs);
	std::unique_ptr<CExpr> MakeUnary(std::string op, std::unique_ptr<CExpr> operand);
	std::unique_ptr<CExpr> MakeStreamInsert(std::string stream, std::string suffix, std::unique_ptr<CExpr> value);
	std::unique_ptr<CExpr> MakeMemberCall(std::unique_ptr<CExpr> object, std::string method,
		std::vector<std::unique_ptr<CExpr>> args);
	std::unique_ptr<CExpr> MakeCall(std::string callee, std::vector<std::unique_ptr<CExpr>> args);
	std::unique_ptr<CStmt> MakeCompound(std::vector<std::unique_ptr<CStmt>> statements);
	std::unique_ptr<CStmt> MakeDeclare(std::string type, std::string name, std::unique_ptr<CExpr> value = {});
	std::unique_ptr<CStmt> MakeAssign(std::unique_ptr<CExpr> target, std::unique_ptr<CExpr> value);
	std::unique_ptr<CStmt> MakeExprStmt(std::unique_ptr<CExpr> expr);
	std::unique_ptr<CStmt> MakeReturn(std::unique_ptr<CExpr> value);
	std::unique_ptr<CStmt> MakeIf(std::unique_ptr<CExpr> cond, std::unique_ptr<CStmt> then_br, std::unique_ptr<CStmt> else_br);
	std::unique_ptr<CStmt> MakeWhile(std::unique_ptr<CExpr> cond, std::unique_ptr<CStmt> body);
	std::unique_ptr<CStmt> MakeFor(std::string index_name, std::unique_ptr<CExpr> first,
		std::unique_ptr<CExpr> last, std::unique_ptr<CStmt> body, std::string result_name = {},
		std::string result_type = {});
	std::unique_ptr<CStmt> MakeSwitch(std::unique_ptr<CExpr> cond, std::vector<CCase> cases);

	std::unique_ptr<CExpr> CloneExpr(const CExpr& expr);

} // namespace CG::Core::cg
