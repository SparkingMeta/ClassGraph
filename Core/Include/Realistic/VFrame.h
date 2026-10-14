#pragma once
#include "Abstract/AFrame.h"

namespace CG::Core {
	class VFrame : public AFrame {
	public:
		explicit VFrame(const VType* type);
		VFrame(const VType* type, bool wildcard);
		virtual ~VFrame();

		VFrame(const VFrame& other);
		VFrame(VFrame&& other) noexcept;

		VFrame& operator=(const VFrame& other) = delete;
		VFrame& operator=(VFrame&& other) = delete;

		bool IsValid() const override;
		bool Reset(const VType* type);
		bool Update(const std::shared_ptr<AFrame>& frame) override;
		bool GetValue(const VType* type, void* value) const override;
		bool SetValue(const VType* type, void* value) override;

		const VType* GetType() const override;
		std::byte* GetBuffer() const override; 
		bool IsWildcard() const override;

	private:
		std::byte* Malloc(size_t size, size_t align);
		void Free(std::byte* ptr, size_t align);
		void Clear();
	private:
		std::byte* data_{ nullptr };
		const VType* type_{ nullptr };
		bool wildcard_{ false };
	};

}
