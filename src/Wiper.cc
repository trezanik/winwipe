
/**
 * @file	Wiper.cc
 * @author	James Warren
 * @copyright	James Warren, 2013-2014
 * @license	Zlib (see license.txt or http://opensource.org/licenses/Zlib)
 */

#include "build.h"			// always first

#include <cassert>
#include <shlwapi.h>
#include <AccCtrl.h>			// security defines
#include <AclApi.h>			// SetNamedSecurityInfo
#include <ShlObj.h>
#include <Psapi.h>			// if using standard API functions
#include <UserEnv.h>			// ExpandEnvironmentStringsForUser

#include "app.h"			// output colours
#include "Subject.h"
#include "Operation.h"
#include "Runtime.h"
#include "Wiper.h"
#include "Configuration.h"
#include "UserInterface.h"		// attaching to operations
#include "utils.h"
#include "Log.h"
#include "ntquerysysteminformation.h"	// if using undocumented functions



/* Windows-centric file. Uses wchar_t and wcs* functions where possible to ease
 * the usage of unicode/utf-16 handling.
 * Does mean some layout goes off (wcscpy_s uses buf,size,buf, for example),
 * but we can keep a more-*nix-style methodology in our own functions, at the
 * cost of conversions */


Wiper::Wiper() : _current_operation(nullptr), _scm(nullptr)
{
	Attach(*runtime.UI());
}



Wiper::~Wiper()
{
	if ( _scm != nullptr )
		CloseServiceHandle(_scm);

	Detach(*runtime.UI());
}



bool
Wiper::DeleteService(
	const wchar_t* name
)
{
	SC_HANDLE	service;
	DWORD		last_error;

	if ( name == nullptr )
	{
		Notify(_current_operation->HadError(ERROR_INVALID_PARAMETER, error_code_as_string(ERROR_INVALID_PARAMETER)));
		return false;
	}	

	if ( _scm == nullptr )
	{
		_scm = OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT);

		if ( _scm == nullptr )
		{
			last_error = GetLastError();
			Notify(_current_operation->HadError(last_error, error_code_as_string(last_error)));
			return false;
		}
	}

	if (( service = OpenService(_scm, name, DELETE)) == nullptr )
	{
		last_error = GetLastError();

		// if it didn't exist, is not an error - success with not needed
		if ( last_error == ERROR_SERVICE_DOES_NOT_EXIST )
		{
			Notify(_current_operation->Finished(false));
			return true;
		}

		Notify(_current_operation->HadError(last_error, error_code_as_string(last_error)));
		return false;
	}

#if 0	// Code disabled: Stop the service manually first, if you want to. Don't do extra work the function doesn't say.
	SERVICE_STATUS_PROCESS	ss;
	DWORD		wait_time;
	uint64_t	start_time = get_ms_time();
	DWORD		timeout = 30000;    // 30-second time-out
	DWORD		bytes_needed;

	if ( !QueryServiceStatusEx( 
		sch,
		SC_STATUS_PROCESS_INFO,
		(LPBYTE)&ss,
		sizeof(SERVICE_STATUS_PROCESS),
		&bytes_needed )
	)
	{
		last_error = GetLastError();
		Notify(_current_operation->HadError(last_error, error_code_as_string(last_error)));
		return false;
	}

	if ( ss.dwCurrentState == SERVICE_STOPPED )
	{
		// already stopped - just delete it now and finish
		if ( !::DeleteService(service) )
		{
			last_error = GetLastError();
			// service not found?
			Notify(_current_operation->HadError(last_error, error_code_as_string(last_error)));
			return false;
		}
		Notify(_current_operation->Finished());
		return true;
	}

	// service is running - stop it, then try to delete it
	StopService(name);

#endif	// Code Disabled

	if ( !::DeleteService(service) )
	{
		last_error = GetLastError();
		Notify(_current_operation->HadError(last_error, error_code_as_string(last_error)));
		return false;
	}

	Notify(_current_operation->Finished());
	return true;
}



bool
Wiper::EradicateFileObject(
	const char* path
)
{
	wchar_t		wpath[1024];
	
	if ( path == nullptr )
	{
		Notify(_current_operation->HadError(ERROR_INVALID_PARAMETER, error_code_as_string(ERROR_INVALID_PARAMETER)));
		return false;
	}

	MbToUTF8(wpath, path, _countof(wpath));

	return EradicateFileObject(wpath);
}



bool
Wiper::EradicateFileObject(
	const wchar_t* wpath
)
{
	Configuration*	cfg = runtime.Config();
	DWORD	attrib;
	DWORD	last_error;
	bool	retval = false;

	if ( wpath == nullptr )
	{
		Notify(_current_operation->HadError(ERROR_INVALID_PARAMETER, error_code_as_string(ERROR_INVALID_PARAMETER)));
		return retval;
	}


	// check for a custom string using variables or wildcards
	if ( wcschr(wpath, '*') != nullptr )
	{
		WIN32_FIND_DATA	wfd;
		HANDLE		hSearch;
		wchar_t		dir[512];

		hSearch = FindFirstFile(wpath, &wfd);

		if ( hSearch == INVALID_HANDLE_VALUE )
		{
			last_error = GetLastError();
			Notify(_current_operation->HadError(last_error, error_code_as_string(last_error)));
			return retval;
		}
		do
		{
			// skip posix paths
			if ( wcscmp(wfd.cFileName, L".") == 0 ||
				wcscmp(wfd.cFileName, L"..") == 0 )
				continue;

			// we don't have a wide-str_format()
			wcscpy_s(dir, _countof(dir), wpath);
			wcscat_s(dir, _countof(dir), L"\\");
			wcscat_s(dir, _countof(dir), wfd.cFileName);

			// new file/folder, new operation
			NewOperation();
			UTF8ToMb(mb, dir, _countof(mb));
			if ( !(wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) )
				Notify(_current_operation->Prepare(OP_DeleteFile, "%s", mb));
			else
				Notify(_current_operation->Prepare(OP_DeleteDirectory, "%s", mb));

			/* Break on failure, as we can enter continuous deletion loops
			 * if for example, we don't have permissions to delete a file */
			if (( retval = EradicateFileObject(dir)) == false )
				break;

		} while ( FindNextFile(hSearch, &wfd) );

		FindClose(hSearch);

		return retval;
	}
	if ( wcsstr(wpath, L"%ALLUSERS%") != nullptr )
	{
	}
	// other vars...


	attrib = GetFileAttributes(wpath);


	if ( attrib == INVALID_FILE_ATTRIBUTES )
	{
		last_error = GetLastError();

		if ( last_error != ERROR_PATH_NOT_FOUND &&
			last_error != ERROR_FILE_NOT_FOUND )
		{
			goto failed;
		}

		// not a failure, but was not needed; succeeded, but note the difference
		goto not_needed;
	}

	if ( attrib & FILE_ATTRIBUTE_READONLY )
	{
		// remove the read-only bit, otherwise we can't delete it
		attrib ^= FILE_ATTRIBUTE_READONLY;

		if ( SetFileAttributes(wpath, attrib) == 0 )
		{
			last_error = GetLastError();
			goto failed;
		}
	}

	if ( attrib & FILE_ATTRIBUTE_DIRECTORY )
	{
		if ( !RemoveDirectory(wpath) )
		{
			last_error = GetLastError();

			if ( last_error == ERROR_DIR_NOT_EMPTY )
			{
				/* All the directories and subdirectories must be empty of files (and folders)
				 * before we can delete them. Recursively search & delete everything that we find */

				WIN32_FIND_DATA	wfd;
				HANDLE		hSearch;
				wchar_t		dir[512];
				bool		success = true;

				wcscpy_s(dir, _countof(dir), wpath);
				wcscat_s(dir, _countof(dir), L"\\*");
				
				hSearch = FindFirstFile(dir, &wfd);

				if ( hSearch == INVALID_HANDLE_VALUE )
				{
					last_error = GetLastError();
					goto failed;
				}
				do
				{
					if ( wcscmp(wfd.cFileName, L".") == 0 ||
						wcscmp(wfd.cFileName, L"..") == 0 )
						continue;

					// we don't have a wide-str_format()
					wcscpy_s(dir, _countof(dir), wpath);
					wcscat_s(dir, _countof(dir), L"\\");
					wcscat_s(dir, _countof(dir), wfd.cFileName);

					// new file/folder, new operation
					NewOperation();
					UTF8ToMb(mb, dir, _countof(mb));
					if ( !(wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) )
						Notify(_current_operation->Prepare(OP_DeleteFile, "%s", mb));
					else
						Notify(_current_operation->Prepare(OP_DeleteDirectory, "%s", mb));

					/* Break on failure, as we can enter continuous deletion loops
					 * if for example, we don't have permissions to delete a file */
					if (( success = EradicateFileObject(dir)) == false )
						break;

				} while ( FindNextFile(hSearch, &wfd) );

				FindClose(hSearch);

				// only attempt deletion of the original path if no errors occurred
				if ( success )
				{
					// need to pop the operations off to become the current
					PopPreviousOperation();

					/* with success, the directory should be empty.
					 * don't use RemoveDirectory though, permissions changes +
					 * Notifications makes this more complicated than it's worth */
					return EradicateFileObject(wpath);
				}

				// Notify() with error already reported from loop
				return false;
			}

			goto failed;
		}

		goto succeeded;
	}
	else
	{
		// standard file, not a directory

		if ( !DeleteFile(wpath) )
		{
			last_error = GetLastError();
			
			if ( last_error == ERROR_ACCESS_DENIED )
			{
				bool	new_owner = false;
				bool	new_perms = false;

				if ( cfg->purging.file.change_permissions )
				{
					NewOperation();
					UTF8ToMb(mb, wpath, sizeof(mb));
					Notify(_current_operation->Prepare(OP_ChangePermissionsFile, "%s", mb));
					new_perms = ReplacePermissionsOfFile(wpath);
					PopPreviousOperation(false);
				}
				if ( cfg->purging.file.take_ownership )
				{
					NewOperation();
					UTF8ToMb(mb, wpath, sizeof(mb));
					Notify(_current_operation->Prepare(OP_ChangeOwnerFile, "%s", mb));
					new_owner = TakeOwnershipOfFile(wpath);
					PopPreviousOperation(false);
				}

				/* resume the operation manually, which needs a
				 * Notify, as we PopPreviousOperation without
				 * performing the notifies. */
				Notify(_current_operation->Resume());

				/* If ownership or permissions change, try to delete the
				 * file again. No point attempting again if nothing else
				 * has changed! */
				if ( new_owner || new_perms )
				{
					if ( !DeleteFile(wpath) )
					{
						last_error = GetLastError();
						goto failed;
					}

					goto succeeded;
				}
			}
			else if ( last_error == ERROR_FILE_CHECKED_OUT )
			{
				/* if the file is locked because it's still
				 * loaded in another process, and if configured
				 * to do so, attempt to rename the file to
				 * bypass it. */
				// if ( cfg->purging.file.attempt_rename )

				//gen_random_string(2, 10, (GetCurrentThreadId() << GetCurrentProcessId()));
			}

			goto failed;
		}

		goto succeeded;
	}

succeeded:
	Notify(_current_operation->Finished());
	return true;
not_needed:
	Notify(_current_operation->Finished(false));
	return true;
failed:
	Notify(_current_operation->HadError(last_error, error_code_as_string(last_error)));
	return false;
}



