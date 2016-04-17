
//	PIC32 Server - Microchip BSD stack socket API
//	MPLAB X C32 Compiler     PIC32MX795F512L
//      Microchip DM320004 Ethernet Starter Board

// 	ECE4532    Dennis Silage, PhD
//	main.c
//	version 1-27-14

//      Starter Board Resources:
//		LED1 (RED)	RD0
//		LED2 (YELLOW)	RD1
//		LED3 (GREEN)	RD2
//		SW1		RD6
//		SW2		RD7
//		SW3		RD13

//	Control Messages
//		02 start of message
//		03 end of message

#include <string.h>
#include <stdio.h>
#include <time.h>
#include <plib.h>		// PIC32 Peripheral library functions and macros
#include "tcpip_bsd_config.h"	// in \source
#include <TCPIP-BSD\tcpip_bsd.h>
#include "hardware_profile.h"
#include "system_services.h"
#include "display_services.h"
#include "mstimer.h"

#define PC_SERVER_IP_ADDR "192.168.2.105"  // IP address of the server
#define SYS_FREQ (80000000)

// Stolen from the Internet to make bit manipulation easier
#define GetBit(var, bit) ((var & (1 << bit)) != 0) // Returns true / false if bit is set
#define SetBit(var, bit) (var |= (1 << bit))
#define ClearBit(var, bit) (var &= ~(1 << bit))
#define FlipBit(var, bit) (var ^= (1 << bit))

void DelayMsec(unsigned int);
char generateCodeword(char p);
char getSyndrome(char r);
char retrieveInfo(char r);

void buildTransmitPacket(char packet[], int plen, int seq, char srcPackets[4][16]);
int generateFCS(char data[]);

