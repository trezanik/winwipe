
/**
 * @file	FileParser.cc
 * @author	James Warren
 * @copyright	James Warren, 2013-2014
 * @license	Zlib (see license.txt or http://opensource.org/licenses/Zlib)
 */


#include "build.h"		// always first

#include <list>
#include <Windows.h>
#include <UserEnv.h>		// ExpandEnvironmentStringsForUser

#include "app.h"
#include "FileParser.h"		// prototypes
#include "Wiper.h"
#include "UserInterface.h"
#include "Runtime.h"
#include "Configuration.h"
#include "Operation.h"
#include "utils.h"




FileParser::FileParser()
{
	DWORD		token_access = TOKEN_QUERY|TOKEN_IMPERSONATE;
	DWORD		last_error;
	UserInterface*	ui = runtime.UI();

	// windows 7+ require greater access rights to the token
	if ( is_windows_version_gte(WINDOWS_7) )
		token_access |= TOKEN_DUPLICATE;

	// open the token ready for environment variable expansion
	if ( !OpenProcessToken(GetCurrentProcess(), token_access, &_token) )
	{
		last_error = GetLastError();
		ui->Output(
			OUTCOLOR_ERROR, 
			"FileParser constructor failed\n\tOpenProcessToken() had error %d (%s)\n",
			last_error, error_code_as_string(last_error)
		);
		throw std::runtime_error("FileParser constructor failed (OpenProcessToken)");
	}

	wcscpy_s(_user_path, _countof(_user_path), L"%USERPROFILE%");

	/* acquire the user profile path, for potential later usage; note we 
	 * have tested using the same source/destination buffer, works fine. */
	if ( ExpandEnvironmentStringsForUser(_token, _user_path, _user_path, _countof(_user_path)) == 0 )
	{
		last_error = GetLastError();
		ui->Output(
			OUTCOLOR_ERROR, 
			"FileParser constructor failed\n\tExpandEnvironmentStringsForUser() had error %d (%s)\n",
			last_error, error_code_as_string(last_error)
		);
		throw std::runtime_error("FileParser constructor failed (ExpandEnvironmentStringsForUser)");
	}
}



FileParser::~FileParser()
{
	if ( _token != NULL && _token != INVALID_HANDLE_VALUE )
		CloseHandle(_token);
}



bool
FileParser::ParseInputFile(
	E_FILE_TYPE filetype,
	const char* filepath
)
{
	Configuration*	cfg = runtime.Config();
	UserInterface*	ui = runtime.UI();
	Wiper*		wiper = runtime.Wiper();
	FILE*		fstream = nullptr;
	bool		retval = false;
	uint32_t	numops_before = wiper->NumQueuedOperations();
	uint32_t	numops_after;

	/* before parsing, check if we're configured not to actually process
	 * the file type. If not, just return from the function silently.. */
	switch ( filetype )
	{
	case FT_Files:
		if ( cfg->purging.file.no_proc )
			return true;
		break;
	case FT_Processes:
		if ( cfg->purging.process.no_proc )
			return true;
		break;
	case FT_Registry:
		if ( cfg->purging.registry.no_proc )
			return true;
		break;
	case FT_Services:
		if ( cfg->purging.service.no_proc )
			return true;
		break;
	case FT_Commands:
		if ( cfg->purging.command.no_proc )
			return true;
		break;
	case FT_Uninstallers:
		if ( cfg->purging.uninstaller.no_proc )
			return true;
		break;
	}


	// enabled in configuration, so proceed with parsing it

	ui->Output(OUTCOLOR_NORMAL, "Parsing '%s'... ", filepath);

	if ( fopen_s(&fstream, filepath, "r") != 0 )
	{
		char	errmsg[256];

		strerror_s(errmsg, sizeof(errmsg), errno);

		ui->Output(OUTCOLOR_ERROR, "failed\n");
		ui->Output(OUTCOLOR_ERROR, "\tErrno %d (%s)\n", errno, errmsg);

		return false;
	}

	switch ( filetype )
	{
	case FT_Files:
		retval = ParseFileFolders(fstream);
		break;
	case FT_Processes:
		retval = ParseProcesses(fstream);
		break;
	case FT_Registry:
		retval = ParseRegistry(fstream);
		break;
	case FT_Services:
		retval = ParseServices(fstream);
		break;
	case FT_Commands:
		retval = ParseCommands(fstream);
		break;
	case FT_Uninstallers:
		retval = ParseUninstallers(fstream);
		break;
	default:
		retval = false;
	}

	// close the file, so we don't lock it longer than necessary
	fclose(fstream);

	// if parsing succeeded, notify; otherwise, error already output
	if ( retval )
	{
		numops_after = wiper->NumQueuedOperations();

		ui->Output(OUTCOLOR_SUCCESS, "ok!\n");
		ui->Output(OUTCOLOR_INFO, "\t%d operations added\n", numops_after - numops_before);
	}

	return retval;
}



