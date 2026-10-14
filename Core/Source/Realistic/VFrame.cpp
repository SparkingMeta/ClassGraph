#include "Realistic/VFrame.h"
#include "Base/VType.h"
using namespace CG::Core;

CG::Core::VFrame::VFrame(const VType* type) : VFrame(type, false)
{
}

CG::Core::VFrame::VFrame(const VType* type, bool wildcard)
	: AFrame(), type_(type), wildcard_(wildcard)
{
	if (type_ && type_->construct_) {
		data_ = Malloc(type_->size_, type_->align_);
		type_->construct_(data_);
	}
}

CG::Core::VFrame::VFrame(const VFrame& other) : type_(other.type_), wildcard_(other.wildcard_)
{
	if (other.data_ && other.type_ && other.type_->copy_construct_) {
		data_ = Malloc(type_->size_, type_->align_);
		type_->copy_construct_(data_, other.data_);
	}
}

CG::Core::VFrame::VFrame(VFrame&& other) noexcept
	: data_(other.data_), type_(other.type_), wildcard_(other.wildcard_)
{
	other.data_ = nullptr;
	other.type_ = nullptr;
	other.wildcard_ = false;
}

CG::Core::VFrame::~VFrame()
{
	Clear();
}

bool CG::Core::VFrame::IsValid() const
{
	return data_ && type_;
}

bool CG::Core::VFrame::Reset(const VType* type)
{
	if (!type || !type->construct_)
		return false;

	Clear();
	type_ = type;

	data_ = Malloc(type_->size_, type_->align_);
	type_->construct_(data_);

	return true;
}

bool VFrame::Update(const std::shared_ptr<AFrame>& frame)
{
	bool ret = false;
	do {
		if (!frame || frame.get() == this) break;

		if (!frame->IsValid()) break;

		const VType* type = frame->GetType();
		if (!type) break;

		if (!IsWildcard() && type_ != type)
			break;

		if (type_ != type) {
			if (!Reset(type)) break;
		}

		if (!SetValue(type, frame->GetBuffer()))
			break;

		ret = true;

	} while (0);
	return ret;
}

bool CG::Core::VFrame::GetValue(const VType* type, void* value) const
{
	bool ret = false;

	do {
		if (!type || !value) break;

		if (!this->IsValid()) break;

		if (type != type_ || !type_->copy_assign_) break;

		type_->copy_assign_(value, data_);

		ret = true;
	} while (0);

	return ret;
}

bool CG::Core::VFrame::SetValue(const VType* type, void* value)
{
	bool ret = false;

	do {
		if (!type || !value) break;

		if (!type_) {
			if (!IsWildcard()) break;
			if (!Reset(type)) break;
		}
		else if (IsWildcard() && type_ != type) {
			if (!Reset(type)) break;
		}

		if (!this->IsValid()) break;

		if (type != type_ || !type_->copy_assign_) break;

		type_->copy_assign_(data_, value);

		ret = true;
	} while (0);

	return ret;
}

const VType* CG::Core::VFrame::GetType() const
{
	return type_;
}

std::byte* CG::Core::VFrame::GetBuffer() const
{
	return data_;
}

bool CG::Core::VFrame::IsWildcard() const
{
	return wildcard_;
}

std::byte* CG::Core::VFrame::Malloc(size_t size, size_t align)
{
	std::byte* ptr = reinterpret_cast<std::byte*>(::operator new(size, std::align_val_t(align)));
	if (ptr) std::memset(ptr, 0, size);
	return ptr;
}

void CG::Core::VFrame::Free(std::byte* ptr, size_t align)
{
	if (ptr) {
		::operator delete(ptr, std::align_val_t(align));
	}
}

void CG::Core::VFrame::Clear()
{
	if (data_ && type_) {
		if (type_->destruct_)
			type_->destruct_(data_);
		Free(data_, type_->align_);
	}
	data_ = nullptr;
	type_ = nullptr;
}
