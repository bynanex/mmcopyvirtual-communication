#pragma once
#include "ProcessUtilities.h"



//playerAimData [61] is going to be localplayer

struct playerAimData
{
	float xyz[3];
	float viewAngles[2];
	int realNumOfPlayers;
	bool dataArrived;
	bool KernelDataArrived;
};
playerAimData* playerArray;



HANDLE	GetProcessPIDapex()
{
	auto				Status = STATUS_SUCCESS;
	UNICODE_STRING		ClientName = { 0 };
	HANDLE				processID = nullptr;
	PVOID				SystemProcessInfo = nullptr;
	DWORD				buffer_size = NULL;



	RtlInitUnicodeString(&ClientName, L"r5apex.exe");
	Status = ZwQuerySystemInformation(SystemProcessInformation, SystemProcessInfo, 0, &buffer_size);
	while (Status == STATUS_INFO_LENGTH_MISMATCH)
	{
		if (SystemProcessInfo)	ExFreePool(SystemProcessInfo);
		SystemProcessInfo = ExAllocatePool(NonPagedPool, buffer_size);
		Status = ZwQuerySystemInformation(SystemProcessInformation, SystemProcessInfo, buffer_size, &buffer_size);
	}
	auto	ProcessInformation = static_cast<PSYSTEM_PROCESS_INFORMATION>(SystemProcessInfo);
	for (;;)
	{

		if (FsRtlIsNameInExpression(&ClientName, &(ProcessInformation->ImageName), FALSE, NULL) == TRUE)
		{
			processID = ProcessInformation->ProcessId;
			break;
		}

		if (ProcessInformation->NextEntryOffset == 0)
		{
			break;
		}
		ProcessInformation = (PSYSTEM_PROCESS_INFORMATION)(((PUCHAR)ProcessInformation) + ProcessInformation->NextEntryOffset);
	}
	ExFreePool(SystemProcessInfo);
	return processID;

}





DWORD64 clientBaseAddress;
PEPROCESS clientProcess;
PEPROCESS	gameProcess;
class apexHaaax
{
public:

	DWORD64		OFFSET_ENTITYLIST = 0x1897F38;		// Will be sig scanned
	DWORD64		OFFSET_LOCALPLAYER = 1231312313;	//needs fix 
	DWORD64		OFFSET_GLOW_ENABLE = 0x390;
	DWORD64		OFFSET_GLOW_CONTEXT = 0x310;
	DWORD64 	OFFSET_GLOW_RANGE = 0x2FC;
	DWORD64		OFFSET_GLOW_COLORS = 0x1D0;
	DWORD64		OFFSET_GLOW_DURATION = 0x2D0;
	DWORD64		OFFSET_HEALTH = 0x3E0;
	DWORD64		OFFSET_TEAMNUM = 0x999; // needs fix
	DWORD64		OFFSET_VIEWANGLES = 0x23D0;
	DWORD64		OFFSET_XYZLOCATION = 12321; //NEEDS FIX



	HANDLE		ProcessID;
	DWORD64		BaseAddress;





	apexHaaax(int i)
	{
		//Entitylist Sig		7F 24 B8 FE 3F 00 00 48 8D 15 ? ? ? ? 2B C1
		//Localplayer Sig		48 8D 0D ? ? ? ? 48 8B D7 FF 50 58
		UCHAR EntityList_Sig[] = "\x7F\x24\xB8\xFE\x3F\x00\x00\x48\x8D\x15\xCC\xCC\xCC\xCC\x2B\xC1";
		UCHAR LocalPlayer_Sig[] = "\x48\x8D\x0D\xCC\xCC\xCC\xCC\x48\x8B\xD7\xFF\x50\x58";

		LARGE_INTEGER Timeout;
		Timeout.QuadPart = -10000000;
		KeDelayExecutionThread(KernelMode, FALSE, &Timeout);


		/*	get PID r5apex		*/
		ProcessID = GetProcessPIDapex();
		NTSTATUS Status;






		/*-------------------Get PEPROCESS--------------------------*/

		Status = PsLookupProcessByProcessId((HANDLE)ProcessID, &gameProcess);
		BOOLEAN isWow64 = (PsGetProcessWow64Process(gameProcess) != NULL) ? TRUE : FALSE;
		DbgPrint("status PEPROCESS is: %i \n", Status);






		/*-------------------Get Base Address--------------------------*/
		UNICODE_STRING programImage;
		RtlInitUnicodeString(&programImage, L"r5apex.exe");




		KAPC_STATE apc;
		KeStackAttachProcess(gameProcess, &apc);





		BaseAddress = (ULONG64)GetUserModule(gameProcess, &programImage, isWow64); //BSOD problem line
		DbgPrint("Base Address is: %p \n", BaseAddress);



		/*--------------- IMPORTANT INFO: ppFound in Bbscansection is location of the beginning of the Sig !!! Add some bytes to get to pointer, add some bytes to get to offset*/
		/*----------------scan entitylist here	--------------------------------------*/
		ULONG64 Entity1 = NULL;
		BBScanSection("safdah", EntityList_Sig, 0xCC, sizeof(EntityList_Sig) - 1, reinterpret_cast<PVOID*>(&Entity1), (PVOID64)BaseAddress);
		KeDelayExecutionThread(KernelMode, FALSE, &Timeout);
		OFFSET_ENTITYLIST = (ULONG64)ResolveRelativeAddress((PVOID)Entity1, 10, 14);
		OFFSET_ENTITYLIST -= BaseAddress;
		DbgPrint("Entitylist offset is: %p \n", OFFSET_ENTITYLIST);


		/*----------------scan localplayer here-----------*/
		ULONG localPlayerAddr = NULL;
		BBScanSection("apexgay", EntityList_Sig, 0xCC, sizeof(LocalPlayer_Sig) - 1, reinterpret_cast<PVOID*>(&localPlayerAddr), (PVOID64)BaseAddress);
		KeDelayExecutionThread(KernelMode, FALSE, &Timeout);
		OFFSET_LOCALPLAYER = (ULONG64)ResolveRelativeAddress((PVOID)localPlayerAddr, 10, 14);
		OFFSET_LOCALPLAYER -= BaseAddress;
		DbgPrint("localplayer offset is: %p \n", OFFSET_LOCALPLAYER);



		KeUnstackDetachProcess(&apc);


		playerArray = (playerAimData*)ExAllocatePool(PagedPool, sizeof(playerAimData) * 61); // space for 61 ents



	}// constructor will scan for sigs