bool
FileParser::ParseCommands(
	FILE* fstream
)
{
	Wiper*		wiper = runtime.Wiper();
	wchar_t		buffer[512];
	uint32_t	line = 0;
	uint32_t	len = 0;
	Operation*	newop;
	DWORD		buf_size = _countof(buffer);

	// read the input file line by line
	while ( fgetws(buffer, buf_size, fstream) )
	{
		// track the line number for error output
		line++;

		len = wcslen(buffer);

		// comments, blank lines don't touch file stats
		if ( len == 0 || buffer[0] == '#' || buffer[0] == '\n' )
		{
			continue;
		}

		Trim(buffer);


		/* no validation to do on a command - it can be any length,
		 * contain any characters, and we would be wrong to limit it in
		 * any shape or form. */


		/*
		 * All commands are a path to a binary (or script), with an
		 * optional command line provided at the same time. We support
		 * running .bat/.cmd/.vbs and of course, .exe/.msi - anything
		 * else will try to execute a process in an invalid way.
		 *
		 * e.g.
		 @verbatim
		 C:\Qt\Qt5.1.1\vcredist\vcredist_sp1_x86.exe /q /x
		 C:\Program Files (x86)\Windows Kits\8.0\Debuggers\Redist\X86 Debuggers And Tools-x86_en-us.msi /q
		 @endverbatim
		 */
		UTF8ToMb(mb, buffer, sizeof(mb));
		newop = new Operation;
		newop->Prepare(OP_Execute, "%s", mb);

		if ( !wiper->QueueNewOperation(newop) )
		{
			delete newop;
			return false;
		}
	}

	return true;	
}



bool
FileParser::ParseFileFolders(
	FILE* fstream
)
{
	uint32_t	len = 0;
	uint32_t	line = 0;
	wchar_t		buffer[1024];
	wchar_t		expanded[1024];
	DWORD		buf_size = _countof(buffer);

	// read the input file line by line
	while ( ReadLine(fstream, buffer, buf_size) )
	{
		// track the line number for error output
		line++;

		len = wcslen(buffer);

		// comments, blank lines don't touch file stats
		if ( len == 0 || buffer[0] == '#' || buffer[0] == '\n' )
		{
			continue;
		}

		Trim(buffer);

		// adds all operations, parsing all environment variables
		if ( !RecursiveExpand(buffer, expanded, _countof(expanded), line) )
			goto failed;
	}

	return true;

failed:
	return false;
}



bool
FileParser::ParseProcesses(
	FILE* fstream
)
{
	Wiper*		wiper = runtime.Wiper();
	wchar_t		buffer[512];
	uint32_t	line = 0;
	uint32_t	len = 0;
	DWORD		buf_size = _countof(buffer);
	Operation*	newop = nullptr;

	// this function is only called if fstream is valid..

	// read the input file line by line
	while ( ReadLine(fstream, buffer, buf_size) )
	{
		// track the line number for error output
		line++;

		len = wcslen(buffer);

		// comments, blank lines don't touch file stats
		if ( len == 0 || buffer[0] == '#' || buffer[0] == '\n' )
		{
			continue;
		}

		Trim(buffer);

		/* no validation to do on a process name; it can be any length,
		 * contain any characters, and we would be wrong to limit it in
		 * any shape or form. */

		UTF8ToMb(mb, buffer, sizeof(mb));
		newop = new Operation;
		newop->Prepare(OP_KillProcess, "%s", mb);

		if ( !wiper->QueueNewOperation(newop) )
		{
			delete newop;
			return false;
		}
	}

	return true;
}



