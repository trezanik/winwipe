
#include <Windows.h>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>



struct uninstaller
{
	wchar_t		cmd[1024];
};



int32_t
main(
	int32_t argc,
	char** argv
)
{
	UNREFERENCED_PARAMETER(argc);
	UNREFERENCED_PARAMETER(argv);

	int32_t		retval = EXIT_FAILURE;
	HKEY		hKey;
	HKEY		hSubkey;
	LONG		res;
	wchar_t		subkey[] = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall";
	wchar_t		buffer[1024];
	wchar_t		next_key[1024];
	wchar_t		disp_buffer[1024];
	wchar_t		uninst_buffer[1024];
	DWORD		buf_size = _countof(buffer);
	DWORD		index = 0;
	std::vector<uninstaller*>	uninstallers;
	std::vector<uninstaller*>::const_iterator	iter;


	if (( res = RegOpenKeyEx(HKEY_LOCAL_MACHINE, subkey, 0, KEY_READ, &hKey)) != ERROR_SUCCESS )
	{
		std::wcout << L"RegOpenKeyEx() failed on: " << subkey << L"\n";
		return (int32_t)res;
	}


	// goto cleanup from here on failure, ensure key is closed


	if (( res = RegEnumKeyEx(hKey, index, buffer, &buf_size, NULL, NULL, NULL, NULL)) != ERROR_SUCCESS )
	{
		std::wcout << L"Initial RegEnumKeyEx() failed\n";
		retval = (int32_t)res;
		RegCloseKey(hKey);
		goto cleanup;
	}

	do
	{
		buf_size = _countof(next_key);
		wcscpy_s(next_key, buf_size, subkey);
		wcscat_s(next_key, buf_size, L"\\");
		wcscat_s(next_key, buf_size, buffer);

		if ( RegOpenKeyEx(HKEY_LOCAL_MACHINE, next_key, NULL, KEY_READ, &hSubkey) != ERROR_SUCCESS )
		{
			std::wcout << L"RegOpenKeyEx() failed on enumerated key: " << next_key << L"\n";
			goto cleanup;
		}

		buf_size = _countof(disp_buffer);

		if (( res = RegQueryValueEx(hSubkey, L"DisplayName", NULL, NULL, (BYTE*)disp_buffer, &buf_size)) != ERROR_SUCCESS )
		{
			// plenty of keys here have no display name, not an error!!
		}
		else
		{
			// case sensitive!
			if ( wcsstr(disp_buffer, L"Java") != nullptr ||
				wcsstr(disp_buffer, L"JInitiator") != nullptr )
			{
				buf_size = _countof(uninst_buffer);

				// uninstall string must begin with msiexec, so we can pass extra parameters */
				if ( RegQueryValueEx(hSubkey, L"UninstallString", NULL, NULL, (BYTE*)uninst_buffer, &buf_size) == ERROR_SUCCESS &&
					_wcsnicmp(uninst_buffer, L"msiexec", 7) == 0 )
				{
					uninstaller*	uninst = new uninstaller;
					wchar_t*	p;

					/* special handling; some versions of Java stupidly have
					 * the uninstallstring identical to the modify one, /I
					 * specified instead of /X - amend on the fly here... */
					if (( p = wcsstr(uninst_buffer, L" /I")) != nullptr )
					{
						// swap /I with /X
						*(p + 2) = 'X';
					}

					// copy the command to execute
					wcscpy_s(uninst->cmd, _countof(uninst->cmd), uninst_buffer);
					// and ensure we won't auto-reboot or present a prompt
					// NOTE: http://bugs.java.com/bugdatabase/view_bug.do?bug_id=6764967
					// this is why we're using /qn instead of providing feedback...
					wcscat_s(uninst->cmd, _countof(uninst->cmd), L" /qn /norestart");

					std::wcout << L"Added uninstall entry: " << uninst->cmd << L"\n";
					uninstallers.push_back(uninst);
				}
			}
		}

		RegCloseKey(hSubkey);

		buf_size = _countof(buffer);
		res = RegEnumKeyEx(hKey, ++index, buffer, &buf_size, NULL, NULL, NULL, NULL);

	} while ( res == ERROR_SUCCESS );


	if ( uninstallers.size() == 0 )
	{
		std::wcout << L"No uninstall matches found\n";
	}


	for ( iter = uninstallers.begin(); iter != uninstallers.end(); iter++ )
	{
		STARTUPINFO             startup_info;
		PROCESS_INFORMATION     proc_info;
		DWORD			exit_code;

		ZeroMemory(&startup_info, sizeof(STARTUPINFO));
		startup_info.cb = sizeof(STARTUPINFO);

		if ( !CreateProcess(nullptr, (*iter)->cmd, 
			NULL, NULL, FALSE, NULL, NULL, NULL,
			&startup_info, &proc_info) )
		{
			std::wcout << L"CreateProcess() failed; error " << GetLastError() << L"\n";
			retval = (int32_t)GetLastError();
			goto cleanup;
		}
		/* if the uninstaller retains an msiexec UI (i.e. not using
		 * /passive or /qb, change the timeout to INFINITE */
		if ( WaitForSingleObject(proc_info.hProcess, 60000) != WAIT_OBJECT_0 )
		{
			std::wcout << L"WaitForSingleObject() failed (60 seconds timeout)\n";
			TerminateProcess(proc_info.hProcess, 0);
			CloseHandle(proc_info.hThread);
			CloseHandle(proc_info.hProcess);
			goto cleanup;
		}

		GetExitCodeProcess(proc_info.hProcess, &exit_code);
		CloseHandle(proc_info.hThread);
		CloseHandle(proc_info.hProcess);

		if ( exit_code != 0 && exit_code != 1614 && exit_code != 3010 )
			goto cleanup;
	}


	retval = EXIT_SUCCESS;
cleanup:
	// free the allocated uninstaller
	if ( uninstallers.size() > 0 )
	{
		for ( iter = uninstallers.begin(); iter != uninstallers.end(); iter++ )
		{
			delete (*iter);
		}

		uninstallers.clear();
	}
	// close the previously opened key
	RegCloseKey(hKey);

	return retval;	
}
