
#include <iostream>
#include <winsock2.h>
#include <chrono>
#include <thread>
#include <vector>
#include <sstream>


using namespace std;

#pragma comment(lib,"ws2_32.lib") 
#pragma warning(disable:4996) 

#include <HD/hd.h>
#include <HDU/hduError.h>
#include <HDU/hduVector.h>

#define SERVER "127.0.0.1"  // or "localhost" - ip address of UDP server
#define BUFLEN 512  // max length of answer
#define PORT 12312  // the port on which to listen for incoming data

// Teleoperation variables
bool isTeleoperationOn = true;
std::vector<double> master_disp = { 0.0, 0.0, 0.0 };
std::vector<double> master_orient = { 0.0, 0.0, 0.0 };
int button_click = 0;
bool recenter = false;

HDCallbackCode HDCALLBACK master_interface(void* data)
{
    HDErrorInfo error;
    hduVector3Dd position;
    hduVector3Dd rotation;
    hduVector3Dd button;
    HDdouble trans[16];
    hduVector3Dd angles;
    
    //hduVector3Dd force;

    HHD hHD = hdGetCurrentDevice();

    hdBeginFrame(hHD);
    hdGetDoublev(HD_CURRENT_POSITION, position);
    hdGetDoublev(HD_CURRENT_TRANSFORM, trans);
    hdGetDoublev(HD_CURRENT_GIMBAL_ANGLES, angles);
    hdGetDoublev(HD_CURRENT_BUTTONS, button);

    double th_x, th_y, th_z;
    th_x = atan2(trans[6], trans[10]);
    th_y = atan2(-trans[2], sqrt(trans[6] * trans[6] + trans[10] * trans[10]));
    th_z = atan2(trans[1], trans[0]);

    master_disp[0] = position[2] * 0.001;
    master_disp[1] = position[0] * 0.001;
    master_disp[2] = position[1] * 0.001;


    //master_orient[0] = th_x;
    //master_orient[1] = th_z;
    //master_orient[2] = -th_y;

    master_orient[0] = angles[0];
    master_orient[1] = angles[1];
    master_orient[2] = angles[2];

    button_click = (int)button[0];
    
    HDfloat baseForce[3];
    float force_multiplier = 0.02;
    float center[3] = { 0.0, 20.0, 40.0 };
    //recenter = !button_click;
    if (recenter == true)
    {
        baseForce[0] = (center[0] - position[0]) * force_multiplier;
        baseForce[1] = (center[1] - position[1]) * force_multiplier;
        baseForce[2] = (center[2] - position[2]) * force_multiplier;
        hdSetFloatv(HD_CURRENT_FORCE, baseForce);
    }
    else
    {
        baseForce[0] = 0;
        baseForce[1] = 0;
        baseForce[2] = 0;
        hdSetFloatv(HD_CURRENT_FORCE, baseForce);
    }
    

    //hdSetDoublev(HD_CURRENT_FORCE, force); // Force rendering example
    hdEndFrame(hHD);


    if (HD_DEVICE_ERROR(error = hdGetError()))
    {
        hduPrintError(stderr, &error, "Error detected while rendering gravity well\n");

        if (hduIsSchedulerError(&error))
        {
            return HD_CALLBACK_DONE;
        }
    }

    return HD_CALLBACK_CONTINUE;
}

int main()
{

    // Haptic Device

    // Initializing haptic device
    HDErrorInfo error_hapt;
    HDSchedulerHandle hMasterSite;


    HHD hHD = hdInitDevice(HD_DEFAULT_DEVICE);
    if (HD_DEVICE_ERROR(error_hapt = hdGetError()))
    {
        hduPrintError(stderr, &error_hapt, "Failed to initialize haptic device");
        fprintf(stderr, "\nPress any key to quit.\n");
        system("pause");
        return -1;
    }

    printf("Found haptic device model: %s.\n\n", hdGetString(HD_DEVICE_MODEL_TYPE));

    /* Schedule the main callback that will render forces to the device. */
    hMasterSite = hdScheduleAsynchronous(master_interface, 0, HD_MAX_SCHEDULER_PRIORITY);

    hdEnable(HD_FORCE_OUTPUT);
    hdStartScheduler();

    /* Check for errors and abort if so. */
    if (HD_DEVICE_ERROR(error_hapt = hdGetError()))
    {
        hduPrintError(stderr, &error_hapt, "Failed to start scheduler");
        fprintf(stderr, "\nPress any key to quit.\n");
        return -1;
    }

    system("title UDP Client");

    // initialise winsock
    WSADATA ws;
    printf("Initialising Winsock...");
    if (WSAStartup(MAKEWORD(2, 2), &ws) != 0)
    {
        printf("Failed. Error Code: %d", WSAGetLastError());
        return 1;
    }
    printf("Initialised.\n");

    // create socket
    sockaddr_in server;
    int client_socket;
    if ((client_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == SOCKET_ERROR) // <<< UDP socket
    {
        printf("socket() failed with error code: %d", WSAGetLastError());
        return 2;
    }

    // setup address structure
    memset((char*)&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.S_un.S_addr = inet_addr(SERVER);

    // start communication
    while (true)
    {
        //char message[BUFLEN];
        //cin.getline(message, BUFLEN);
        std::stringstream msg_strea;
        msg_strea << "{\"pos_orin\":[" << master_disp[0] << ", "
            << master_disp[1] << ", " << master_disp[2] << ", "
            << master_orient[0] << ", " << master_orient[1] << ", "
            << master_orient[2] << ", " << button_click << "]}";

        string msg_str = msg_strea.str();

        // send the message
        if (sendto(client_socket, msg_str.c_str(), strlen(msg_str.c_str()), 0, (sockaddr*)&server, sizeof(sockaddr_in)) == SOCKET_ERROR)
        {
            printf("sendto() failed with error code: %d", WSAGetLastError());
            return 3;
        }

        this_thread::sleep_for(std::chrono::milliseconds(20));

        
         //receive a reply and print it
         //clear the answer by filling null, it might have previously received data
        char answer[BUFLEN] = {};

        // try to receive some data, this is a blocking call
        int slen = sizeof(sockaddr_in);
        int answer_length;
        /*if (answer_length = recvfrom(client_socket, answer, BUFLEN, 0, (sockaddr*)&server, &slen) == SOCKET_ERROR)
        {
            printf("recvfrom() failed with error code: %d", WSAGetLastError());
            exit(0);
        }*/
        answer_length = recvfrom(client_socket, answer, BUFLEN, 0, (sockaddr*)&server, &slen);
        
        //cout << answer << "\n";
        if (strcmp(answer, "Push") == 0)
        {
            //cout << answer << "\n";
            recenter = false;
        }
        else {
            recenter = true;
        }

        
    }

    closesocket(client_socket);
    WSACleanup();
}