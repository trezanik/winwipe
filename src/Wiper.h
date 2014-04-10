#ifndef WIPER_H_INCLUDED
#define WIPER_H_INCLUDED

/**
 * @file	Wiper.h
 * @author	James Warren
 * @brief	The core of the functionality that executes and purges
 */


// required inclusions
#include "build.h"
#include "Subject.h"
#include <queue>
#include <stack>
#include <Windows.h>		// HKEY, SC_HANDLE

// forward declarations
class Operation;



/**
 * The main processing class of the application; once the operations queue is
 * populated from parsing the input files, this will execute all the required
 * functionality to process the relevant operation, and is the point at which
 * environment variables are expanded.
 *
 * @class Wiper
 */
class Wiper : public Subject
{
	// only the runtime is allowed to construct us
	friend class Runtime;
private:
	NO_CLASS_ASSIGNMENT(Wiper);
	NO_CLASS_COPY(Wiper);

	/** Handle to the service control manager; never acquired unless a
	 * service to work on is supplied in the wiping input */
	SC_HANDLE		_scm;

	/** Pending operations, added by parsing the input files */
	std::queue<Operation*>	_operations;

	/** Pending operations, required as part of the operation currently
	 * running from the queue, in the event more operations are needed
	 * dynamically to complete the first - such as deleting a folder that
	 * is not empty, generates extra operations for each file/folder. */
	std::stack<Operation*>	_operations_stack;

	/** The current operation being worked on*/
	Operation*		_current_operation;
	
	/** conversion local to the class; within the eradicate functions the
	 * recursion can consume quite a bit of memory needlessly; we only
	 * ever need one conversion buffer, we're single threaded. */
	char			mb[1024];


	/**
	 * Deletes the service specified by name.
	 *
	 * If the service is running, it will NOT be stopped; call StopService
	 * manually beforehand if this is desired.
	 *
	 * @param[in] name The wide-character name of the service to delete
	 * @return true if the service is deleted, or does not exist at all
	 * @return false if access is denied, or deletion failed
	 */
	bool
	DeleteService(
		const wchar_t* name
	);


	/**
	 * Calls the wide-character version after conversion to UTF-8.
	 *
	 * @param[in] path The multi-byte path to eradicate
	 * @return the result of the wchar_t EradicateFileObject()
	 * @return false if path is a nullptr
	 */
	bool
	EradicateFileObject(
		const char* path
	);


	/**
	 * Wipes out a file or folder specified by wpath. The application 
	 * config determines whether permissions or change of ownership is to
	 * be performed if a deletion fails.
	 *
	 * For folders with subdirectories, this will call itself recursively
	 * for every file and folder within it, generating new operations for
	 * each one found on the fly.
	 *
	 * @param[in] wpath The wide-character path to eradicate
	 * @return true if the file/folder is deleted
	 * @return false if the file/folder is not deleted, or wpath is a nullptr
	 */
	bool
	EradicateFileObject(
		const wchar_t* wpath
	);


	/**
	 * Calls the wide-character version after conversion to UTF-8.
	 *
	 * @param[in] key_root The HKEY_xxx to open
	 * @param[in] subkey The multi-byte subkey name to eradicate
	 * @return the result of the wchar_t EradicateRegistryObject()
	 * @return false if subkey is a nullptr, or the key_root is unknown
	 */
	bool
	EradicateRegistryObject(
		HKEY key_root,
		const char* subkey
	);


	/**
	 * Deletes the registry key supplied under the key_root set.
	 *
	 * The key root is one of:
	 * - HKEY_LOCAL_MACHINE
	 * - HKEY_CURRENT_USER
	 * - HKEY_USERS
	 * - HKEY_CLASSES_ROOT
	 *
	 * This will be output in textual format in the shorthand (to try and
	 * fit as much data in the output window as possible, without needing
	 * to scroll). As such, the above will be output as:
	 * - HKLM
	 * - HKCU
	 * - HKU
	 * - HKCR
	 *
	 * If access is denied when attempting deletion, the function will
	 * optionally try to take ownership + change the permissions to enable
	 * deletion (as the minimum; we always aim for least privilege). This
	 * setting is controlled through the runtime configuration.
	 *
	 * @param[in] key_root The HKEY_xxx to open
	 * @param[in] subkey The wide-character subkey name to eradicate
	 * @return the result of the wchar_t EradicateRegistryObject()
	 * @return false if subkey is a nullptr, or the key_root is unknown
	 */
	bool
	EradicateRegistryObject(
		HKEY key_root,
		const wchar_t* subkey
	);


