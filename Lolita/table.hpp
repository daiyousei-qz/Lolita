#pragma once
#include <vector>
#include <cassert>

namespace lolita
{
	template <typename T>
	class Table
	{
	public:
		Table(size_t columns)
			: columns_(columns) { }
		Table(size_t columns, size_t rows, const T& value = {})
			: columns_(columns)
			, data_(columns*rows, value) { }

		const auto& Data() const { return data_; }

		size_t Size() const { return data_.size(); }
		size_t Columns() const { return columns_; }
		size_t Rows() const { return Size() / Columns(); }

		bool Empty() const { return Size() == 0; }

		T& At(size_t row, size_t column)
		{
			return data_[CalculateIndex(row, column)];
		}
		const T& At(size_t row, size_t column) const
		{
			return data_[CalculateIndex(row, column)];
		}

		void Clear()
		{
			data_.clear();
		}
		void AddRow(const T& value = {})
		{
			data_.resize(Size() + Columns(), value);
		}
		void RemoveRow()
		{
			assert(!data_.empty());
			data_.resize(Size() - Columns());
		}

	private:
		size_t CalculateIndex(size_t row, size_t column) const
		{
			assert(row < Rows() && column < Columns());
			return row * columns_ + column;
		}

	private:
		size_t columns_;
		std::vector<T> data_;
	};

	template <typename T>
	inline bool operator==(const Table<T>& lhs, const Table<T>& rhs)
	{
		return lhs.Columns() == rhs.Columns() && lhs.Data() == rhs.Data();
	}

	template <typename T>
	inline bool operator!=(const Table<T>& lhs, const Table<T>& rhs)
	{
		return !(lhs == rhs);
	}

	template <typename T>
	inline T& operator[](Table<T>& ins, size_t row, size_t column)
	{
		return ins.At(row, column);
	}

	template <typename T>
	inline const T& operator[](const Table<T>& ins, size_t row, size_t column)
	{
		return ins.At(row, column);
	}
}