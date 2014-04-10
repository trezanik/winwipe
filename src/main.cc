
/**
 * @file	main.cc
 * @author	James Warren
 * @copyright	James Warren, 2013-2014
 * @license	zlib (see license.txt or http://opensource.org/licenses/Zlib)
 */


#include "build.h"		// always first

#include <cstdlib>		// EXIT_[FAILURE|SUCCESS]
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>

#include "app.h"



/* Include Directories:
$(ProjectDir)../../third-party/gtest/gtest;
$(ProjectDir)../../third-party/tinyxml2/src;
$(ProjectDir)../../third-party/sqlite-amalgamation-3080001;
*/
// unicode handling on windows
// http://www.utf8everywhere.org/
// http://stackoverflow.com/questions/11503145/how-to-represent-strings-in-a-cross-platform-windows-ios-android-c-app


#if defined(TESTING)
// path must be like this for gtest-all.cc without changes
#	include "../gtest/gtest/gtest.h"


int32_t
main(
	int32_t argc,
	char** argv
)
{
	std::string	s;
	std::cout << "Executing all tests\n\n";

	testing::InitGoogleTest(&argc, argv);;
	RUN_ALL_TESTS();

	std::cout << "\nPress any key to close\n";
	std::cin >> s;
	return EXIT_SUCCESS;
}


#else	// TESTING



#if 0	// Code Removed: Used for /SUBSYSTEM:Windows, we're using Console
#pragma comment ( linker, "/SUBSYSTEM:Windows" )
int32_t WINAPI
WinMain(
	HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR lpCmdLine,
	int32_t nShowCmd
)
{
	int32_t		return_code = EXIT_FAILURE;

	UNREFERENCED_PARAMETER(hInstance);
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);
	UNREFERENCED_PARAMETER(nShowCmd);

	// argc & argv need to be __argc and __argv with /SUBSYSTEM:Windows
	int32_t argc = __argc;
	char** argv = __argv;
#else

int32_t
main(
	int32_t argc,
	char** argv
)
{
	int32_t		return_code = EXIT_FAILURE;

#endif

	/* initialize the application. These are the core essentials; if an
	 * error or exception is raised, we cannot continue with normal startup
	 * and must quit. Only this function has access to expose items to the
	 * application runtime.
	 *
	 * Any attempts to access runtime methods before this is completed will
	 * result in crashing, guaranteed. So don't call runtime.Config() before
	 * now, ok?... */
	try
	{
		app_init(argc, argv);
	}
	catch ( std::exception& e )
	{
		std::cerr << "Critical exception in initialization:\n\t" << e.what() << "\n";
		goto abort;
	}
	catch ( ... )
	{
		std::cerr << "Unhandled, critical exception in initialization\n";
		goto abort;
	}


	try
	{
		// application execution, endless loop
		app_exec();
	}
	catch ( std::runtime_error& e )
	{
		std::cerr << "Runtime Error:\n\t" << e.what() << "\n";
		goto abort;
	}
	catch ( std::exception& e )
	{
		std::cerr << "Critical exception in application execution:\n\t" << e.what() << "\n";
		goto abort;
	}
	catch ( ... )
	{
		std::cerr << "Unhandled, critical exception in application execution\n";
		goto abort;
	}


	try
	{
		// cleanup resources
		app_stop();
	}
	catch ( std::exception& e )
	{
		std::cerr << "Critical exception in closure:\n\t" << e.what() << "\n";
		goto abort;
	}
	catch ( ... )
	{
		std::cerr << "Unhandled, critical exception in closure\n";
		goto abort;
	}

	return_code = EXIT_SUCCESS;

abort:
	return return_code;
}


#endif	// TESTING