bool
Wiper::EradicateRegistryObject(
	HKEY key_root,
	const char* subkey
)
{
	wchar_t		wsubkey[1024];
	
	if ( subkey == nullptr )
	{
		Notify(_current_operation->HadError(ERROR_INVALID_PARAMETER, error_code_as_string(ERROR_INVALID_PARAMETER)));
		return false;
	}

	MbToUTF8(wsubkey, subkey, _countof(wsubkey));

	return EradicateRegistryObject(key_root, wsubkey);
}



bool
Wiper::EradicateRegistryObject(
	HKEY key_root,
	const wchar_t* subkey
)
{
	HKEY		hKey;
	char		key[8];	// used purely for output
	DWORD		last_error = ERROR_SUCCESS;
	Configuration*	cfg = runtime.Config();

	if ( subkey == nullptr )
	{
		Notify(_current_operation->HadError(ERROR_INVALID_PARAMETER, error_code_as_string(ERROR_INVALID_PARAMETER)));
		return false;
	}

	// note: can't switch/case this - used purely for output
	if ( key_root == HKEY_CURRENT_USER )
		strlcpy(key, "HKCU", sizeof(key));
	else if ( key_root == HKEY_CLASSES_ROOT )
		strlcpy(key, "HKCR", sizeof(key));
	else if ( key_root == HKEY_USERS )
		strlcpy(key, "HKU", sizeof(key));
	else
		strlcpy(key, "HKLM", sizeof(key));

	/* Unlike file objects, we don't need to delete every single value beneath a subkey before
	 * we can delete the key itself - we just need to track back to the end. This convenience
	 * is removed by the extra operations that have to be done in the registry however... */

	if (( last_error = RegDeleteKey(key_root, subkey)) != ERROR_SUCCESS )
	{
		/* reports access denied when subkeys exist (strange choice!). The 
		 * real shame of the matter, is if we have no Delete rights (but can
		 * open it for writing), then we can't tell the difference!
		 * EnumKeys will fail if it has no subkeys, so we need a way to
		 * tell that our access WAS denied (preferably without creating
		 * sids and using the security api).
		 *
		 * Our access permissions when opening the key shall be used for
		 * this - though this is untested with 100% of the possibilities
		 */
		if (( last_error = RegOpenKeyEx(key_root, subkey, 0, KEY_ENUMERATE_SUB_KEYS|KEY_QUERY_VALUE|DELETE, &hKey)) != ERROR_SUCCESS )
		{
			if ( last_error == ERROR_ACCESS_DENIED )
			{
				bool	new_owner = false;
				bool	new_perms = false;
				
				if ( cfg->purging.file.change_permissions )
				{
					NewOperation();
					_current_operation->GetExtraData()->vparam1 = key_root;
					UTF8ToMb(mb, subkey, sizeof(mb));
					Notify(_current_operation->Prepare(OP_ChangePermissionsKey, "%s", mb));
					new_perms = ReplacePermissionsOfKey(key_root, subkey);
				}
				if ( cfg->purging.file.take_ownership )
				{
					NewOperation();
					_current_operation->GetExtraData()->vparam1 = key_root;
					UTF8ToMb(mb, subkey, sizeof(mb));
					Notify(_current_operation->Prepare(OP_ChangeOwnerKey, "%s", mb));
					new_owner = TakeOwnershipOfKey(key_root, (wchar_t*)subkey);
				}

				PopPreviousOperation();

				/* back to original caller if we made changes */
				if ( new_owner || new_perms )
				{
					return EradicateRegistryObject(key_root, subkey);
				}

				Notify(_current_operation->HadError(last_error, error_code_as_string(last_error)));
				return false;
			}
			else if ( last_error == ERROR_FILE_NOT_FOUND )
			{
				Notify(_current_operation->Finished(false));
				return true;
			}

			Notify(_current_operation->HadError(last_error, error_code_as_string(last_error)));
			return false;
		}
		else
		{
			wchar_t	del_key[512];
			wchar_t	buffer[512];
			DWORD	buf_size = _countof(buffer);
			UINT	ui;
			bool	success = true;

			if (( last_error = RegEnumKeyEx(hKey, 0, buffer, &buf_size, nullptr, nullptr, nullptr, nullptr)) != ERROR_SUCCESS )
			{
				RegCloseKey(hKey);
				Notify(_current_operation->HadError(last_error, error_code_as_string(last_error)));
				return false;
			}
			else
			{
				do
				{
					buf_size = _countof(del_key);
					ui = wcscpy_s(del_key, buf_size, subkey);

					if ( del_key[ui] != '\\' )
						wcscat_s(del_key, buf_size, L"\\");

					wcscat_s(del_key, buf_size, buffer);

					UTF8ToMb(mb, del_key, _countof(mb));
					/* new reg key, new operation */
					NewOperation();
					_current_operation->GetExtraData()->vparam1 = key_root;				
					Notify(_current_operation->Prepare(OP_DeleteRegKey, "%s", mb));

					/* Break on failure, as we can enter continuous deletion loops
					 * if for example, we don't have permissions to delete a key */
					if (( success = EradicateRegistryObject(key_root, del_key)) == false )
						break;

					buf_size = _countof(buffer);
					last_error = RegEnumKeyEx(hKey, 0, buffer, &buf_size, nullptr, nullptr, nullptr, nullptr);

				} while ( last_error == ERROR_SUCCESS );

				RegCloseKey(hKey);

				// only attempt deletion of the original key if no errors occurred
				if ( success )
				{
					// need to pop the operations off to become the current
					PopPreviousOperation();

					/* with success, all child keys should be gone 
					 * and this should then succeed (permissions granting) */
					return EradicateRegistryObject(key_root, subkey);
				}

				// Notify() with error already reported from loop
				return false;
				
			}
		}
	}

	Notify(_current_operation->Finished());
	return true;
}



