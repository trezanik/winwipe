
/**
 * @file	utils.cc
 * @author	James Warren
 * @copyright	James Warren, 2013-2014
 * @license	Zlib (see license.txt or http://opensource.org/licenses/Zlib)
 */


#include "build.h"		// always first

// we're C++ using C functions, so headers are slightly different from normal C
#include <cstdio>		// vsnprintf
#include <cstring>		// strlen
#include <ctype.h>		// isspace
#include <cassert>
#include <cstdlib>		// _countof
#include <exception>

#include "utils.h"

#include <Windows.h>		// UTF-8 conversion
#include <Psapi.h>		// get loaded modules, etc.



#if IS_VISUAL_STUDIO
#	pragma warning ( push )
#	pragma warning ( disable : 4723 )	// potential divide by zero
#endif

unsigned __stdcall
divide_by_zero(
	void* null
)
{
	UNREFERENCED_PARAMETER(null);

	// 'normal' numbers just in case a compiler optimizes out
	int32_t	x = 5;
	int32_t	y = 7;
	int32_t	z = x + y / 0;
	
	// should never reach here!!
	x /= 0;
	y /= 0;
	z = z * z * z;
	z /= 0;

	return 0;
}

#if IS_VISUAL_STUDIO
#	pragma warning ( pop )
#endif



char*
error_code_as_string(
	uint64_t code
)
{
	static char	error_str[512];

	error_str[0] = '\0';

	if ( !FormatMessageA(
		FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr,
		(DWORD)code,
		0,
		error_str,
		sizeof(error_str),
		nullptr) )
	{
		// always return a string, some callers expect it :)
		str_format(error_str, sizeof(error_str), "(unknown error code)");
		return &error_str[0];
	}


	char*	p;

	// some of the messages helpfully come with newlines at the end..
	while (( p = strchr(error_str, '\r')) != nullptr )
		*p = '\0';
	while (( p = strchr(error_str, '\n')) != nullptr )
		*p = '\0';

	return &error_str[0];
}



#if IS_VISUAL_STUDIO
#	pragma warning ( push )
#	pragma warning ( disable : 4702 )	// unreachable code (return 0;)
#endif

unsigned __stdcall
exit_process(
	void* null
)
{
	UNREFERENCED_PARAMETER(null);

	ExitProcess(0);
	return 0;
}

#if IS_VISUAL_STUDIO
#	pragma warning ( pop )
#endif



uint32_t
get_current_binary_path(
	char* buffer,
	uint32_t buffer_size
)
{
	char	*r;

	/* this is a VERY platform-independent task; one code block for each
	 * OS we are supporting! */

	if ( buffer_size < 2 )
	{
		throw std::runtime_error("Insufficient buffer size");
	}

#if defined(_WIN32)

	char	path_sep = '\\';
	uint32_t	res;
	wchar_t	tmp[MAX_PATH];

	if (( res = GetModuleFileName(nullptr, tmp, _countof(tmp))) == 0 ||
		res == buffer_size )	// http://msdn.microsoft.com/en-us/library/windows/desktop/ms683197(v=vs.85).aspx @ WinXP return value
	{
		throw std::runtime_error("Failed to fully retrieve the current binarys module path");
	}

	UTF8ToMb(buffer, tmp, buffer_size);

#elif __linux__

	char	path_sep = '/';
	int32_t	res = readlink("/proc/self/exe", buffer, buffer_size);

	if ( res == -1 )
	{
		throw std::runtime_error("readlink() for the current binary failed");
	}

#elif __FreeBSD__
#	error "Incomplete - /proc/curproc/exe"
#else
#	error "Unimplemented on the current Operating System; please add it if you can!"
#endif

	// find the last trailing path separator
	if (( r = strrchr(buffer, path_sep)) == nullptr )
	{
		throw std::runtime_error("The buffer for the current path contained no path separators");
	}

	// nul out the character after it, ready for appending
	*++r = '\0';

	// return number of characters written, after taking into account the new nul
	return ( r - &buffer[0] );
}



