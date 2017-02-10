#ifndef NAMEDPIPESERVER_H
#define NAMEDPIPESERVER_H

#include <windows.h>
#include <tchar.h>
#include <string>
#include "server.h"
#include "MethodDispatcher.h"

using namespace std;
using namespace jsonrpc;

namespace RCC_NativeService
{
	const int BUFSIZE = 4096;

	class NamedPipeServer
	{
	public:
		NamedPipeServer(string &pipeName);
		~NamedPipeServer();
		void AddDispatcher(MethodDispatcher * methods);
		bool Start();
		void Stop();
		static DWORD WINAPI ConnectThread(LPVOID lp_data);
		void ConnectThread2();
		static DWORD WINAPI TalkThread(LPVOID lp_data);
		void TalkThread2();


		string & GetPipeName() { return m_PipeName; }

	private:
		string	m_PipeName;
		HANDLE  m_PipeHandle;
		HANDLE  m_hStoppedEvent;
		HANDLE	m_ListenerThread;
		bool	m_running;
		bool	m_listening;
		TCHAR	*m_RequestBuffer;
		TCHAR	*m_ReplyBuffer;
		Server	m_Server;		//jsonrpc_server
		vector<MethodDispatcher*> m_dispatchers;
		jsonrpc::JsonFormatHandler *m_jsonFormatHandler;
	};
}
#endif