int main() {
    srand(time(NULL));
    
    int rlen, tlen;
    SOCKET srvr, StreamSock = INVALID_SOCKET;
    IP_ADDR curr_ip;
    static BYTE rbfr[100]; // receive data buffer
    static BYTE tbfr[1500]; // transmit data buffer

    struct sockaddr_in addr;
    int addrlen = sizeof (struct sockaddr_in);
    unsigned int sys_clk, pb_clk;

    // Variable delay time in ms
    int delayT = 0;

    // Declare the arrays for the full text to be transmitted -----------------
    char fullText[128] = "aaaaaaaaaaaaaaaabbbbbbbbbbbbbbbbccccccccccccccccddddddddddddddddeeeeeeeeeeeeeeeeffffffffffffffffgggggggggggggggghhhhhhhhhhhhhhhh";
    int fullLen = strlen(fullText); // Obtain length of fullText1 array

    // STOP AND WAIT "CONSTANTS"
    char packet[20];
    int plen = 16;
    int P = 4;
    
    // STOP AND WAIT VARIABLES
    int sendWait = 0;
    int currentP = 1;
    int rcvdSeqs = 0;
    int ACK = 0;
    int sentSeqs = 0;
    char sendPackets[4][16];
    char fullSendPackets[4][20];
    char rcvdPackets[4][20];
    char rcvdData[128];
    int sendIndex[4] = {0,0,0,0};
    
    // Initialize because I'm not convinced I'm not going insane
    memset(fullSendPackets[0], 0, 20);
    memset(fullSendPackets[1], 0, 20);
    memset(fullSendPackets[2], 0, 20);
    memset(fullSendPackets[3], 0, 20);
    memset(rcvdPackets[0], 0, 20);
    memset(rcvdPackets[1], 0, 20);
    memset(rcvdPackets[2], 0, 20);
    memset(rcvdPackets[3], 0, 20);
    memset(rcvdData, 0, 128);
            
    
    // LED setup
    mPORTDSetPinsDigitalOut(BIT_0 | BIT_1 | BIT_2); // RD0, RD1 and RD2 as outputs
    mPORTDClearBits(BIT_0 | BIT_1 | BIT_2);
    // switch setup			
    mPORTDSetPinsDigitalIn(BIT_6 | BIT_7 | BIT_13); // RD6, RD7, RD13 as inputs

    mPORTDSetBits(BIT_0); // Test that the board is running the code by turning on a light
    // system clock			
    sys_clk = GetSystemClock();
    pb_clk = SYSTEMConfigWaitStatesAndPB(sys_clk);
    // interrupts enabled
    INTEnableSystemMultiVectoredInt();
    // system clock enabled
    SystemTickInit(sys_clk, TICKS_PER_SECOND);

    // initialize TCP/IP
    TCPIPSetDefaultAddr(DEFAULT_IP_ADDR, DEFAULT_IP_MASK, DEFAULT_IP_GATEWAY,
            DEFAULT_MAC_ADDR);
    if (!TCPIPInit(sys_clk))
        return -1;
    DHCPInit();

    // create TCP server socket
    if ((srvr = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == SOCKET_ERROR) {
        return -1;
    }
    // bind to a local port
    addr.sin_port = 6653;
    addr.sin_addr.S_un.S_addr = IP_ADDR_ANY;
    if (bind(srvr, (struct sockaddr*) &addr, addrlen) == SOCKET_ERROR)
        return -1;
    listen(srvr, 5);

    while (1) {
        IP_ADDR ip;
        TCPIPProcess();
        DHCPTask();

        ip.Val = TCPIPGetIPAddr();
        if (curr_ip.Val != ip.Val) // DHCP server change IP address?
            curr_ip.Val = ip.Val;

        // TCP Server Code
        if (StreamSock == INVALID_SOCKET) {
            StreamSock = accept(srvr, (struct sockaddr*) &addr, &addrlen);

            // If we have a valid socket
            if (StreamSock != INVALID_SOCKET) {
                tlen = 1296; // send buffer size
                setsockopt(StreamSock, SOL_SOCKET, TCP_NODELAY, (char*) &tlen, sizeof (int));
            }
        } else {  
            //THIS IS WHERE IT ALL BREAKs
            // receive TCP data
            rlen = recvfrom(StreamSock, rbfr, sizeof (rbfr), 0, NULL, NULL);
            if (rlen > 0) {
                if (rbfr[0] == 2) // 02 start of message                               
                {
                    if (rbfr[1] == 71) //G global reset
                    {
                        mPORTDSetBits(BIT_0); // LED1=1
                        DelayMsec(50);
                        mPORTDClearBits(BIT_0); // LED1=0
                    }
                }

                if (rbfr[1] == 76) // Not a Linear Block Code, but I like L
                {
                    rcvdSeqs++;
                    if (rbfr[18] > ACK)
                        ACK = rbfr[18];
                    
                    int ok = generateFCS(rbfr);
                    
                    if(ok != 0){
                        tbfr[1] = 84;
                        tbfr[2] = 32 + ACK;   // 32 for 20h + ACK + 1 for next
                        tbfr[3] = 0;
                        send(StreamSock, tbfr, 4, 0);
                        DelayMsec(delayT);
                        // Reset number of sequence numbers received
                        rcvdSeqs = 0;
                    }
                    
                    int j;
                    for(j=0;j<16;j++){
                        rcvdPackets[(rbfr[18]-1)%P][j] = rbfr[j+2];
                        rcvdData[(rbfr[18]-1)*16+j] = rcvdPackets[(rbfr[18]-1)%P][j];
                    }
                    
                    //if((rbfr[18]-1)%P == 0){
                    if(rcvdSeqs%P == 0 && ok == 0){
                        DelayMsec(delayT);
                        // Send ACK for all okay
                        tbfr[1] = 84;
                        tbfr[2] = 50 + ACK;   // 49 for 30h + ACK + 1 for next
                        tbfr[3] = 0;
                        send(StreamSock, tbfr, 4, 0);
                        DelayMsec(delayT);
                        // Reset number of sequence numbers received
                        rcvdSeqs = 0;
                        // Maybe save some of this text in a master rcvd array 
                    }   
                    int checkRBFR = 1;
                }

                if (rbfr[1] == 84) //T transfer
                {
                    if (rbfr[2] == 49 + currentP){
                        sendWait = 0;
                        int wegud = 1;
                    }
                    
                    DelayMsec(delayT);
                    
                    // 2 bytes for header, 16 for data, 1 for seq #, 1 for FCS, 1 for null
                    tlen = plen + 5;
                    
                    if((currentP-1)%P==0){ 
                        int p,q;
                        for (p=0;p<4;p++){
                            
                            int s = rand()%P;
                            while(sendIndex[s] != 0){
                                s = rand()%P;
                            }
                            sendIndex[s] = 1;
                            
                            DelayMsec(delayT);
                            fullSendPackets[s][0] = 0;
                            fullSendPackets[s][1] = 76;
                            fullSendPackets[s][18] = currentP+p;
                            fullSendPackets[s][19] = 0;
                            
                            DelayMsec(delayT);
                            
                            for (q=0;q<16;q++){
                                fullSendPackets[s][q+2] = fullText[((currentP-1)+p)*16];
                            } 
                            
                            fullSendPackets[s][19] = generateFCS(fullSendPackets[s]);
                        } 
                        int l;
                        for(l=0; l<4; l++){
                            sendIndex[l] = 0;
                        }
                        int zeroed = 0;
                    }
                    
                    int m;
                    for(m=0; m<20; m++){
                        tbfr[m] = fullSendPackets[(currentP-1)%4][m];
                    }

                    if (sendWait == 0){
                        send(StreamSock, tbfr, tlen, 0);
                        DelayMsec(delayT);
                        currentP++;
                    }
                     
                }
                mPORTDClearBits(BIT_0); // LED1=0
            } else if (rlen < 0) {
                closesocket(StreamSock);
                StreamSock = SOCKET_ERROR;
            }
        }
    } // end while(1)
} // end

// DelayMsec( )   software millisecond delay

void DelayMsec(unsigned int msec) {
    unsigned int tWait, tStart;
    tWait = (SYS_FREQ / 2000) * msec;
    tStart = ReadCoreTimer();
    while ((ReadCoreTimer() - tStart) < tWait);
}

void buildTransmitPacket(char packet[], int plen, int seq, char srcPackets[4][16]){
    packet[0] = 0;
    packet[1] = 76;
    
    // Fill the 16 bytes of data
    int i;
    for(i=0; i<16; i++){
        // Note that the 4 here is for P
        packet[i+2] = srcPackets[(seq-1)%4][i]; //65 + seq;
    }
    
    // Set the sequence number
    packet[18] = seq;
    
    // Add the null terminator
    packet[19] = 0;
   
}

int generateFCS(char data[]){
    int divisor = 263;
  
    int start = data[0];
    int second = data[1];
    int last = data[19];
    int dividend = data[0]*256 + data[1];
    
    int r = dividend%divisor;
    
    int i;
    for(i=2; i<20; i++){
        dividend = r*256 + data[i]; 
        r = dividend % divisor;
    }
    
    dividend = r * 256;
    r = dividend % divisor;
    return r;
}