bool
get_file_version_info(
	wchar_t* path,
	file_version_info* fvi
)
{
	/* copy of the code in GetFunctionAddress, with relevant bits removed;
	 * this is not debug-only code */

	DWORD			size;
	DWORD			dummy;
	uint8_t*		data = nullptr;
	VS_FIXEDFILEINFO*	finfo;
	uint32_t		length;

	/* set to defaults; prevents old information if the struct is reused, 
	 * no need for the caller to handle failures itself */
	fvi->description[0]	= '\0';
	fvi->build		= 0;
	fvi->major		= 0;
	fvi->minor		= 0;
	fvi->revision		= 0;



	if (( size = GetFileVersionInfoSize(path, &dummy)) == 0 )
		goto failed;

	if (( data = (uint8_t*)malloc(size)) == nullptr )
		goto failed;

	if ( !GetFileVersionInfo(path, NULL, size, &data[0]) )
		goto failed;
	
	if ( !VerQueryValue(data, L"\\", (void**)&finfo, &length) )
		goto failed;

	fvi->major = HIWORD(finfo->dwFileVersionMS);
	fvi->minor = LOWORD(finfo->dwFileVersionMS);
	fvi->revision = HIWORD(finfo->dwFileVersionLS);
	fvi->build = LOWORD(finfo->dwFileVersionLS);
	
	/** @todo get file description */

	// required data copied, can now free it
	free(data);

	return true;

failed:
	if ( data != nullptr )
		free(data);
	return false;
}



void*
get_function_address(
	char* func_name,
	wchar_t* module_name
)
{
	HMODULE		module;
	void		*func_address;

	if ( func_name == nullptr )
		return nullptr;
	if ( module_name == nullptr )
		return nullptr;

	module = GetModuleHandle(module_name);

	if ( module == NULL )
	{
		return nullptr;
	}

	func_address = GetProcAddress(module, func_name);

	return func_address;
}



std::vector<ModuleInformation*>
get_loaded_modules()
{
	std::vector<ModuleInformation*>	ret;
	ModuleInformation*		mi;
	HANDLE		process_handle = GetCurrentProcess();
	HMODULE*	modules;
	wchar_t		module_path[MAX_PATH];
	DWORD		size;
	DWORD		module_count;

	/* EnumProcessModulesEx handles 32 and 64-bit binaries properly, or at
	 * least with choice; it is only available on Vista and Server 2008
	 * onwards however. */
#if MINIMUM_TARGET < _WIN32_WINNT_VISTA
	// xp, 5.2 era
	if ( !EnumProcessModules(process_handle, nullptr, 0, &size) )
		return ret;

	modules = (HMODULE*)malloc(size);

	if ( !EnumProcessModules(process_handle, modules, size, &size) )
	{
		free(modules);
		return ret;
	}
#else
	if ( !EnumProcessModulesEx(process_handle, nullptr, 0, &size, LIST_MODULES_ALL) )
		return ret;

	modules = (HMODULE*)malloc(size);

	if ( !EnumProcessModulesEx(process_handle, modules, size, &size, LIST_MODULES_ALL) )
	{
		app_free(modules);
		return ret;
	}
#endif

	module_count = (size / sizeof(HMODULE));

	for ( DWORD i = 0; i < module_count; i++ )
	{
		// errors not handled; not much we can do with them anyway..
		if ( GetModuleFileNameEx(process_handle, modules[i], module_path, sizeof(module_path)) > 0 )
		{
			mi = (ModuleInformation*)malloc(sizeof(ModuleInformation));

			wcscpy_s(mi->name, module_path);
			get_file_version_info(mi->name, &mi->fvi);

			ret.push_back(mi);
		}
	}

	free(modules);

	return ret;
}



// acquired from http://stackoverflow.com/questions/1861294/how-to-calculate-execution-time-of-a-code-snippet-in-c
uint64_t
get_ms_time()
{
#ifdef _WIN32
	FILETIME	ft;
	LARGE_INTEGER	li;

	/* Get the amount of 100 nano seconds intervals elapsed since January 1, 1601 (UTC) and copy it
	 * to a LARGE_INTEGER structure. */
	GetSystemTimeAsFileTime(&ft);
	li.LowPart = ft.dwLowDateTime;
	li.HighPart = ft.dwHighDateTime;

	uint64_t ret = li.QuadPart;
	ret -= 116444736000000000LL;	// Convert from file time to UNIX epoch time.
	ret /= 10000;	// From 100 nano seconds (10^-7) to 1 millisecond (10^-3) intervals

	return ret;
#else
	struct timeval tv;

	gettimeofday(&tv, NULL);

	uint64 ret = tv.tv_usec;
	// Convert from micro seconds (10^-6) to milliseconds (10^-3)
	ret /= 1000;

	// Adds the seconds (10^0) after converting them to milliseconds (10^-3)
	ret += (tv.tv_sec * 1000);

	return ret;
#endif
}



