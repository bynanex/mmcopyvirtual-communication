#include <iostream>
#include <Windows.h>
#include <thread>
#include <TlHelp32.h>
using namespace std;


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



//driver has to find the process


/*      COMMANDS/SIGNATURES FOR BUFFER      */    
//0 for no command
// 1 for test send
//2 for test RPM
//3 for test WPM
//4 for enable glow
// 5 for toggle aimbot
//6 for unload


communicationStruct* sharedMem;
/*      I have to create the pointer outside of CommandHandler() so i have an easy .data offset   (DO NOT CHANGE)    */


DWORD getProcId(const wchar_t* procName)
{
    DWORD procID = 0;
    HANDLE hSnap = (CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (hSnap != INVALID_HANDLE_VALUE)
    {
        PROCESSENTRY32 procEntry;
        procEntry.dwSize = sizeof(procEntry);
        if (Process32First(hSnap, &procEntry))
        {
            do
            {
                if (!_wcsicmp(procEntry.szExeFile, procName))
                {
                    procID = procEntry.th32ProcessID;
                    break;
                }

            } while (Process32Next(hSnap, &procEntry));
        }
    }
    CloseHandle(hSnap);
    return procID;
}



int command;
int CommandHandler()
{
    cout << "started commandHandler " << endl;


    sharedMem = new communicationStruct();
    sharedMem->Signature = 1;


    while (1)
    {
        Sleep(1500);
        cout << "waiting for driver... " << endl;
        if (strcmp(sharedMem->message, "driver found process") == 0 || sharedMem->Signature == 4)
        {
            cout << "driver found process" << endl;
            break;
        }
    }



    int apexProcessID;
    while (1)
    {
        apexProcessID = getProcId(L"r5apex.exe");
        if (apexProcessID != 0)
        {
            cout << "found process id of apex legends: " << apexProcessID << endl;
            sharedMem->ProcessID = apexProcessID;
            break;
        }
    }





    while (1)
    {
       
        std::cin >> command;
       
        
        switch (command)
        {
/*test*/    case 1: 
                sharedMem->Signature = command;
                sharedMem->dataArrived = true;
                strcpy(sharedMem->message, "Test");
                cout << "posted test command" << endl; // because memory isnt "sent" anywhere, it is posted in the buffer
                break;



/*READ*/    case 2: 
                sharedMem->Signature = command;
                sharedMem->Address = 0x000;  // set your own process here
                sharedMem->ProcessID = 9999;
                sharedMem->dataArrived = true;
                cout << "posted Read command" << endl;
                break;



/*WRITE*/   case 3:
                sharedMem->Signature = command;
                sharedMem->Address = 0x0000;
                sharedMem->dataArrived = true;
                sharedMem->ProcessID = 9999;
                cout << "posted Write command" << endl;
                break;


            case 4:         // enable GLOW COMMAND (because I am too lazy to do synchronization)
                sharedMem->dataArrived = true;
                sharedMem->Signature = command;
                //no process ID here because I get it from kernel mode
                //no address because I am too lazy to do synchronization
                cout << "posted glow command" << endl;
                break;

            
            case 5:          // TOGGLE AIMBOT COMMAND

                //for this, i cannot avoid synchronization
                sharedMem->dataArrived = true;
                sharedMem->Signature = command;
                cout << "posted aimbot signal" << endl;
                break;


            case 6:
                sharedMem->Signature = command;
                sharedMem->dataArrived = true;
                Sleep(4000);
                delete sharedMem;
                return 0;



            default:
            break;
        }


        /*command = 0;
        sharedMem->dataArrived = false;
        
        command and dataArrived are set from kernel
        
        */
      
        Sleep(50);
    }
    return 0;
}


void aimbotHandler()
{
    
}



int main()
{
    std::thread commandhandler(CommandHandler);
    aimbotHandler();
    commandhandler.join();
    return 0;
}
