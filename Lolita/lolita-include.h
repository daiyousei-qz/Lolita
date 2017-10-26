#pragma once
#include "ast-basic.h"
#include "ast-proxy.h"

namespace eds::loli
{
	// TODO: refine this
	template <typename T>
	void QuickAssignField(T& dest, AstTypeWrapper item)
	{
		dest = item.Extract<T>();
	}
}