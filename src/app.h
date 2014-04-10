#ifndef APP_H_INCLUDED
#define APP_H_INCLUDED

/**
 * @file	app.h
 * @author	James Warren
 * @brief	Core definitions and functions for the application
 */


// required inclusions
#include "build.h"			// always first


// required definitions (Win32 Graphical)
#define WM_WINWIPE_APPINIT	(WM_USER + 1)
#define ID_OUTPUT_WINDOW	4242


#define OUTCOLOR_BACKGROUND	RGB(  0,  0,  0)	// Black
#define OUTCOLOR_NORMAL		RGB(255,255,255)	// White
#define OUTCOLOR_ERROR		RGB(255,  0,  0)	// Red
#define OUTCOLOR_SUCCESS	RGB(  0,255,  0)	// Green
#define OUTCOLOR_INFO		RGB(  0,255,255)	// Aqua
#define OUTCOLOR_WARNING	RGB(255,255,  0)	// Yellow



#include <Windows.h>	// OS API, Winsock
// user accounts
#include <LM.h>		// net api
#include <sddl.h>	// sid conversion
#if IS_VISUAL_STUDIO
#	pragma comment ( lib, "Ws2_32.lib" )
#	pragma comment ( lib, "Netapi32.lib" )
#	pragma comment ( lib, "Userenv.lib" )
#	pragma comment ( lib, "Psapi.lib" )
#	pragma comment ( lib, "Secur32.lib" )
#	pragma comment ( lib, "Version.lib" )
#	if defined(_INC_SHLWAPI)
#		pragma comment ( lib, "shlwapi.lib" )
#	endif
#endif



/**
 * Win32 message loop if graphical, otherwise a cli-output
 */
void
app_exec();



/**
 * Initializes the application; loads the configuration, handles the creation
 * and movement of the windows, and prepares the windows if using a graphical
 * configuration.
 *
 * The static classes in Runtime can be called safely once this function has
 * completed, not before.
 *
 * Receives the argc and argv passed in from main() directly; if using the GUI,
 * argc & argv are acquired by a Visual Studio specific ability - __argc and
 * __argv. They contain the same data in the same way.
 * 
 * @param[in] argc The number of command-line arguments supplied
 * @param[in] argv A pointer to an array of command-line arguments
 */
void
app_init(
	int32_t argc,
	char** argv
);



/**
 * Shuts down any resources used by the application, preparing for termination.
 */
void
app_stop();



/**
 * 
 */
bool
get_debugging_privileges();



/**
 * 
 */
bool
is_windows_version_gte(
	WORD os_version
);
// supporting macros for the above version
#define WINDOWS_XP	_WIN32_WINNT_WINXP
#define WINDOWS_VISTA	_WIN32_WINNT_VISTA
#define WINDOWS_7	_WIN32_WINNT_WIN7
#define WINDOWS_8	_WIN32_WINNT_WIN8
#define WINDOWS_BLUE	_WIN32_WINNT_BLUE




/**
 * Obtains the profiles directory for the system.
 *
 * The pointer returned is to a static buffer within the functions, so it
 * does not and must not be freed.
 */
wchar_t*
get_user_appdata_path();



/**
 * 
 */
uint32_t
get_user_path_offset();



/**
 * Checks the specified path against the POSIX current & up directory path
 * names ('.', and '..' respectively).
 * If it matches, skip is set to true, otherwise it remains untouched.
 *
 * @param[in] path The folder path to check
 * @param[in] skip The boolean to toggle if the path matches
 */
void
skip_if_path_is_posix(
	wchar_t* path,
	bool* skip
);



/**
 * Checks the specified path against known common profile names in Windows, 
 * e.g. C:\\Users\\Public
 * If it matches, skip is set to true, otherwise it remains untouched.
 * 
 * @warning
 * Does not handle situations where a duplicate profile name is found, and
 * Windows creates a suffixed .WINDOWS, or .NTAUTHORITY, etc.
 *
 * @param[in] path The folder path to check
 * @param[in] skip The boolean to toggle if the path matches
 */
void
skip_if_path_is_default_profile(
	wchar_t* path,
	bool* skip
);



/**
 * Intercepts signals sent to the console; allows us to terminate child threads
 * and handles cleanly if, for example, Ctrl+C is sent.
 * 
 * @param[in] signal The signal received by the command prmopt
 * @return Always returns TRUE
 */
BOOL WINAPI
sig_handler(
	DWORD signal
);



/**
 * Main window callback for the Windows native gui.
 *
 * Refer to the plentiful documentation on msdn for the vast expanse of 
 * variations the parameters can be.
 * http://www.functionx.com/win32/Lesson05.htm
 */
LRESULT CALLBACK
main_callback(
	HWND hWnd,
	UINT msg,
	WPARAM wParam,
	LPARAM lParam
);



#endif	// APP_H_INCLUDED
