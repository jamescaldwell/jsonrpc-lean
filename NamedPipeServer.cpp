#include "NamedPipeServer.h"
#include <iostream>

namespace RCC_NativeService
{
	NamedPipeServer::NamedPipeServer(string &pipeName)
	{
		m_PipeName = pipeName;
		m_hStoppedEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

		m_running = false;
		m_listening = false;
		m_RequestBuffer = new TCHAR[BUFSIZE];

		m_jsonFormatHandler = new jsonrpc::JsonFormatHandler();
		m_Server.RegisterFormatHandler(*m_jsonFormatHandler);
	}

	NamedPipeServer::~NamedPipeServer()
	{
		if (m_hStoppedEvent != NULL)
		{
			CloseHandle(m_hStoppedEvent);
			m_hStoppedEvent = NULL;
		}
		if (m_RequestBuffer != NULL)
		{
			delete[] m_RequestBuffer;
		}
		for (vector<MethodDispatcher*>::iterator it = m_dispatchers.begin(); it != m_dispatchers.end(); it++)
		{
			if (*it != NULL)
				delete *it;
		}
		if (m_jsonFormatHandler != NULL)
		{
			delete m_jsonFormatHandler;
		}
	}


	void NamedPipeServer::AddDispatcher(MethodDispatcher * dispatcher)
	{
		if (dispatcher != NULL)
		{
			dispatcher->AddMethods(m_Server.GetDispatcher());
			m_dispatchers.push_back(dispatcher);
		}
	}


	bool NamedPipeServer::Start()
	{
		DWORD	dwThreadId;
		m_ListenerThread = CreateThread(NULL, 0, ConnectThread, (LPVOID)this, 0, &dwThreadId);
		return (m_ListenerThread != NULL);
	}

	void NamedPipeServer::Stop()
	{
		m_running = false;
		m_listening = false;
	}

	// wrapper to pass into class member
	DWORD WINAPI NamedPipeServer::ConnectThread(LPVOID lp_data)
	{
		NamedPipeServer	*server = (NamedPipeServer*)lp_data;
		server->ConnectThread2();
		return 0;
	}

	void NamedPipeServer::ConnectThread2()
	{
		m_PipeHandle = NULL;
		bool	connected;

		if (m_PipeName.length() > 0)
		{
			char pipeName[128];
			sprintf_s(pipeName, "\\\\.\\pipe\\%s", m_PipeName.c_str());
			m_PipeHandle = CreateNamedPipe(
				pipeName,             // pipe name 
				PIPE_ACCESS_DUPLEX,       // read/write access 
				PIPE_TYPE_MESSAGE |       // message type pipe 
				PIPE_READMODE_MESSAGE |   // message-read mode 
				PIPE_WAIT,                // blocking mode 
				PIPE_UNLIMITED_INSTANCES, // max. instances  
				BUFSIZE * sizeof(TCHAR),                  // output buffer size 
				BUFSIZE * sizeof(TCHAR),                  // input buffer size 
				0,                        // client time-out 
				NULL);                    // default security attribute 
		}
		else
		{
			cout << "Pipe name is empty:\n";
			return;
		}

		if (m_PipeHandle == INVALID_HANDLE_VALUE)
		{
#ifdef _DEBUG
			cout << "CreateNamedPipe failed: " << GetLastError() << "\n";
			return;
#endif
		}
		// indicate we are listening
		m_listening = true;

		while (m_listening)		// stay in listening mode until we are signaled to close
		{
			cout << "Waiting for client connection.\n";
			connected = ConnectNamedPipe(m_PipeHandle, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
			if (connected)
			{
				cout << "Connection good.Create talk thread.\n";

				DWORD	dwThreadId;
				HANDLE hThread = CreateThread(NULL, 0, TalkThread, (LPVOID)this, 0, &dwThreadId);
				if (hThread == NULL)
				{
					cout << "Could not create talk thread: " << GetLastError() << "\n";
					m_listening = false;
					break;
				}
				//		return (hThread != NULL);
				DWORD wo = WaitForSingleObject(hThread, INFINITE);
				if (wo != WAIT_OBJECT_0)
				{
					m_listening = false;
				}
			}
		}
		CloseHandle(m_PipeHandle);
		m_PipeHandle = NULL;
	}

	// wrapper to pass into class member
	DWORD WINAPI NamedPipeServer::TalkThread(LPVOID lp_data)
	{
		NamedPipeServer	*server = (NamedPipeServer*)lp_data;
		server->TalkThread2();
		return 0;
	}


	void NamedPipeServer::TalkThread2()
	{
		BOOL	fSuccess;
		DWORD cbBytesRead = 0, cbReplyBytes = 0, cbWritten = 0;

		std::shared_ptr<jsonrpc::FormattedData> formattedData;
		m_running = true;

		while (m_running)
		{
			fSuccess = ReadFile(
				m_PipeHandle,        // handle to pipe 
				m_RequestBuffer,    // buffer to receive data 
				BUFSIZE * sizeof(TCHAR), // size of buffer 
				&cbBytesRead, // number of bytes read 
				NULL);        // not overlapped I/O 
			if (!fSuccess || cbBytesRead == 0)
			{
				if (GetLastError() == ERROR_BROKEN_PIPE)
				{
					cout << "TalkThread: client disconnected. " << GetLastError() << "\n";
				}
				else
				{
					cout << "TalkThread ReadFile failed. " << GetLastError() << "\n";
				}
				m_running = false;
				break;
			}

			// make a json string of request
			string jsonStr(m_RequestBuffer, cbBytesRead);
			cout << "Request: " << jsonStr << "\n";

			// handle the request and get reply
			formattedData.reset();
			formattedData = m_Server.HandleRequest(jsonStr);

			// send reply to client
			// Write the reply to the pipe. 
			const char * outJson = formattedData->GetData();
			cbReplyBytes = (DWORD)formattedData->GetSize();
			cout << "Reply: " << outJson << "\n";

			fSuccess = WriteFile(
				m_PipeHandle,        // handle to pipe 
				outJson,     // buffer to write from 
				cbReplyBytes, // number of bytes to write 
				&cbWritten,   // number of bytes written 
				NULL);        // not overlapped I/O 

			if (!fSuccess || cbReplyBytes != cbWritten)
			{
				cout << "TalkThread WriteFile failed: " << GetLastError() << "(" << cbReplyBytes << ":" << cbWritten << ")\n";
			}
		}
		FlushFileBuffers(m_PipeHandle);
		DisconnectNamedPipe(m_PipeHandle);
	}

}