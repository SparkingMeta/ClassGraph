#include "Base/VMeta.h"

using namespace CG::Core;

LayoutMeta CG::Core::BuildLayout(const ExecMeta& exec_meta, const CallMeta& call_meta)
{
	LayoutMeta layout;
	layout.name = call_meta.name;
	if (exec_meta.has_in)
		layout.inputs.push_back(PinMeta{ {}, PinKind::Exec });
	if (exec_meta.has_out)
		layout.outputs.push_back(PinMeta{ {}, PinKind::Exec });

	for (const auto& arg : call_meta.args) {
		PinMeta pin_meta;
		pin_meta.name = arg.name;
		pin_meta.type = arg.type;
		pin_meta.value = arg.value;
		pin_meta.kind = arg.is_runtime ? PinKind::Data : PinKind::Exec;

		if (arg.is_output)
			layout.outputs.push_back(pin_meta);
		else
			layout.inputs.push_back(pin_meta);
	}
	return layout;
}
