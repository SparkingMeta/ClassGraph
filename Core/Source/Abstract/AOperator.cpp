#include "Abstract/AOperator.h"
#include "Abstract/ANode.h"

using namespace CG::Core;

bool CG::Core::AOperator::IsPure() const
{
	return false;
}

int CG::Core::AOperator::GetKind() const
{
	return CK_Operator;
}

CallMeta CG::Core::AOperator::GetCallMeta() const
{
	return meta_;
}