#if IS_VISUAL_STUDIO && defined(_WIN32)

bool
MbToUTF8(
	wchar_t* buf,
	const char* mb,
	uint32_t buf_size
)
{
	if ( buf == nullptr || mb == nullptr || buf_size < 2 )
	{
		return false;
	}

	if ( MultiByteToWideChar(CP_UTF8, 0, mb, -1, buf, buf_size) == 0 )
	{
		printf("MultiByteToWideChar() failed: %d\n", GetLastError());
		return false;
	}

	return true;
}



bool
UTF8ToMb(
	char* buf,
	const wchar_t* wchar,
	uint32_t buf_size
)
{
	if ( buf == nullptr || wchar == nullptr || buf_size < 2 )
	{
		return false;
	}

	if ( WideCharToMultiByte(CP_UTF8, 0, wchar, -1, buf, buf_size, nullptr, nullptr) == 0 )
	{
		printf("WideCharToMultiByte() failed: %d\n", GetLastError());
		return false;
	}

	return true;
}


#endif	// IS_VISUAL_STUDIO && _WIN32



bool
process_exists(
	unsigned long pid
)
{
	/* msdn: if we have SeDebugPrivilege, OpenProcess always succeeds:
	 * http://msdn.microsoft.com/en-us/library/windows/desktop/ms684320%28v=vs.85%29.aspx
	 * comment also highlights issue in regards to the id
	 */
	DWORD	access_mask =
#if _WIN32_WINNT >= _WIN32_WINNT_VISTA
		PROCESS_QUERY_LIMITED_INFORMATION;
#else
		PROCESS_QUERY_INFORMATION;
#endif
	HANDLE	process = OpenProcess(access_mask, FALSE, pid);
	DWORD	last_error = GetLastError();

	if ( process != nullptr && GetProcessId(process) != pid )
	{
		// not the process id we asked for!!
		CloseHandle(process);
		return false;
	}
	if ( process == nullptr )
	{
		/* possibilities for failures to OpenProcess outside of access
		 * denied? such as handle limit reached?? */
		if ( last_error != ERROR_ACCESS_DENIED )
			return false;

		// denied access, so exists
		return true;
	}

	CloseHandle(process);
	return true;
}



char*
skip_whitespace(
	char* str
)
{
	while ( isspace(*str) )
		++str;
	return str;
}



uint32_t
strlcat(
	char* dest,
	const char* src,
	uint32_t size
)
{
	char*		d = dest;
	const char*	s = src;
	uint32_t	n = size;
	uint32_t	len;

	assert(d != nullptr);
	assert(s != nullptr);

	// Find the end of dst and adjust bytes left but don't go past end
	while ( *d != '\0' && n-- != 0 )
		d++;

	len = d - dest;
	n = size - len;

	if ( n == 0 )
	{
		// amendment from OpenBSD source
		if ( size != 0 )
			*--d = '\0';

		return (len + strlen(s));
	}

	while ( *s != '\0' )
	{
		if ( n != 1 )
		{
			*d++ = *s;
			n--;
		}
		s++;
	}

	*d = '\0';

	return (len + (s - src));	// count does not include NUL
}



uint32_t
strlcpy(
	char* dest,
	const char* src,
	uint32_t size
)
{
	char*		d = dest;
	const char*	s = src;
	uint32_t	n = size;

	assert(d != nullptr);
	assert(s != nullptr);

	// Copy as many bytes as will fit
	if ( n != 0 && --n != 0 )
	{
		do
		{
			if ( (*d++ = *s++) == 0 )
				break;
		} while ( --n != 0 );
	}

	// Not enough room in dest, add NUL and traverse rest of src
	if ( n == 0 )
	{
		if ( size != 0 )
			*d = '\0';	// nul-terminate dest
		while (*s++);
	}

	return (s - src - 1);	// count does not include NUL
}