bool
FileParser::ParseRegistry(
	FILE* fstream
)
{
	Wiper*		wiper = runtime.Wiper();
	UserInterface*	ui = runtime.UI();
	wchar_t		key_root[20];
	wchar_t		key_name[1024];
	wchar_t		buffer[1024];
	wchar_t		delim[] = L"\t";
	wchar_t*	p;
	wchar_t*	ps;
	wchar_t*	psz;
	uint32_t	line = 0;
	uint32_t	errs = 0;
	uint32_t	len = 0;
	DWORD		buf_size = _countof(buffer);
	Operation*	newop;
	
	// read the input file line by line
	while ( fgetws(buffer, buf_size, fstream) )
	{
		// track the line number for error output
		line++;

		len = wcslen(buffer);

		// comments, blank lines don't touch file stats
		if ( len == 0 || buffer[0] == '#' || buffer[0] == '\n' )
		{
			continue;
		}

		// ignore those with obviously invalid values
		if ( len < 6 )
		{
			errs++;
			ui->Output(
				OUTCOLOR_ERROR,
				"\n\tSyntax error on line %d; insufficient length",
				line
			);
			continue;
		}

		Trim(buffer);


		p = wcstok_s(buffer, delim, &psz);
		/* based on the example above, if the character is found, the string will now look like this:
		 * HKEY_LOCAL_MACHINE SOFTWARE\Microsoft\Windows\CurrentVersion\Run	SunJavaUpdateSched
		 * ^                  ^
		 * p                  psz (string has been modified so *--psz is null)
		 */

		if ( p == nullptr )
		{
			errs++;
			ui->Output(
				OUTCOLOR_ERROR,
				"\n\tSyntax error on line %d; no separator",
				line
			);
			continue;
		}

		// input is limited to these two root keys (others are accessible within them)
		if ( wcsncmp(p, L"HKEY_LOCAL_MACHINE", 18) == 0 || 
			wcsncmp(p, L"HKLM", 4) == 0 ||
			wcsncmp(p, L"HKEY_CURRENT_USER", 17) == 0 ||
			wcsncmp(p, L"HKCU", 4) == 0 )
		{
			wcscpy_s(key_root, _countof(key_root), p);
		}
		else
		{
			errs++;
			// erroring string is wide, ui outputs ansi
			UTF8ToMb(mb, p, _countof(mb));

			ui->Output(
				OUTCOLOR_ERROR,
				"\n\tSyntax error on line %d; invalid or unsupported HKEY root '%s'",
				line, mb
			);
			continue;
		}

		ps = wcstok_s(nullptr, delim, &psz);
		/* and again, based on the example above:
		 * HKEY_LOCAL_MACHINE SOFTWARE\Microsoft\Windows\CurrentVersion\Run SunJavaUpdateSched
		 * ^                  ^                                             ^
		 * p                  ps                                            psz (*--psz == NULL)
		 */

		bool    is_hklm = true;

		if ( wcscmp(key_root, L"HKEY_CURRENT_USER") == 0 ||
			wcscmp(key_root, L"HKCU") == 0 )
		{
			is_hklm = false;
		}

		wcscpy_s(key_name, _countof(key_name), ps);

#if 0	// Code Removed: Not critical to include, only valid if same user installed it..
		/* ## Extra from java file:
		 * Replace values which need to be dynamic at run-time
		 * Primary Example: windows\installer key in hklm, contains user sid-installed packages */

		/** @todo :: Store the sid so we don't recalculate it with each found value */

		wchar_t*	loc;

		// get the SID of the current user
		if (( loc = wcsstr(ps, L"%SID%")) != nullptr )
		{
			wchar_t*	sid_str = nullptr;
			wchar_t*	amend = key_name + (loc - ps);
			HANDLE		token_handle;
			TOKEN_USER	token_info;
			DWORD		ret_len;

			if ( OpenProcessToken(GetCurrentProcess(), TOKEN_READ, &token_handle) )
			{
				if ( GetTokenInformation(token_handle, TokenUser, &token_info, sizeof(token_info), &ret_len) &&
					ConvertSidToStringSid(token_info.User.Sid, &sid_str) )
				{
					// move to rest of real string
					loc += 7;
					// copy the sid, starting at the found location
					wcscpy_s(amend, _countof(key_name) - (ps - key_name), sid_str);
					// append the original text
					wcscat_s(key_name, _countof(key_name), loc);

					LocalFree(sid_str);
				}

				CloseHandle(token_handle);
			}
		}
#endif	// Code Removed

		/* psz is set to null by wcstok if no additional pipe is found;
		 * the original string is now in ps */
		if ( *psz == '\0' )
		{
			// no separator, so we are looking at an actual registry key to delete
			if ( wcslen(ps) == 0 )
			{
				errs++;
				ui->Output(
					OUTCOLOR_ERROR,
					"\n\tSyntax error on line %d; no key name specified",
					line
				);
				continue;
			}

			UTF8ToMb(mb, ps, sizeof(mb));
			newop = new Operation;
			newop->Prepare(OP_DeleteRegKey, "%s", mb);
			newop->GetExtraData()->vparam1 = is_hklm ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;

			if ( !wiper->QueueNewOperation(newop) )
			{
				delete newop;
				return false;
			}
		}
		else
		{
			// 2nd separator found; validate there is actually a value specified...
			if ( wcslen(ps) == 0 )
			{
				errs++;
				ui->Output(
					OUTCOLOR_ERROR,
					"\n\tSyntax error on line %d; no value specified",
					line
				);
				continue;
			}

			char	val[1024];

			UTF8ToMb(val, psz, sizeof(val));
			UTF8ToMb(mb, ps, sizeof(mb));
			newop = new Operation;
			newop->Prepare(OP_DeleteRegValue, "%s", mb);
			newop->GetExtraData()->vparam1 = is_hklm ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
			newop->GetExtraData()->sparam1 = val;

			if ( !wiper->QueueNewOperation(newop) )
			{
				delete newop;
				return false;
			}
		}
	}

	if ( errs > 0 )
	{
		/* we haven't been putting newlines on the end (to provide a
		 * continual feedback, rather than erroring one line at a time
		 * which is INCREDIBLY annoying!) - so do it now, ready for
		 * the execution aborted message */
		ui->Output(OUTCOLOR_ERROR, "\n");
		return false;
	}

	return true;
}



