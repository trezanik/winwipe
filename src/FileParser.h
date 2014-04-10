#ifndef FILEPARSER_H_INCLUDED
#define FILEPARSER_H_INCLUDED

/**
 * @file	FileParser.h
 * @author	James Warren
 * @brief	File parsing for generating wipe operations
 * @copyright	zlib; license.txt or http://www.gzip.org/zlib/zlib_license.html
 */


// required inclusions
#include "build.h"		// always first
#include <cstdio>		// FILE




/**
 * The type of input file to parse. Files have a different input method than
 * the registry, processes, services, and so forth. One physical file is used
 * to store the relevant type, and they are parsed separately.
 *
 * @enum E_FILE_TYPE
 */
enum E_FILE_TYPE
{
	FT_Files = 0,
	FT_Registry,
	FT_Processes,
	FT_Services,
	FT_Uninstallers,
	FT_Commands
};


/**
 * Used for storing a bit of text in a cache, statically used in a function.
 * POD.
 */
struct text_cache
{
	wchar_t		buffer[MAX_LEN_GENERIC];
};



/**
 * All the normal Windows environment variables are also supported automatically (such as \%TMP%, \%PUBLIC%, \%LOGONSERVER%, etc.).
 * In addition to these, we also support the following variables within files:
 * <TABLE>
 *   <TR><TD><B>Variable</B></TD><TD><B>Description</B></TD><TD><B>Example</B></TD></TR>
 *   <TR>
 *     <TD>\%SID%</TD><TD>The current users SID (executing the binary)</TD><TD>S-1-5-21-1336347285-13732138-2673565989-1002</TD>
 *   </TR>
 *   <TR>
 *     <TD>\%ALLUSERS%</TD><TD>All user folders (i.e. not the 'All Users' folder, but for every actual user profile); excludes the system ones - 'DefaultUser', 'NetworkService', 'LocalService'...</TD><TD>C:\\Users\\ProfileName <BR> C:\\Users\\OtherProfileName</TD>
 *   </TR>
 *   <TR>
 *     <TD>\%USERAPPDATA%</TD><TD>The path to a users Application Data folder</TD><TD>AppData\\Roaming (Windows Vista+) <BR> Application Data (Windows 2000, XP)</TD>
 *   </TR>
 *   <TR>
 *     <TD> </TD><TD> </TD><TD> </TD>
 *   </TR>
 * </TABLE>
 *
 * Remember that all UI output here is done without Operations (we're in the
 * process of populating them) - this means the output is done manually, using
 * the same format. Errors are therefore output like:
 @code
 "failed\n\t<message>\n"
 @endcode
 * And success is simply:
 @code
 "ok!\n"
 @endcode
 *
 * @class FileParser
 */
class FileParser
{
	// only the runtime is allowed to construct us
	friend class Runtime;
private:
	NO_CLASS_ASSIGNMENT(FileParser);
	NO_CLASS_COPY(FileParser);

	/** conversion local to the class; operations are prepared using char*
	 * strings (since we're not a totally Windows-centric build, most of
	 * the sources have come from cross-platform projects) */
	char			mb[1024];

	/** Handle to the current user token needed when expanding environment
	 * strings for the current user.
	 * Maintaining it for the class and not the function means we need to
	 * worry less about tracking whether or not we've released it.
	 * NULL and acquired in the constructor, release in the destructor if 
	 * needed. */
	HANDLE			_token;

	/** Like the _token, this is an acquire-once-and-forget; holds the
	 * current users profile directory, for use in variable expansion */
	wchar_t			_user_path[MAX_PATH];

	
	/**
	 * Parses the input file stream and extracts commands to execute. This
	 * is used for pre + post installer commands, as well as the actual
	 * installers to run too. The caller will be executing these at the
	 * appropriate time.
	 *
	 * Errors can be output on the fly, as and when needed, as there is no
	 * associated Operation. A custom failure message is required, if the
	 * parsing fails (it will silently jump straight to the application end
	 * error otherwise).
	 * 
	 * @param[in] fstream A FILE stream acquired from ParseInputFile()
	 * @return true if all file contents were parsed and their created 
	 * operations queued
	 * @return false if a syntax error is found, or the attempt to queue
	 * a new operation for an item fails
	 */
	bool
	ParseCommands(
		FILE* fstream
	);


	/**
	 * Parses the input file stream and extracts file + folder paths.
	 *
	 * Errors can be output on the fly, as and when needed, as there is no
	 * associated Operation. A custom failure message is required, if the
	 * parsing fails (it will silently jump straight to the application end
	 * error otherwise).
	 * 
	 * @param[in] fstream A FILE stream acquired from ParseInputFile()
	 * @return true if all file contents were parsed and their created 
	 * operations queued
	 * @return false if a syntax error is found, or the attempt to queue
	 * a new operation for an item fails
	 */
	bool
	ParseFileFolders(
		FILE* fstream
	);