uint32_t
str_format(
	char* destination,
	uint32_t dest_size,
	char* format,
	...
)
{
	int32_t		res = 0;
	va_list		varg;

	assert(destination != nullptr);
	assert(format != nullptr);
	
	if ( dest_size <= 1 )
		return 0;

	va_start(varg, format);

	/* always leave 1 for the nul terminator - this is the security complaint
	 * that visual studio will warn us about. Since we have coded round it,
	 * forcing each instance to include '-1' with a min 'dest_size' of 1, this
	 * is perfectly safe. Visual Studio needs alternatives. */
#if defined(_WIN32)
	res = vsnprintf_s(destination, dest_size, dest_size, format, varg);

	va_end(varg);

	if ( res == -1 )
	{
		// destination text has been truncated or error
		destination[dest_size-1] = '\0';
		return 0;
	}
	
#else
	// res is the number of characters that would have been written
	res = vsnprintf(destination, (dest_size - 1), format, varg);

	va_end(varg);

	if ( res >= dest_size )
	{
		/* destination text has been truncated */
		destination[dest_size-1] = '\0';
		return 0;
	}

	// this means it fit in the buffer, so can use the return value as is

#endif	// _WIN32

	// to ensure nul-termination
	destination[res] = '\0';

	// number of characters written without the nul
	return (uint32_t)res;
}



char*
str_token(
	char* src,
	const char* delim,
	char** context
)
{
	char*	ret = nullptr;

	if ( src == nullptr )
		src = *context;

	// skip leading delimiters
	while ( *src && strchr(delim, *src) )
		++src;

	if ( *src == '\0' )
		return ret;

	ret = src;

	// break on end of string or upon finding a delimiter
	while ( *src && !strchr(delim, *src) )
		++src;

	// if a delimiter was found, nul it
	if ( *src )
		*src++ = '\0';

	*context = src;

	return ret;
}



char*
str_trim(
	char* src
)
{
	size_t	len;
	char*	e;

	assert(src != nullptr);

	src = skip_whitespace(src);
	len = strlen(src);

	if ( !len )
		return src;

	e = src + len - 1;
	while ( e >= src && isspace(*e) )
		e--;
	*(e + 1) = '\0';

	return src;
}



bool
time_diff_as_string(
	time_t start_time,
	time_t end_time,
	char* dest,
	uint32_t dest_size
)
{
	time_t		conv = end_time - start_time;
	uint32_t	days = 0;
	uint32_t	hours = 0;
	uint32_t	minutes = 0;
	uint32_t	seconds = 0;

	if ( start_time > end_time )
		return false;

	if ( conv == 0 )
		return false;

	seconds	= conv % 60;
	conv	= conv / 60;
	minutes = conv % 60;
	conv	= conv / 60;
	hours	= conv % 24;
	conv	= conv / 24;
	days	= conv % 365;

	if ( days > 0 )
	{
		str_format(dest, dest_size, 
			"%d days, %d hours, %d minutes, %d seconds",
			days, hours, minutes, seconds);
	}
	else if ( hours > 0 )
	{
		str_format(dest, dest_size, 
			"%d hours, %d minutes, %d seconds",
			hours, minutes, seconds);
	}
	else if ( minutes > 0 )
	{
		str_format(dest, dest_size, 
			"%d minutes, %d seconds",
			minutes, seconds);
	}
	else if ( seconds > 0 )
	{
		str_format(dest, dest_size, 
			"%d seconds",
			seconds);
	}
	else
	{
		strlcpy(dest, "less than 1 second", dest_size);
	}

	return true;
}



// Google Tests
#if defined(TESTING_UTILS)
#	include "../gtest/gtest/gtest.h"


TEST(StringFunctions, strlcat)
{
	char	str1[] = "\0";
	char	str2[] = "abcdefg";
	char	str3[] = "hijklmn";
	char	str4[] = "opqrstuvwxyz";
	char	buf1[14] = { '\xF' };
	char	buf2[2] = { '\xF' };
	char	buf3[27] = { '\xF' };
	size_t	n;

// appending string is 1 char too long for the buffer
	n = strlcpy(buf1, str2, sizeof(buf1));
	// should be the length of the two strings
	EXPECT_EQ(strlen(str3) + n, strlcat(buf1, str3, sizeof(buf1)));
	// should have truncated 'n', in its place the nul-terminator
	EXPECT_EQ(true, buf1[13] == '\0');
	EXPECT_EQ(true, buf1[12] == 'm');
// filled buffer with nul, test strlen
	strlcpy(buf3, buf1, sizeof(buf3));
	buf3[13] = 'n';		// as it wasn't copied in previous test!
	buf3[14] = '\0';	// need to nul-terminate again
	// should copy all successfully
	EXPECT_EQ(sizeof(buf3) - 1, strlcat(buf3, str4, sizeof(buf3)));
// tiny buffer, cannot write, but will nul-terminate (non-standard)
	buf2[0] = 'g';
	buf2[1] = 'q';
	strlcat(buf2, str2, sizeof(buf2));
	EXPECT_EQ(true, buf2[1] == '\0');
// append zero-length string
	n = strlcpy(buf3, str2, sizeof(buf3));
	EXPECT_EQ(n, strlcat(buf3, str1, sizeof(buf3)));
}