bool
FileParser::ParseServices(
	FILE* fstream
)
{
	Wiper*		wiper = runtime.Wiper();
	wchar_t		buffer[512];
	uint32_t	line = 0;
	uint32_t	len = 0;
	Operation*	newop;
	DWORD		buf_size = _countof(buffer);
	
	// read the input file line by line
	while ( fgetws(buffer, buf_size, fstream) )
	{
		// track the line number for error output
		line++;

		len = wcslen(buffer);

		// comments, blank lines don't touch file stats
		if ( len == 0 || buffer[0] == '#' || buffer[0] == '\n' )
		{
			continue;
		}

		Trim(buffer);

		/* rest of the buffer should be the name of the service; don't
		 * impose any limitiation on the content/name */

		UTF8ToMb(mb, buffer, sizeof(mb));

		newop = new Operation;
		newop->Prepare(OP_StopService, "%s", mb);

		if ( !wiper->QueueNewOperation(newop) )
		{
			delete newop;
			return false;
		}

		newop = new Operation;
		newop->Prepare(OP_DeleteService, "%s", mb);

		if ( !wiper->QueueNewOperation(newop) )
		{
			delete newop;
			return false;
		}
	}

	return true;	
}



bool
FileParser::ParseUninstallers(
	FILE* fstream
)
{
	Wiper*		wiper = runtime.Wiper();
	UserInterface*	ui = runtime.UI();
	wchar_t		key_root[20];
	wchar_t		key_name[1024];
	wchar_t		buffer[1024];
	wchar_t		delim[] = L"\t";
	wchar_t		delim_char = L'\t';
	wchar_t*	p;
	wchar_t*	ps;
	wchar_t*	psz;
	uint32_t	line = 0;
	uint32_t	len = 0;
	uint32_t	errs = 0;
	Operation*	newop = nullptr;
	const DWORD	buf_size = _countof(buffer);
	DWORD		regbuf_size;
	DWORD		last_error;
	HKEY		hKeyRoot;
	HKEY		hSubkey;

	// read the input file line by line
	while ( fgetws(buffer, buf_size, fstream) )
	{
		// track the line number for error output
		line++;

		len = wcslen(buffer);

		// comments don't touch file stats
		if ( len == 0 || buffer[0] == '#' || buffer[0] == '\n' )
		{
			continue;
		}

		Trim(buffer);


		/*
		 * Determine if registry format, or just a path to an executable. Simple
		 * enough; our registry format separates the key, name, and value with
		 * pipe characters, that are invalid in a standard path. Doesn't hold true
		 * with non-Windows, but that isn't a problem here.
		 * ---
		 * HKEY_LOCAL_MACHINE	SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\{0682374A-9702-4C7A-8BE5-4E2A160C4918}	UninstallString
		 * HKEY_LOCAL_MACHINE	SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\GAC	UninstallString
		 * HKEY_LOCAL_MACHINE	SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\{C67CF93A-A5FF-4986-8D4A-D0CFD5315152}	UninstallString
		 * ---
		 * C:\Qt\Qt5.1.1\vcredist\vcredist_sp1_x86.exe /q
		 */

		if ( wcschr(buffer, delim_char) == nullptr )
		{
			// path format

			UTF8ToMb(mb, buffer, sizeof(mb));
			newop = new Operation;
			newop->Prepare(OP_Execute, "%s", mb);
		}
		else
		{
			// registry format (this matches ParseRegistry, hence hacky, unclean c+p)

			p = wcstok_s(buffer, delim, &psz);
			if ( p == nullptr )
			{
				errs++;
				ui->Output(
					OUTCOLOR_ERROR,
					"\n\tSyntax error on line %d; no separator",
					line
				);
				continue;
			}			

			// p = key root, nul-terminated
			if ( wcscmp(p, L"HKEY_LOCAL_MACHINE") == 0 ||
				wcscmp(p, L"HKLM") == 0 )
			{
				hKeyRoot = HKEY_LOCAL_MACHINE;
				wcscpy_s(key_root, _countof(key_root), p);
			}
			else if ( wcscmp(p, L"HKEY_CURRENT_USER") == 0 ||
				wcscmp(p, L"HKCU") == 0 )
			{
				hKeyRoot = HKEY_CURRENT_USER;
				wcscpy_s(key_root, _countof(key_root), p);
			}
			else if ( wcscmp(key_root, L"HKEY_CLASSES_ROOT") == 0 ||
				wcscmp(key_root, L"HKCR") == 0 )
			{
				hKeyRoot = HKEY_CLASSES_ROOT;
				wcscpy_s(key_root, _countof(key_root), p);
			}
			else if ( wcscmp(p, L"HKEY_USERS") == 0 ||
				wcscmp(p, L"HKU") == 0 )
			{
				hKeyRoot = HKEY_USERS;
				wcscpy_s(key_root, _countof(key_root), p);
			}
			else
			{
				errs++;
				// erroring string is wide, ui outputs ansi
				UTF8ToMb(mb, p, _countof(mb));

				ui->Output(
					OUTCOLOR_ERROR,
					"\n\tSyntax error on line %d; invalid or unsupported HKEY root '%s'",
					line, mb
				);
				continue;
			}

			ps = wcstok_s(nullptr, delim, &psz);
			// ps = subkey name, nul-terminated
			// psz = value, nul-terminated

			wcscpy_s(key_name, _countof(key_name), ps);

			if ( wcslen(ps) == 0 )
			{
				errs++;
				ui->Output(
					OUTCOLOR_ERROR,
					"\n\tSyntax error on line %d; no subkey specified",
					line
				);
				continue;
			}
			// must have a second pipe, no uninstall string exists as a key name
			if ( *psz == '\0' )
			{
				errs++;
				ui->Output(
					OUTCOLOR_ERROR,
					"\n\tSyntax error on line %d; uninstallers must be specified in a registry value",
					line
				);
				continue;
			}

			/* query the string contained within this value, and 
			 * interpret it as needed */
			if (( last_error = RegOpenKey(hKeyRoot, key_name, &hSubkey)) != ERROR_SUCCESS )
			{
				// if it doesn't exist, no fault - carry on
				if ( last_error == ERROR_FILE_NOT_FOUND )
					continue;

				errs++;
				ui->Output(
					OUTCOLOR_ERROR,
					"\n\tError on line %d; RegOpenKey failed with error %d (%s)",
					line, last_error, error_code_as_string(last_error)
				);
				continue;
			}

			char	val[1024];

			UTF8ToMb(val, psz, sizeof(val));

			regbuf_size = _countof(buffer);

			if (( last_error = RegQueryValueExW(
				hSubkey, 
				psz, 
				nullptr, nullptr, 
				(BYTE*)&buffer, &regbuf_size
				)) != ERROR_SUCCESS )
			{
				// if it doesn't exist, no fault - carry on
				if ( last_error == ERROR_FILE_NOT_FOUND )
				{
					RegCloseKey(hSubkey);
					continue;
				}

				errs++;
				ui->Output(
					OUTCOLOR_ERROR,
					"\n\tError on line %d; RegQueryValueEx on '%s' failed with error %d (%s)",
					line, val, last_error, error_code_as_string(last_error)
				);
				continue;
			}

			// close the handle to the reg key
			RegCloseKey(hSubkey);

			// buffer now contains the command to execute
			UTF8ToMb(mb, buffer, sizeof(mb));

			/* the command to execute, if a default msi setup, will
			 * contain msiexec.exe, so we can execute a normal
			 * command, no additional work needed. */

			newop = new Operation;
			newop->Prepare(OP_Execute, "%s", mb);
			newop->GetExtraData()->vparam1 = hKeyRoot;
			newop->GetExtraData()->sparam1 = val;
		}

		if ( !wiper->QueueNewOperation(newop) )
		{
			delete newop;
			return false;
		}
	}

	if ( errs > 0 )
	{
		/* we haven't been putting newlines on the end (to provide a
		 * continual feedback, rather than erroring one line at a time
		 * which is INCREDIBLY annoying!) - so do it now, ready for
		 * the execution aborted message */
		ui->Output(OUTCOLOR_ERROR, "\n");
		return false;
	}

	return true;
}



