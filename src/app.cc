
/**
 * @file	app.cc
 * @author	James Warren
 * @copyright	James Warren, 2013-2014
 * @license	Zlib (see license.txt or http://opensource.org/licenses/Zlib)
 */

#include "build.h"			// always first

#include <iostream>

#include <process.h>			// _beginthreadex
#include <Windows.h>
#include <Richedit.h>
#define SECURITY_WIN32			// sspi.h, needed for Security.h
#include <Security.h>			// get current user
// we want to support SHGetSpecialFolderPath but target xp at minimum; requires this
#if (_WIN32_WINNT < 0x0601)
#	undef _WIN32_WINNT
#	define _WIN32_WINNT 0x0601
#endif
#include <Shlobj.h>		// Shell acquisition
#include <Knownfolders.h>

#include "app.h"
#include "Configuration.h"
#include "UserInterface.h"
#include "FileParser.h"
#include "Runtime.h"
#include "Wiper.h"
#include "utils.h"
#include "resource.h"
#include "Log.h"



// Global/Singleton assignment for application access
Runtime	&runtime = Runtime::Instance();



void
app_exec()
{
	Configuration*	cfg = runtime.Config();
	UserInterface*	ui = runtime.UI();

	if ( cfg->ui.graphical )
	{
		BOOL	gm;
		MSG	msg;
		HWND	app_wnd = ui->native.app_window;
		
		ShowWindow(app_wnd, SW_SHOW);
		PostMessage(app_wnd, WM_WINWIPE_APPINIT, 0, 0);

		// prepare the output structures for usage
		ui->Prepare();


		do
		{
			gm = GetMessage(&msg, app_wnd, NULL, NULL);

			if ( gm == -1 )
			{
				if ( !IsWindow(app_wnd) )
					break;

				break;
			}
			else
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}

		} while ( gm != 0 );
	}
	else
	{
		std::string	s;
		
		// getline won't block otherwise!
		std::cin.clear();
		std::cin.sync();


		/** @todo full implementation of cli */
		// begin_thread, other work, etc.

		
		std::cout << "\n\nPress the Return key to quit";
		//std::getline(std::cin, s);
		std::cin >> s;
	}
}



