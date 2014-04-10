#ifndef RUNTIME_H_INCLUDED
#define RUNTIME_H_INCLUDED

/**
 * @file	Runtime.h
 * @author	James Warren
 * @brief	Application globally-accessible singleton containing other classes
 */


// required inclusions
#include "build.h"

// forward declarations
class Configuration;
class FileParser;
class Log;
class UserInterface;
class Wiper;



/**
 * Dedicated class for storing all 'global' variables. This itself is accessed
 * through an extern by the runtime code, so is in itself actually a global.
 *
 * While this is a technical misuse of a Singleton, since there can only ever
 * be one runtime (which we can then use to interface with the host OS and also
 * maintains if the app is quitting, for example), it seems appropriate and
 * can easily be replaced with the way it has been designed.
 *
 * The classes accessed are not constructed until their first call, which keeps
 * the application startup brief. You can therefore make assumptions in the
 * child classes about the lifetimes of other objects, and safely know each
 * will have their destructor called before the runtime is destroyed.
 *
 * Yes, this can be implemented in other ways - but this is clear and concise,
 * without _too_ many of the issues commonly associated with bad singleton use.
 *
 * On Windows, this constructor is executed before main() is entered; the 
 * actual call stack is (assuming Windows subsystem):
 @verbatim
 - Runtime()
 - _initterm
 - __tmainCRTStartup (also calls main)
 - WinMainCRTStartup
 @endverbatim
 *
 * @class Runtime
 */
class Runtime
{
private:
	NO_CLASS_ASSIGNMENT(Runtime);
	NO_CLASS_COPY(Runtime);

	Runtime();
	~Runtime();

public:

	/**
	 * Acquires the singleton reference to the class. Only used within
	 * app.cc in order to make the runtime accessible globally (accessed by
	 * 'runtime') - shouldn't be used outside of this.
	 */
	static Runtime& Instance()
	{
		static Runtime	rtime;
		return rtime;
	}


	/**
	 * Retrieves a pointer to the static Configuration variable.
	 *
	 * @return Always returns a pointer to a Configuration instance; never
	 * fails
	 */
	Configuration*
	Config() const;


	/**
	 * Retrieves a pointer to the static Log variable.
	 *
	 * @return Always returns a pointer to a Log instance; never fails 
	 */
	Log*
	Logger() const;


	/**
	 * Retrieves a pointer to the static Parser variable.
	 *
	 * @return Always returns a pointer to a Parser instance; never fails
	 */
	FileParser*
	Parser() const;


	/**
	 * Retrieves a pointer to the static UserInterface variable.
	 *
	 * @return Always returns a pointer to a UserInterface instance; never
	 * fails
	 */
	UserInterface*
	UI() const;


	/**
	 * Retrieves a pointer to the static Wiper variable.
	 *
	 * @return Always returns a pointer to a Wiper instance; never fails
	 */
	Wiper*
	Wiper() const;
};



extern Runtime		&runtime;



#endif	// RUNTIME_H_INCLUDED
