#pragma once
#include "Abstract/APin.h"
#include "Abstract/AFrame.h"
#include <set>

namespace CG::Core {

	class VExecPin : public APin {
	public:
		VExecPin(const std::shared_ptr<ANode>& node, EDirect direct);
		void BindLine(std::shared_ptr<ALine> line) override;
		void UnbindLine(std::shared_ptr<ALine> line) override;
		std::shared_ptr<ALine> GetLine() const override;
		void SetFrame(const std::shared_ptr<AFrame>& frame) override;
		std::shared_ptr<AFrame> GetFrame() const override;
		void Update() override;
	private:
		std::shared_ptr<ALine> line_;
	};

	class VDataPin : public APin{
	public:
		VDataPin(const std::shared_ptr<ANode>& node, EDirect direct);
		void BindLine(std::shared_ptr<ALine> line) override;
		void UnbindLine(std::shared_ptr<ALine> line) override;
		std::shared_ptr<ALine> GetLine() const override;
		std::vector<std::shared_ptr<ALine>> GetLines() const override;
		void SetFrame(const std::shared_ptr<AFrame>& frame) override;
		std::shared_ptr<AFrame> GetFrame() const override;
		void Update() override;
	private:
		std::set<std::shared_ptr<ALine>> lines_;
		std::weak_ptr<AFrame> data_;
	};

	/*class VDataExecPin : public VExecPin {
	public:
		void SetFrame(AFrame* frame) override;
		AFrame* GetFrame() const override;
		void Update() override;
	private:
		AFrame* data_;
	};*/
}