void
app_init(
	int32_t argc,
	char** argv
)
{
	Configuration*	cfg = runtime.Config();
	UserInterface*	ui = runtime.UI();
	Log*		log = runtime.Logger();
	uint64_t	start_time = get_ms_time();
	uint64_t	end_time;

	/* very first thing; ensure our current directory is that of the binary
	 * if we're on Windows; in Visual Studio debugging, this is not the case
	 * automatically! */
	char		mb[MAX_PATH];
	wchar_t		wpath[MAX_PATH];

	get_current_binary_path(mb, sizeof(mb));
	MbToUTF8(wpath, mb, _countof(wpath));

	SetCurrentDirectory(wpath);

	// load default configuration
	cfg->LoadDefaults();

	// parse the command line, override defaults
	if ( !parse_commandline(argc, argv) )
	{
		/* display help, or invalid argument. Exit process here, as
		 * otherwise we still jump into the app_exec code. This is due
		 * to us not wanting to return false (no need for this to be
		 * an exception), so just bail directly. */
		ExitProcess(0);
	}

	/* load configuration from file. Must be 3/3, as cmdline can set where
	 * the config file is */
	cfg->Load(cfg->ConfigPath());


	// if we're logging, open the file now and begin - well, logging
	if ( cfg->general.log )
	{
		char	log_name[MAX_PATH];
		DWORD	buf_size = sizeof(mb);

		//GetComputerNameA(hostname, &host_size);
		// ComputerNameDnsDomain will return a blank string if not joined to a domain
		// This brings back 'hostname.domain', or just 'hostname' if no domain
		GetComputerNameExA(ComputerNameDnsFullyQualified, mb, &buf_size);
		str_format(log_name, sizeof(log_name), "%s.log", mb);

		log->Open(log_name);
		log->SetLogLevel(LL_Debug);	// log everything we input

		// get running user, can help identify permission issues
		buf_size = sizeof(mb);
		GetUserNameExA(NameSamCompatible, mb, &buf_size);
		LOG(LL_Debug) << "Running as user: " << mb << "\n";

		// if config path is different, we may be in a different folder
		GetCurrentDirectoryA(sizeof(mb), mb);
		LOG(LL_Debug) << "Current Directory: " << mb << "\n";

		// with the logfile open, we can dump the configuration setup
		cfg->Dump();

		// debug info log; make this optional
		{
			std::vector<ModuleInformation*>	miv;
			uint32_t		num = 0;
			std::stringstream	ss;

			miv = get_loaded_modules();
			if ( miv.size() > 0 )
			{
				ss << "Outputting loaded modules:\n";

				// VS2010 brings *some* C++11 support, but not auto
#if MSVC_BEFORE_VS11
				for ( std::vector<ModuleInformation*>::iterator iter = miv.begin();
					iter != miv.end(); iter++ )
				{
					ModuleInformation*	mi = (*iter);
#else
				for ( auto mi : miv )
				{
#endif
					// noooo....!!
					UTF8ToMb(mb, mi->name, sizeof(mb));
					ss << "\t* [" << num << "]\t" <<
						mb << "  [" << 
						mi->fvi.major << "." <<
						mi->fvi.minor << "." <<
						mi->fvi.revision << "." <<
						mi->fvi.build << "]\n";
					num++;

					free(mi);
				}
				miv.clear();

				// already has newline from last entry
				LOG(LL_Debug) << ss.str();
			}
		}
	}


	if ( cfg->ui.graphical )
	{
		// hide our console window, which exists by default
		HWND		hwnd_console = GetConsoleWindow();
		ShowWindow(hwnd_console, SW_HIDE);

		// and we can move on to the normal gui code
		WNDCLASSEX      wcex;
		HWND		hWnd;
		HMODULE		hMod_richedit;
		HWND		hOutputWnd;
		RECT		crect;

		ZeroMemory(&wcex, sizeof(WNDCLASSEX));

		wcex.cbSize		= sizeof(WNDCLASSEX);
		wcex.hInstance		= GetModuleHandle(NULL);
		wcex.lpfnWndProc	= main_callback;
		wcex.hCursor		= LoadCursor(GetModuleHandle(NULL), IDC_ARROW);
		wcex.lpszClassName	= L"10-jw_winwipe";
		wcex.hIcon		= LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(APPICON));
		wcex.hbrBackground	= (HBRUSH)GetStockObject(BLACK_BRUSH);
		wcex.lpszMenuName	= MAKEINTRESOURCE(IDR_MENU1);

		// errors here will be highly abnormal, so just throw if so

		if ( RegisterClassEx(&wcex) == 0 )
		{
			throw std::runtime_error("RegisterClassEx() failed");
		}

		/** @todo
		 * Add WS_MAXIMIZEBOX|WS_MINIMIZEBOX| and support a resizing
		 * window. Not critical to add at the current time.
		 */
		if (( hWnd = CreateWindowEx(NULL, wcex.lpszClassName, NULL, 
			WS_CAPTION|WS_SYSMENU,
			CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
			GetDesktopWindow(), NULL, wcex.hInstance, NULL)) == NULL )
		{
			throw std::runtime_error("CreateWindowEx() failed");
		}

		ui->native.app_window = hWnd;

		if (( hMod_richedit = LoadLibrary(L"msftedit.dll")) == NULL )
		{
			throw std::runtime_error("LoadLibrary() failed");
		}

		// store it so we can free it on app closure
		ui->native.richedit_module = hMod_richedit;

		GetClientRect(ui->native.app_window, &crect);
		
		if (( hOutputWnd = CreateWindowEx(NULL, MSFTEDIT_CLASS, NULL, WS_BORDER|WS_VISIBLE|WS_CHILD|WS_HSCROLL|WS_VSCROLL|ES_AUTOHSCROLL|ES_MULTILINE|ES_READONLY,
			10, 10, ((crect.right - 10) - 10), ((crect.bottom - 10) - 10), ui->native.app_window, (HMENU)ID_OUTPUT_WINDOW,
			wcex.hInstance, NULL))
			== NULL )
		{
			throw std::runtime_error("CreateWindowEx() failed");
		}
		SendMessage(hOutputWnd, EM_SETBKGNDCOLOR, (WPARAM)NULL, (LPARAM)RGB(0,0,0));
		ui->native.out_window = hOutputWnd;

		SetWindowText(ui->native.app_window, L"WinWipe");
	}
	else
	{
		RECT	work_area = { 0 };

		if ( SystemParametersInfo(SPI_GETWORKAREA, 0, &work_area, 0) )
		{
			HWND	hwnd = GetConsoleWindow();
			RECT	wnd_rect;

			GetWindowRect(hwnd, &wnd_rect);

			// relocate the console window
#if 0	// Bottom-Left
			work_area.top = work_area.bottom - (wnd_rect.bottom - wnd_rect.top);
#endif
#if 0	// Bottom-Right
			work_area.top = work_area.bottom - (wnd_rect.bottom - wnd_rect.top);
			work_area.left = work_area.right - (wnd_rect.right - wnd_rect.left);
#endif
#if 1	// Top-Left
#endif
#if 0	// Top-Right
			work_area.left = work_area.right - (wnd_rect.right - wnd_rect.left);
#endif

			MoveWindow(hwnd, work_area.left, work_area.top, 800, 500, TRUE);
		}

		/* intercept ctrl+c - mostly useful for debugging; don't care if
		 * we fail */
		SetConsoleCtrlHandler((PHANDLER_ROUTINE)sig_handler, TRUE);
	}

	end_time = get_ms_time();
	std::cout << "Application startup completed in " << (end_time - start_time) << "ms\n";

	if ( cfg->general.log )
	{
		// chain of responsibility would prevent this duplication
		LOG(LL_Info) << "Application startup completed in " << (end_time - start_time) << "ms\n";
	}
}



