#pragma once
#include "Undocumented.h"



#define MM_UNLOADED_DRIVERS_SIZE 50
#define IMAGE_SCN_CNT_CODE 0x00000020
#define IMAGE_SCN_MEM_EXECUTE 0x20000000
#define IMAGE_SCN_MEM_DISCARDABLE 0x02000000
#define IMAGE_NT_SIGNATURE 0x00004550
#define IMAGE_DOS_SIGNATURE 0x5A4D // MZ
#define STANDARD_RIGHTS_ALL 0x001F0000L



ULONG KernelSize;
PVOID KernelBase;
PVOID getKernelBase(OUT PULONG pSize)
{
	NTSTATUS Status = STATUS_SUCCESS;
	ULONG Bytes = 0;
	PRTL_PROCESS_MODULES arrayOfModules;
	PVOID routinePtr = NULL; /*RoutinePtr points to a
	routine and checks if it is in Ntoskrnl*/

	UNICODE_STRING routineName;

	if (KernelBase != NULL)
	{
		if (pSize)
			*pSize = KernelSize;
		return KernelBase;
	}

	RtlUnicodeStringInit(&routineName, L"NtOpenFile");
	routinePtr = MmGetSystemRoutineAddress(&routineName); //get address of NtOpenFile


	if (routinePtr == NULL)
	{
		return NULL;
	}
	else
	{

		DbgPrint("MmGetSystemRoutineAddress inside getkernelbase succeed\n");
	}


	//get size of system module information
	Status = ZwQuerySystemInformation(SystemModuleInformation, 0, Bytes, &Bytes);
	if (Bytes == 0)
	{
		DbgPrint("%s: Invalid SystemModuleInformation size\n");
		return NULL;
	}


	arrayOfModules = (PRTL_PROCESS_MODULES)ExAllocatePoolWithTag(NonPagedPool, Bytes, 0x454E4F45); //array of loaded kernel modules
	RtlZeroMemory(arrayOfModules, Bytes); //clean memory


	Status = ZwQuerySystemInformation(SystemModuleInformation, arrayOfModules, Bytes, &Bytes);
	if (NT_SUCCESS(Status))
	{
		DbgPrint("ZwQuerySystemInformation inside getkernelbase succeed\n");
		PRTL_PROCESS_MODULE_INFORMATION pMod = arrayOfModules->Modules;
		for (int i = 0; i < arrayOfModules->NumberOfModules; ++i)
		{

			if (routinePtr >= pMod[i].ImageBase && routinePtr < (PVOID)((PUCHAR)pMod[i].ImageBase + pMod[i].ImageSize))
			{

				KernelBase = pMod[i].ImageBase;
				KernelSize = pMod[i].ImageSize;

				if (pSize)
					*pSize = KernelSize;
				break;
			}
		}
	}
	if (arrayOfModules)
		ExFreePoolWithTag(arrayOfModules, 0x454E4F45); // 'ENON'

	DbgPrint("KernelSize : %i\n", KernelSize);
	DbgPrint("g_KernelBase : %p\n", KernelBase);
	return (PVOID)KernelBase;
}


NTSTATUS BBSearchPattern(IN PCUCHAR pattern, IN UCHAR wildcard, IN ULONG_PTR len, IN const VOID* base, IN ULONG_PTR size, OUT PVOID* ppFound)
{
	ASSERT(ppFound != NULL && pattern != NULL && base != NULL);
	if (ppFound == NULL || pattern == NULL || base == NULL)
		return STATUS_INVALID_PARAMETER;

	for (ULONG_PTR i = 0; i < size - len; i++)
	{
		BOOLEAN found = TRUE;
		for (ULONG_PTR j = 0; j < len; j++)
		{
			if (pattern[j] != wildcard && pattern[j] != ((PCUCHAR)base)[i + j])
			{
				found = FALSE;
				break;
			}
		}

		if (found != FALSE)
		{
			*ppFound = (PUCHAR)base + i;
			return STATUS_SUCCESS;
		}
	}

	return STATUS_NOT_FOUND;
}