bool
Wiper::EradicateRegistryObject(
	HKEY key_root,
	const wchar_t* subkey,
	const wchar_t* value
)
{
	HKEY		hKey;
	char		key[8];	// used purely for output
	DWORD		last_error = ERROR_SUCCESS;
	Configuration*	cfg = runtime.Config();

	if ( subkey == nullptr )
	{
		Notify(_current_operation->HadError(ERROR_INVALID_PARAMETER, error_code_as_string(ERROR_INVALID_PARAMETER)));
		return false;
	}

	// note: can't switch/case this - used purely for output
	if ( key_root == HKEY_CURRENT_USER )
		strlcpy(key, "HKCU", sizeof(key));
	else if ( key_root == HKEY_CLASSES_ROOT )
		strlcpy(key, "HKCR", sizeof(key));
	else if ( key_root == HKEY_USERS )
		strlcpy(key, "HKU", sizeof(key));
	else
		strlcpy(key, "HKLM", sizeof(key));

	if (( last_error = RegOpenKeyEx(key_root, subkey, 0, KEY_SET_VALUE, &hKey)) != ERROR_SUCCESS )
	{
		if ( last_error == ERROR_ACCESS_DENIED )
		{
			bool	new_owner = false;
			bool	new_perms = false;

			/** @todo : infinite recursion (resulting in stack overflow) if
			 * access denied, so long as take_ownership or replace_permissions
			 * succeeds, without fixing the reason for the failing access */

			if ( cfg->purging.file.take_ownership )
			{
				NewOperation();
				UTF8ToMb(mb, subkey, sizeof(mb));
				Notify(_current_operation->Prepare(OP_ChangeOwnerKey, "%s\\%s", key, mb));
				new_owner = TakeOwnershipOfKey(key_root, subkey);
			}
			if ( cfg->purging.file.change_permissions )
			{
				NewOperation();
				UTF8ToMb(mb, subkey, sizeof(mb));
				Notify(_current_operation->Prepare(OP_ChangePermissionsKey, "%s\\%s", key, mb));
				new_perms = ReplacePermissionsOfKey(key_root, subkey);
			}

			// back to original caller if we made changes
			if ( new_owner || new_perms )
			{
				PopPreviousOperation();
				return EradicateRegistryObject(key_root, subkey, value);
			}

			Notify(_current_operation->HadError(last_error, error_code_as_string(last_error)));
			return false;
		}
		else if ( last_error == ERROR_FILE_NOT_FOUND )
		{
			// registry key did not exist, so no value to delete
			Notify(_current_operation->Finished(false));
			return true;
		}

		Notify(_current_operation->HadError(last_error, error_code_as_string(last_error)));
		return false;
	}
	else
	{
		if (( last_error = RegDeleteValue(hKey, value)) != ERROR_SUCCESS )
		{
			if ( last_error == ERROR_FILE_NOT_FOUND )
			{
				// registry key existed, but the value didn't
				Notify(_current_operation->Finished(false));
				return true;
			}

			Notify(_current_operation->HadError(last_error, error_code_as_string(last_error)));
			return false;
		}

		Notify(_current_operation->Finished());
	}
	
	return true;
}



bool
Wiper::Execute()
{
	Configuration*	cfg = runtime.Config();
	Log*		log = runtime.Logger();
	bool		retval = true;
	Operation*	queued_op;
	extra_data*	extra;
	wchar_t		buffer[1024];
	wchar_t		val[255];

	if ( _operations.empty() )
		return retval;

	do
	{
		queued_op = _operations.front();
		_operations.pop();

		_current_operation = queued_op;

		/* just convert everything; most of them need it, and will
		 * cause no harm in doing so. Keeps each case statement a
		 * little tidier as well.
		 * Side note: this also means we can do away with all the
		 * multibyte version of the wiper functions; we'll leave
		 * them for now, as they're a good reference. */
		MbToUTF8(buffer, queued_op->GetData().c_str(), _countof(buffer));
		extra = queued_op->GetExtraData();

		/* notify for the current operation */
		Notify(queued_op);

		switch ( queued_op->GetOperation() )
		{
		case OP_DeleteFile:
		case OP_DeleteDirectory:
			retval = EradicateFileObject(buffer);
			break;
		case OP_KillProcess:
			retval = KillProcess(buffer);
			break;
		case OP_KillThread:
			throw std::runtime_error("Not yet implemented");
			break;
		case OP_ChangeRights:
			break;
		case OP_ChangeOwnerFile:
			retval = TakeOwnershipOfFile(buffer);
			break;
		case OP_ChangePermissionsFile:
			retval = ReplacePermissionsOfFile(buffer);
			break;
		case OP_ChangeOwnerKey:
			retval = TakeOwnershipOfKey((HKEY)extra->vparam1, buffer);
			break;
		case OP_ChangePermissionsKey:
			retval = ReplacePermissionsOfKey((HKEY)extra->vparam1, buffer);
			break;
		case OP_DeleteRegKey:
			retval = EradicateRegistryObject((HKEY)extra->vparam1, buffer);
			break;
		case OP_DeleteRegValue:
			MbToUTF8(val, extra->sparam1.c_str(), _countof(val));
			retval = EradicateRegistryObject((HKEY)extra->vparam1, buffer, val);
			break;
		case OP_DeleteService:
			retval = DeleteService(buffer);
			break;
		case OP_StopService:
			retval = StopService(buffer);
			break;
		case OP_Execute:
			retval = ExecuteCommand(buffer);
			break;
		default:
			throw std::runtime_error("The operation supplied was not recognized");
		}

		/* free allocated memory; every Operation was allocated with
		 * new, and must be deleted */
		delete _current_operation;

		/* we also break if an operation fails, as we may break things
		 * if we carry on deleting certain files or folders (like drivers) */
	} while ( !_operations.empty() && retval != false );


	/* if execution failed, we need to wipe out the remainder of the 
	 * operations (at least in the event the user clicks execute again,
	 * repopulating the same operations) in order to free the memory */
	while ( !_operations.empty() )
	{
		_current_operation = _operations.front();
		_operations.pop();
		delete _current_operation;
	}


	/* flush the output logfile, if it exists, so the file contents can be
	 * read without having to close the application first */
	if ( cfg->general.log )
	{
		log->Flush();
	}

	return retval;
}



bool
Wiper::ExecuteCommand(
	const char* command
)
{
	wchar_t		wcommand[1024];
	
	if ( command == nullptr )
	{
		Notify(_current_operation->HadError(ERROR_INVALID_PARAMETER, error_code_as_string(ERROR_INVALID_PARAMETER)));
		return false;
	}

	MbToUTF8(wcommand, command, _countof(wcommand));

	return ExecuteCommand(wcommand);
}