wchar_t*
FileParser::ReadLine(
	FILE* fstream,
	wchar_t* buffer,
	uint32_t buffer_size
)
{
	return fgetws(buffer, buffer_size, fstream);
}



bool
FileParser::RecursiveExpand(
	const wchar_t* const source,
	wchar_t* dest,
	const uint32_t dest_size,
	const uint32_t line
)
{
	const wchar_t	all_users[] = L"%ALLUSERS%";
	const wchar_t	user_appdata[] = L"%USERAPPDATA%";
	wchar_t		expanded[1024];
	Operation*	newop = nullptr;
	wchar_t*	p;
	bool		has_custom_operation = false;
	DWORD		data_size;
	DWORD		attrib;
	DWORD		last_error;
	UserInterface*	ui = runtime.UI();
	Wiper*		wiper = runtime.Wiper();
	uint32_t	len_to_username_end = 0;

	// no checks for nullptr/size - we trust ourselves :)


	// ready up
	has_custom_operation = false;

	// let Windows handle its own environment variables
	if ( ExpandEnvironmentStringsForUser(_token, source, dest, dest_size) == 0 )
	{
		last_error = GetLastError();
		ui->Output(
			OUTCOLOR_ERROR, 
			"failed\n\tExpandEnvironmentStringsForUser() failed; error %d (%s) on line %d\n",
			last_error, error_code_as_string(last_error), line
		);
		goto failed;
	}
		

	// Wildcard support
	if ( wcschr(dest, '*') != nullptr )
	{
		WIN32_FIND_DATA	wfd;
		HANDLE		search_handle;
		wchar_t		path[MAX_PATH];
		uint32_t	len;
		bool		skip;

		// don't interpret this line in the file as an operation
		has_custom_operation = true;

		if (( p = wcsrchr(dest, '\\')) == nullptr )
		{
			ui->Output(
				OUTCOLOR_ERROR, 
				"failed\n\tNo path separator found in the file path on line %d\n",
				line
			);
			goto failed;
		}

		/* FindFirstFile() does not work with wildcards in the folder up
		 * from where the wildcard is (C:\Bla\*\somefile); with this
		 * knowledge, we can make presumptions about the path to use. */

		// maintain a copy of the path
		wcscpy_s(path, _countof(path), dest);
		// calculate the end (p is pointing to the original string!)
		len = (p - dest);
		// +1 to skip backslash, we want to keep it
		path[len + 1] = '\0';
		// can now use path as the prefix to each NextFile operation

		if (( search_handle = FindFirstFile(dest, &wfd)) == INVALID_HANDLE_VALUE )
		{
			last_error = GetLastError();
			if ( last_error == ERROR_FILE_NOT_FOUND )
				goto succeeded;

			ui->Output(
				OUTCOLOR_ERROR, 
				"failed\n\tFindFirstFile() failed; error %d (%s) while processing line %d\n",
				last_error, error_code_as_string(last_error), line
			);
			goto failed;
		}

		do
		{
			skip = false;
			skip_if_path_is_posix(wfd.cFileName, &skip);

			if ( skip )
				continue;

			// add whatever filename was discovered to the path
			wcscat_s(path, _countof(path), wfd.cFileName);

			if ( !RecursiveExpand(path, dest, dest_size, line) )
			{
				FindClose(search_handle);
				goto failed;
			}

			// as in the prep, make this ready for next loop
			path[len + 1] = '\0';

		} while ( FindNextFile(search_handle, &wfd) );

		FindClose(search_handle);
	}


	/* custom environment variable expansion; we can reuse the
	 * original buffer to save having another, third one. Inline
	 * due to the nature of the addition */

	/* 1) For every user profile on the system. Skips system/default folders
	 * Will expand to, for example:
	 * C:\\Users\\profile_name\\
	 * C:\\Users\\another_profile\\
	 * .. and so on. With no path after the variable, will wipe out every
	 * user profile folder (but not the profile itself, naturally).
	 */
	if ( wcsncmp(all_users, dest, 10) == 0 )
	{
		/* we make these static so we can reuse them for additional
		 * input lines. Not needed to be class members as they're not
		 * used elsewhere, keeps the class cleaner. */
		static wchar_t	profiles_path[512] = { '\0' };
		// list, not a vector, so destructor deletes the allocated memory
		static std::list<text_cache*>	profile_names;
		static wchar_t	fn[1024];
		static size_t	size;

		wchar_t		env[512];   // holds the 'current' user directory being processed		
		LONG		result;
		WIN32_FIND_DATA	wfd;
		HANDLE		search_handle;
		wchar_t		expanded[1024];
		bool		skip = false;

		// don't interpret this line in the file as an operation
		has_custom_operation = true;

		// caching the result so we don't lookup every line of the input
		if ( profiles_path[0] == '\0' )
		{
			// skip initialization when not used, keep in scope
			const wchar_t	subkey[] = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\ProfileList";
			const wchar_t	value[] = L"ProfilesDirectory";
			HKEY		hKey;

			if (( result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, subkey, NULL, KEY_READ, &hKey)) == ERROR_SUCCESS )
			{
				data_size = sizeof(profiles_path);
				if (( result = RegQueryValueEx(hKey, value, NULL, NULL, (BYTE*)&profiles_path, &data_size)) == ERROR_SUCCESS )
				{
					if ( ExpandEnvironmentStrings(profiles_path, env, _countof(env)) == 0 )
					{
						last_error = GetLastError();
						ui->Output(
							OUTCOLOR_ERROR, 
							"failed\n\tExpandEnvironmentStrings() failed; error %d (%s) while processing line %d\n",
							last_error, error_code_as_string(last_error), line
						);
						goto failed;
					}
					else
					{
						wcscpy_s(profiles_path, _countof(profiles_path), env);
					}

					// append the trailing slash and wildcard, ready for each user
					wcscat_s(profiles_path, _countof(profiles_path), L"\\*");
				}

				RegCloseKey(hKey);
			}
		}

		// caching the names so we don't rescan the folder each line of the input
		if ( profile_names.empty() )
		{
			if (( search_handle = FindFirstFile(profiles_path, &wfd)) == INVALID_HANDLE_VALUE )
			{
				last_error = GetLastError();

				/* the User profile directory always contains at
				* least a few items, so (should) be no chance
				* of last_error = file not found */
				ui->Output(
					OUTCOLOR_ERROR, 
					"failed\n\tFindFirstFile() failed; error %d (%s) while processing line %d\n",
					last_error, error_code_as_string(last_error), line
				);
				goto failed;
			}
			else
			{
				wcscpy_s(fn, _countof(fn), profiles_path);
				size = wcslen(fn) - 1;

				do
				{
					skip = false;
					skip_if_path_is_posix(wfd.cFileName, &skip);
					skip_if_path_is_default_profile(wfd.cFileName, &skip);

					if ( skip )
						continue;

					// prep for this next profile
					fn[size] = '\0';
					// append the profile name
					wcscat_s(fn, _countof(fn), wfd.cFileName);
					// and append the rest of the text we have
					wcscat_s(fn, _countof(fn), (dest+10));

					/* if we've got more variables in the string, simply
					 * adding a new operation as it stands now will not
					 * work; we must recurse until no further custom
					 * expansion has been done */
					if ( !RecursiveExpand(fn, expanded, _countof(expanded), line) )
					{
						FindClose(search_handle);
						goto failed;
					}

					/* while we're here, cache the name; not interested
					 * in the ones that we're skipping already. We can
					 * alloc with new and just let the static list call
					 * the destructor on each when the program closes */
					text_cache*	c = new text_cache;
					wcscpy_s(c->buffer, _countof(c->buffer), wfd.cFileName);
					profile_names.push_back(c);

				} while ( FindNextFile(search_handle, &wfd) );

				FindClose(search_handle);
			}
		}
		else    // using the cached data
		{
			/* for each profile name, do the same as what we did
			 * with the non-cached results. */
#if MSVC_BEFORE_VS11
			std::list<text_cache*>::const_iterator name;
			for ( name = profile_names.begin();
				name != profile_names.end();
				name++ )
			{
				wchar_t*	buffer = (*name)->buffer;
#else
			for ( auto name : profile_names )
			{
				wchar_t*	buffer = name->buffer;
#endif
				// prep for this next profile
				fn[size] = '\0';
				// append the profile name
				wcscat_s(fn, _countof(fn), buffer);
				// and append the rest of the text we have
				wcscat_s(fn, _countof(fn), (dest+10));

				if ( !RecursiveExpand(fn, expanded, _countof(expanded), line) )
				{
					goto failed;
				}
			}
		}
	}
	/* unlike %ALLUSERS%, %USERAPPDATA% (and most of the others)
	 * can appear anywhere within the string, not just at the
	 * beginning, so use wcsstr instead of a direct wcsncmp.
	 * %USERAPPDATA% is only ever combined following an %ALLUSERS%,
	 * so we can make some assumptions. */
	/* also, as this is a recursive function, only enter ONE of the
	 * custom expansion scopes at a time, unless you want duplicates */
	else if (( p = wcsstr(dest, user_appdata)) != nullptr )
	{
		// temporary store for the suffix of the supplied string
		wchar_t		tmp[MAX_PATH];
		// path to the executing users application data path
		wchar_t*	appdata_path;

		/* the only reason we usually use %USERAPPDATA% is if 
		 * we're going through each users app data folder, so
		 * the string is likely to be like:
		 * C:\Documents and Settings\Network Service\%USERAPPDATA%\...
		 */

		has_custom_operation = true;
		
		// temporary copy while we modify the original
		wcscpy_s(tmp, _countof(tmp), (p + wcslen(user_appdata)));

		/** @warning
		 * If the user has customized their profile to use non
		 * defaults (or your executing account has), then this
		 * path will be totally invalid and unusable. We wouldn't
		 * be able to find the 'real' paths without loading the
		 * profile, its hive, and querying the relevant registry
		 * value. Way too much work for a rarity.
		 */
		/* ALL the pathwork done here is based off the current users
		 * profile (executing the application), and THAT is the one
		 * we use to determine what the appdata path is (if the above
		 * warning is unclear).
		 * We only want its name ("Application Data" or 
		 * "AppData\Roaming"), so the name difference is no concern.
		 */
		len_to_username_end = get_user_path_offset();

		if ( len_to_username_end == 0 )
			goto failed;
		
		appdata_path = get_user_appdata_path();
		appdata_path += len_to_username_end;

		/* copy "AppData\Roaming" or equivalent where %USERAPPDATA% is 
		 * in dest, which is already pointed to by p */
		wcscpy_s(p, dest_size - (p - dest), appdata_path);
		// finally, copy the remainder of the path specified
		wcscat_s(dest, dest_size, tmp);

		if ( !RecursiveExpand(dest, expanded, _countof(expanded), line) )
			goto failed;
	}


	if ( !has_custom_operation )
	{
		// should match the !has_custom_expansion section
		UTF8ToMb(mb, dest, sizeof(mb));
		newop = new Operation;

		/* this is the best we can do in regards to determining if it's
		 * a file or folder; if it doesn't exist (or we have no perms 
		 * to get the attribs), all we can do is check for a file ext.
		 * Some folders have dots, and some files don't, but if it
		 * doesn't exist we can't really check.. */

		if (( attrib = GetFileAttributes(dest)) != INVALID_FILE_ATTRIBUTES )
		{
			if ( attrib & FILE_ATTRIBUTE_DIRECTORY )
				newop->Prepare(OP_DeleteDirectory, "%s", mb);
			else
				newop->Prepare(OP_DeleteFile, "%s", mb);
		}
		else
		{
			if (( p = wcsrchr(dest, L'.')) == nullptr )
			{
				// assume it's a directory
				newop->Prepare(OP_DeleteDirectory, "%s", mb);
			}
			else
			{
				/* if we can't find a path separator after the
				 * final dot, assume it's a file */
				if ( wcsrchr(p, '\\') == nullptr )
					newop->Prepare(OP_DeleteFile, "%s", mb);
				else
					newop->Prepare(OP_DeleteDirectory, "%s", mb);
			}
		}

		if ( !wiper->QueueNewOperation(newop) )
		{
			delete newop;
			goto failed;
		}
	}

succeeded:
	return true;

failed:
	return false;
}



wchar_t*
FileParser::Trim(
	wchar_t* str
)
{
	size_t		len = wcslen(str);
	wchar_t*	end;
	
	/* this is str_trim, with wchar_t, and only removing newlines;
	 * we want to include trailing spaces, they can be needed..! */
	
	end = str + len - 1;
	while ( end >= str && ( *end == '\n' || *end == '\r' ) )
		end--;
	*(end + 1) = '\0';
	return str;
}
