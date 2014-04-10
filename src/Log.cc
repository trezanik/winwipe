
/**
 * @file	Log.cc
 * @author	James Warren
 * @copyright	James Warren, 2013-2014
 * @license	Zlib (see license.txt or http://opensource.org/licenses/Zlib)
 */


#include "build.h"		// always first

#include <iostream>		// std::cout in IS_DEBUG_BUILD
#include <ctime>		// time + date acquistion
#include <cassert>		// debug assertions

#include "Log.h"		// prototypes
#include "utils.h"		// string formatting for reporting



/** number of bytes the stream can contain before it will be flushed */
#define STREAM_FLUSH_THRESHOLD		1024



void
Log::Append(
	const std::string& append_string
)
{
	_next_log << append_string;

	if ( _next_log.gcount() > STREAM_FLUSH_THRESHOLD )
		Flush();
}



void
Log::Close()
{
	if ( _file != nullptr )
	{
		// don't print file/line info, call direct
		LogWithLevel(LL_Force) << "*** Log file closed ***\n";
		Flush();

		fclose(_file);
		_file = nullptr;
	}
}



void
Log::Flush()
{
	if ( _file == nullptr )
	{
		// don't keep the data
		_next_log.str("");
		return;
	}

	std::string	writing = _next_log.str();
	size_t		written;
	int32_t		flush_res;

	if ( writing.empty() )
		return;

	written = fwrite(writing.c_str(), sizeof(char), writing.length(), _file);
	
	if (( flush_res = fflush(_file)) != 0 )
	{
		char	errmsg[256];

		strerror_s(errmsg, sizeof(errmsg), ferror(_file));
		std::cout << "fflush failed; " << errmsg << "\n";
	}

	_next_log.str("");
}



std::stringstream&
Log::LogWithLevel(
	uint8_t log_level,
	const char* file,
	const char* function,
	const uint32_t line
)
{
	if ( log_level != LL_Force && log_level > _log_level )
	{
		/* ugly, ugly, nasty hack. Returns another stringstream that is
		 * used temporarily, and cleared at every opportunity. */
		_null_log.clear();
		return _null_log;
	}

	// force log flushing if too much data is buffered
	if ( _next_log.gcount() > STREAM_FLUSH_THRESHOLD )
		Flush();

	if ( file != nullptr )
	{
#if _WIN32
		const char	path_sep = '\\';
#else
		const char	path_sep = '/';
#endif
		const char*	p;
		char		cur_datetime[32];
		time_t		cur_time = time(NULL);
		uint32_t	len;

#if IS_VISUAL_STUDIO	// damn crt security; not an issue here
		ctime_s(cur_datetime, sizeof(cur_datetime), &cur_time);
		len = strlen(cur_datetime);
#else
		len = strlcpy(cur_datetime, ctime(&cur_time), sizeof(cur_datetime));
#endif
		// remove the linefeed ctime inserts 'helpfully'...
		assert(cur_datetime[len-1] == '\n');
		cur_datetime[len-1] = '\0';

		_next_log << cur_datetime << "\t";

		switch ( log_level )
		{
		case LL_Debug:	_next_log << "[DEBUG]    "; break;
		case LL_Error:	_next_log << "[ERROR]    "; break;
		case LL_Warn:	_next_log << "[WARNING]  "; break;
		case LL_Info:	_next_log << "[INFO]     "; break;
		case LL_Force:	_next_log << "[FORCED]   "; break;
		default:
			// no custom text by default
			break;
		}

		// we don't want the full path that some compilers set
		if (( p = strrchr(file, path_sep)) != nullptr )
			file = (p + 1);

		_next_log << function << " (" << file << ":" << line << "): ";
	}

	return _next_log;
}



bool
Log::Open(
	char* filename
)
{
	/* allow others to open the log file at runtime, but not for writing.
	 * This is implementation and operating system dependant as to what
	 * action is taken, if errors are forwarded (e.g. cpp streams setting
	 * errno, and win32 being able to use GetLastError()), and what errors
	 * we can actually retrieve (beyond just knowing an error occurred).
	 * Until there is a standard for doing this, back to the nice C-style
	 * methods here */
#if defined(_WIN32)
	if (( _file = _fsopen(filename, "w", _SH_DENYWR)) == nullptr )
	{
		std::cout << "Failed to open log file:\n\n" << filename << "\n\nerrno = " << errno << "\n";
		return false;
	}
#else
	int	fd;
	if (( fd = open(LOG_NAME, O_WRONLY|O_CREAT, O_NOATIME|S_IRWXU|S_IRGRP|S_IROTH)) == -1 )
	{
		std::cerr << fg_red << "Could not open the log file " << LOG_NAME << "; errno " << errno << "\n";
		return false;
	}

	if (( _file = fdopen(fd, "w")) == nullptr )
	{
		std::cerr << fg_red << "fdopen failed on the file descriptor for " << LOG_NAME << "; errno " << errno << "\n";
		return false;
	}
#endif

	// Default log message (verifies path used); don't print file/line info, call direct
	LogWithLevel(LL_Force) << "*** Log File '" << filename << "' opened ***\n";

	return true;
}