void
app_stop()
{
	Configuration*	cfg = runtime.Config();
	Log*		log = runtime.Logger();
	UserInterface*	ui = runtime.UI();

	if ( cfg->ui.graphical )
	{
		if ( ui->native.richedit_module != NULL )
			FreeLibrary(ui->native.richedit_module);
	}

	// if we're logging, close the file
	if ( cfg->general.log )
	{
		log->Close();
	}
}



bool
get_debugging_privileges()
{
	/* since we have admin privileges, assign ourselves debugging rights; note
	 * that this is not essential for the fix to succeed, but it can help if
	 * any of the installation components or purging is failing. */

	HANDLE			token;
	LUID			luid;
	const wchar_t*		privilege = SE_DEBUG_NAME;
	TOKEN_PRIVILEGES	tp = { 0 };
	DWORD			last_error;
	bool			retval = false;
	UserInterface*		ui = runtime.UI();

	// no operations valid yet, just output raw
	ui->Output(
		OUTCOLOR_NORMAL,
		"Granting ourselves debugging rights... "
	);

	if ( !OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES|TOKEN_QUERY, &token) )
	{
		last_error = GetLastError();
		ui->Output(
			OUTCOLOR_ERROR,
			"failed\n\tOpenProcessToken() failed with error %d (%s)... ",
			last_error, error_code_as_string(last_error)
		);
		return retval;
	}

	if ( !LookupPrivilegeValue(NULL, privilege, &luid) )
	{
		last_error = GetLastError();
		CloseHandle(token);
		ui->Output(
			OUTCOLOR_ERROR,
			"failed\n\tLookupPrivilegeValue() failed with error %d (%s)... ",
			last_error, error_code_as_string(last_error)
		);
		return retval;
	}

	tp.PrivilegeCount		= 1;
	tp.Privileges[0].Luid		= luid;
	tp.Privileges[0].Attributes	= SE_PRIVILEGE_ENABLED;

	if ( !AdjustTokenPrivileges(token, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL) )
	{
		last_error = GetLastError();
		CloseHandle(token);
		ui->Output(
			OUTCOLOR_ERROR,
			"failed\n\tAdjustTokenPrivileges() failed with error %d (%s)... ",
			last_error, error_code_as_string(last_error)
		);
		return retval;
	}

	retval = true;
	ui->Output(OUTCOLOR_SUCCESS, "ok!\n");

	return retval;
}



