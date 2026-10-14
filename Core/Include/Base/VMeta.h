#pragma once
#include "Base/VCore.h"
#include <vector>

namespace CG::Core {
	struct VType;

	enum class PinDirect : uint8_t { In, Out };
	enum class PinKind : uint8_t { Exec, Data };

	struct Meta : Struct {
		std::string name{};
	};

	struct ArgMeta : Meta {
		bool is_output{false};
		bool is_runtime{true};
		const VType* type{};
	};

	struct CallMeta : Meta {
		bool pure{false};
		std::vector<ArgMeta> args;
	};

	struct FlowMeta : Meta {
		std::vector<ArgMeta> params;
	};

	struct ExecMeta : Meta {
		bool has_in{false};
		bool has_out{ false };
	};
	
	struct PinMeta : Meta {
		PinKind kind;
		const VType* type{};
	};

	struct LayoutMeta : Meta {
		std::vector<PinMeta> inputs;
		std::vector<PinMeta> outputs;
	};

	// CodeGen: how a VFunction node lowers to a C++ expression
	enum class EmitKind : uint8_t { EmitCall, EmitBinary, EmitUnary, EmitSymbol };

	struct EmitMeta : Meta {
		EmitKind emit_kind{ EmitKind::EmitCall };
		std::string op;      // binary/unary operator, e.g. ">", "!"
		std::string callee;  // call target or stream object; empty falls back to node name
		std::string suffix;  // stream-insert trailing text, e.g. " << std::endl"
		std::vector<std::string> includes;
	};

	struct Layout{
		std::vector<PinMeta> inputs;
		std::vector<PinMeta> outputs;
	};

	struct NodeMeta : Meta {
		CallMeta call_meta;
	};

	LayoutMeta BuildLayout(const ExecMeta& exec_meta, const CallMeta& call_meta);
}