bool
Wiper::ExecuteCommand(
	const wchar_t* command
)
{
	wchar_t		cmd_line[1024];
	wchar_t		cmd[1024];
	wchar_t*	p;
	const wchar_t*	ext;
	const wchar_t*	first_quote = nullptr;
	const wchar_t*	second_quote = nullptr;
	bool		is_quoted = false;
	HANDLE		token;
	DWORD		token_access = TOKEN_QUERY|TOKEN_IMPERSONATE;
	wchar_t		expanded[1024];
	UserInterface*	ui = runtime.UI();
	STARTUPINFO             si;
	PROCESS_INFORMATION     pi;
	DWORD                   exit_code;
	DWORD			last_error;

	// windows 7+ require greater access rights to the token
	if ( is_windows_version_gte(WINDOWS_7) )
		token_access |= TOKEN_DUPLICATE;

	// open the token ready for environment variable expansion
	if ( !OpenProcessToken(GetCurrentProcess(), token_access, &token) )
	{
		last_error = GetLastError();
		ui->Output(
			OUTCOLOR_ERROR, 
			"failed\n\tOpenProcessToken() failed; error %d (%s)\n",
			last_error, error_code_as_string(last_error)
		);
		return false;
	}

	// expand the variables before we start playing with the quotes
	if ( ExpandEnvironmentStringsForUser(token, command, expanded, _countof(expanded)) == 0 )
	{
		last_error = GetLastError();
		ui->Output(
			OUTCOLOR_ERROR, 
			"failed\n\tExpandEnvironmentStringsForUser() failed; error %d (%s)\n",
			last_error, error_code_as_string(last_error)
		);
		CloseHandle(token);
		return false;
	}

	// not needed again, close it now to avoid handle leaks
	CloseHandle(token);



	// double-quote
	if (( first_quote = wcschr(expanded, L'"')) != nullptr )
		is_quoted = true;
	else
	{
		// single-quote
		if (( first_quote = wcschr(expanded, L'\'')) != nullptr )
			is_quoted = true;
	}

	if ( !is_quoted )
	{
		/* attempt to insert the quotes ourselves. This is very different
		 * to the way other interpreters work, but we're only supposed to
		 * be a simple application; I don't have the time to write another
		 * one.
		 * We assume that the first dot is the extension; a quote will be
		 * added to the very end, or if a space is present, directly
		 * before it.
		 */
		cmd[0] = '"';

		/* we have a problem with our method; some of the strings specified
		 * in UninstallString can have no quotes, but spaces and dots in 
		 * names (like 'Identity Agent 11.02.00a'...) */
		
		if (( ext = wcschr(expanded, L'.')) == nullptr )
		{
			// no dot; just assume the user is right, use the whole command
			wcscpy_s(&cmd[1], _countof(cmd) - 1, expanded);
			wcscat_s(cmd, _countof(cmd), L"\"");
		}
		else
		{
			if (( p = (wchar_t*)wcschr(ext, ' ')) == nullptr )
			{
				// no space after extension, whole command
				wcscpy_s(&cmd[1], _countof(cmd) - 1, expanded);
				wcscat_s(cmd, _countof(cmd), L"\"");
			}
			else
			{
				// command has extension and space after it (so has args)
				wcsncpy_s(&cmd[1], _countof(cmd) - 1, expanded, (p - expanded));
				wcscat_s(cmd, _countof(cmd), L"\"");
				wcscat_s(cmd, _countof(cmd), p);
			}

			ext++;
		}		
	}
	else
	{
		if (( second_quote = (wchar_t*)wcschr(first_quote+1, *first_quote)) == nullptr )
		{
			// error, only one quote present
			last_error = ERROR_INVALID_DATA;
			goto failed;
		}

		// we are now wrapped around what SHOULD be the binary/script name

		/* grab the command/script/binary name without any quote marks,
		 * so we can identify the extension */
		wcsncpy_s(cmd, _countof(cmd), expanded + 1, (second_quote - first_quote) - 1);

		ext = wcsrchr(cmd, L'.');

		if ( ext == nullptr )
		{
			// need an extension to know what to run...
			last_error = ERROR_INVALID_DATA;
			goto failed;
		}

		ext++;
	}


	if ( wcsncmp(ext, L"exe", 3) == 0 )
	{
		wcscpy_s(cmd_line, _countof(cmd_line), expanded);
	}
	else if ( wcsncmp(ext, L"msi", 3) == 0 )
	{
		/* we execute with the assumption we are configuring or
		 * installing - uninstallations should always be done by the
		 * uninstall routines. 
		 * This may indeed limit us to certain orders, but nothing
		 * that can't be sorted by two independent configuration runs,
		 * and if the situation is that bad, you may have other things
		 * to worry about... */
		wcscpy_s(cmd_line, _countof(cmd_line), L"msiexec.exe /i ");
		wcscat_s(cmd_line, _countof(cmd_line), expanded);
	}
	else if ( wcsncmp(ext, L"vbs", 3) == 0 )
	{
		// wscript is windows based, cscript is console based
		wcscpy_s(cmd_line, _countof(cmd_line), L"wscript.exe ");
		wcscat_s(cmd_line, _countof(cmd_line), expanded);
	}
	else if ( wcsncmp(ext, L"cmd", 3) == 0 ||
		wcsncmp(ext, L"bat", 3) == 0 )
	{
		wcscpy_s(cmd_line, _countof(cmd_line), L"cmd.exe /c ");
		wcscat_s(cmd_line, _countof(cmd_line), expanded);
	}
	else
	{
		// what to execute??
		//Notify(_current_operation->HadError(ERROR_INVALID_DATA, "Unsupported or unknown file extension"));
		//return false;
		/* let's assume that this is just a normal application... or
		 * that the full command line is valid, and quiet */
		wcscpy_s(cmd_line, _countof(cmd_line), expanded);
	}

	/* special handling for uninstallers; we want quiet and zero prompting
	 * wherever possible; we're an automation app */
	if ( _wcsnicmp(L"msiexec.exe", cmd_line, 11) == 0 )
	{
		if ( wcsstr(cmd_line, L"/x") != nullptr ||
			wcsstr(cmd_line, L"/X") != nullptr ||
			wcsstr(cmd_line, L"/uninstall") != nullptr )
		{
			// add '/qn' if no UI level is present
			if ( wcsstr(cmd_line, L"/q") == nullptr )
				wcscat_s(cmd_line, _countof(cmd_line), L" /qn");
		}

		// add 'norestart' if not present
		if ( wcsstr(cmd_line, L"/norestart") == nullptr )
			wcscat_s(cmd_line, _countof(cmd_line), L" /norestart");
	}

	
	// <temporary, for debugging>
	UTF8ToMb(mb, cmd, sizeof(mb));
	LOG(LL_Debug) << "Binary: " << mb << "\n";
	UTF8ToMb(mb, cmd_line, sizeof(mb));
	LOG(LL_Debug) << "Full Command-Line: " << mb << "\n";
	// </temporary>


	ZeroMemory(&si, sizeof(STARTUPINFO));
	si.cb = sizeof(STARTUPINFO);


	if ( !CreateProcess(
		nullptr,	// appname (useless using it, just give a full command line)
		cmd_line,	// cmdline
		nullptr,	// procattrib
		nullptr,	// threadattrib
		false,		// inherit
		NORMAL_PRIORITY_CLASS,	// createflags
		nullptr,	// environment
		nullptr,	// current dir
		&si,
		&pi)
	)
	{
		last_error = GetLastError();

		/* uninstallers may not exist if they've been previously wiped,
		 * or we're doing a blanket approach to a whole batch of files
		 * or registry keys. As such, if the file or path does not
		 * exist, do not fail and bail.
		 * We may want a big fail on a normal command though - consider
		 * separating the uninstall/command execution to support this */
		if ( last_error == ERROR_PATH_NOT_FOUND || last_error == ERROR_FILE_NOT_FOUND )
		{
			Notify(_current_operation->HadError(last_error, error_code_as_string(last_error)));
			return true;
		}

		goto failed;
	}
	
	
	Notify(_current_operation->Finished());

	NewOperation();
	Notify(_current_operation->Prepare(OP_Dummy, "Waiting for process (id %d) to finish", pi.dwProcessId));
	WaitForSingleObject(pi.hProcess, INFINITE);
	Notify(_current_operation->Finished());

	NewOperation();
	Notify(_current_operation->Prepare(OP_Dummy, "Checking process exit code"));

	GetExitCodeProcess(pi.hProcess, &exit_code);
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);

	// ok exit codes: 0 (success), 3010 (success, reboot required)
	if ( exit_code != 0 && exit_code != 3010 )
	{
		// install failure
		Notify(_current_operation->HadError(exit_code, error_code_as_string(exit_code)));

		/*
		 * 1602		User cancelled installation
		 * 1603		Installation failure
		 * 1605		This action is only valid for products that are installed
		 */

		if ( exit_code == 1603 )
		{
		}
		else if ( exit_code == 1605 )
		{
			/* special handler; ignore these uninstall failures, but
			 * have them reported so we're aware */
			return true;
		}

		return false;
	}

	Notify(_current_operation->Finished());
	return true;

failed:
	Notify(_current_operation->HadError(last_error, error_code_as_string(last_error)));
	return false;
}



bool
Wiper::ExecutorHasAdminPrivileges()
{
	SID_IDENTIFIER_AUTHORITY	ntauth = SECURITY_NT_AUTHORITY;
	UserInterface*	ui = runtime.UI();
	PSID	sid_group_admin;
	BOOL	is_member;
	DWORD	last_error;
	
	NewOperation();
	Notify(_current_operation->Prepare(OP_Dummy, "%s", "Checking we're running with administrator privileges"));

	if ( !AllocateAndInitializeSid(&ntauth, 2, 
		SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
		0, 0, 0, 0, 0, 0, &sid_group_admin) )
	{
		last_error = GetLastError();
		Notify(_current_operation->HadError(last_error, std::string("AllocateAndInitializeSid() -> ") + error_code_as_string(last_error)));
		return false;
	}
	
	if ( !CheckTokenMembership(nullptr, sid_group_admin, &is_member) )
	{
		last_error = GetLastError();
		FreeSid(sid_group_admin);
		Notify(_current_operation->HadError(last_error, std::string("CheckTokenMembership() -> ") + error_code_as_string(last_error)));
		return false;
	}
	
	FreeSid(sid_group_admin);

	if ( !is_member )
	{
		Notify(_current_operation->HadError(0, "The operation succeeded, but the user account has no administrator rights"));
		ui->Output(OUTCOLOR_WARNING, "\tThis application must be run with administrator rights in order to function!\n");
		return false;
	}

	Notify(_current_operation->Finished());
	return true;
}



