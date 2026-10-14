#include "CodeGen/CEmitter.h"

#include "Base/VCore.h"
#include "Realistic/VBlueprintObject.h"

#include <algorithm>
#include <sstream>

using namespace CG::Core::cg;

namespace {

	std::string QualifiedName(const std::string& space, const std::string& name)
	{
		if (space.empty()) return name;
		return space + "::" + name;
	}

	std::string FileIdToken(const std::string& class_name)
	{
		return class_name + "_h";
	}

	int CountLines(const std::string& text)
	{
		return static_cast<int>(std::count(text.begin(), text.end(), '\n'));
	}

	void EmitInclude(std::ostream& out, const std::string& path)
	{
		if (path.empty()) return;
		if (path.front() == '<' || path.front() == '"') {
			out << "#include " << path << '\n';
			return;
		}
		if (path.find('/') != std::string::npos || path.find('.') != std::string::npos)
			out << "#include \"" << path << "\"\n";
		else
			out << "#include <" << path << ">\n";
	}

	void EmitIncludes(std::ostream& out, const std::set<std::string>& includes)
	{
		for (const auto& inc : includes)
			EmitInclude(out, inc);
	}

	void EmitExpr(std::ostream& out, const CExpr& expr)
	{
		switch (expr.kind) {
		case ExprKind::EK_Literal:
			out << expr.text;
			break;
		case ExprKind::EK_Member:
			out << expr.text;
			break;
		case ExprKind::EK_Binary:
			out << '(';
			EmitExpr(out, *expr.lhs);
			out << ' ' << expr.op << ' ';
			EmitExpr(out, *expr.rhs);
			out << ')';
			break;
		case ExprKind::EK_Unary:
			out << expr.op;
			EmitExpr(out, *expr.lhs);
			break;
		case ExprKind::EK_StreamInsert:
			out << expr.text << " << ";
			if (!expr.args.empty())
				EmitExpr(out, *expr.args.front());
			out << expr.op;
			break;
		case ExprKind::EK_MemberCall:
			EmitExpr(out, *expr.lhs);
			out << (expr.op.empty() ? "->" : expr.op) << expr.text << '(';
			for (size_t i = 0; i < expr.args.size(); ++i) {
				if (i) out << ", ";
				EmitExpr(out, *expr.args[i]);
			}
			out << ')';
			break;
		case ExprKind::EK_Caller:
			out << expr.text << '(';
			for (size_t i = 0; i < expr.args.size(); ++i) {
				if (i) out << ", ";
				EmitExpr(out, *expr.args[i]);
			}
			out << ')';
			break;
		}
	}

	void EmitStmt(std::ostream& out, const CStmt& stmt, int indent)
	{
		auto pad = [&]() {
			for (int i = 0; i < indent; ++i) out << '\t';
		};

		switch (stmt.kind) {
		case StmtKind::SK_Compound:
			for (const auto& child : stmt.statements)
				EmitStmt(out, *child, indent);
			break;
		case StmtKind::SK_Assign:
			pad();
			EmitExpr(out, *stmt.target);
			out << " = ";
			EmitExpr(out, *stmt.value);
			out << ";\n";
			break;
		case StmtKind::SK_ExprStmt:
			pad();
			EmitExpr(out, *stmt.expr);
			out << ";\n";
			break;
		case StmtKind::SK_Return:
			pad();
			out << "return ";
			EmitExpr(out, *stmt.value);
			out << ";\n";
			break;
		case StmtKind::SK_If:
			pad();
			out << "if (";
			EmitExpr(out, *stmt.expr);
			out << ") {\n";
			EmitStmt(out, *stmt.then_br, indent + 1);
			pad();
			out << "} else {\n";
			EmitStmt(out, *stmt.else_br, indent + 1);
			pad();
			out << "}\n";
			break;
		case StmtKind::SK_While:
			pad();
			out << "while (";
			EmitExpr(out, *stmt.expr);
			out << ") {\n";
			EmitStmt(out, *stmt.body, indent + 1);
			pad();
			out << "}\n";
			break;
		case StmtKind::SK_For:
			pad();
			out << "for (int " << stmt.index_name << " = ";
			EmitExpr(out, *stmt.first);
			out << "; " << stmt.index_name << " < ";
			EmitExpr(out, *stmt.last);
			out << "; ++" << stmt.index_name << ") {\n";
			EmitStmt(out, *stmt.body, indent + 1);
			pad();
			out << "}\n";
			break;
		case StmtKind::SK_Switch:
			pad();
			out << "switch (";
			EmitExpr(out, *stmt.expr);
			out << ") {\n";
			for (const auto& c : stmt.cases) {
				pad();
				if (c.label == "default")
					out << "default:\n";
				else
					out << "case " << c.label << ":\n";
				EmitStmt(out, *c.body, indent + 1);
				pad();
				out << "\tbreak;\n";
			}
			pad();
			out << "}\n";
			break;
		}
	}

