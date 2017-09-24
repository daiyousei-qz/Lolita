#pragma once

namespace eds
{
	template <typename IterType>
	class Range
	{
	public:
		using iterator = typename IterType;

		Range(iterator begin, iterator end)
			: begin_(begin), end_(end) { }

		iterator begin() { return begin_; }
		iterator end() { return end_; }

	private:
		iterator begin_;
		iterator end_;
	};
}