	/**
	 * Identical to EradicateRegistryObject(), only instead of deleting a
	 * key, it deletes a value instead, input as an extra parameter.
	 *
	 * @param[in] key_root The HKEY_xxx to open
	 * @param[in] subkey The wide-character subkey name the values resides in
	 * @param[in] value The wide-character name of the value to delete
	 * @return the result of the wchar_t EradicateRegistryObject()
	 * @return false if subkey is a nullptr, or the key_root is unknown
	 */
	bool
	EradicateRegistryObject(
		HKEY key_root,
		const wchar_t* subkey,
		const wchar_t* value
	);


	/**
	 * Calls the wide-character version after conversion to UTF-8.
	 *
	 * @param[in] command The multi-byte command to run
	 * @return the result of the wchar_t ExecuteCommand()
	 * @return false if command is a nullptr
	 */
	bool
	ExecuteCommand(
		const char* command
	);


	/**
	 * Executes the specified program/script/installer. The action to
	 * perform is determined by the file extension (we don't use the
	 * shell mechanism for it - we hardcode it ourselves. Unlikely to
	 * ever change so shouldn't ever be an issue).
	 *
	 * @warning
	 * If you're doing anything beyond running an exe (with no params)
	 * or similar, ALWAYS double-quote the application name. This enables
	 * the code to correctly identify what executable is being executed
	 * in situations where you need to use dots, etc., in the command.
	 * Best example:
	 @verbatim
	 taskkill /F /IM taskmgr.exe
	 @endverbatim
	 * If not quoted, this will believe everything before the . is the 
	 * binary/script to run, and can cause the operation to fail. There's
	 * also no extension directly provided, and so this code will fall
	 * back to a default operation (which happens to be running a .exe).
	 * Best instead:
	 @verbatim
	 "taskkill.exe" /F /IM taskmgr.exe
	 @endverbatim
	 *
	 * @param[in] command The path to the item to run, including any
	 * command line options suffixed. The extension will be determined
	 * up to the first space character, unless enclosed in single or
	 * double quotes, where the closing character is then the end.
	 * @return true if the process exit code is 0, or the msi installer
	 * returns a success code but needing a reboot. An actual failure will
	 * make this return false.
	 */
	bool
	ExecuteCommand(
		const wchar_t* command
	);


	/**
	 * Kills the process identified by name. This will only kill the first
	 * instance of the name found, as returned by NtQuerySystemInformation.
	 *
	 * Use the utility function process_exists() after each function call
	 * to determine if a loop should continue if killing the process by its
	 * name, otherwise use KillProcess() supplying the process id.
	 *
	 * @param[in] by_name The wide-character name of the process to kill
	 * @return true if the process was terminated or not found
	 * @return false if the process exists and could not be terminated
	 */
	bool
	KillProcess(
		const wchar_t* by_name
	);


	/**
	 * Kills the process identified by its Process ID.
	 *
	 * @param[in] by_pid The process identifier of the process to kill
	 * @return true if the process was terminated or not found
	 * @return false if the process exists and could not be terminated
	 */
	bool
	KillProcess(
		uint32_t by_pid
	);


	/**
	 * Internally, artificially adds another operation to process at 
	 * runtime, in response to a pending operation - 99% of the time it is
	 * due to a recursive delete, finding other things to remove.
	 */
	void
	NewOperation();


	/**
	 * Pops an operation off the stack to return to the originally running
	 * Operation. Valid only if NewOperation() has been called while 
	 * another operation was still running (primarily recursive functions).
	 *
	 * @param[in] notify Defaulted true, set to false if you don't want 
	 * the operation text output again. Used when making more than one
	 * change before returning to the original operation (like changing
	 * permissions of a file, then taking ownership).
	 */
	void
	PopPreviousOperation(
		bool notify = true
	);


	/**
	 * Replaces the permissions of the file/folder.
	 *
	 * Only executed if, when trying to delete a file/folder, access is
	 * denied, and modifying permissions is enabled in the runtime 
	 * configuration.
	 *
	 * @todo further elaboration of changes made
	 *
	 * @param[in] path The full path of the file/folder to modify
	 * @return true if the permissions are modified such that deletion, if
	 * attempted, will work
	 * @return false if the permissions fail to get modified, or the 
	 * changes would still prevent deletion
	 */
	bool
	ReplacePermissionsOfFile(
		const wchar_t* path
	);


