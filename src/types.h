#ifndef TYPES_H_INCLUDED
#define TYPES_H_INCLUDED

/**
 * @file	types.h
 * @author	James Warren
 * @brief	Defines the platform data types for the application
 */


// required inclusions
#include "compiler.h"

// Visual Studio 2010+ started to use standards, like <cstdint>
#if MSVC_BEFORE_VS10
	/* This is just a copy of the VS2012 stdint.h, with a definition
	 * modification to prevent a warning, as it's mixed with the older 
	 * headers in VS2008 (lines 120, 123). */
#	include "c99/stdint.h"
#else
#	include <cstdint>
#endif


#endif	// TYPES_H_INCLUDED
