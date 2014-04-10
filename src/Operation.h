#ifndef OPERATION_H_INCLUDED
#define OPERATION_H_INCLUDED

/**
 * @file	Operation.h
 * @author	James Warren
 * @brief	Holds the current state and data for an action to perform
 */


// required inclusions
#include "build.h"
#include "Subject.h"



/**
 * 
 *
 * @enum E_OPERATION
 */
enum E_OPERATION
{
	OP_Dummy = 0,
	OP_DeleteFile,
	OP_DeleteDirectory,
	OP_DeleteRegValue,
	OP_DeleteRegKey,
	OP_DeleteService,
	OP_KillProcess,
	OP_KillThread,
	OP_StopService,
	OP_OpenFile,
	OP_OpenRegKey,
	OP_ChangeRights,
	OP_ChangePermissionsFile,
	OP_ChangeOwnerFile,
	OP_ChangePermissionsKey,
	OP_ChangeOwnerKey,
	OP_Execute,
	OP_MAX
};



/**
 * 
 *
 * @enum E_OPERATION_STATE
 */
enum E_OPERATION_STATE
{
	OS_Unprepared = 0,
	OS_Prepared,
	OS_Paused,
	OS_Succeeded,
	OS_NotNeeded,
	OS_Failed
};



/**
 * Extra data required for an operation, that can't be supplied with a single
 * std::string. In fairness, we could delimit the string, but that would mean
 * modifying and extracting from a path, which will get messy. As such, we use
 * this struct as an additional data parameter, which will contain anything!
 *
 * We use it for example, in case of a registry value deletion. The main data
 * parameter is the path to the subkey; an extra parameter #1 will be the HKEY
 * root this subkey is in, and the extra parameter #2 is the value name itself.
 *
 * We use only 1 of each parameter in this case (which is the worst that we can
 * currently get), and I envisage no more than 3 with any future advancements;
 * Even if it isn't enough, nothing will break by adding additionals.
 *
 * @struct extra_data
 */
struct extra_data
{
	void*		vparam1;
	void*		vparam2;
	void*		vparam3;
	std::string	sparam1;
	std::string	sparam2;
	std::string	sparam3;

	extra_data()
	{
		vparam1 = nullptr;
		vparam2 = nullptr;
		vparam3 = nullptr;
	}
};



/**
 * 
 * The functions return a pointer to the operation itself, so it can be used as
 * a one-liner in a call to Notify().
 *
 * @class Operation
 */
class Operation
{
private:
	NO_CLASS_ASSIGNMENT(Operation);
	NO_CLASS_COPY(Operation);

	E_OPERATION		_operation;
	std::string		_data;
	extra_data		_extra_data;
	DWORD			_err_code;
	std::string		_err_msg;
	E_OPERATION_STATE	_op_state;

protected:
	

public:
	Operation()
	{
		// enables all Prepare()'s to have assertion checks
		_op_state = OS_Unprepared;
	}

	~Operation()
	{
	}


	E_OPERATION	GetOperation() const	{ return _operation; }
	std::string	GetData() const		{ return _data; }
	extra_data*	GetExtraData()		{ return &_extra_data; }
	DWORD		GetErrorCode() const	{ return _err_code; }
	std::string	GetErrorMsg() const	{ return _err_msg; }
	bool		GetSuccess() const	{ return (_op_state == OS_Succeeded || _op_state == OS_NotNeeded); }
	bool		HasExecuted() const	{ return (_op_state == OS_Succeeded || _op_state == OS_Failed || _op_state == OS_NotNeeded); }
	bool		IsPrepared() const	{ return _op_state == OS_Prepared; }
	bool		IsPaused() const	{ return _op_state == OS_Paused; }
	bool		WasNeeded() const	{ return _op_state == OS_NotNeeded; }


	/**
	 * Completes the operation successfully.
	 *
	 * @param[in] was_needed Flag if the operation was actually required;
	 * if a file doesn't exist for example, this is set to false, as it was
	 * marked to do, but was never found. Can be expanded to incorporate
	 * passing it to the notification, so it doesn't output to the UI.
	 * @return the pointer to the operation
	 */
	Operation*
	Finished(
		bool was_needed = true
	);


	/**
	 * Completes the operation, marking it as failed in execution.
	 *
	 * @param[in] err_code The error code
	 * @param[in] err_msg The error message for the code
	 * @return the pointer to the operation
	 */
	Operation*
	HadError(
		DWORD err_code,
		std::string err_msg
	);


	/**
	 * Pauses the operation, usually when another operation is required in
	 * order for this one to succeed (usually in recursive folder deletes).
	 *
	 * This is needed for the UI to be notified, so that it can drop down
	 * to a new line (as this one can't report completion or failure yet).
	 *
	 * @return the pointer to the operation
	 */
	Operation*
	Pause();


	/**
	 * Generates the textual output to put to the screen, and also 
	 * depending on the operation, sets the data for it (i.e. the data
	 * format will be the path to a file if the operation is OP_DeleteFile,
	 * and so on).
	 *
	 * @param[in] operation The type of operation to perform
	 * @param[in] data_format The format-string of the parameters to use as
	 * the input data
	 * @param[in] ... The rest of the parameters as required by the format
	 *
	 * @return the pointer to the operation
	 */
	Operation*
	Prepare(
		E_OPERATION operation,
		char* data_format,
		...
	);


	/**
	 * Resumes the operation after a prior Pause().
	 *
	 * @return the pointer to the operation
	 */
	Operation*
	Resume();
};




#endif	// OPERATION_H_INCLUDED