TEST(StringFunctions, strlcpy)
{
	char	str1[] = "\0";
	char	str2[] = "abcdefg";
	char	buf1[1] = { 'z' };
	char	buf2[5];
	char	buf3[9];
	size_t	n;

// copying zero-length string
	n = strlcpy(buf1, str1, sizeof(buf1));
	// length of copy should be 0
	EXPECT_EQ(0, n);
	// as there was enough room, should have nul-terminated
	EXPECT_EQ(true, buf1[0] == '\0');
// copying string too long for buffer
	n = strlcpy(buf2, str2, sizeof(buf2));
	EXPECT_EQ(strlen(str2), n);
	EXPECT_EQ(true, buf2[4] == '\0');
	EXPECT_EQ(true, buf2[3] == 'd');
// copying string shorted than buffer
	n = strlcpy(buf3, str2, sizeof(buf3));
	EXPECT_EQ(strlen(str2), n);
	EXPECT_EQ(true, buf3[7] == '\0');
	EXPECT_EQ(true, buf3[6] == 'g');
}

TEST(StringFunctions, str_token)
{
	char	str1[] = "\0";
	char	str2[] = "12345678901234567890";
	char	str2a[] = "12345678901234567890";
	char	str3[] = "1234567890";
	char	str4[] = "1write1,2write2,3write3";
	char	str5[] = "1text1,2text2,3text3,";
	char	delim_str1[] = "90";
	char	delim_str2[] = "12";
	char	delim_str3[] = "te";
	char	delim_str4[] = ",";
	char	*last;
	char	*p;

// non-null source, but nul-terminated instantaneously
	last = nullptr;
	EXPECT_EQ(nullptr, str_token(str1,   delim_str1, &last));
// numerics, end of string + middle
	last = nullptr;
	// first portion, 1..8
	EXPECT_NE(nullptr, str_token(str2,   delim_str1, &last));
	// nul's 9+0, returns 1..8
	EXPECT_NE(nullptr, str_token(nullptr,delim_str1, &last));
	// final delims skipped, no more tokens
	EXPECT_EQ(nullptr, str_token(nullptr,delim_str1, &last));
// numerics, start of string + middle
	last = nullptr;
	// skip first 2 chars, 3..0
	EXPECT_NE(nullptr, str_token(str2a,  delim_str2, &last));
	// nul's 1+2, returns 3..0
	EXPECT_NE(nullptr, str_token(nullptr,delim_str2 ,&last));
	// end of string
	EXPECT_EQ(nullptr, str_token(nullptr,delim_str2, &last));
// numerics, end of string
	last = nullptr;
	// 1..8
	EXPECT_NE(nullptr, str_token(str3,   delim_str1, &last));
	// final chars are tokens, skipped
	EXPECT_EQ(nullptr, str_token(nullptr,delim_str1, &last));
// strings
	last = nullptr;
	// 1wri
	p = str_token(str4, delim_str3, &last);
	EXPECT_EQ(0, strcmp("1wri", p));
	// skips te, '1,2wri'
	p = str_token(nullptr, delim_str3, &last);
	EXPECT_EQ(0, strcmp("1,2wri", p));
	// skips te, '2,3wri'
	p = str_token(nullptr, delim_str3, &last);
	EXPECT_EQ(0, strcmp("2,3wri", p));
	// skips te, '3' then end of string
	p = str_token(nullptr, delim_str3, &last);
	EXPECT_EQ(0, strcmp("3", p));
	EXPECT_EQ(nullptr, str_token(nullptr,delim_str3, &last));

	last = nullptr;
	// 1text1
	EXPECT_NE(nullptr, str_token(str5,   delim_str4, &last));
	// 2text2
	EXPECT_NE(nullptr, str_token(nullptr,delim_str4, &last));
	// 3text3
	p = str_token(nullptr, delim_str4, &last);
	EXPECT_EQ(0, strcmp("3text3", p));
	// end of string, final delim skipped
	EXPECT_EQ(nullptr, str_token(nullptr,delim_str4, &last));
}


#endif	// TESTING_UTILS