bool
Wiper::KillProcess(
	const wchar_t* by_name
)
{
	SYSTEM_PROCESS_INFORMATION*	procinfo;
	Configuration*	cfg = runtime.Config();
	PCHAR	buffer;
	ULONG	procinfo_size;
	ULONG	offset = 0;
	HANDLE	process_handle = nullptr;
	bool	success = false;

	/* if we're not 'authorized' to use undocumented-style functions, then
	 * we'll stick to the slower, less functional, but supported APIs */
	if ( !cfg->purging.use_undocumented )
	{

	}

	/* this code makes use of the ntdll structs and functionality, a 
	 * separate scope setup and code rewrite is needed to switch to
	 * using the documented-only functions */

	NtQuerySystemInformation(SystemProcessInformation, NULL, 0, &procinfo_size);

	// always goto cleanup after this point to ensure the buffer is freed
	if (( buffer = (PCHAR)malloc(procinfo_size)) == nullptr )
		return false;
	
	if ( NtQuerySystemInformation(SystemProcessInformation, buffer, procinfo_size, NULL) != ERROR_SUCCESS )
		goto cleanup;

	do
	{
		procinfo = (SYSTEM_PROCESS_INFORMATION*)&buffer[offset];

#if 0
		procinfo->ProcessId;
		procinfo->BasePriority;
		procinfo->ThreadCount;
#endif
		if ( procinfo->ProcessName.Length > 0 &&
			wcscmp((wchar_t*)procinfo->ProcessName.Buffer, by_name) == 0 )
		{
			break;
		}

		offset += procinfo->NextEntryOffset;

	} while ( procinfo->NextEntryOffset != 0 );

	
	if ( procinfo->NextEntryOffset != 0 )
	{
		DWORD		last_error;
		char		name[MAX_PATH];
		unsigned long	pid = procinfo->ProcessId;

		UTF8ToMb(name, (wchar_t*)procinfo->ProcessName.Buffer, _countof(name));

		/* this bit highlights the issue with the method of operation
		 * notification - we have to create a new operation for each
		 * thread to kill - for example - otherwise we must remain silent,
		 * since we'll screw up the line output! */

		// 1) Attempt standard termination
		process_handle = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
		if ( process_handle == nullptr )
		{
			last_error = GetLastError();
			Notify(_current_operation->HadError(last_error, std::string("OpenProcess() -> ") + error_code_as_string(last_error)));
		}
		else
		{
			if ( TerminateProcess(process_handle, 0) )
			{
				success = true;
				goto finish;
			}
			last_error = GetLastError();
			Notify(_current_operation->HadError(last_error, std::string("TerminateProcess() -> ") + error_code_as_string(last_error)));
		}
		
		/* if we haven't terminated it by now, use advanced, alternate
		 * methods to kill off the process - in order of stability */
		if ( runtime.Config()->purging.process.advanced_methods )
		{
			// 2) Terminate the threads
			NewOperation();
			Notify(_current_operation->Prepare(OP_KillProcess, "%s (id %d) [Terminate Threads]", name, pid));
			for ( ULONG i = 0; i < procinfo->ThreadCount; i++ )
			{
				HANDLE	thread;
				DWORD	tid = (DWORD)procinfo->Threads[i].ClientId.UniqueThread;

				NewOperation();
				Notify(_current_operation->Prepare(OP_KillThread, "%d", tid));

				thread = OpenThread(THREAD_TERMINATE, FALSE, tid);
				if ( thread == nullptr )
				{
					last_error = GetLastError();
					Notify(_current_operation->HadError(last_error, std::string("OpenThread() -> ") + error_code_as_string(last_error)));
					break;
				}
				if ( !TerminateThread(thread, 0) )
				{
					last_error = GetLastError();
					CloseHandle(thread);
					Notify(_current_operation->HadError(last_error, std::string("TerminateThread() -> ") + error_code_as_string(last_error)));
					break;
				}
				Notify(_current_operation->Finished());
				CloseHandle(thread);
			}
			PopPreviousOperation();
			// check procid does not exist
			if ( process_exists(procinfo->ProcessId) )
			{
				Notify(_current_operation->HadError(ERROR_FUNCTION_FAILED, error_code_as_string(ERROR_FUNCTION_FAILED)));
			}
			else
			{
				success = true;
				goto finish;
			}

			// 3) Debug the process, then stop (Windows will kill on teardown)
			NewOperation();
			Notify(_current_operation->Prepare(OP_KillProcess, "%s (id %d) [Debug Process]", name, pid));
			if ( !DebugActiveProcess(procinfo->ProcessId) )
			{
				last_error = GetLastError();
				Notify(_current_operation->HadError(last_error, std::string("DebugActiveProcess() -> ") + error_code_as_string(last_error)));
			}
			else
			{
				// 'wait' for attach to finish
				Sleep(150);

				if ( !DebugActiveProcessStop(procinfo->ProcessId) )
				{
					last_error = GetLastError();
					Notify(_current_operation->HadError(last_error, std::string("DebugActiveProcessStop() -> ") + error_code_as_string(last_error)));
				}
				else
				{
					if ( !process_exists(procinfo->ProcessId) )
					{
						success = true;
						goto finish;
					}
					Notify(_current_operation->HadError(ERROR_FUNCTION_FAILED, error_code_as_string(ERROR_FUNCTION_FAILED)));
				}
			}

			/* we still have the old process handle open, don't leak
			 * it, as we're about to open a fresh handle with 
			 * different access rights */
			CloseHandle(process_handle);

			// 4a) Create a thread in the process that calls ExitProcess()
			DWORD	tid;
			HANDLE	thread_handle;
			process_handle = OpenProcess(PROCESS_CREATE_THREAD|PROCESS_QUERY_INFORMATION|PROCESS_VM_OPERATION|PROCESS_VM_WRITE|PROCESS_VM_READ, FALSE, pid);
			thread_handle = CreateRemoteThread(
				process_handle,
				nullptr,
				128,
				(LPTHREAD_START_ROUTINE)exit_process, 
				nullptr, 
				0, 
				&tid);

			if ( thread_handle != nullptr ) 
			{
				// wait 1 1/2 seconds for execution to potentially finish before timeout
				WaitForSingleObject(thread_handle, 1500);
				CloseHandle(thread_handle);
			}

			// 4b) Create a thread in the process that makes it crash (divide by 0)
			thread_handle = CreateRemoteThread(
				process_handle,
				nullptr,
				128,
				(LPTHREAD_START_ROUTINE)divide_by_zero, 
				nullptr, 
				0, 
				&tid);

			if ( thread_handle != nullptr ) 
				CloseHandle(thread_handle);

			// 4c) Inject a DLL that just calls ExitProcess
			/** @todo add method 4c; dll injection */

		} // advanced methods

		CloseHandle(process_handle);
	}
	else
	{
		/* process not found, returns true */
		Notify(_current_operation->Finished(false));
		success = true;
		goto cleanup;
	}

finish:
	if ( process_handle != nullptr )
		CloseHandle(process_handle);

	if ( success )
		Notify(_current_operation->Finished());
cleanup:
	free(buffer);
	return success;
}



void
Wiper::NewOperation()
{
	/* 
	 * We allocate an operation to use initially. 
	 * If a new operation is requested when the last one is not finished (recursive functions),
	 * it is pushed into a list of pending completions.
	 * Otherwise, the original/last used operation is returned
	 */
	if ( _current_operation != nullptr )
	{
		if ( _current_operation->HasExecuted() )
			return;
		if ( !_current_operation->IsPaused() )
		{
			// must notify so the UI can enter a newline
			Notify(_current_operation->Pause());
		}
		_operations_stack.push(_current_operation);
	}

	_current_operation = new Operation;
}



uint32_t
Wiper::NumQueuedOperations()
{
	return _operations.size();
}



void
Wiper::PopPreviousOperation(
	bool notify
)
{
	// retrieve newest queued operation
	Operation*	queued_op = _operations_stack.top();

	assert(queued_op != nullptr);

	// remove it from the queue
	_operations_stack.pop();
	// set it back as the current operation
	_current_operation = queued_op;
	// and if wanted, output the original text again
	if ( notify )
		Notify(_current_operation->Resume());
}



