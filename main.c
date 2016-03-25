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

int main() {
    int rlen, tlen, i;
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
    char fullText[18] = "EE is my avocation";
    int fullLen = strlen(fullText); // Obtain length of fullText1 array
    // !! strlen does not include the null terminator
    
    // Obtain the data packet array
    char info[42];
    int blocks;
    int packets;
    int third;
    int test = 0;
    
    // Initialize the array
    int z;
    for (z=0; z<42; z++){
        info[z] = 0;
    }
    
    int byte=0;
    for(blocks=0;blocks<6;blocks++){   
        int bbc = 6;    // byte bit counter
        
        for(packets=0; packets <7; packets++){
            for(third=2; third >=0; third--){
                test = GetBit(fullText[byte], bbc);
                if(test==1)
                    SetBit(info[packets+blocks*7], third);
                else
                    ClearBit(info[packets+blocks*7], third);
                bbc--;
                if(bbc==-1){
                    bbc=6;
                    byte++;
                }
            }
        }
    }
    
    // Create the codeword matrix ---------------------------------------------
    char codeword[42];
    int c;
    for (c=0;c<42;c++){
        codeword[c] = generateCodeword(info[c]);
    }
  
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
                setsockopt(StreamSock, SOL_SOCKET, SO_SNDBUF, (char*) &tlen, sizeof (int));
            }
        } else {
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
                
                if (rbfr[1] == 76) // L Transmit of a Linear Block Code
                {
                    // this is where you decode the receive
                    int r;
                    for(r=2; r<44; r++){
                        int s;
                        s = getSyndrome(rbfr[r]);
                    }
                }

                if (rbfr[1] == 84) //T transfer
                {
                    tlen = 44; // length of codeword array + 2 start bytes + null
                    tbfr[0] = 00;
                    tbfr[1] = 76;   // Send a message
                    
                    int j;
                    // Copy the long text into the transmit buffer
                    for (j = 0; j < tlen-2; j++) {
                        tbfr[j+2] = codeword[j];
                    }

                    send(StreamSock, tbfr, tlen + 1, 0);
                    DelayMsec(delayT);                 
                }
                mPORTDClearBits(BIT_0); // LED1=0

                if (rbfr[1] == 85) //U User1
                {
                    // I have to declare this shit up here because this version 
                    // of C doesn't like me declaring variables in the for loop
                    int j;

                    mPORTDSetBits(BIT_1); // LED2=1
                    tlen = fullLen;

                    // Copy the long text into the transmit buffer
                    for (j = 0; j < tlen; j++) {
                        tbfr[j] = fullText[j];
                    }

                    send(StreamSock, tbfr, tlen, 0);
                    DelayMsec(delayT);
                    mPORTDClearBits(BIT_1); // LED3=0
                }

                if (rbfr[1] == 86) //V User2
                {
                    mPORTDSetBits(BIT_1); // LED2=1
                    DelayMsec(1000);
                    mPORTDClearBits(BIT_1); // LED2=1
                }

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

// CREATING LINEAR BLOCK CODE JAMS

char generateCodeword(char p){
    int i;
    char bitVal;
    char x = 0; // implicit zero-stuffing
    
    char Gb0 = 13;  // 00001101
    char Gb1 = 23;  // 00010111
    char Gb2 = 38;  // 00100110
    // 6 because of the (6,3) LBC
    for (i=0; i<6; i++){
        bitVal = ((GetBit(p,2))&(GetBit(Gb2,i)))^((GetBit(p,1))&(GetBit(Gb1,i)))^((GetBit(p,0))&(GetBit(Gb0,i)));
        
        if(bitVal == 1)
            SetBit(x,i);
        // else if bitVal == 0, the bit is already clear
    }
    
    return x;
}


char getSyndrome(char r){
    int bitVal2, bitVal1, bitVal0;
    char z = 0;
    char Hb2 = 60;
    char Hb1 = 50;
    char Hb0 = 25;
    
    bitVal2 = (GetBit(Hb2,5)&GetBit(r,5))^(GetBit(Hb2,4)&GetBit(r,4))^(GetBit(Hb2,3)&GetBit(r,3))^(GetBit(Hb2,2)&GetBit(r,2))^(GetBit(Hb2,1)&GetBit(r,1))^(GetBit(Hb2,0)&GetBit(r,0));
    bitVal1 = (GetBit(Hb1,5)&GetBit(r,5))^(GetBit(Hb1,4)&GetBit(r,4))^(GetBit(Hb1,3)&GetBit(r,3))^(GetBit(Hb1,2)&GetBit(r,2))^(GetBit(Hb1,1)&GetBit(r,1))^(GetBit(Hb1,0)&GetBit(r,0));
    bitVal0 = (GetBit(Hb0,5)&GetBit(r,5))^(GetBit(Hb0,4)&GetBit(r,4))^(GetBit(Hb0,3)&GetBit(r,3))^(GetBit(Hb0,2)&GetBit(r,2))^(GetBit(Hb0,1)&GetBit(r,1))^(GetBit(Hb0,0)&GetBit(r,0));
    
    if(bitVal2==1)
        SetBit(z,2);
    if(bitVal1==1)
        SetBit(z,1);
    if(bitVal0==0)
        SetBit(z,0);
    
    return z;
}
