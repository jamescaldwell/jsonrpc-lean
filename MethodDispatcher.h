#pragma once
#include "server.h"

using namespace jsonrpc;

namespace RCC_NativeService
{
	class MethodDispatcher
	{
	public:
		MethodDispatcher();
		~MethodDispatcher();
		virtual void AddMethods(Dispatcher & dispatcher) = 0;
	};

#define REGISTER_METHOD(method) \
	dispatcher.AddMethod(#method, &RCCMethods::##method)

}