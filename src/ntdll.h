#ifndef NTDLL_H_INCLUDED
#define NTDLL_H_INCLUDED

/**
 * @file	ntdll.h
 * @author	James Warren (sources referenced for enums/structs)
 * @brief	Local implementation of Windows ntdll functions we use
 */


/* all 'reserved' members you can probably find within process hacker:
 * http://processhacker.sourceforge.net/doc/ntexapi_8h_source.html
 * they're not all inbuilt here since we aim to support XP from SP2,
 * and it'd cause even more spammage in the code via #ifs
 *
 * This file is __really__ a quick mash of requirements to call the NTDLL funcs
 * we use. There's so much crap to the WinAPI at this stage that there'll never
 * be a 'clean' way of organising this (prepare for redeclaration errors!).
 */

typedef LONG	NTSTATUS;	// not defined within ntstatus.h (?)
typedef LONG	KPRIORITY;	// first found in BOINC source, lol

#define MAXIMUM_FILENAME_LENGTH			256	// taken from NTDDK

	
// source: http://www.nirsoft.net/kernel_struct/vista/KTHREAD_STATE.html
typedef enum _KTHREAD_STATE
{
	 Initialized = 0,
	 Ready = 1,
	 Running = 2,
	 Standby = 3,
	 Terminated = 4,
	 Waiting = 5,
	 Transition = 6,
	 DeferredReady = 7,
	 GateWait = 8
} KTHREAD_STATE;


// source: http://www.nirsoft.net/kernel_struct/vista/KWAIT_REASON.html
typedef enum _KWAIT_REASON
{
	Executive = 0,
	FreePage = 1,
	PageIn = 2,
	PoolAllocation = 3,
	DelayExecution = 4,
	Suspended = 5,
	UserRequest = 6,
	WrExecutive = 7,
	WrFreePage = 8,
	WrPageIn = 9,
	WrPoolAllocation = 10,
	WrDelayExecution = 11,
	WrSuspended = 12,
	WrUserRequest = 13,
	WrEventPair = 14,
	WrQueue = 15,
	WrLpcReceive = 16,
	WrLpcReply = 17,
	WrVirtualMemory = 18,
	WrPageOut = 19,
	WrRendezvous = 20,
	Spare2 = 21,
	Spare3 = 22,
	Spare4 = 23,
	Spare5 = 24,
	WrCalloutStack = 25,
	WrKernel = 26,
	WrResource = 27,
	WrPushLock = 28,
	WrMutex = 29,
	WrQuantumEnd = 30,
	WrDispatchInt = 31,
	WrPreempted = 32,
	WrYieldExecution = 33,
	WrFastMutex = 34,
	WrGuardedMutex = 35,
	WrRundown = 36,
	MaximumWaitReason = 37
} KWAIT_REASON;

// source: http://www.nirsoft.net/kernel_struct/vista/CLIENT_ID.html
typedef struct _CLIENT_ID
{
	PVOID		UniqueProcess;
	PVOID		UniqueThread;
} CLIENT_ID, *PCLIENT_ID;

// source: http://www.nirsoft.net/kernel_struct/vista/UNICODE_STRING.html
typedef struct _UNICODE_STRING
{
	WORD		Length;
	WORD		MaximumLength;
	WORD		*Buffer;
} UNICODE_STRING, *PUNICODE_STRING;

// source: http://forum.sysinternals.com/code-for-total-vm-of-a-process_topic6037.html
typedef struct _VM_COUNTERS
{
	ULONG		PeakVirtualSize;
	ULONG		VirtualSize;
	ULONG		PageFaultCount;
	ULONG		PeakWorkingSetSize;
	ULONG		WorkingSetSize;
	ULONG		QuotaPeakPagedPoolUsage;
	ULONG		QuotaPagedPoolUsage;
	ULONG		QuotaPeakNonPagedPoolUsage;
	ULONG		QuotaNonPagedPoolUsage;
	ULONG		PagefileUsage;
	ULONG		PeakPagefileUsage;
} VM_COUNTERS, *PVM_COUNTERS; 


#endif	// NTDLL_H_INCLUDED
