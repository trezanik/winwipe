
/**
 * @file	Runtime.cc
 * @author	James Warren
 * @copyright	James Warren, 2013-2014
 * @license	Zlib (see license.txt or http://opensource.org/licenses/Zlib)
 */


#include "build.h"		// always first

#include "Configuration.h"	// we own, create, and destroy
#include "Log.h"		// we own, create, and destroy
#include "UserInterface.h"	// we own, create, and destroy
#include "Wiper.h"		// we own, create, and destroy
#include "FileParser.h"		// we own, create, and destroy
#include "Runtime.h"		// prototypes



Runtime::Runtime()
{
}



Runtime::~Runtime()
{
}



Configuration*
Runtime::Config() const
{
	static Configuration	config;
	return &config;
}



Log*
Runtime::Logger() const
{
	static Log		log;
	return &log;
}



FileParser*
Runtime::Parser() const
{
	static FileParser	parser;
	return &parser;
}



UserInterface*
Runtime::UI() const
{
	static UserInterface	ui;
	return &ui;
}



Wiper*
Runtime::Wiper() const
{
	static class Wiper	wiper;
	return &wiper;
}
