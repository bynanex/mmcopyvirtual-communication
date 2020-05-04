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


VOID DriverUnload()
{
	
}




void DriverLoop()
{
	/*		initialize basic stuff			*/
	LARGE_INTEGER interval;
	interval.QuadPart = -30000000;




	/*			initialize PID			*/
	HANDLE CommmunicationProcessID = GetProcessPID();




	/*		get PEPROCESS		*/
	PEPROCESS clientProcess;
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
	DbgPrint("baseaddress is: %p", clientBaseAddress);
	




	/*		Prepare for reading memory		*/
	DWORD64 structOffset = 0x56C8;
	communicationStruct localBuffer;
	localBuffer.Signature = 0;
	KeDelayExecutionThread(KernelMode, FALSE, &interval);
	interval.QuadPart = -10000;
	int firstConnection = 1;
	SIZE_T bytes;




	DWORD64 structLocation = READ<DWORD64>(clientBaseAddress + structOffset, clientProcess);

	apexHaaax haaax(1);
	bool aimbot = false;

	while (1)
	{
		//locate struct, read from usermode buffer
		
		localBuffer = READ<communicationStruct>(structLocation, clientProcess);



		DbgPrint("command from Usermode is: %i \n", localBuffer.Signature);
		//check if everything is OK by sending test message. if signature is "1" then OK



		if (firstConnection == 1)
		{	
			communicationStruct testStruct;
			strcpy(testStruct.message, "driver found process");
			testStruct.Signature = 4;
			Write<communicationStruct>(structLocation, testStruct, clientProcess);
			firstConnection = 0;
		}


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
				ObDereferenceObject(clientProcess);
				DriverUnload();
				return;
			default:
				break;
			}
		}

		if (aimbot)
		{
			haaax.Aimbot();
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



	UNREFERENCED_PARAMETER(DriverObject);
	UNREFERENCED_PARAMETER(RegistryPath);
	DriverLoop();
	return Status;
}