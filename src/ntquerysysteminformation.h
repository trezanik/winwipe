#ifndef NTQUERYSYSTEMINFORMATION_H_INCLUDED
#define NTQUERYSYSTEMINFORMATION_H_INCLUDED

/**
 * @file	ntquerysysteminformation.h
 * @author	James Warren (NtInternals acquired enums/structs)
 * @brief	Local implementation of Windows' NtQuerySystemInformation
 */


// required inclusions
#include "ntdll.h"


// source: http://undocumented.ntinternals.net/UserMode/Undocumented%20Functions/System%20Information/SYSTEM_INFORMATION_CLASS.html
typedef enum _SYSTEM_INFORMATION_CLASS {
	SystemBasicInformation,
	SystemProcessorInformation,
	SystemPerformanceInformation,
	SystemTimeOfDayInformation,
	SystemPathInformation,
	SystemProcessInformation,
	SystemCallCountInformation,
	SystemDeviceInformation,
	SystemProcessorPerformanceInformation,
	SystemFlagsInformation,
	SystemCallTimeInformation,
	SystemModuleInformation,
	SystemLocksInformation,
	SystemStackTraceInformation,
	SystemPagedPoolInformation,
	SystemNonPagedPoolInformation,
	SystemHandleInformation,
	SystemObjectInformation,
	SystemPageFileInformation,
	SystemVdmInstemulInformation,
	SystemVdmBopInformation,
	SystemFileCacheInformation,
	SystemPoolTagInformation,
	SystemInterruptInformation,
	SystemDpcBehaviorInformation,
	SystemFullMemoryInformation, 
	SystemLoadGdiDriverInformation,
	SystemUnloadGdiDriverInformation,
	SystemTimeAdjustmentInformation,
	SystemSummaryMemoryInformation,
	SystemNextEventIdInformation,
	SystemEventIdsInformation,
	SystemCrashDumpInformation,
	SystemExceptionInformation,
	SystemCrashDumpStateInformation,
	SystemKernelDebuggerInformation,
	SystemContextSwitchInformation,
	SystemRegistryQuotaInformation,
	SystemExtendServiceTableInformation,
	SystemPrioritySeperation,
	SystemPlugPlayBusInformation,
	SystemDockInformation,
	//SystemPowerInformation,
	/* SystemPowerInformation conflicts with:
	 * VS2008: "microsoft sdks\windows\v6.0a\include\winnt.h" line 8463
	 * VS2010: 
	 * VS2012: "windows kits\8.0\include\um\winnt.h" line 13090
	 * POWER_INFORMATION_LEVEL enumeration 
	 */
	SysInfoClass_SystemPowerInformation,
	SystemProcessorSpeedInformation,
	SystemCurrentTimeZoneInformation,
	SystemLookasideInformation
} SYSTEM_INFORMATION_CLASS, *PSYSTEM_INFORMATION_CLASS;


// source: http://msdn.microsoft.com/en-us/library/gg750724%28prot.20%29.aspx
typedef struct _SYSTEM_THREAD_INFORMATION {
	LARGE_INTEGER	KernelTime;
	LARGE_INTEGER	UserTime;
	LARGE_INTEGER	CreateTime;
	ULONG		WaitTime;
	PVOID		StartAddress;
	CLIENT_ID	ClientId;
	KPRIORITY	Priority;
	KPRIORITY	BasePriority;
	ULONG		ContextSwitchCount;
	KTHREAD_STATE	State;
	KWAIT_REASON	WaitReason;
} SYSTEM_THREAD_INFORMATION, *PSYSTEM_THREAD_INFORMATION;


// source: http://undocumented.ntinternals.net/UserMode/Undocumented%20Functions/System%20Information/Structures/SYSTEM_PROCESS_INFORMATION.html
typedef struct _SYSTEM_PROCESS_INFORMATION
{
	ULONG		NextEntryOffset;
	ULONG		ThreadCount;
	ULONG		Reserved1[6];
	LARGE_INTEGER	CreateTime;
	LARGE_INTEGER	UserTime;
	LARGE_INTEGER	KernelTime;
	UNICODE_STRING	ProcessName;
	KPRIORITY	BasePriority;
	ULONG		ProcessId;
	ULONG		InheritedFromProcessId;
	ULONG		HandleCount;
	ULONG		SessionId;
	ULONG		Reserved2;
	VM_COUNTERS	VmCounters;
	IO_COUNTERS	IoCounters;
	SYSTEM_THREAD_INFORMATION	Threads[1];  // array 0 to ThreadCount - 1 of SYSTEM_THREADS_INFORMATION struct
} SYSTEM_PROCESS_INFORMATION, *PSYSTEM_PROCESS_INFORMATION;

// source: http://undocumented.ntinternals.net/UserMode/Structures/SYSTEM_MODULE.html
typedef struct _SYSTEM_MODULE
{
	ULONG		Reserved1;
	ULONG		Reserved2;
	PVOID		ImageBaseAddress;
	ULONG		ImageSize;
	ULONG		Flags;
	WORD		Id;
	WORD		Rank;
	WORD		w018;
	WORD		NameOffset;
	BYTE		Name[MAXIMUM_FILENAME_LENGTH];

} SYSTEM_MODULE, *PSYSTEM_MODULE;

#if IS_VISUAL_STUDIO
#	pragma warning ( push )
#	pragma warning ( disable : 4200 )	// zero-sized array
#endif

// source: http://undocumented.ntinternals.net/UserMode/Structures/SYSTEM_MODULE_INFORMATION.html
typedef struct _SYSTEM_MODULE_INFORMATION
{
	ULONG		ModulesCount;
	SYSTEM_MODULE	Modules[0];
} SYSTEM_MODULE_INFORMATION,*PSYSTEM_MODULE_INFORMATION;

#if IS_VISUAL_STUDIO
#	pragma warning ( pop )
#endif


/**
 * Loads and calls the 'NtQuerySystemInformation' function.
 *
 * @param SystemInformationClass One of the values enumerated in SYSTEM_INFORMATION_CLASS
 * @param SystemInformation A pointer to a buffer that receives the requested information.
 * @param SystemInformationLength The size of the buffer pointed to by the SystemInformation parameter, in bytes.
 * @param ReturnLength An optional pointer to a location where the function writes the actual size of the information requested.
 * @return Returns an NTSTATUS success or error code.
 */
NTSTATUS
NtQuerySystemInformation(
	SYSTEM_INFORMATION_CLASS SystemInformationClass,
	PVOID SystemInformation,
	ULONG SystemInformationLength,
	PULONG ReturnLength
);


#endif	// NTQUERYSYSTEMINFORMATION_H_INCLUDED
