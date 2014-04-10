#ifndef BUILD_H_INCLUDED
#define BUILD_H_INCLUDED

/**
 * @file	build.h
 * @author	James Warren
 * @brief	Mandatory build-time requirements & settings for the project
 */


/* Note: build.h should (or will) always be included _first_ amongst one of the
 * source files, either directly or indirectly. This enables customization of
 * the application from one file - at the cost of a total recompile on a change
 * (which may be desirable in some instances), and remembering to not put stuff
 * in here unless it _really_ is needed!
 *
 * USING_ = Enabling some functionality, using source/methods
 * IS_ = Marks 'truth' in the statement
 * DISABLE_ = Disables functionality
 *
 * The src/c99 directory contains files needed to support C99 in Visual Studio
 * 2010.
 */


/*
 * [Application Inclusions]
 * Header files required by every source file (or needed on the sly). Use very
 * sparingly; changes to these will force a total rebuild on compilation. These
 * headers also know about pre-requisites, so will not include them themselves.
 */

/* Compiler detection, functionality and overrides. Provides the ability to use
 * preprocessors such as 'IS_VISUAL_STUDIO', 'IS_GCC' and 'IS_DEBUG_BUILD' */
#include "compiler.h"

// our data typedefs and definitions; int32_t, char, etc.
#include "types.h"



/*
 * [Application Options]
 * uncomment to enable, comment to disable
 */


/** Disables warnings when they are handled internally - use with care, AFTER checking they are handled correctly;
 * best example is list.h -> disabling conditional expression is constant; the do {} while(0) loop is intentional,
 * and therefore we can ignore it, assuming nothing else causes a match elsewhere in source. */
#define DISABLE_IGNORABLE_WARNINGS

/** Use the ntdll functionality for some calls for accuracy/performance/features */
#define USING_NTDLL_FUNCTIONS

/** 'Optimize' Windows */
#define WIN32_LEAN_AND_MEAN

// /** Bind to the current version of the CRT */
//#define _BIND_TO_CURRENT_CRT_VERSION	1

// for targeted builds - no difference behind just doing _WIN32_WINNT direct.
// Windows 2000		_WIN32_WINNT_WIN2K
// Windows XP		_WIN32_WINNT_WINXP
// Windows Vista	_WIN32_WINNT_VISTA
// Windows 7		_WIN32_WINNT_WIN7
// Windows 8		_WIN32_WINNT_WIN8
// Windows Server 2003	_WIN32_WINNT_WS03
// Windows Server 2008	_WIN32_WINNT_WS08
/** Specifies the minimum version of Windows our application targets to run on.
 * Unsupported APIs will not be used if not available. This can be commented
 * out to free the way for _WIN32_WINNT to be manually/automatically defined. */
#define MINIMUM_TARGET		_WIN32_WINNT_WINXP

#if defined(MINIMUM_TARGET)
#	undef _WIN32_WINNT
#	define _WIN32_WINNT	MINIMUM_TARGET
#endif


/*
 * [Application Defaults]
 * Do not remove; modify the value or Find&Replace instead
 */


/* /** @todo Implement unit tests for this to work with 
 * Enable if you want to run unit tests; otherwise, for a normal application 
 * run, leave this commented out. */
//#define TESTING				1

/** Text buffer size for basic or system operations */
#define MAX_LEN_GENERIC			250

/** Buffer sizes when formatting an error string */
#define ERR_FORMAT_BUFFER_SIZE		2048

/** The string format used for time functions, like localtime */
#define DEFAULT_OUTPUT_TIME_FORMAT	"%X"



/*
 * [Handy Macros/Definitions]
 * Do not remove; should never need modification
 */

/** Prevents the class from being assigned */
#define NO_CLASS_ASSIGNMENT(class_name)		private: class_name operator = (class_name const&)

/** Prevents the class from being copied */
#define NO_CLASS_COPY(class_name)		private: class_name(class_name const&)



/*
 * [Compiler Macros/Preprocessor directives]
 * Do not remove; may require modification on project completion, or to
 * correctly support compiler or linking functionality.
 */





#endif	// BUILD_H_INCLUDED
