#ifndef UTILS_H_INCLUDED
#define UTILS_H_INCLUDED

/**
 * @file	utils.h
 * @author	James Warren
 * @brief	Utility functions, mostly secure string operations
 */


// required inclusions
#include "build.h"
#include <vector>

// standards-compliance definitions
#define strcasecmp	_stricmp



/** Holds version information for a file; to be used with binaries */
struct file_version_info
{
	/*Comments	InternalName	ProductName
	CompanyName	LegalCopyright	ProductVersion
	FileDescription	LegalTrademarks	PrivateBuild
	FileVersion	OriginalFilename	SpecialBuild*/

	// Module version (e.g. 6.1.7201.17932)
	uint16_t	major;
	uint16_t	minor;
	uint16_t	revision;
	uint16_t	build;
	// File description
	wchar_t		description[1024];
};

/** Holds information about a module (DLL) */
struct ModuleInformation
{
	wchar_t			name[512];	/**< Module name (full path) */
	file_version_info	fvi;
};





/**
 * Allocates a buffer large enough to hold src at the address specified by dst,
 * and copies it in.
 *
 * *dst MUST be a nullptr, otherwise this function returns false - this is to
 * protect against memory leaks, where the pointer could be pointing to memory
 * allocated dynamically, and should be freed - or if it's only on the stack.
 *
 * @param[in,out] dst Where the allocated string will be saved to
 * @param[in] src The source string to be copied
 * @retval true if the allocation succeeds
 * @retval false on an invalid parameter, or malloc() failure
 */
bool
alloc_and_copy(
	char** dst,
	char* src
);



/**
 * Does the same as alloc_and_copy(), only it will call free() on the address
 * of the destination before performing the copy. Will check if it's a nullptr
 * first, and return false if it is.
 *
 * Will naturally be slower than simply reusing the buffer, but it depends on
 * if you know the buffer size, or just the plain usage scenario. Should be
 * used rarely (C++ allocations are to be faster here).
 *
 * @param[in,out] dst The destination (and existing) allocation
 * @param[in] src The source string to be copied
 * @retval true if the reallocation succeeds
 * @retval false on an invalid parameter, or malloc() failure
 */
bool
alloc_and_replace(
	char** dst,
	char* src
);



/**
 * Triggers a divide by zero exception - used for forcing another process to be
 * killed.
 *
 * Executed as part of a created thread, which is why it needs a void * as a
 * parameter, and returns unsigned.
 * 
 * @param[in] null A placeholder (just pass in nullptr) - not touched by the function
 * @return Returns 0, even if it never gets reached
 */
unsigned __stdcall
divide_by_zero(
	void* null
);



/**
 * Converts a Windows error code to its string counterpart, as processed by the
 * system.
 *
 * The pointer returned must not be freed - the function maintains a static
 * buffer, and the pointer is looking at it when it is returned. This limits an
 * error message to 511 characters, which should be enough for anything.
 *
 * @param[in] code The Win32 error code (sometimes a long, sometimes unsigned
 * long, but all should be good for the system).
 * @return A pointer to the start of the error string, or a nullptr if the
 * FormatMessage() function failed.
 */
char*
error_code_as_string(
	uint64_t code
);



/**
 * Calls ExitProcess() - used for forcing another process to be killed.
 *
 * Executed as part of a created thread, which is why it needs a void * as a
 * parameter, and returns unsigned.
 *
 * @param[in] null A placeholder (just pass in nullptr) - not touched by the function
 * @return Returns 0, even if it never gets reached
 */
unsigned __stdcall
exit_process(
	void* null
);



/**
 * Retrieves the current path for the executing binary, storing the result in
 * the supplied @a buffer. Comes with the trailing [back]slash.
 *
 * @param[out] buffer The preallocated buffer to store the result in
 * @param[in] buffer_size The size of the preallocated buffer
 * @return Returns the amount of characters written to buffer if successful
 * @return On failure, 0 is returned
 */
uint32_t
get_current_binary_path(
	char* buffer,
	uint32_t buffer_size
);



/**
 * Obtains the version information for the specified file.
 *
 * A file_version_info struct is passed in, which will contain the results of
 * the data acquisition. It is immediately reset to 0 or blank values, in the
 * event of failure.
 * ntdll on Win7 SP1 x64 would be:
 * - major = 6
 * - minor = 1
 * - rev = 7601
 * - build = 18229
 *
 * @retval true if the acquisition succeeds, and the struct populated
 * @retval false if the acquisition fails
 */
bool
get_file_version_info(
	wchar_t* path,
	file_version_info* fvi
);



/**
 * Obtains the memory address of func_name, which resides in the module_name
 * library. Essentially a wrapper around GetProcAddress(), which loads the
 * module for you.
 *
 * @param[in] func_name The name of the function to get
 * @param[in] module_name The name of the module to search within for func_name
 * @retval nullptr on failure, as a result of an invalid parameter, or if the
 * function was not found.
 * @return On success, a pointer to the requested functions memory address is 
 * returned.
 */
void*
get_function_address(
	char* func_name,
	wchar_t* module_name
);



/**
 * Enumerates all the loaded modules in the running process, and logs them to
 * file. Does not check if the config has logging enabled; the caller should do
 * so.
 *
 * @warning
 * Each ModuleInformation* returned needs its memory freeing when it is no
 * longer needed; failure to do so will result in a memory leak.
 *
 * @return A vector of structs containing information, such as the name and
 * address, of each loaded module.
 */
std::vector<ModuleInformation*>
get_loaded_modules();



/**
 * Gets the current time in milliseconds.
 *
 * Used for timing operations - store the first call to this as a start_time,
 * and then use the second call to determine the duration.
 *
 * @return Returns the number of milliseconds since January 1, 1601 (UTC).
 */