	void EmitMethodParams(std::ostream& out, const CMethod& method)
	{
		for (size_t i = 0; i < method.params.size(); ++i) {
			if (i) out << ", ";
			out << method.params[i].type << ' ' << method.params[i].name;
		}
	}

	void EmitClassHeader(std::ostream& out, const CClass& cls, int& body_line)
	{
		std::ostringstream prefix;
		prefix << "#pragma once\n\n";
		EmitInclude(prefix, "Base/VClass.h");
		EmitIncludes(prefix, cls.includes);
		prefix << "#include \"" << cls.name << ".gen.h\"\n\n";

		if (!cls.space.empty())
			prefix << "namespace " << cls.space << " {\n\n";

		prefix << "class " << cls.name << " : public " << cls.parent << " {\n";
		prefix << "public:\n";

		body_line = CountLines(prefix.str()) + 1;
		out << prefix.str();
		out << "\tGENERATED_BODY()\n";

		for (const auto& field : cls.fields) {
			out << "\t" << field.type << ' ' << field.name;
			if (!field.value.empty())
				out << " = " << field.value;
			out << ";\n";
		}
		if (!cls.fields.empty())
			out << '\n';

		for (const auto& method : cls.methods) {
			out << '\t' << method.return_type << ' ' << method.name << '(';
			EmitMethodParams(out, method);
			out << ");\n";
		}
		out << "};\n";

		if (!cls.space.empty())
			out << "\n} // namespace " << cls.space << '\n';
	}

	void EmitClassGenHeader(std::ostream& out, const CClass& cls, int body_line)
	{
		const std::string file_id = FileIdToken(cls.name);
		const std::string construct_fn = "Z_Construct_VClass_" + cls.name;

		out << "#pragma once\n\n";
		out << "#undef CURRENT_FILE_ID\n";
		out << "#define CURRENT_FILE_ID " << file_id << "\n\n";
		out << "namespace CG::Core {\n\tclass VClass;\n}\n\n";
		out << "CG::Core::VClass* " << construct_fn << "();\n\n";
		out << "#define " << file_id << '_' << body_line << "_GENERATED_BODY \\\n";
		out << "public: \\\n";
		out << "\tinline static const CG::Core::VClass* StaticClass() { \\\n";
		out << "\t\treturn " << construct_fn << "(); \\\n";
		out << "\t} \\\n";
		out << "\tconst CG::Core::VClass* GetClass() const override { \\\n";
		out << "\t\treturn StaticClass(); \\\n";
		out << "\t}\n";
	}

	void EmitClassSource(std::ostream& out, const CClass& cls)
	{
		const std::string qualified = QualifiedName(cls.space, cls.name);

		out << "#include \"" << cls.name << ".h\"\n";
		EmitIncludes(out, cls.source_includes);
		out << '\n';

		for (const auto& method : cls.methods) {
			out << method.return_type << ' ' << qualified << "::" << method.name << '(';
			EmitMethodParams(out, method);
			out << ") {\n";
			if (method.body)
				EmitStmt(out, *method.body, 1);
			out << "}\n\n";
		}
	}