	/**
	 * Replaces the permissions of the registry key specified.
	 *
	 * Only executed if, when trying to delete a registry key/value, access
	 * is denied, and modifying permissions is enabled in the runtime 
	 * configuration.
	 *
	 * @todo further elaboration of changes made
	 *
	 * @param[in] key_root The HKEY_Xxx the subkey resides within
	 * @param[in] subkey The wide-character subkey name to modify
	 * @return true if the permissions are modified such that deletion, if
	 * attempted, will work
	 * @return false if the permissions fail to get modified, or the 
	 * changes would still prevent deletion
	 */
	bool
	ReplacePermissionsOfKey(
		HKEY key_root,
		const wchar_t* subkey
	);


	/**
	 * Stops the service as supplied by name.
	 *
	 * This function will not return until the service has stopped, timed
	 * out (using the service specified wait hint), or the service control 
	 * manager has reported a failure.
	 *
	 * @param[in] name The wide-character service name to stop
	 * @return true if the service does not exist, or has been stopped
	 * @return false if permissions are denied or the service failed to stop
	 */
	bool
	StopService(
		const wchar_t* name
	);


	/**
	 * Replaces the owner of the file with the user account currently being
	 * used to execute the application.
	 *
	 * Only executed if, when trying to delete a registry key/value, access
	 * is denied, and modifying permissions is enabled in the runtime 
	 * configuration.
	 *
	 * @param[in] path The path to the file/folder to take over
	 * @return true if the executing account has ownership of the file/folder
	 * @return false if the path was invalid, or access was denied when 
	 * attempting to take over
	 */
	bool
	TakeOwnershipOfFile(
		const wchar_t* path
	);


	/**
	 * Calls the wide-character version after conversion to UTF-8.
	 *
	 * @param[in] key_root The HKEY_Xxx to open
	 * @param[in] subkey The multi-byte subkey name to take over
	 * @return true if the executing account has ownership of the key
	 * @return false if subkey is a nullptr, the key_root is unknown, or
	 * access was denied when attempting to take over
	 */
	bool
	TakeOwnershipOfKey(
		HKEY key_root,
		const char* subkey
	);


	/**
	 * Replaces the owner of the registry key with the user account
	 * currently being used to execute the application.
	 * 
	 * Only executed if, when trying to delete a registry key/value, access
	 * is denied, and modifying permissions is enabled in the runtime 
	 * configuration.
	 *
	 * @param[in] key_root The HKEY_Xxx to open
	 * @param[in] subkey The wide-character subkey name to take over
	 * @return true if the executing account has ownership of the file/folder
	 * @return false if subkey is a nullptr, the key_root is unknownw, or
	 * access was denied when attempting to take over
	 */
	bool
	TakeOwnershipOfKey(
		HKEY key_root,
		const wchar_t* subkey
	);


	// one controlled instance of this class; created in Runtime
	Wiper();

public:
	~Wiper();


	/**
	 * Begins the execution run of all the queued operations.
	 *
	 * @return true if no operations encountered an error (including if
	 * there are no operations to do)
	 * @return false if any operation fails; the queue is paused at the
	 * entry currently being processed
	 */
	bool
	Execute();

	
	/**
	 * Grants the SeDebugPrivilege to this process' token. Having this
	 * enables greater access when doing things like process termination,
	 * where this privilege means OpenProcess() will always succeed.
	 *
	 * @return true if the permission is enabled or has turned on
	 * @return false if the permission could not be acquired
	 */
	bool
	ExecutorGrantDebugging();


	/**
	 * Checks if the user account executing this application has admin
	 * rights on the machine.
	 *
	 * Run on application initialization to prevent a plethora of errors
	 * when run by a non-admin. Will enable the menu option to execute
	 * if this passes in the message loop callback (app.cc).
	 *
	 * @return true if the user account is a member of Administrators
	 * @return false if the user account is not a member of Administrators,
	 * or a function fails when attempting to check
	 */
	bool
	ExecutorHasAdminPrivileges();


	/**
	 * Retrieves the number of operations currently sitting in the queue.
	 */
	uint32_t
	NumQueuedOperations();


	/**
	 * Adds a new Operation to the internal queue, ready for processing.
	 *
	 * Identical operations can be added multiple times (so long as it is
	 * not the same object) - this is in case an uninstaller or automated
	 * procedure recreates a file already deleted, and it's desired to 
	 * delete it before and after.
	 *
	 * @param[in] operation The operation to queue
	 * @return true if the operation is added to the queue
	 * @return false if the operation is invalid, or could not be added
	 */
	bool
	QueueNewOperation(
		Operation* operation
	);


	/**
	 * 
	 */
	bool
	WipeJavaAppData();
	bool
	WipeFolderTest();
	bool
	WipeRegistryTest();
	bool
	WipeProcessTest();
	bool
	WipeServiceTest();
};



#endif	// WIPER_H_INCLUDED