	/**
	 * Parses the input file stream and extracts process names.
	 *
	 * Errors can be output on the fly, as and when needed, as there is no
	 * associated Operation. A custom failure message is required, if the
	 * parsing fails (it will silently jump straight to the application end
	 * error otherwise).
	 * 
	 * @param[in] fstream A FILE stream acquired from ParseInputFile()
	 * @return true if all file contents were parsed and their created 
	 * operations queued
	 * @return false if a syntax error is found, or the attempt to queue
	 * a new operation for an item fails
	 */
	bool
	ParseProcesses(
		FILE* fstream
	);


	/**
	 * Parses the input file stream and extracts registry keys + values.
	 *
	 * Errors can be output on the fly, as and when needed, as there is no
	 * associated Operation. A custom failure message is required, if the
	 * parsing fails (it will silently jump straight to the application end
	 * error otherwise).
	 * 
	 * @param[in] fstream A FILE stream acquired from ParseInputFile()
	 * @return true if all file contents were parsed and their created 
	 * operations queued
	 * @return false if a syntax error is found, or the attempt to queue
	 * a new operation for an item fails
	 */
	bool
	ParseRegistry(
		FILE* fstream
	);


	/**
	 * Parses the input file stream and extracts service names.
	 *
	 * Errors can be output on the fly, as and when needed, as there is no
	 * associated Operation. A custom failure message is required, if the
	 * parsing fails (it will silently jump straight to the application end
	 * error otherwise).
	 * 
	 * @param[in] fstream A FILE stream acquired from ParseInputFile()
	 * @return true if all file contents were parsed and their created 
	 * operations queued
	 * @return false if a syntax error is found, or the attempt to queue
	 * a new operation for an item fails
	 */
	bool
	ParseServices(
		FILE* fstream
	);


	/**
	 * Parses the input file stream and extracts uninstallers to execute.
	 *
	 * Errors can be output on the fly, as and when needed, as there is no
	 * associated Operation. A custom failure message is required, if the
	 * parsing fails (it will silently jump straight to the application end
	 * error otherwise).
	 * 
	 * @param[in] fstream A FILE stream acquired from ParseInputFile()
	 * @return true if all file contents were parsed and their created 
	 * operations queued
	 * @return false if a syntax error is found, or the attempt to queue
	 * a new operation for an item fails
	 */
	bool
	ParseUninstallers(
		FILE* fstream
	);


	/**
	 * Reads the next line from the file stream.
	 *
	 * Is simply a wrapper around fgetws.
	 */
	wchar_t*
	ReadLine(
		FILE* fstream,
		wchar_t* buffer,
		uint32_t buffer_size
	);


	/**
	 * Fully expands the source buffer, using both our internal, and the
	 * system environment variables.
	 *
	 * Used for ParseFileFolders() - not designed for the others.
	 *
	 * Operations will be generated on the fly when no additional custom
	 * expansion has been performed (so nothing left to process).
	 *
	 * @param[in] source A nul-terminated string to expand
	 * @param[in] dest The destination buffer to put the stored result in
	 * @param[in] dest_size The size of the destination buffer
	 * @param[in] line The line of the file; used for error reporting
	 * @retval true If expansion fully succeeds
	 * @retval false If any function fails; the dest buffer will be in an
	 * indeterminate state, depending upon the point of failure.
	 */
	bool
	RecursiveExpand(
		const wchar_t* const source,
		wchar_t* dest,
		const uint32_t dest_size,
		const uint32_t line
	);


	/**
	 * str_trim, using wchar_t. Only needed for this class, so not in utils
	 * (which was written for ansi/linux sources). We also want to keep
	 * trailing spaces, which makes this totally different - this only
	 * strips newlines.
	 *
	 * @param[in] str The string to trim whitespace (but not spaces)
	 * @return Returns the original pointer with the inserted nuls, if any
	 * newlines are found trailing.
	 */
	wchar_t*
	Trim(
		wchar_t* str
	);



	// one controlled instance of this class; created in Runtime
	FileParser();

public:
	~FileParser();

	/**
	 * Calls the handler for the filetype provided, ready to generate 
	 * operations for each line of content in the input file.
	 *
	 * @param[in] filetype The type of content within the input file to parse
	 * @param[in] filepath The relative or full path to the input file
	 * @return true if the file is parsed successfully (or has no content)
	 * @return false if the filetype is invalid, the filepath is not found,
	 * or a syntax error prevents the file from being fully parsed.
	 */
	bool
	ParseInputFile(
		E_FILE_TYPE filetype,
		const char* filepath
	);
};



#endif	// FILEPARSER_H_INCLUDED