	void	enableGlow()
	{
		DWORD64 entAddress;

		// PEPROCESS should already be found

		for (int iii = 0; iii < 60; ++iii)
		{
			entAddress = READ<ULONG64>(BaseAddress + OFFSET_ENTITYLIST + (iii << 5), gameProcess);


			if (entAddress > 0)
			{
				Write<bool>(entAddress + OFFSET_GLOW_ENABLE, true, gameProcess);
				Write<int>(entAddress + OFFSET_GLOW_CONTEXT, 1, gameProcess);
				Write<float>(entAddress + OFFSET_GLOW_COLORS, 0.f, gameProcess);
				Write<float>(entAddress + OFFSET_GLOW_COLORS + 0x4, 0.f, gameProcess);
				Write<float>(entAddress + OFFSET_GLOW_COLORS + 0x8, 255.f, gameProcess);

				for (int offset = 0x2D0; offset <= 0x2E8; offset += 0x4) //beginning of glow is what i find in glow xref - 0x18, or -24 
				{
					Write<float>(entAddress + offset, FLT_MAX, gameProcess);
				}
				Write<float>(entAddress + OFFSET_GLOW_RANGE, FLT_MAX, gameProcess);  //Write glow range
			}


		}
	}



	void	disableGlow();



	void Aimbot()	// represents ONE iteration of aimbot
	{
		// PEPROCESS should already be found
		DWORD64 entAddress;

		static int i = 0;
		static int j = 0;
		int health;
		int teamNum;
		int localTeamNum;
		DWORD64 localPlayerAddress = READ<DWORD64>(BaseAddress + OFFSET_LOCALPLAYER, gameProcess);




		while (i < 60)
		{

			entAddress = READ<DWORD64>(BaseAddress + OFFSET_ENTITYLIST + (i << 5), gameProcess);
			health = READ<int>(entAddress + OFFSET_HEALTH, gameProcess);


			if (health > 0)
			{
				teamNum = READ<int>(entAddress + OFFSET_TEAMNUM, gameProcess);
				localTeamNum = READ<int>(localPlayerAddress + OFFSET_TEAMNUM, gameProcess);

				if (teamNum != localTeamNum)
				{

					playerArray[j].xyz[0] = READ<float>(entAddress + OFFSET_XYZLOCATION, gameProcess);
					playerArray[j].xyz[1] = READ<float>(entAddress + OFFSET_XYZLOCATION + 0X4, gameProcess);
					playerArray[j].xyz[2] = READ<float>(entAddress + OFFSET_XYZLOCATION + 0X8, gameProcess);

					j += 1;
				}
			}


			i += 1;
		}

		playerArray[0].realNumOfPlayers = j;
		playerArray[0].dataArrived = 1;
		playerArray[j + 1].xyz[0] = READ<float>(localPlayerAddress + OFFSET_XYZLOCATION, gameProcess);		//get localplayer locatin
		playerArray[j + 1].xyz[1] = READ<float>(localPlayerAddress + OFFSET_XYZLOCATION + 0X4, gameProcess);
		playerArray[j + 1].xyz[2] = READ<float>(localPlayerAddress + OFFSET_XYZLOCATION + 0X8, gameProcess);

		//56D8 is playerArray Offset
		DWORD	playerArrayAddress = READ<DWORD>(clientBaseAddress + 0x56D8, clientProcess);


		for (int i = 0; i < j; ++i)
		{
			Write<playerAimData>(playerArrayAddress + i * sizeof(playerAimData), clientProcess);
		}
	}

};			//			--------------------------------	CLASS END HERE		------------------------------------------------------







