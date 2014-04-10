
/**
 * @file	ntquerysysteminformation.cc
 * @author	James Warren
 * @copyright	James Warren, 2013-2014
 * @license	Zlib (see license.txt or http://opensource.org/licenses/Zlib)
 */


#include "build.h"		// always first

#include <Windows.h>		// OS API

#include "ntquerysysteminformation.h"	// prototypes
#include "utils.h"			// get_function_address



NTSTATUS
NtQuerySystemInformation(
	SYSTEM_INFORMATION_CLASS SystemInformationClass,
	PVOID SystemInformation,
	ULONG SystemInformationLength,
	PULONG ReturnLength
)
{
	typedef NTSTATUS (WINAPI *pf_NtQuerySystemInformation)(SYSTEM_INFORMATION_CLASS, PVOID, ULONG, PULONG);

	static pf_NtQuerySystemInformation	_NtQuerySystemInformation = (pf_NtQuerySystemInformation)get_function_address("NtQuerySystemInformation", L"ntdll.dll");

	if ( _NtQuerySystemInformation == nullptr )
		return ERROR_INVALID_FUNCTION;

	return _NtQuerySystemInformation(SystemInformationClass, SystemInformation, SystemInformationLength, ReturnLength);
}
