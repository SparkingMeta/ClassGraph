#pragma once

#include "Export.h"

namespace ui {

	class MACRO_UI_API Application {
	public:
		static int Run(int argc, char* argv[]);

	protected:
		Application() = default;
	};

}  // namespace ui
