
/**
 * @file	Operation.cc
 * @author	James Warren
 * @copyright	James Warren, 2013-2014
 * @license	Zlib (see license.txt or http://opensource.org/licenses/Zlib)
 */


#include "build.h"		// always first

#include <cassert>
#include <Windows.h>

#include "Operation.h"		// prototypes



Operation*
Operation::Finished(
	bool was_needed
)
{
	// _err_code, _err_msg should be leftover from Prepare()
	assert(_err_code == 0);
	assert(_err_msg.empty());
	// shouldn't be able to have success twice on a single operation
	if ( was_needed )
	{
		assert(_op_state != OS_Succeeded);
		_op_state	= OS_Succeeded;
	}
	else
	{
		assert(_op_state != OS_NotNeeded);
		_op_state	= OS_NotNeeded;
	}

	_data.clear();

	return this;
}



Operation*
Operation::HadError(
	DWORD err_code,
	std::string err_msg
)
{
	// shouldn't be able to have an error twice on a single operation
	assert(_op_state != OS_Failed);

	_err_code	= err_code;
	_err_msg	= err_msg;

	_op_state	= OS_Failed;

	_data.clear();

	return this;
}



Operation*
Operation::Pause()
{
	// must be prepared before we can pause it
	assert(_op_state == OS_Prepared);

	_op_state = OS_Paused;
	return this;
}



Operation*
Operation::Prepare(
	E_OPERATION operation,
	char* data_format,
	...
)
{
	char		buffer[2048];
	va_list		varg;

	// shouldn't be able to prepare without failing or succeeding 
	// (or the initial constructor state)
	assert(_op_state != OS_Prepared);
	assert(data_format != nullptr);

	va_start(varg, data_format);
#if MSVC_IS_VS10_OR_LATER	// changed to 4 args since VS2010
	vsnprintf_s(buffer, sizeof(buffer), data_format, varg);
#else
	vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, data_format, varg);
#endif
	va_end(varg);

	_operation	= operation;
	_data		= buffer;

	_op_state	= OS_Prepared;
	_err_code	= 0;
	_err_msg.clear();

	return this;
}



Operation*
Operation::Resume()
{
	assert(_op_state == OS_Paused);

	_op_state = OS_Prepared;
	return this;
}
