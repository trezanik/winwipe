#ifndef COMPILER_H_INCLUDED
#define COMPILER_H_INCLUDED

/**
 * @file	compiler.h
 * @author	James Warren
 * @brief	Supported compiler macros, definitions and functionality
 */


// Included by build.h, no pre-requisites


#if defined(_MSC_VER) && defined(__GNUC__)
#	error "Multiple compiler definitions detected"
#endif



//#############
// GCC-specific
//#############

#if defined(__GNUC__)
#	define IS_GCC		1
#endif

#if IS_GCC

#	define GCC_IS_V2		(__GNUC__ == 2)
#	define GCC_IS_V3		(__GNUC__ == 3)
#	define GCC_IS_V4		(__GNUC__ == 4)
#	define GCC_IS_V2_OR_LATER	(__GNUC__ >= 2)
#	define GCC_IS_V3_OR_LATER	(__GNUC__ >= 3)
#	define GCC_IS_V4_OR_LATER	(__GNUC__ >= 4)

#	define GCC_VER_IS_OR_LATER_THAN(maj,min) \
		__GNUC__ > maj || \
			(__GNUC__ == maj && (__GNUC_MINOR__ > min) || \
				(__GNUC_MINOR__ == min))

#	define GCC_VER_IS_OR_LATER_THAN_PATCH(maj,min,patch) \
		__GNUC__ > maj || \
			(__GNUC__ == maj && (__GNUC_MINOR__ > min) || \
				(__GNUC_MINOR__ == min && __GNUC__PATCHLEVEL__ > patch))

#	if GCC_VER_IS_OR_LATER_THAN(4,6)
//#		pragma message "GCC version 4.6 or later detected; using nullptr"
#	else
#		error nullptr does not exist until GCC version 4.6.0 - update the compiler or provide a workaround
#	endif
#endif	// IS_GCC



//#######################
// Visual Studio-specific
//#######################

#if defined(_MSC_VER)
#	define IS_VISUAL_STUDIO	1
#endif

#if IS_VISUAL_STUDIO
#	define MS_VISUAL_CPP_VS8	1400	/**< Visual Studio 2005 */
#	define MS_VISUAL_CPP_VS9	1500	/**< Visual Studio 2008 */
#	define MS_VISUAL_CPP_VS10	1600	/**< Visual Studio 2010 */
#	define MS_VISUAL_CPP_VS11	1700	/**< Visual Studio 2012 */

#	define MSVC_IS_VS8		(_MSC_VER == MS_VISUAL_CPP_VS8)		/**< Is Visual Studio 2005 */
#	define MSVC_IS_VS9		(_MSC_VER == MS_VISUAL_CPP_VS9)		/**< Is Visual Studio 2008 */
#	define MSVC_IS_VS10		(_MSC_VER == MS_VISUAL_CPP_VS10)	/**< Is Visual Studio 2010 */
#	define MSVC_IS_VS11		(_MSC_VER == MS_VISUAL_CPP_VS11)	/**< Is Visual Studio 2012 */
#	define MSVC_IS_VS8_OR_LATER	(_MSC_VER >= MS_VISUAL_CPP_VS8)		/**< Is or later than Visual Studio 2005 */
#	define MSVC_IS_VS9_OR_LATER	(_MSC_VER >= MS_VISUAL_CPP_VS9)		/**< Is or later than Visual Studio 2008 */
#	define MSVC_IS_VS10_OR_LATER	(_MSC_VER >= MS_VISUAL_CPP_VS10)	/**< Is or later than Visual Studio 2010 */
#	define MSVC_IS_VS11_OR_LATER	(_MSC_VER >= MS_VISUAL_CPP_VS11)	/**< Is or later than Visual Studio 2012 */
#	define MSVC_BEFORE_VS8		(_MSC_VER < MS_VISUAL_CPP_VS8)		/**< Is Pre-Visual Studio 2005 */
#	define MSVC_BEFORE_VS9		(_MSC_VER < MS_VISUAL_CPP_VS9)		/**< Is Pre-Visual Studio 2008 */
#	define MSVC_BEFORE_VS10		(_MSC_VER < MS_VISUAL_CPP_VS10)		/**< Is Pre-Visual Studio 2010 */
#	define MSVC_BEFORE_VS11		(_MSC_VER < MS_VISUAL_CPP_VS11)		/**< Is Pre-Visual Studio 2012 */
#	define MSVC_LATER_THAN_VS8	(_MSC_VER > MS_VISUAL_CPP_VS8)		/**< Is Post-Visual Studio 2005 */
#	define MSVC_LATER_THAN_VS9	(_MSC_VER > MS_VISUAL_CPP_VS9)		/**< Is Post-Visual Studio 2008 */
#	define MSVC_LATER_THAN_VS10	(_MSC_VER > MS_VISUAL_CPP_VS10)		/**< Is Post-Visual Studio 2010 */
#	define MSVC_LATER_THAN_VS11	(_MSC_VER > MS_VISUAL_CPP_VS11)		/**< Is Post-Visual Studio 2012 */


#	if MSVC_BEFORE_VS10
		/* VS2008 and earlier do not have nullptr. Be warned that many
		 * features we're starting to use are not supported before now.. */
#		define nullptr	NULL
		/* We also add the Windows versions that did not exist at the
		 * time VS2008 was released (see sdkddkver.h) */
#		define _WIN32_WINNT_WIN6	0x0600
#		define _WIN32_WINNT_VISTA	0x0600
#		define _WIN32_WINNT_W08		0x0600
#		define _WIN32_WINNT_LONGHORN	0x0600
#		define _WIN32_WINNT_WIN7	0x0601
#		define _WIN32_WINNT_WIN8	0x0602
#		define _WIN32_WINNT_WINBLUE	0x0603
#	endif
#endif	// IS_VISUAL_STUDIO





/* Debug-build detection; we try our best using the common idioms for using a
 * debug/release build, and use that.
 *
 * Internally, we define DEBUG_BUILD to 1 if in debug mode, otherwise, no
 * definition is provided. This allows #if DEBUG_BUILD or #ifdef DEBUG_BUILD to
 * work, and easier identification of the setup; you're in release mode if you
 * cannot find this.
 */
#if defined(IS_DEBUG_BUILD)
	// our definition; overrides all others
#	warning "Using IS_DEBUG_BUILD override; it is safer to define/undefine NDEBUG as appropriate"
#elif defined(NDEBUG) || defined(_NDEBUG)
	// something saying we're in NON-DEBUG mode; check for conflicts
#	if defined(_DEBUG) || defined(DEBUG) || defined(__DEBUG__)
#		error "Conflicting definitions for Debug/Release mode"
#	endif
	// no conflict - we're in release mode.
//#	pragma message "Building in Release mode."
#else
	// release mode not specified - check if DEBUG mode specified 
#	if defined(_DEBUG) || defined(DEBUG) || defined(__DEBUG__)
		// yup; debug mode is definitely desired.
#		define IS_DEBUG_BUILD	1
//#		pragma message "Building in Debug mode."
#	else
		/* debug mode also not specified; do it, but warn the user */
#		warning "No Debug/Release definition found; assuming Release"
//#		pragma message "Building in Release mode."
#	endif
#endif	// DEBUG_BUILD


#endif	// COMPILER_H_INCLUDED
