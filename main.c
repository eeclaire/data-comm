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
void generateParity(char text[], int len);
int compareParity(char packet[], int plen); // return number of detected errors

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

    // Declare the arrays for the full text to be transmitted
    char fullText1[7] = "claire";
    char fullText2[7] = "durand";
    char fullText3[7] = "aaabbb";
    char fullText4[7] = "cccddd";
    char fullText5[7] = "eeefff";
    int fullLen = strlen(fullText1) + 1; // Obtain length of fullText1 array

    // CREATE PARITY MATRICES HERE
    generateParity(fullText1, fullLen);
    generateParity(fullText2, fullLen);
    generateParity(fullText3, fullLen);
    generateParity(fullText4, fullLen);
    generateParity(fullText5, fullLen);

    // Form the full output transmission
    char fullText[36];
    int t;
    for (t = 0; t < fullLen; t++) {
        fullText[t] = fullText1[t];
        fullText[t + fullLen] = fullText2[t];
        fullText[t + (2 * fullLen)] = fullText3[t];
        fullText[t + (3 * fullLen)] = fullText4[t];
        fullText[t + (4 * fullLen)] = fullText5[t];
    }
    fullText[35] = 0x00;


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
                //setsockopt(StreamSock, IPPROTO_TCP, TCP_NODELAY, (char*) &tlen, sizeof (int));
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
                } else {
                    // Blink lights for mood
                    mPORTDSetBits(BIT_0); // LED2=1
                    DelayMsec(1000);
                    mPORTDClearBits(BIT_0); // LED2=1
                    mPORTDSetBits(BIT_1); // LED2=1
                    DelayMsec(1000);
                    mPORTDClearBits(BIT_1); // LED2=1
                    mPORTDSetBits(BIT_2); // LED2=1
                    DelayMsec(1000);
                    mPORTDClearBits(BIT_2); // LED2=1

                    char rcvdTxt1[8];
                    char rcvdTxt2[8];
                    char rcvdTxt3[8];
                    char rcvdTxt4[8];
                    char rcvdTxt5[8];
                    char colRcvdPar[8];

                    // Break up datagram into packets
                    int r;
                    for (r = 0; r < 7; r++) {
                        rcvdTxt1[r] = rbfr[r];
                        rcvdTxt2[r] = rbfr[r + 7];
                        rcvdTxt3[r] = rbfr[r + 14];
                        rcvdTxt4[r] = rbfr[r + 21];
                        rcvdTxt5[r] = rbfr[r + 28];
                        //colRcvdPar[r] = rbfr[r + 35];    
                    }

                    int err1, err2, err3, err4, err5;

                    err1 = compareParity(rcvdTxt1, 7);
                    err2 = compareParity(rcvdTxt2, 7);
                    err3 = compareParity(rcvdTxt3, 7);
                    err4 = compareParity(rcvdTxt4, 7);
                    err5 = compareParity(rcvdTxt5, 7);

                    // Generate new parity and XOR it with rcvd parity
                    // Re-generate + compare parities
                    
                    if(err1 > 0 || err2 > 0 || err3 > 0 || err4 > 0 || err5>0 ){
                        send(StreamSock, tbfr, tlen + 1, 0);
                    }

                    mPORTDSetBits(BIT_0); // LED2=1
                    DelayMsec(1000);
                    mPORTDClearBits(BIT_0); // LED2=1
                    mPORTDSetBits(BIT_1); // LED2=1
                    DelayMsec(1000);
                    mPORTDClearBits(BIT_1); // LED2=1
                }

                if (rbfr[1] == 84) //T transfer
                {
                    int j;

                    mPORTDSetBits(BIT_1); // LED2=1
                    tlen = strlen(fullText);

                    // Copy the long text into the transmit buffer
                    for (j = 0; j < tlen; j++) {
                        tbfr[j] = fullText[j];
                    }

                    send(StreamSock, tbfr, tlen + 1, 0);
                    DelayMsec(delayT);
                    mPORTDClearBits(BIT_1); // LED3=0                  
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
                        tbfr[j] = fullText1[j];
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

// CREATING PARITY MATRIX JAMS

void generateParity(char text[], int len) {
    // Declare index variables for the byte and bit 
    int by, bi;
    int colParity = 0;
    int rowParity;

    // Obtain parity matrix
    for (by = 0; by < len - 1; by++) {
        // Shift every byte by 1 to make room for parity
        text[by] = text[by] << 1;
        rowParity = 0; // Reset rowParity for each byte

        // iterate through the bits of each byte (skipping least sign bit)
        for (bi = 1; bi < 8; bi++) {
            // Delay added because MPLABX can't handle this
            DelayMsec(10);
            if (GetBit(text[by], bi)) {
                rowParity++;
                FlipBit(text[6], bi); // Actually colParity
            } // else bit is not set, do nothing
            DelayMsec(10);
        } // end of bits for loop

        // End of byte, calculate and set the parity bit
        if (rowParity % 2 == 0) { // even parity 
            ClearBit(text[by], 0);
        } else { // odd parity
            SetBit(text[by], 0);
            colParity++;
        }
    } // end of bytes for loop

    if (colParity % 2 == 0) { // even parity     
        ClearBit(text[6], 0);
    } else { // odd parity
        SetBit(text[6], 0);
    }
    // END OF CREATING PARITY MATRIX JAMS  
}


// 

int compareParity(char packet[], int plen) {
    // Declare index variables for the byte and bit 
    int by, bi;
    int error = 0; // Assume no error
    int rowNewP = 0;
    char colNewP = 0x00;

    char p1 = packet[0];
    char p2 = packet[1];
    char p3 = packet[2];
    char p4 = packet[3];
    char p5 = packet[4];
    char p6 = packet[5];
    char cp = packet[6];

    // Obtain parity matrix
    for (by = 0; by < plen - 1; by++) {
        rowNewP = 0; // Reset rowParity for each byte

        // iterate through the bits of each byte (skipping least sign bit)
        for (bi = 0; bi < 8; bi++) {
            // Delay added because MPLABX can't handle this
            DelayMsec(10);
            if (GetBit(packet[by], bi)) {
                if (bi != 0) {
                    rowNewP++;
                }
                FlipBit(colNewP, bi); // Actually colParity
            } // else bit is not set, do nothing
            DelayMsec(10);
        } // end of bits for loop

        if (bi > 0) {
            // End of byte, calculate and set the parity bit
            // if newP is even and old is odd OR newP is odd and old is even
            if (((rowNewP % 2 == 0) && (GetBit(packet[by], 0))) || ((rowNewP % 2 == 1) && !(GetBit(packet[by], 0)))) {
                // Should record the coordinates 
                error++;
            } else { // odd parity
                // SUCCESSFUL ROW
            }
        }
    } // end of bytes for loop

    if ((colNewP ^ packet[6]) == 0x00) {
        // We're Gucci
    } else {
        // ERROR. Need to loop through to find x coordinate
        error++;
    }

    return error;
}