#pragma once

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <math.h>

#define BAUDRATE B9600
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FLAG 0x7E
#define A 0x03
#define READER_A 0x01
#define C_SET 0x03
#define C_UA 0x07
#define C_RR 0x05
#define C_REJ 0x01
#define C_DISC 0x0B
#define FALSE 0
#define TRUE 1

#define MAX_RETRIES 3 /* Numero de envios adicionais a efectuar em caso de nao haver resposta */
#define CHUNK_SIZE 100 

int	conta = 0, //contador para o alarm
	c=0, // campo de controlo a alternar entre 0 e 2
	globalFD, // para passar para signal handler
	writeBufLen,
	numTramas=0,
	numTimeouts=0,
	numRejects=0;
	
struct termios oldtio; //para restaurar definições da porta série no final

char writeBuf[CHUNK_SIZE*2+10], readBuf[1]; //pior caso possível, vão ser enviados o dobro dos caracteres após stuffing


int stuffBytes(unsigned int numChars);
void sendFrame();
void sendControlFrame(int fd, int c);
int setConnection(char * porta, int user);
int readData(int fd, char * buffer);
int readFrame(int fd, char * buffer, int *res);
int disconnect(int fd, int user);
int sendData(int fd, char *packet, int packetChars);
