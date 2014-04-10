#ifndef LOG_H_INCLUDED
#define LOG_H_INCLUDED

/**
 * @file	Log.h
 * @author	James Warren
 * @brief	Log file writing for the application
 */


// required inclusions
#include "build.h"
#include <sstream>
#include <fstream>
#include "Runtime.h"		// our class exists through Runtime





enum E_LOG_LEVEL
{
	LL_None = 0,	/**< no logging at all */
	LL_Error,	/**< logging errors */
	LL_Warn,	/**< logging warnings + above */
	LL_Info,	/**< logging info + above */
	LL_Debug,	/**< logging debug (all) */
	LL_Force	/**< logging forced; statement WILL be logged */
};



/**
 * @todo consider using a ChainOfResponsibility style for this; will enable us
 * to have a single LOG() line of code, with all errors always being output to
 * cerr, but only certain things going to a physical file. The complications
 * will be multi-line and immediate or delayed flush...
 *
 * @class Log
 */
class Log
{
	// only the runtime is allowed to construct us
	friend class Runtime;
private:
	NO_CLASS_ASSIGNMENT(Log);
	NO_CLASS_COPY(Log);

	// private constructor; we want one instance that is controlled.
	Log()
	{
		// operations append to the stream, and don't replace it
		//_next_log.setstate(std::ios_base::app);
		// logging at debug level by default 
		_log_level = LL_Warn;
	}

	~Log()
	{
		// if it hasn't already been closed, do it
		if ( _file != nullptr )
			Close();
	}

	/** This holds all of the data written into the stream inbetween calls
	 * to the @a flush function, which actually writes out to file */
	std::stringstream	_next_log;

	/** A stream to hold logging events that don't reach the required level
	 * to be put to the normal stream. Nasty hack, as all the data is still
	 * written to the stream, where it then gets wiped straight away. */
	std::stringstream	_null_log;

	/** we need error handling beyond that of 'an error occurred' so we need
	 * to call into the platforms APIs to retain control; hence the usage of
	 * the C-style FILE and not a std::ofstream */
	FILE*			_file;

	/** The active logging level; log requests with a lower rating will not
	 * log those with higher levels unless set here. */
	E_LOG_LEVEL		_log_level;

public:

	/**
	 * Used for inserting data when desiring a custom layout going to the
	 * log file. This bypasses any LogLevel, file/line info, and accesses
	 * the stream directly.
	 *
	 * It's usage should be limited to formatting within the file, such as
	 * printing out a list of memory addresses with text - having a file +
	 * line for each at the start would be detrimental to legibility.
	 *
	 * Use LogWithLevel(...) to get a similar result, and should be
	 * preferred where possible.
	 *
	 * @param append_string The string to append to the stream
	 */
	void
	Append(
		const std::string& append_string
	);


	/**
	 * Closes the file previously opened by Open(), if any. Will Flush()
	 * before the file is closed, with a notification suffix line.
	 *
	 * Will cause _file to be a nullptr after execution.
	 */
	void
	Close();


	/**
	 * Forces the current contents of the logging stream to be written to
	 * file, if it's open.
	 *
	 * If the file is not open, the stream is reset, just like if there was
	 * data to write.
	 */
	void
	Flush();


	/**
	 * Writes to the logging stream, preparing for the input from a runtime
	 * file. Returns the actual stream, so game code can perform logging in
	 * a single, non-descriptive line:
	 @code
	 LOG(LL_Warn) << "This operation should have succeeded: " << info_detail;
	 @endcode
	 *
	 * Newlines are not automatically added (since the caller has the stream
	 * on return), so must be provided.
	 *
	 * If the logging level would not write the supplied data, it is
	 * redirected to a unused stream.
	 *
	 * While all other parameters are optional, if you supply a filename,
	 * then the function + line are mandatory to be supplied. This is why,
	 * amongst clarity, the LOG() macro should be used for all output,
	 * unless you need fine grained control.
	 *
	 * @param[in] log_level The criticality level at which the incoming
	 * entry is deemed
	 * @param[in] file Optional pointer to the filename this log entry will
	 * contain
	 * @param[in] function Optional pointer to the function this log entry
	 * will contain
	 * @param[in] line Optional line number of the filename this log entry
	 * will contain
	 * @return A reference to the logging stream
	 */
	std::stringstream&
	LogWithLevel(
		uint8_t log_level,
		const char* file = nullptr,
		const char* function = nullptr,
		const uint32_t line = 0
	);


	/**
	 * Retrieves the logging level currently set.
	 *
	 * @return The logging level, as an E_LOG_LEVEL, currently set
	 */
	E_LOG_LEVEL
	LogLevel()
	{ return _log_level; }


	/**
	 * Opens the file name specified for writing, erasing any prior data in
	 * the log file. This is opened in such a way that other processes may
	 * not write anything to the file, but are free to read it.
	 *
	 * This is useful for checking for an event at runtime, when we don't
	 * want to have to close the application to check, while still stopping
	 * another process from being able to wipe our data out.
	 *
	 * @param[in] filename The name of the file to open
	 * @return true if the file is opened with a restricted share mode
	 * @return false if the file could not be opened with the desired share mode
	 */
	bool
	Open(
		char* filename
	);


	/**
	 * Changes what level of events will be logged, and those that will be
	 * sent to /dev/null.
	 *
	 * @param[in] loglevel The new log level to use
	 */
	void
	SetLogLevel(
		E_LOG_LEVEL loglevel
	)
	{ _log_level = loglevel; }
};



// quick access definition
#define LOG(LogLevel)	runtime.Logger()->LogWithLevel(LogLevel, __FILE__, __FUNCTION__, __LINE__)



#endif	// LOG_H_INCLUDED