bool
is_windows_version_gte(
	WORD os_version
)
{
	static OSVERSIONINFOEX	osvi;
	BYTE	os_major;
	BYTE	os_minor;
	
	if ( osvi.dwOSVersionInfoSize != sizeof(osvi) )
	{
		osvi.dwOSVersionInfoSize = sizeof(osvi);
		GetVersionEx((OSVERSIONINFO*)&osvi);
	}

	os_major = HIBYTE(os_version);
	os_minor = LOBYTE(os_version);

	if ( osvi.dwMajorVersion > os_major  )
		return true;

	if ( osvi.dwMajorVersion == os_major )
	{
		/* remember, we're checking for if the supplied version is
		 * greater than or equal to the version supplied */
		if ( osvi.dwMinorVersion >= os_minor )
		{
			return true;
		}
	}

	return false;
}



wchar_t*
get_user_appdata_path()
{
	UserInterface*	ui = runtime.UI();
	DWORD		last_error;
	wchar_t*	known_path;
	static wchar_t	path[MAX_PATH] = { '\0' };

	// if we've already done the hard work, return the result
	if ( path[0] != '\0' )
		return &path[0];

	// why does Windows make simple things more complicated??!?
	if ( is_windows_version_gte(_WIN32_WINNT_VISTA) )
	{
		/* dynamically link to the function as the ordinal
		 * will be missing if this is run on XP. */
		typedef HRESULT (__stdcall *pf_SHGetKnownFolderPath)(REFKNOWNFOLDERID, DWORD, HANDLE, PWSTR*);
		pf_SHGetKnownFolderPath exec_SHGetKnownFolderPath;
		if (( exec_SHGetKnownFolderPath = (pf_SHGetKnownFolderPath)get_function_address("SHGetKnownFolderPath", L"Shell32.dll")) == nullptr )
		{
			last_error = GetLastError();
			ui->Output(
				OUTCOLOR_ERROR,
				"failed\n\tFailed to find 'SHGetKnownFolderPath' in 'shell32.dll'; error %d (%s)... ",
				last_error, error_code_as_string(last_error)
			);
			return nullptr;
		}
		if ( !SUCCEEDED(exec_SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &known_path)) )
		{
			ui->Output(
				OUTCOLOR_ERROR,
				"failed\n\tUnexpected failure executing 'SHGetKnownFolderPath()'"
			);
			return nullptr;
		}
		
		wcscpy_s(path, _countof(path), known_path);
		CoTaskMemFree(known_path);
	}
	else
	{
		if ( !SUCCEEDED(SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, 0, path)) )
		{
			ui->Output(
				OUTCOLOR_ERROR,
				"failed\n\tUnexpected failure executing 'SHGetFolderPath()'"
			);
			return nullptr;
		}
	}

	return &path[0];
}



uint32_t
get_user_path_offset()
{
	uint32_t	offset = 0;
	wchar_t*	userpath = get_user_appdata_path();
	wchar_t		userprof[MAX_PATH];
	
	/* get the environment variable for the users path; we can then compare
	 * this (or rather, just go to the end of the string) to find out where
	 * the users path begins, and go from there. Simples.
	 * Note that if the user account this is being run as is the same as
	 * the sessions, then these strings will match. We need to be able to
	 * handle cases where we're logged in as a low-privilege user, however,
	 * while still being able to execute this fix. This code supports it. */

	if ( !ExpandEnvironmentStrings(L"%USERPROFILE%", userprof, _countof(userprof)) )
		return 0;

	wchar_t*	src = userpath;
	wchar_t*	cmp = userprof;

	while ( *src++ == *cmp++ )
		offset++;

	// we increment the offset to handle a path separator in future code
	return (offset + 1);
}



void
skip_if_path_is_posix(
	wchar_t* path,
	bool* skip
)
{
	if (	wcscmp(path, L".") == 0 ||
		wcscmp(path, L"..") == 0
	)
		*skip = true;
}



