#pragma once

#include <LinkedList.h>

template<class T>
struct CLinkListIterator
{
	using difference_type = std::ptrdiff_t;
	using value_type = T;

	CLLNode<T>* node = nullptr;

	T& operator*() const
	{
		return node->m_data;
	}

	T* operator->() const
	{
		return &node->m_data;
	}

	auto& operator++()
	{
		node = CLinkList<T>::next(node);
		return *this;
	}

	auto operator++(int)
	{
		auto tmp = *this;
		++*this;
		return tmp;
	}

	bool operator==(const CLinkListIterator&) const = default;
};

template<class T>
struct CLinkListNodeIterator
{
	using difference_type = std::ptrdiff_t;
	using value_type = CLLNode<T>;

	CLLNode<T>* node = nullptr;

	CLLNode<T>& operator*() const
	{
		return *node;
	}

	auto& operator++()
	{
		node = CLinkList<T>::next(node);
		return *this;
	}

	auto operator++(int)
	{
		auto tmp = *this;
		++*this;
		return tmp;
	}

	bool operator==(const CLinkListNodeIterator&) const = default;
};

static_assert(std::forward_iterator<CLinkListIterator<int>>);

template<class T>
inline std::ranges::subrange<CLinkListIterator<T>> viewCLinkList(const CLinkList<T>& list)
{
	return { CLinkListIterator<T>(list.head()), CLinkListIterator<T>() };
}

template<class T>
inline std::ranges::subrange<CLinkListNodeIterator<T>> viewCLinkListNodes(const CLinkList<T>& list)
{
	return { CLinkListNodeIterator<T>(list.head()), CLinkListNodeIterator<T>() };
}