NTSTATUS BBScanSection(IN PCCHAR section, IN PCUCHAR pattern, IN UCHAR wildcard, IN ULONG_PTR len, OUT PVOID* ppFound, PVOID base = nullptr)
{
	//ASSERT(ppFound != NULL);
	if (ppFound == NULL)
		return STATUS_ACCESS_DENIED; //STATUS_INVALID_PARAMETER

	if (nullptr == base)
		base = getKernelBase(NULL);
	if (base == nullptr)
		return STATUS_ACCESS_DENIED; //STATUS_NOT_FOUND;

	PIMAGE_NT_HEADERS64 pHdr = (PIMAGE_NT_HEADERS64)RtlImageNtHeader(base);
	if (!pHdr)
		return STATUS_ACCESS_DENIED; // STATUS_INVALID_IMAGE_FORMAT;

	//PIMAGE_SECTION_HEADER pFirstSection = (PIMAGE_SECTION_HEADER)(pHdr + 1);
	PIMAGE_SECTION_HEADER pFirstSection = (PIMAGE_SECTION_HEADER)((uintptr_t)&pHdr->FileHeader + pHdr->FileHeader.SizeOfOptionalHeader + sizeof(IMAGE_FILE_HEADER));

	for (PIMAGE_SECTION_HEADER pSection = pFirstSection; pSection < pFirstSection + pHdr->FileHeader.NumberOfSections; pSection++)
	{
		//DbgPrint("section: %s\r\n", pSection->Name);
		ANSI_STRING s1, s2;
		RtlInitAnsiString(&s1, section);
		RtlInitAnsiString(&s2, (PCCHAR)pSection->Name);
		if ((RtlCompareString(&s1, &s2, TRUE) == 0) || (pSection->Characteristics & IMAGE_SCN_CNT_CODE) || (pSection->Characteristics & IMAGE_SCN_MEM_EXECUTE))
		{
			PVOID ptr = NULL;
			NTSTATUS status = BBSearchPattern(pattern, wildcard, len, (PUCHAR)base + pSection->VirtualAddress, pSection->Misc.VirtualSize, &ptr);
			if (NT_SUCCESS(status)) {
				*(PULONG64)ppFound = (ULONG_PTR)(ptr); //- (PUCHAR)base
				//DbgPrint("found\r\n");
				return status;
			}
			//we continue scanning because there can be multiple sections with the same name.
		}
	}

	return STATUS_ACCESS_DENIED; //STATUS_NOT_FOUND;
}

PVOID ResolveRelativeAddress(
	_In_ PVOID Instruction,
	_In_ ULONG OffsetOffset,
	_In_ ULONG InstructionSize
)
{
	ULONG_PTR Instr = (ULONG_PTR)Instruction;
	LONG RipOffset = *(PLONG)(Instr + OffsetOffset);
	PVOID ResolvedAddr = (PVOID)(Instr + InstructionSize + RipOffset);

	return ResolvedAddr;
}



UCHAR PiDDBLockPtr_sig[] = "\x48\x8D\x0D\x00\x00\x00\x00\xE8\x00\x00\x00\x00\x4C\x8B\x8C";
UCHAR PiDDBCacheTablePtr_sig[] = "\x66\x03\xD2\x48\x8D\x0D";

bool LocatePiDDB(PERESOURCE* lock, PRTL_AVL_TABLE* table)
{
	PVOID PiDDBLockPtr = nullptr, PiDDBCacheTablePtr = nullptr;
	if (!NT_SUCCESS(BBScanSection("PAGE", PiDDBLockPtr_sig, 0, sizeof(PiDDBLockPtr_sig) - 1, reinterpret_cast<PVOID*>(&PiDDBLockPtr)))) {
		DbgPrint("Unable to find PiDDBLockPtr sig. Piddblockptr is: %p.\n", PiDDBLockPtr);
		return false;
	}
	DbgPrint("found PiDDBLockPtr sig. Piddblockptr is: %p\n", PiDDBLockPtr);

	if (!NT_SUCCESS(BBScanSection("PAGE", PiDDBCacheTablePtr_sig, 0, sizeof(PiDDBCacheTablePtr_sig) - 1, reinterpret_cast<PVOID*>(&PiDDBCacheTablePtr)))) {
		DbgPrint("Unable to find PiDDBCacheTablePtr sig. PiDDBCacheTablePtr is: %p\n", PiDDBCacheTablePtr);
		return false;
	}
	DbgPrint("found PiDDBCacheTablePtr sig. PiDDBCacheTablePtr is: %p\n", PiDDBCacheTablePtr);


	PiDDBCacheTablePtr = PVOID((uintptr_t)PiDDBCacheTablePtr + 3);

	*lock = (PERESOURCE)(ResolveRelativeAddress(PiDDBLockPtr, 3, 7));
	*table = (PRTL_AVL_TABLE)(ResolveRelativeAddress(PiDDBCacheTablePtr, 3, 7));

	return true;
}