uint64_t
get_ms_time();



/**
 * Obtains the version information for a file.
 *
 * Example: 
 *
 * @param[in] path The path, relative or full, to the file/folder
 * @param[out] dest_buffer The buffer to copy the information to
 * @param[in] buffer_size The site of the destination buffer
 * @return The amount of characters written to dest_buffer is returned
 */
uint32_t
get_win_file_version_info(
	wchar_t* path,
	wchar_t* dest_buffer,
	uint32_t buffer_size
);



/**
 * Take heed of the security implications; see: http://msdn.microsoft.com/en-us/library/dd319072(v=vs.85).aspx
 *
 * @param[out] buf The destination buffer
 * @param[in] mb The multi-byte string to convert
 * @param[in] buf_size The size of the buffer pointed to by buf, in characters
 * @retval false if the pointers are null, buf_size is not at least 2, or if
 * MultiByteToWideChar() fails
 * @retval true if the conversion succeeds
 */
bool
MbToUTF8(
	wchar_t* buf,
	const char* mb,
	uint32_t buf_size
);



/**
 * Take heed of the security implications; see: http://msdn.microsoft.com/en-us/library/dd374130(v=vs.85).aspx
 *
 * @param[out] buf The destination buffer
 * @param[in] wchar The wide-character string to convert
 * @param[in] buf_size The size of the buffer pointed to by buf
 * @retval false if the pointers are null, buf_size is not at least 2, or if
 * MultiByteToWideChar() fails
 * @retval true if the conversion succeeds
 */
bool
UTF8ToMb(
	char* buf,
	const wchar_t* wchar,
	uint32_t buf_size
);



/**
 * 
 */
bool
process_exists(
	unsigned long pid
);


/**
 * Skips over all preceeding whitespace present in @a str.
 *
 * Has Test Case: no
 *
 * @param str The source string to skip over
 * @return Returns a pointer to the incremented source string
 */
char*
skip_whitespace(
	char* str
);



/**
 * A duplicate of OpenBSD's strlcat.
 *
 * Appends @a src into the buffer specified by @a dest, up to a limit of @a size
 * - 1. Nul termination is guaranteed if @a size is at least 1.
 *
 * Unlike the OpenBSD version of strlcat, if no nul is found in @a dest, it is 
 * inserted at the final position. This is a change for tiny safety sake.
 *
 * Has Test Case: yes
 * 
 * @param dest The destination buffer
 * @param src The string to append
 * @param size The size of the destination buffer
 * @return The length of the string that was attempted to be created. So, strlen
 * of @a dest + strlen of @a src. If this is greater than or equal to @a size,
 * truncation has occurred, and should be handled by the caller.
 */
uint32_t
strlcat(
	char* dest,
	const char* src,
	uint32_t size
);



/**
 * A duplicate of OpenBSD's strlcpy.
 *
 * Copies @a src into the buffer specified by @a dest, up to a limit of @a size
 * - 1. Always starts copying @a src and overwrites anything previously there.
 * nul termination is guaranteed if @a size is at least 1.
 *
 * Has Test Case: yes
 * 
 * @param dest The destination buffer
 * @param src The string to copy
 * @param size The size of the destination buffer
 * @return The length of the string that was attempted to be created. So, strlen
 * of @a src. If this is greater than or equal to @a size, truncation has 
 * occurred, and should be handled by the caller.
 */
uint32_t
strlcpy(
	char* dest,
	const char* src,
	uint32_t size
);



/**
 * Formats a string into the buffer specified by @a dest.
 *
 * This function is identical to snprintf, only there is no need to concern with
 * the buffer size. Nul-termination is guaranteed if @a dest_size is at least 1.
 * Improper use of format strings can still result in security risks, so always
 * use as much safety as you would to normal statements.
 *
 * char		buf[24];
 * int32_t	num = 5;
 * str_format("The integer is: %i; amazing stuff!\\n", num);
 *
 * In this case, the string is truncated to "The integer is: 5; amaz".
 *
 * @param destination The destination buffer
 * @param dest_size The size of the destination buffer
 * @param format The format of the string to generate, printf-style
 * @param ... va_args as determined by the format string.
 * @return Returns the number of characters written to @a destination, excluding
 * the nul. If the buffer is not large enough, and truncation has occurred, the
 * return value is 0 - but @a destination is populated with as much as it can.
 */
uint32_t
str_format(
	char* destination,
	uint32_t dest_size,
	char* format,
	...
);



/**
 * A reentrant version of strtok; return values and functionality is identical
 * to that of strtok_r (as it does not exist on Windows).
 * The @a input will be modified, so store a copy if you wish to retain any
 * existing data it holds.
 *
 * Has Test Case: yes
 *
 * @param input The input string to parse; should be NULL after the initial
 * call until all tokens are obtained
 * @param delim The set of characters that delimit the tokens in input; i.e.
 * "\\r\\n" will tokenize every instance of '\\r', then '\\n', etc.
 * @param last Maintains context - must point to a nullptr in the first call
 * @return Returns a pointer to the next token, or nullptr if there are no more.
 */
char*
str_token(
	char* input,
	const char* delim,
	char** last
);



/**
 * Removes any trailing and preceeding whitespace from @a src. A nul terminator 
 * is inserted at the final whitespace character when working in reverse.
 *
 * " This string has a tab and space at the end\t "
 * will be trimmed down to:
 * "This string has a tab and space at the end"
 *
 * Has Test Case: no
 *
 * @param src The string to be trimmed
 * @return Returns src at a non-leading whitespace offset
 */
char*
str_trim(
	char* src
);



/**
 * 
 */
bool
time_diff_as_string(
	time_t start_time,
	time_t end_time,
	char* dest,
	uint32_t dest_size
);


#endif	// UTILS_H_INCLUDED