bool
Wiper::QueueNewOperation(
	Operation* operation
)
{
	// sanity checks
	if ( operation == nullptr )
		throw std::runtime_error("The operation to queue is a nullptr");
	if ( operation->HasExecuted() )
		throw std::runtime_error("The operation to queue has already executed");

	// validate desired operation
	if ( operation->GetOperation() == OP_Dummy )
		return false;
	if ( operation->GetData().empty() )
		return false;

	_operations.push(operation);
	return true;
}



bool
Wiper::ReplacePermissionsOfFile(
	const wchar_t* path
)
{
	SECURITY_DESCRIPTOR	sd;
	HANDLE		token_handle = NULL;
	PTOKEN_USER	tokeninfo_user = nullptr;
	PACL		dacl = nullptr;
	LSTATUS		res;
	DWORD		dwAclSize = 0;
	DWORD		cbtiSize = 0;
	DWORD		last_error;
	bool		retval = false;

	if ( !OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES|TOKEN_QUERY, &token_handle) )
	{
		last_error = GetLastError();
		goto failed;
	}

#if 0
	if ( !GetTokenInformation(token_handle, TokenUser, NULL, 0, &cbtiSize) )
	{
		last_error = GetLastError();
		goto failed;
	}
#else
	GetTokenInformation(token_handle, TokenUser, NULL, 0, &cbtiSize);
#endif
	tokeninfo_user = (PTOKEN_USER)HeapAlloc(GetProcessHeap(), 0, cbtiSize);
	if ( !GetTokenInformation(token_handle, TokenUser, tokeninfo_user, cbtiSize, &cbtiSize) )
	{
		last_error = GetLastError();
		goto failed;
	}
	dwAclSize = sizeof(ACL) + sizeof(ACCESS_ALLOWED_ACE) - sizeof(DWORD) + GetLengthSid(tokeninfo_user->User.Sid);


	if (( dacl = (PACL)HeapAlloc(GetProcessHeap(), 0, dwAclSize)) == NULL )
	{
		last_error = GetLastError();
		goto failed;
	}

	if ( !InitializeAcl(dacl, dwAclSize, ACL_REVISION) )
	{
		last_error = GetLastError();
		goto failed;
	}

	if ( !AddAccessAllowedAce(
		dacl, 
		ACL_REVISION, 
		STANDARD_RIGHTS_ALL|SPECIFIC_RIGHTS_ALL, 
		tokeninfo_user->User.Sid) )
	{
		last_error = GetLastError();
		goto failed;
	}

	if ( !InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION) )
	{
		last_error = GetLastError();
		goto failed;
	}

	// REPLACES the existing dacl
	if ( !SetSecurityDescriptorDacl(&sd, TRUE, dacl, FALSE) )
	{
		last_error = GetLastError();
		goto failed;
	}

	// Apply the changes to the file/folder
	if (( res = SetFileSecurity(path, DACL_SECURITY_INFORMATION, &sd)) != ERROR_SUCCESS )
	{
		last_error = GetLastError();
		goto failed;
	}

	retval = true;
	Notify(_current_operation->Finished());
	goto clean;

failed:
	Notify(_current_operation->HadError(last_error, error_code_as_string(last_error)));
clean:
	if ( dacl != NULL )
		HeapFree(GetProcessHeap(), 0, dacl);
	if ( token_handle != NULL && token_handle != INVALID_HANDLE_VALUE )
		CloseHandle(token_handle);
	if ( tokeninfo_user != nullptr )
		HeapFree(GetProcessHeap(), 0, tokeninfo_user);

	return retval;
}



bool
Wiper::ReplacePermissionsOfKey(
	HKEY key_root,
	const wchar_t* subkey
)
{
	SECURITY_DESCRIPTOR	sd;
	HANDLE		token_handle = NULL;
	PTOKEN_USER	tokeninfo_user = nullptr;
	PACL		dacl = nullptr;
	LSTATUS		res;
	DWORD		dwAclSize = 0;
	DWORD		cbtiSize = 0;
	HKEY		hKey;
	DWORD		last_error;
	DWORD		token_access = TOKEN_ADJUST_PRIVILEGES|TOKEN_QUERY;
	bool		success = false;

	// needed within setkeysecurity
	if (( last_error = RegOpenKeyEx(key_root, subkey, NULL, WRITE_DAC, &hKey)) != ERROR_SUCCESS )
	{
		if ( last_error == ERROR_FILE_NOT_FOUND )
		{
			Notify(_current_operation->Finished(false));
			return true;
		}

		Notify(_current_operation->HadError(last_error, error_code_as_string(last_error)));
		return false;
	}

	if ( is_windows_version_gte(WINDOWS_7) )
		token_access |= TOKEN_DUPLICATE;

	/* from this point on, handles need closing and allocations need freeing,
	 * so jump to the cleanup code after reporting the error */
	if ( !OpenProcessToken(GetCurrentProcess(), token_access, &token_handle))
	{
		last_error = GetLastError();
		goto failed;
	}

#if 0
	if ( !GetTokenInformation(token_handle, TokenUser, NULL, 0, &cbtiSize) )
	{
		last_error = GetLastError();
		goto failed;
	}
#else
	GetTokenInformation(token_handle, TokenUser, NULL, 0, &cbtiSize);
#endif
	tokeninfo_user = (PTOKEN_USER)HeapAlloc(GetProcessHeap(), 0, cbtiSize);
	if ( !GetTokenInformation(token_handle, TokenUser, tokeninfo_user, cbtiSize, &cbtiSize) )
	{
		last_error = GetLastError();
		goto failed;
	}
	dwAclSize = sizeof(ACL) + sizeof(ACCESS_ALLOWED_ACE) - sizeof(DWORD) + GetLengthSid(tokeninfo_user->User.Sid);


	if (( dacl = (PACL)HeapAlloc(GetProcessHeap(), 0, dwAclSize)) == NULL )
	{
		last_error = GetLastError();
		goto failed;
	}

	if ( !InitializeAcl(dacl, dwAclSize, ACL_REVISION) )
	{
		last_error = GetLastError();
		goto failed;
	}

	if ( !AddAccessAllowedAce(
		dacl, 
		ACL_REVISION, 
		STANDARD_RIGHTS_ALL|SPECIFIC_RIGHTS_ALL, 
		tokeninfo_user->User.Sid) )
	{
		last_error = GetLastError();
		goto failed;
	}

	if ( !InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION) )
	{
		last_error = GetLastError();
		goto failed;
	}

	// REPLACES the existing dacl
	if ( !SetSecurityDescriptorDacl(&sd, TRUE, dacl, FALSE) )
	{
		last_error = GetLastError();
		goto failed;
	}

	// Apply the changes to the registry key [This key only]
	if (( res = RegSetKeySecurity(hKey, DACL_SECURITY_INFORMATION, &sd)) != ERROR_SUCCESS )
	{
		last_error = GetLastError();
		goto failed;
	}

	success = true;
	Notify(_current_operation->Finished());
	goto clean;

failed:
	Notify(_current_operation->HadError(last_error, error_code_as_string(last_error)));
clean:
	RegCloseKey(hKey);
	if ( dacl != NULL )
		HeapFree(GetProcessHeap(), 0, dacl);
	if ( token_handle != NULL && token_handle != INVALID_HANDLE_VALUE )
		CloseHandle(token_handle);
	if ( tokeninfo_user != nullptr )
		HeapFree(GetProcessHeap(), 0, tokeninfo_user);

	return success;
}