	void EmitClassGenSource(std::ostream& out, const CClass& cls)
	{
		const std::string construct_fn = "Z_Construct_VClass_" + cls.name;
		const std::string qualified = QualifiedName(cls.space, cls.name);
		const std::string qualified_class = cls.space.empty() ? cls.name : cls.space + "::" + cls.name;

		out << "#include \"" << cls.name << ".h\"\n";
		out << "#include \"" << cls.parent_header << "\"\n";
		out << "#include \"Base/VClass.h\"\n";
		out << "#include \"Base/VSystem.h\"\n";
		out << "#include \"Base/VRegister.h\"\n";
		out << "#include \"Base/VType.h\"\n";
		out << "#include \"Base/VObject.h\"\n";
		out << "#include \"Realistic/VFunction.h\"\n";
		out << "#include \"Realistic/VProperty.h\"\n\n";

		out << "CG::Core::VClass* " << construct_fn << "() {\n";
		out << "\tstatic CG::Core::VClass* c = []() {\n";
		out << "\t\tCG::Core::RegisterClassType<" << qualified << ">(\"" << qualified << "\", \""
			<< cls.space << "/" << cls.name << ".h\");\n";
		out << "\t\tauto* cls = CG::Core::CreateClass<" << qualified_class << ", " << cls.parent << ">(\""
			<< cls.space << "\", \"" << cls.name << "\");\n";
		out << "\t\tcls->SetCreator([](const std::string& name) -> CG::Core::VObject* {\n";
		out << "\t\t\tauto* obj = new " << qualified_class << "();\n";
		out << "\t\t\tif (!name.empty()) obj->SetName(name);\n";
		out << "\t\t\treturn obj;\n";
		out << "\t\t});\n";

		for (const auto& field : cls.fields) {
			out << "\t\tcls->AddProperty(CG::Core::LoadProperty(&" << qualified << "::" << field.name
				<< ", \"" << field.name << "\"));\n";
		}

		for (const auto& method : cls.methods) {
			out << "\t\t{\n";
			out << "\t\t\tauto fn = std::make_shared<CG::Core::VFunction>();\n";
			out << "\t\t\tfn->SetName(\"" << method.name << "\");\n";
			out << "\t\t\tauto meta = CG::Core::Load(&" << qualified << "::" << method.name;
			for (const auto& param : method.params)
				out << ", \"" << param.name << "\"";
			out << ");\n";
			out << "\t\t\tmeta.owner = cls;\n";
			out << "\t\t\tfn->SetMeta(meta);\n";
			out << "\t\t\tcls->AddMethod(fn);\n";
			out << "\t\t}\n";
		}

		out << "\t\tCG::Core::VSystem::Get().RegisterCls(cls);\n";
		out << "\t\treturn cls;\n";
		out << "\t}();\n";
		out << "\treturn c;\n";
		out << "}\n\n";
		out << "#if defined(_WIN32)\n";
		out << "#define CLASSGRAPH_TYPE_API __declspec(dllexport)\n";
		out << "#else\n";
		out << "#define CLASSGRAPH_TYPE_API __attribute__((visibility(\"default\")))\n";
		out << "#endif\n\n";
		out << "extern \"C\" CLASSGRAPH_TYPE_API CG::Core::VClass* ClassGraphLoadType() {\n";
		out << "\treturn " << construct_fn << "();\n";
		out << "}\n";
	}

	void EmitClass(const CClass& cls, COutputs& out)
	{
		int body_line = 0;
		std::ostringstream hdr, gen_h, src, gen;
		EmitClassHeader(hdr, cls, body_line);
		EmitClassGenHeader(gen_h, cls, body_line);
		EmitClassSource(src, cls);
		EmitClassGenSource(gen, cls);

		out.header += hdr.str();
		out.gen_header += gen_h.str();
		out.source += src.str();
		out.gen_source += gen.str();
	}

} // namespace

void CEmitter::Emit(const CUnit& unit, COutputs& out)
{
	for (const auto& cls : unit.classes)
		EmitClass(cls, out);
}

void CEmitter::Emit(const CUnit& unit, std::string& header, std::string& gen_header,
	std::string& source, std::string& gen_source)
{
	COutputs out;
	Emit(unit, out);
	header = std::move(out.header);
	gen_header = std::move(out.gen_header);
	source = std::move(out.source);
	gen_source = std::move(out.gen_source);
}
