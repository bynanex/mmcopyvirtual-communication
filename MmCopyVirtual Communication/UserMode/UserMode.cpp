#include <iostream>
#include <Windows.h>
#include <thread>

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
//2 for RPM
//3 for WPM
//4 for test receive


communicationStruct* sharedMem;
/*      I have to create the pointer outside of CommandHandler() so i have an easy .data offset       */




int command;
int CommandHandler()
{
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
                sharedMem->dataArrived = true;
                sharedMem->Signature = command;
                cout << "posted aimbot signal" << endl;
                break;


            case 6:
                sharedMem->Signature = command;
                sharedMem->dataArrived = true;
                Sleep(4000);
                delete sharedMem;
                return;



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



int main()
{
    CommandHandler();
    return 0;
}