BOOLEAN ClearPiddbCacheTable()
{
	PERESOURCE PiDDBLock = NULL;
	PRTL_AVL_TABLE PiDDBCacheTable = NULL;
	NTSTATUS Status = LocatePiDDB(&PiDDBLock, &PiDDBCacheTable);
	if (PiDDBCacheTable == NULL || PiDDBLock == NULL)
	{
		DbgPrint("LocatePIDDB lock and/or cachetable not found\n");
		return Status;
	}
	else
	{
		DbgPrint("Successfully found PiddbCachetable and lock!!!1111\n");
		DbgPrint("PiddbLock: %p\n", PiDDBLock);
		DbgPrint("PiddbCacheTable: %p\n", PiDDBCacheTable);

		PIDCacheobj Entry;
		UNICODE_STRING DriverName = RTL_CONSTANT_STRING(L"iqvw64e.sys");
		Entry.DriverName = DriverName;
		Entry.TimeDateStamp = 0x5284EAC3;
		ExAcquireResourceExclusiveLite(PiDDBLock, TRUE);
		PIDCacheobj* pFoundEntry = (PIDCacheobj*)RtlLookupElementGenericTableAvl(PiDDBCacheTable, &Entry);

		if (pFoundEntry == NULL)
		{
			DbgPrint("pFoundEntry not found !!!\n");
			// release ddb resource lock
			ExReleaseResourceLite(PiDDBLock);
			return FALSE;
		}
		else
		{
			DbgPrint("Found iqvw64e.sys in PiDDBCachetable!!\n");
			//unlink from list
			RemoveEntryList(&pFoundEntry->List);
			RtlDeleteElementGenericTableAvl(PiDDBCacheTable, pFoundEntry);
			// release the ddb resource lock
			ExReleaseResourceLite(PiDDBLock);
			DbgPrint("Clear success and finish !!!\n");
			return TRUE;
		}

	}
}



PMM_UNLOADED_DRIVER MmUnloadedDrivers;
PULONG				MmLastUnloadedDriver;
UCHAR MmUnloadedDriverSig[] = "\x48\x8B\x05\x00\x00\x00\x00\x48\x8D\x1C\xD0";


BOOLEAN isMmUnloadedDriversFilled()
{
	PMM_UNLOADED_DRIVER entry;
	for (ULONG Index = 0; Index < MM_UNLOADED_DRIVERS_SIZE; ++Index)
	{
		entry = &MmUnloadedDrivers[Index];
		if (entry->Name.Buffer == NULL || entry->Name.Length == 0 || entry->Name.MaximumLength == 0)
		{
			return FALSE;
		}

	}
	return TRUE;
}




BOOLEAN IsUnloadedDriverEntryEmpty(
	_In_ PMM_UNLOADED_DRIVER Entry
)
{
	if (Entry->Name.MaximumLength == 0 ||
		Entry->Name.Length == 0 ||
		Entry->Name.Buffer == NULL)
	{
		return TRUE;
	}

	return FALSE;
}

UNICODE_STRING DriverName = RTL_CONSTANT_STRING(L"iqvw64e.sys");
UNICODE_STRING NewDriverName = RTL_CONSTANT_STRING(L"bwrtyff.sys");




NTSTATUS findMMunloadedDrivers()
{
	PVOID MmUnloadedDriversPtr = NULL;

	NTSTATUS status = BBScanSection("PAGE", MmUnloadedDriverSig, 0x00, sizeof(MmUnloadedDriverSig) - 1, (PVOID*)(&MmUnloadedDriversPtr));
	if (!NT_SUCCESS(status)) {
		DbgPrint("Unable to find MmUnloadedDriver sig %p\n", MmUnloadedDriversPtr);
		return FALSE;
	}
	DbgPrint("MmUnloadedDriversPtr address found: %p  \n", MmUnloadedDriversPtr);


	MmUnloadedDrivers = *(PMM_UNLOADED_DRIVER*)ResolveRelativeAddress(MmUnloadedDriversPtr, 3, 7);
	//REAL REAL mmunloadeddrivers
	DbgPrint("MmUnloadedDrivers real location is: %p\n", &MmUnloadedDrivers);

	return status;
}

BOOLEAN cleanUnloadedDriverString()
{
	findMMunloadedDrivers();
	BOOLEAN cleared = FALSE;
	BOOLEAN Filled = isMmUnloadedDriversFilled();



	for (ULONG Index = 0; Index < MM_UNLOADED_DRIVERS_SIZE; ++Index)
	{


		PMM_UNLOADED_DRIVER Entry = &MmUnloadedDrivers[Index];

		if (RtlCompareUnicodeString(&DriverName, &Entry->Name, TRUE))
		{
			//random 7 letter name
			RtlCopyUnicodeString(&Entry->Name, &NewDriverName);
			Entry->UnloadTime = MmUnloadedDrivers[Index - 1].UnloadTime + 100;	

			DbgPrint("DONE randomizing name inside CleanUnloadedDriverString\n");
			return TRUE;
		}
	}
	DbgPrint("cannot find iqvw64e.sys!!!!111 cleanunloadeddriverstring fail!!1111\n");

	return FALSE;
}