bool
Wiper::StopService(
	const wchar_t* name
)
{
	SC_HANDLE	service = NULL;
	SERVICE_STATUS	ss;
	DWORD		last_error;
	DWORD		wait_time;
	//DWORD		bytes_needed;
	uint64_t	start_time;

	if ( name == nullptr )
	{
		last_error = ERROR_INVALID_PARAMETER;
		goto failed;
	}	

	if ( _scm == nullptr )
	{
		_scm = OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT);

		if ( _scm == nullptr )
		{
			last_error = GetLastError();
			goto failed;
		}
	}

	if (( service = OpenService(_scm, name, SERVICE_QUERY_STATUS|SERVICE_STOP)) == nullptr )
	{
		last_error = GetLastError();

		// if it didn't exist, is not an error - success with not needed
		if ( last_error == ERROR_SERVICE_DOES_NOT_EXIST )
			goto not_needed;

		goto failed;
	}

	if ( !QueryServiceStatus( //Ex
		service,&ss)
		/*SC_STATUS_PROCESS_INFO,
		(LPBYTE)&ssp,
		sizeof(SERVICE_STATUS_PROCESS),
		&bytes_needed )*/
	)
	{
		last_error = GetLastError();
		goto failed;
	}

	if ( ss.dwCurrentState == SERVICE_STOPPED )
	{
		// already stopped - just finish up
		goto not_needed;
	}

	if ( !ControlService(service, SERVICE_CONTROL_STOP, &ss) )
	{
		last_error = GetLastError();
		CloseServiceHandle(service);

		if ( last_error != ERROR_SERVICE_NOT_ACTIVE )
			goto failed;

		goto not_needed;
	}

	// kick off the counter if we need it
	start_time = get_ms_time();

	while ( ss.dwCurrentState == SERVICE_STOP_PENDING ) 
	{
		/* Do not wait longer than the wait hint. A good interval is 
		 * one-tenth of the wait hint but not less than 1 second  
		 * and not more than 10 seconds. */

		wait_time = ss.dwWaitHint / 10;

		if ( wait_time < 1000 )
			wait_time = 1000;
		else if ( wait_time > 10000 )
			wait_time = 10000;

		Sleep(wait_time);

		if ( !QueryServiceStatus( //Ex
			service,&ss)
			/*SC_STATUS_PROCESS_INFO,
			(LPBYTE)&ssp,
			sizeof(SERVICE_STATUS_PROCESS),
			&bytes_needed ) )*/
		)
		{
			last_error = GetLastError();
			goto failed;
		}

		if ( ss.dwCurrentState == SERVICE_STOPPED )
			break;

		if ( (get_ms_time() - start_time) > wait_time )
		{
			last_error = ERROR_TIMEOUT;
			goto failed;
		}
	}

//succeeded:
	CloseServiceHandle(service);
	Notify(_current_operation->Finished());
	return true;
not_needed:
	if ( service != nullptr )
		CloseServiceHandle(service);
	Notify(_current_operation->Finished(false));
	return true;
failed:
	if ( service != nullptr )
		CloseServiceHandle(service);
	Notify(_current_operation->HadError(last_error, error_code_as_string(last_error)));
	return false;
}



bool
Wiper::TakeOwnershipOfFile(
	const wchar_t* path
)
{
	DWORD		res = 0;
	DWORD		cbtiSize = 0;
	HANDLE		token_handle = NULL;
	PTOKEN_USER	tokeninfo_user = nullptr;
	bool		retval = false;
	DWORD		last_error;

	/* from this point on, handles need closing and allocations need freeing,
	 * so jump to the cleanup code after reporting the error */
	if ( !OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token_handle) )
	{
		last_error = GetLastError();
		goto failed;
	}

#if 0
	if ( !GetTokenInformation(token_handle, TokenUser, NULL, 0, &cbtiSize) )
	{
		last_error = GetLastError();
		goto failed;
	}
#else
	GetTokenInformation(token_handle, TokenUser, NULL, 0, &cbtiSize);
#endif
	tokeninfo_user = (PTOKEN_USER)HeapAlloc(GetProcessHeap(), 0, cbtiSize);
	if ( !GetTokenInformation(token_handle, TokenUser, tokeninfo_user, cbtiSize, &cbtiSize) )
	{
		last_error = GetLastError();
		goto failed;
	}

	// Set ourselves as the owner
	if (( res = SetNamedSecurityInfo(
		(wchar_t*)path, 
		SE_FILE_OBJECT,		// ObjectType
		OWNER_SECURITY_INFORMATION,	// SecurityInfo
		tokeninfo_user->User.Sid,	// psidOwner
		NULL,	// pSidGroup
		NULL,	// pDacl
		NULL)	// pSacl
		) != ERROR_SUCCESS )
	{
		last_error = res;
		goto failed;
	}

	retval = true;
	Notify(_current_operation->Finished());
	goto clean;

failed:
	Notify(_current_operation->HadError(last_error, error_code_as_string(last_error)));
clean:
	if ( token_handle != NULL && token_handle != INVALID_HANDLE_VALUE )
		CloseHandle(token_handle);
	if ( tokeninfo_user != nullptr )
		HeapFree(GetProcessHeap(), 0, tokeninfo_user);

	return retval;
}



bool
Wiper::TakeOwnershipOfKey(
	HKEY key_root,
	const wchar_t* subkey
)
{
	DWORD		res = 0;
	DWORD		cbtiSize = 0;
	HANDLE		token_handle = NULL;
	PTOKEN_USER	tokeninfo_user = nullptr;
	bool		retval = false;
	wchar_t		buffer[1024];
	wchar_t*	path = &buffer[0];
	uint32_t	alloc = 0;
	DWORD		last_error;

	// SE_OBJECT_TYPE names use a different format for reg entries
	if ( key_root == HKEY_CURRENT_USER )
		wcscpy_s(buffer, _countof(buffer), L"CURRENT_USER\\");
	else if ( key_root == HKEY_CLASSES_ROOT )
		wcscpy_s(buffer, _countof(buffer), L"CLASSES_ROOT\\");
	else if ( key_root == HKEY_USERS )
		wcscpy_s(buffer, _countof(buffer), L"USERS\\");
	else
		wcscpy_s(buffer, _countof(buffer), L"MACHINE\\");

	/* if our buffer is too small (rare), allocate dynamically; see the reg limits:
	 * http://msdn.microsoft.com/en-us/library/windows/desktop/ms724872%28v=vs.85%29.aspx */
	if ( wcslen(subkey) > 490 )
	{
		alloc = wcslen(subkey)+16;	// nul, key_prefix
		path = (wchar_t*)malloc(alloc);
		if ( path == nullptr )
		{
			last_error = ERROR_OUTOFMEMORY;
			goto failed;
		}

		wcscpy_s(path, alloc, buffer);
	}
	else
	{
		wcscat_s(buffer, _countof(buffer), subkey);
	}

	/* from this point on, handles need closing and allocations need freeing,
	 * so jump to the cleanup code after reporting the error */
	if ( !OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token_handle) )
	{
		last_error = GetLastError();
		goto failed;
	}

#if 0
	if ( !GetTokenInformation(token_handle, TokenUser, NULL, 0, &cbtiSize) )
	{
		last_error = GetLastError();
		goto failed;
	}
#else
	GetTokenInformation(token_handle, TokenUser, NULL, 0, &cbtiSize);
#endif
	tokeninfo_user = (PTOKEN_USER)HeapAlloc(GetProcessHeap(), 0, cbtiSize);
	if ( !GetTokenInformation(token_handle, TokenUser, tokeninfo_user, cbtiSize, &cbtiSize) )
	{
		last_error = GetLastError();
		goto failed;
	}

	// Set ourselves as the owner
	if (( res = SetNamedSecurityInfo(
		(wchar_t*)path, 
		SE_REGISTRY_KEY,		// ObjectType
		OWNER_SECURITY_INFORMATION,	// SecurityInfo
		tokeninfo_user->User.Sid,	// psidOwner
		NULL,	// pSidGroup
		NULL,	// pDacl
		NULL)	// pSacl
		) != ERROR_SUCCESS )
	{
		last_error = res;
		goto failed;
	}

	retval = true;
	Notify(_current_operation->Finished());
	goto clean;

failed:
	Notify(_current_operation->HadError(last_error, error_code_as_string(last_error)));
clean:
	if ( token_handle != NULL && token_handle != INVALID_HANDLE_VALUE )
		CloseHandle(token_handle);
	if ( tokeninfo_user != nullptr )
		HeapFree(GetProcessHeap(), 0, tokeninfo_user);
	// free our allocated memory (in the event of a long subkey length)
	if ( alloc != 0 && path != nullptr )
		free(path);

	return retval;
}



