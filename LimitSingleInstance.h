#pragma once

#ifndef WS_VISIBLE
#include <windows.h>
#endif

// Limit programm to be started only once
class CLimitSingleInstance
{
	protected:
		DWORD  m_dwLastError;
		HANDLE m_hMutex;

	public:
		CLimitSingleInstance(TCHAR *strMutexName)
		{
			m_hMutex = CreateMutex(NULL, FALSE, strMutexName);
			m_dwLastError = GetLastError();
		}

		~CLimitSingleInstance()
		{
			if (m_hMutex)
			{
				CloseHandle(m_hMutex);
				m_hMutex = NULL;
			}
		}

		bool IsAnotherInstanceRunning()
		{
			return (ERROR_ALREADY_EXISTS == m_dwLastError);
		}
};
