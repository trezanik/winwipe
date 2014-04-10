#ifndef GETOPT_H_INCLUDED
#define GETOPT_H_INCLUDED

/**
 * @file	getopt.h
 * @author	James Warren
 * @brief	Win32 version of *nix getopt for parsing command line options
 */


// required inclusions
#include "build.h"


extern char*	getopt_arg;		/**< argument pointer */
extern int32_t	getopt_ind;		/**< argv index */



/**
 * Near-identical in functionality to the normal getopt utility function. As
 * this does not exist on Windows, this is a custom built-one designed to do the
 * same as on a *nix build.
 *
 * @param[in] argc
 * @param[in] argv
 * @param[in,out] opt
 * @return
 */
int32_t
getopt(
	int32_t argc,
	char** argv,
	char* opt
);


#endif  // GETOPT_H_INCLUDED
