#ifndef USERINTERFACE_H_INCLUDED
#define USERINTERFACE_H_INCLUDED

/**
 * @file	UserInterface.h
 * @author	James Warren
 * @brief	The User Interface
 */


// required inclusions
#include "build.h"
#include <Windows.h>		// HWND, HMODULE
#include <Richedit.h>		// charformat
#include "Observer.h"		// derived from
#include "Operation.h"		// non-pointer object



/**
 * The recipient of Operation Notify() events, and holder of the raw handles
 * acquired within app_init().
 *
 * Has the sole task of outputting formatted text to the previously created
 * window.
 *
 * @class UserInterface
 */
class UserInterface : public Observer
{
	// only the runtime is allowed to construct us
	friend class Runtime;
private:
	NO_CLASS_ASSIGNMENT(UserInterface);
	NO_CLASS_COPY(UserInterface);

	/** the current executing operation */
	Operation	_operation;

	/** output config for the richedit window */
	CHARFORMAT2	_cf;

	/** conversion local to the class */
	char		mb[1024];


	// one controlled instance of this class; created in Runtime
	UserInterface()
	{
	}
	
public:
	virtual ~UserInterface()
	{
	}


	/** Sets up the charformat structure, so for each call into Output we
	 * are not duplicating work, slowing things down. 
	 * native.out_window must exist prior to calling this function, or it
	 * will fail, and likely crash further usage. */
	void
	Prepare();


	// @TODO:: observer, notify() hooks for output/success/error and DRY

	void
	Output(
		DWORD text_color,
		char* text_format,
		...
	);


	/*void
	OutputFailure();*/


	/*void
	OutputSuccess();*/



	virtual void
	Update(
		Operation *operation
	);


	/*
	 * Thank you guys @ StackOverflow:
	 * http://stackoverflow.com/questions/5424042/class-variables-public-access-read-only-but-private-access-read-write
	 */
	template <class T>
	class proxy
	{
		friend class UserInterface;
		// app_init sets these values in a one-off
		friend void app_init(int32_t argc, char **argv);
		// app_stop frees these values in a one-off
		friend void app_free();
	private:
		T data;
		T operator= (const T& arg) { data = arg; return data; }
	public:
		// 0/NUL/nullptr the datatype on construction
		proxy()
		{
		}
		operator const T&() const { return data; }
		operator const T&() { return data; }
	};

	struct {
		proxy<HWND>		app_window;
		proxy<HWND>		out_window;
		proxy<HMODULE>		richedit_module;
	} native;

	
};



#endif	// USERINTERFACE_H_INCLUDED