void
skip_if_path_is_default_profile(
	wchar_t* path,
	bool* skip
)
{
	if ( is_windows_version_gte(_WIN32_WINNT_LONGHORN) )
	{
		// folders only on Windows Vista+
		if (	wcscmp(path, L"Public") == 0 ||
			wcscmp(path, L"Default") == 0 ||
			wcscmp(path, L"desktop.ini") == 0 )	// special case!
			*skip = true;
	}
	else
	{
		// folders only on Windows XP
		if (	wcscmp(path, L"Local Service") == 0 ||
			wcscmp(path, L"Network Service") == 0 )
			*skip = true;
	}

	/* even if these are junctions/hard links on newer Windows versions, 
	 * the folder will still exist so we skip it all the same */
	if (	wcscmp(path, L"All Users") == 0 ||
		wcscmp(path, L"Default User") == 0 )
		*skip = true;
}



BOOL WINAPI
sig_handler(
	DWORD signal
)
{
	switch ( signal )
	{
	case CTRL_C_EVENT:
	case CTRL_SHUTDOWN_EVENT:
	case CTRL_LOGOFF_EVENT:
	case CTRL_CLOSE_EVENT:
		/* application will be closing, but we're still running. If the
		 * threads are sleeping, we'll hang until they timeout - so just
		 * kill them here and now. 
		 * The application will then proceed to close down normally. */
		//if ( runtime.Wiper()->thread )
		//TerminateThread(runtime.Wiper()->thread, EXIT_SUCCESS);
		break;
	}

	return TRUE;
}



unsigned __stdcall
wiper_thread(
	void* arg
)
{
	UNREFERENCED_PARAMETER(arg);

	try
	{
		Configuration*	cfg = runtime.Config();
		FileParser*	parser = runtime.Parser();
		Wiper*		wiper = runtime.Wiper();
		UserInterface*	ui = runtime.UI();

		ui->Output(OUTCOLOR_INFO, "\nStarting execution\n");
		ui->Output(OUTCOLOR_NORMAL, "Parsing input files, please wait...\n");

		/* parse in the order we want to perform the operations - as we 
		* operate on a queue, the first operations pushed in are the first
		* ones to be executed.
		* As the order can determine things like file locks, missing entries
		* from the wiper files, etc., we do it in the following way:
		*
		* 1) Uninstallers
		* These can remove files, services, registry entries, and processes 
		* without our forcible removal, and can remove settings applied that
		* are not specified in the cleanup routines.
		*
		* 2) Services
		* Killing a process, when it exists as a service, poses a chance based
		* on the service config for it to be restarted/respawned, potentially
		* when we're doing other work - that may cause it to crash.
		*
		* 3) Processes
		* Running processes cannot be deleted (including any modules they have
		* loaded), so we're releasing their locks where possible. These kills
		* are forcible and unclean; they should be taken care of by an 
		* uninstaller or service first.
		*
		* 4) Registry
		* Registry entries can direct configuration settings towards files or
		* folders - by deleting these first, there's no risk of configuration
		* existing, pointing to a non-existent file. Only really applicable if
		* the machine unexpectedly reboots during this period, but safety is
		* always first.
		*
		* 5) Files/Folders
		* The lowest rung of the ladder - files simply exist, other settings
		* determine when and how they get used, so these are deleted last.
		*
		* 6) Commands
		* With everything wiped out, we're free to install any further 
		* software, run any special commands, etc. - this covers the reinstall
		* of whatever we wiped out in the previous steps, assuming we're 
		* running it as a fix for a broken install.
		*/
		if ( !parser->ParseInputFile(FT_Uninstallers, cfg->PurgingPath(FT_Uninstallers)) )
		{
			ui->Output(OUTCOLOR_ERROR, "[ Failed to parse the input uninstallers file ]");
			return EXIT_FAILURE;
		}
		if ( !parser->ParseInputFile(FT_Services, cfg->PurgingPath(FT_Services)) )
		{
			ui->Output(OUTCOLOR_ERROR, "[ Failed to parse the input services file ]");
			return EXIT_FAILURE;
		}
		if ( !parser->ParseInputFile(FT_Processes, cfg->PurgingPath(FT_Processes)) )
		{
			ui->Output(OUTCOLOR_ERROR, "[ Failed to parse the input processes file ]");
			return EXIT_FAILURE;
		}
		if ( !parser->ParseInputFile(FT_Registry, cfg->PurgingPath(FT_Registry)) )
		{
			ui->Output(OUTCOLOR_ERROR, "[ Failed to parse the input registry file ]");
			return EXIT_FAILURE;
		}
		if ( !parser->ParseInputFile(FT_Files, cfg->PurgingPath(FT_Files)) )
		{
			ui->Output(OUTCOLOR_ERROR, "[ Failed to parse the input file ]");
			return EXIT_FAILURE;
		}
		if ( !parser->ParseInputFile(FT_Commands, cfg->PurgingPath(FT_Commands)) )
		{
			ui->Output(OUTCOLOR_ERROR, "[ Failed to parse the input commands file ]");
			return EXIT_FAILURE;
		}


		if ( !wiper->Execute() )
		{
			ui->Output(OUTCOLOR_ERROR, "[ Execution Aborted ]");
			return EXIT_FAILURE;
		}

		ui->Output(OUTCOLOR_SUCCESS, "[ Execution Completed ]");

		return EXIT_SUCCESS;
	}
	catch ( ... )
	{
		return EXIT_FAILURE;
	}
}



