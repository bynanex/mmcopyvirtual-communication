#include "apexHaaax.h"
#include "ProcessUtilities.h"
#include "PiddbMmunload.h"





struct communicationStruct
{
	int Signature; // specify the type of command
	DWORD ProcessID;
	DWORD64 Address;
	DWORD intValue;
	float floatValue;
	bool dataArrived;
	char message[30];
};





/*-------------------------------------Things that are freed, unloaded or dereferenced--------------------------*/
/*
	1. process list			- freed outside
	2. player array			- freed inside
	3. module info array	- freed outside
	4. apex legends process	- freed inside
	5. client process		- freed inside

*/

PEPROCESS clientProcess;
bool aimbotHasbeenTriggered;
VOID DriverUnload()
{
	DbgPrint("unload\n");
	if (aimbotHasbeenTriggered)
	{
		ExFreePool(playerArray);
		DbgPrint("freed array of players\n");
	}
	if (gameProcess)
	{
		ObDereferenceObject(gameProcess);
		DbgPrint("dereferenced game process\n");
	}
	if (clientProcess)
	{
		ObDereferenceObject(clientProcess);
		DbgPrint("dereferenced client process\n");
	}
}




void DriverLoop()
{
	/*		initialize basic stuff			*/
	LARGE_INTEGER interval;
	interval.QuadPart = -30000000;




	/*			initialize PID			*/
	HANDLE CommmunicationProcessID = GetProcessPID();




	/*		get PEPROCESS		*/
	PsLookupProcessByProcessId(CommmunicationProcessID, &clientProcess);
	BOOLEAN isWow64 = (PsGetProcessWow64Process(clientProcess) != NULL) ? TRUE : FALSE;




	/*		get base address		*/	
	KAPC_STATE apc;
	DWORD64 clientBaseAddress;
	UNICODE_STRING processName;
	RtlInitUnicodeString(&processName, L"UserModeClient.exe");
	KeStackAttachProcess(clientProcess, &apc);
	clientBaseAddress = (ULONG64)GetUserModule(clientProcess, &processName, isWow64);
	KeUnstackDetachProcess(&apc);
	DbgPrint("baseaddress is: %p \n", clientBaseAddress);
	




	/*		Prepare for reading memory		*/
	DWORD64 structOffset = 0x56C8;			//	56C8
	communicationStruct localBuffer;
	localBuffer.Signature = 0;
	KeDelayExecutionThread(KernelMode, FALSE, &interval);
	interval.QuadPart = -7000000;	//0.7 seconds
	int firstConnection = 1;
	SIZE_T bytes;









	/*		First connection		*/


	DWORD64 structLocation = READ<DWORD64>(clientBaseAddress + structOffset, clientProcess);

	localBuffer =	 READ<communicationStruct>(structLocation, clientProcess);

	communicationStruct testStruct;

	strcpy(testStruct.message, "driver found process");
	testStruct.Signature =	4;
	Write<communicationStruct>(structLocation, testStruct, clientProcess);

	DbgPrint("Client process communication starts\n");





	/*--		Check if Apex is open		--*/

	while (1)
	{
		localBuffer = READ<communicationStruct>(structLocation, clientProcess);
		KeDelayExecutionThread(KernelMode, TRUE, &interval);
		if ((localBuffer.ProcessID != 0)		&&		(localBuffer.dataArrived == true))
		{
			DbgPrint("found r5apex process ID: %i\n", localBuffer.ProcessID);
			break;
		}
	}
	
	LARGE_INTEGER delayTime;
	delayTime.QuadPart = -20000000;		//2 seconds
	KeDelayExecutionThread(KernelMode, TRUE, &delayTime);


	apexHaaax haaax(1);
	bool aimbot = false;

	while (1)
	{
		//locate struct, read from usermode buffer

		localBuffer = READ<communicationStruct>(structLocation, clientProcess);



		DbgPrint("command from Usermode is: %i \n", localBuffer.Signature);
		//check if everything is OK by sending test message. if signature is "1" then OK

		if (localBuffer.dataArrived == false || (localBuffer.Signature == 0 && localBuffer.dataArrived == true)) // nothing
		{
			
			DbgPrint("0 command \n");
		}



		if (localBuffer.dataArrived == true)
		{
			switch (localBuffer.Signature)
			{
			case 1:
				DbgPrint("found command: signature 1\n");
				DbgPrint("message retrieved: ");
				DbgPrint(localBuffer.message);
				DbgPrint("\n");
				break;
			case 2:
				// EMPTY COMMAND
				DbgPrint("Read Request \n");
				break;
			case 3:
				// EMPTY COMMAND
				DbgPrint("Write Request\n");
				break;
			case 4:
				haaax.enableGlow();		//Only player glow for now
				break;
			case 5:
				aimbot = !aimbot;
				break;
			case 6:
				DriverUnload();
				return;
			default:
				break;
			}
		}

		if (aimbot)
		{
			haaax.Aimbot();
			aimbotHasbeenTriggered = true;
		}

		if (localBuffer.dataArrived == 1)
		{
			localBuffer.dataArrived = 0;
			Write<communicationStruct>(structLocation, localBuffer, clientProcess);
		}

	}

}




extern "C"
NTSTATUS DriverEntry(_In_ _DRIVER_OBJECT *DriverObject, _In_ PUNICODE_STRING RegistryPath)
{
	NTSTATUS Status = STATUS_SUCCESS;
	UNREFERENCED_PARAMETER(DriverObject);
	UNREFERENCED_PARAMETER(RegistryPath);
	DriverLoop();
	return Status;
}




NTSTATUS DriverA(_In_ _DRIVER_OBJECT* DriverObject, _In_ PUNICODE_STRING RegistryPath)
{
	DbgPrint("driver start \n");
	NTSTATUS Status = STATUS_SUCCESS;
	BOOLEAN ClearStatus;



	ClearStatus = ClearPiddbCacheTable();
	if (ClearStatus == FALSE)
	{
		DbgPrint("PiDDB clear fail ! ! !\n");
		return Status;
	}


	ClearStatus = cleanUnloadedDriverString();
	if (ClearStatus == FALSE)
	{
		DbgPrint("MMunload clear fail ! ! !\n");
		return Status;
	}
	DbgPrint("finished clearing mmunload and piddbcachetable \n");


	UNREFERENCED_PARAMETER(DriverObject);
	UNREFERENCED_PARAMETER(RegistryPath);
	DriverEntry(NULL, NULL);
	return Status;
}