bool
Wiper::WipeJavaAppData()
{
	HKEY	profile_key;
	DWORD	buf_size;
	DWORD	last_error = ERROR_SUCCESS;
	wchar_t		buffer[1024];
	wchar_t		env[1024];
	wchar_t		purge_path[1024];
	const wchar_t*	subkey = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\ProfileList";
	const wchar_t*	value = L"ProfilesDirectory";
	UserInterface*	ui = runtime.UI();

	buf_size = _countof(buffer);

	NewOperation();
	Notify(_current_operation->Prepare(OP_Dummy, "%s", "user profiles directory"));

	if (( last_error = RegOpenKeyEx(HKEY_LOCAL_MACHINE, subkey, NULL, KEY_READ, &profile_key)) != ERROR_SUCCESS )
	{
		Notify(_current_operation->HadError(last_error, error_code_as_string(last_error)));
		return false;
	}
	if (( last_error = RegQueryValueEx(profile_key, value, NULL, NULL, (BYTE*)&buffer, &buf_size)) != ERROR_SUCCESS )
	{
		RegCloseKey(profile_key);
		Notify(_current_operation->HadError(last_error, error_code_as_string(last_error)));
		return false;
	}

	RegCloseKey(profile_key);

	// buffer usually comes out with: %SystemDrive%\[Users|Documents and Settings]
	if ( ExpandEnvironmentStrings(buffer, env, _countof(env)) == 0 )
	{
		last_error = GetLastError();
		Notify(_current_operation->HadError(last_error, error_code_as_string(last_error)));
		return false;
	}

	Notify(_current_operation->Finished());

	wcscpy_s(buffer, _countof(buffer), env);

	UTF8ToMb(mb, buffer, _countof(mb));
	ui->Output(OUTCOLOR_INFO, "\t%s\n", mb);

	NewOperation();
	Notify(_current_operation->Prepare(OP_Dummy, "%s", "Enumerating user profiles"));

	
	/* all users appdata. for the programdata (all users appdata):
	#include <shlwapi.h>
	
	*/
	
	WIN32_FIND_DATA     wfd;
	HANDLE              hSearch;

	wcscat_s(buffer, _countof(buffer), L"\\*");

	if (( hSearch = FindFirstFile(buffer, &wfd)) == INVALID_HANDLE_VALUE )
	{
		last_error = GetLastError();

		if ( last_error != ERROR_FILE_NOT_FOUND )
		{
			Notify(_current_operation->HadError(last_error, error_code_as_string(last_error)));
			return false;
		}

		Notify(_current_operation->Finished(false));
	}
	else
	{
		Notify(_current_operation->Finished());
		
		uint32_t	i = wcslen(buffer);

		// 'i' cannot be 0 here (we appended before entering this scope)

		// remove the wildcard character
		buffer[(i-1)] = '\0';

		do
		{
			/* nop on the posix directory pathing */
			if ( wcscmp(wfd.cFileName, L".") != 0 &&
				wcscmp(wfd.cFileName, L"..") != 0 )
			{
				/* not interested in individual files */
				if ( !(wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) )
					continue;

				/* there are certain profile names we want to skip,
				 * as they either aren't real or part of the system.
				 * However, an older version will allow this as a 
				 * username, so we have to account for that! */
				if ( is_windows_version_gte(_WIN32_WINNT_LONGHORN) )
				{
					if ( wcscmp(wfd.cFileName, L"All Users") == 0 )
						continue;
				}
				else
				{
					if ( wcscmp(wfd.cFileName, L"All Users") == 0 ||
						wcscmp(wfd.cFileName, L"Default User") == 0 || 
						wcscmp(wfd.cFileName, L"Public") == 0 )
						continue;
				}

				// copy the profile path
				wcscpy_s(purge_path, _countof(purge_path), buffer);
				// append the users folder
				wcscat_s(purge_path, _countof(purge_path), wfd.cFileName);

				// append the appdata/sun folder (or wiper input from config)
				if ( is_windows_version_gte(_WIN32_WINNT_LONGHORN) )
				{
					wcscat_s(purge_path, _countof(purge_path), L"\\Application Data\\Sun\\Java");
				}
				else
				{
					wcscat_s(purge_path, _countof(purge_path), L"\\AppData\\Local\\Sun\\Java");
				}

				UTF8ToMb(mb, purge_path, _countof(mb));
				NewOperation();
				Notify(_current_operation->Prepare(OP_DeleteDirectory, "%s", mb));

				// will finish the current operation for us
				if ( !EradicateFileObject(purge_path) )
					break;
			}

		} while ( FindNextFile(hSearch, &wfd) );

		FindClose(hSearch);
	}

	return true;
}



bool
Wiper::WipeFolderTest()
{
	UserInterface*	ui = runtime.UI();
	wchar_t		program_data[MAX_PATH];
	wchar_t		all_startup[MAX_PATH];
	wchar_t		user_startup[MAX_PATH];
	wchar_t		path[MAX_PATH*4];

#if MINIMUM_TARGET >= _WIN32_WINNT_VISTA
	if ( !SUCCEEDED(SHGetKnownFolderPath()) )
	{
	}
#else
	NewOperation();
	Notify(_current_operation->Prepare(OP_Dummy, "%s", "All Users Startup"));
	if ( !SUCCEEDED(SHGetFolderPath(nullptr, CSIDL_COMMON_APPDATA, nullptr, SHGFP_TYPE_CURRENT, all_startup)) )
	{
		//get_path_manually(CSIDL_COMMON_APPDATA, program_data, _countof(program_data));
	}
	Notify(_current_operation->Finished());
	UTF8ToMb(mb, program_data, sizeof(mb));
	ui->Output(OUTCOLOR_INFO, "\t%s\n", mb);

	NewOperation();
	Notify(_current_operation->Prepare(OP_Dummy, "%s", "All Users Startup"));
	if ( !SUCCEEDED(SHGetFolderPath(nullptr, CSIDL_COMMON_STARTUP, nullptr, SHGFP_TYPE_CURRENT, all_startup)) )
	{
		//get_path_manually(CSIDL_COMMON_STARTUP, all_startup, _countof(all_startup));
	}
	Notify(_current_operation->Finished());
	UTF8ToMb(mb, all_startup, sizeof(mb));
	ui->Output(OUTCOLOR_INFO, "\t%s\n", mb);

	NewOperation();
	Notify(_current_operation->Prepare(OP_Dummy, "%s", "User Startup"));
	if ( !SUCCEEDED(SHGetFolderPath(nullptr, CSIDL_STARTUP, nullptr, SHGFP_TYPE_CURRENT, user_startup)) )
	{
		//get_path_manually(CSIDL_STARTUP, user_startup, _countof(user_startup));
	}
	Notify(_current_operation->Finished());
	UTF8ToMb(mb, user_startup, sizeof(mb));
	ui->Output(OUTCOLOR_INFO, "\t%s\n", mb);
#endif

	wcscpy_s(path, _countof(path), all_startup);
	wcscat_s(path, _countof(path), L"\\Sun\\Java");
	UTF8ToMb(mb, path, sizeof(mb));
	NewOperation();
	Notify(_current_operation->Prepare(OP_DeleteDirectory, "%s", mb));
	EradicateFileObject(path);

	wcscpy_s(path, _countof(path), all_startup);
	wcscat_s(path, _countof(path), L"\\WiperTestFolder");
	UTF8ToMb(mb, path, sizeof(mb));
	NewOperation();
	Notify(_current_operation->Prepare(OP_DeleteDirectory, "%s", mb));
	EradicateFileObject(path);

	wcscpy_s(path, _countof(path), user_startup);
	wcscat_s(path, _countof(path), L"\\WiperTestFile.exe");
	UTF8ToMb(mb, path, sizeof(mb));
	NewOperation();
	Notify(_current_operation->Prepare(OP_DeleteFile, "%s", mb));
	EradicateFileObject(path);

	return true;
}



bool
Wiper::WipeRegistryTest()
{
	/* we changed Prepare() purely because of registry descriptions...! While
	 * we don't need them here (hardcoded), they are needed when reading the
	 * input from the file */

	NewOperation();
	Notify(_current_operation->Prepare(OP_DeleteRegValue, "%s", "HKCU\\Software\\WiperTest\\XYZ News->testval"));
	EradicateRegistryObject(HKEY_CURRENT_USER, L"Software\\WiperTest\\XYZ News", L"testval");

	NewOperation();
	Notify(_current_operation->Prepare(OP_DeleteRegValue, "%s", "HKCU\\Software\\WiperTest\\XYZ News->testval2"));
	EradicateRegistryObject(HKEY_CURRENT_USER, L"Software\\WiperTest\\XYZ News", L"testval2");

	NewOperation();
	Notify(_current_operation->Prepare(OP_DeleteRegKey, "%s", "HKCU\\Software\\WiperTest"));
	EradicateRegistryObject(HKEY_CURRENT_USER, L"Software\\WiperTest\\");

	return true;
}



bool
Wiper::WipeProcessTest()
{
	// have powershell and regedit opened or closed, as desired

	NewOperation();
	Notify(_current_operation->Prepare(OP_KillProcess, "%s", "calc.exe"));
	KillProcess(L"calc.exe");

	return true;
}



bool
Wiper::WipeServiceTest()
{
	// service to stop that'll be likely running yet won't stop the system if closed
	NewOperation();
	Notify(_current_operation->Prepare(OP_StopService, "%s", "service_test"));
	StopService(L"service_test");

	// service to delete - we create this manually for the lack of another test one!
	// sc create service_test binpath= C:\WINDOWS\system32\test.exe type= own start= demand
	NewOperation();
	Notify(_current_operation->Prepare(OP_DeleteService, "%s", "service_test"));
	DeleteService(L"service_test");

	return true;
}
