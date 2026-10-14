#include "Abstract/AController.h"
#include "Abstract/ANode.h"
#include "Abstract/APin.h"

using namespace CG::Core;

bool CG::Core::AController::IsPure() const
{
	return false;
}

int CG::Core::AController::GetKind() const
{
	return CK_Controller;
}

CallMeta CG::Core::AController::GetCallMeta() const
{
	return meta_;
}

Result AController::Call(VObject* object, AContext* ctx) {
	int code = -1;
	bool flag = false;
	do {
		ANode* node = object->Cast<ANode>();
		if (!node) break;

		auto status = Init(ctx);

		code = status.code;
		if (code != 0) break;

		bool loop = false;
		do {
			Invoke(ctx, status);

			code = status.code;
			if (code != 0) break;

			std::string token = node->GetToken();
			auto next = node->Next(status.index);
			while (next) {
				auto result = next->Work(token);
				code = result.code;
				if (code != 0)
					break;
				next = next->Next(result.index);
			}

			if (code != 0)
				break;

			loop = status.loop;
		} while (loop);

		flag = status.flag;
	} while (0);

	return Result{ {}, code, -1, flag};
}