LRESULT CALLBACK
main_callback(
	HWND hWnd,
	UINT msg,
	WPARAM wParam,
	LPARAM lParam
)
{
	static HANDLE		thread = NULL;
	static uint32_t		thread_id = 0;
	static Configuration*	cfg = runtime.Config();
	static UserInterface*	ui = runtime.UI();
	static Wiper*		wiper = runtime.Wiper();

	switch ( msg )
	{
	case WM_CREATE:
		{
			return 0;
		}
	case WM_WINWIPE_APPINIT:
		{
			// check permissions and rights, enable the menu options if suitable
			if ( get_debugging_privileges() && wiper->ExecutorHasAdminPrivileges() )
			{
				EnableMenuItem(GetMenu(hWnd), ID_WINWIPE_EXECUTE, MF_ENABLED);

				ui->Output(OUTCOLOR_NORMAL, "Use the menu option to initiate execution\n");
			}
			return 0;
		}
	case WM_COMMAND:
		{
			switch ( LOWORD(wParam) )
			{
			case ID_WINWIPE_EXECUTE:
				{
					if ( thread != NULL && thread != INVALID_HANDLE_VALUE )
					{
						CloseHandle(thread);
					}

					/* so the UI remains responsive, execute all the
					 * wipe processing in a dedicated thread. Failure
					 * should only happen on incredibly low resourced
					 * machines... */
					if (( thread = (HANDLE)_beginthreadex(
						NULL, NULL, 
						wiper_thread, (void*)nullptr,
						CREATE_SUSPENDED, (unsigned int*)&thread_id)) == NULL )
					{
						if ( cfg->general.log )
						{
							LOG(LL_Error) << "Thread creation failed; errno " << errno << "\n";
						}

						throw std::runtime_error("Thread creation failed");
					}

					if ( cfg->general.log )
					{
						LOG(LL_Debug) << "Beginning wiper run\n";
					}

					ResumeThread(thread);

					return 0;
				}
			case ID_HELP_ABOUT:
				{
					MessageBox(hWnd, L"WinWipe Tool version 1.0\n\nCoder:\tJames Warren\n", L"About", MB_OK);
					return 0;
				}
			case ID_WINWIPE_EXIT:
				{
					SendMessage(hWnd, WM_CLOSE, 0, 0);
					return 0;
				}
			default:
				break;
			}
			break;
		}
	case WM_DESTROY:
		{
			PostQuitMessage(0);
			return 0;
		}
	case SC_CLOSE:
	case WM_CLOSE:
		{
			if ( cfg->general.log )
			{
				LOG(LL_Debug) << "Close event received; destroying the application window\n";
			}

			DestroyWindow(hWnd);
			return 0;
		}
	default:
		break;
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);
}
