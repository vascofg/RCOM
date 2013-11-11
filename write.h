#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>

#define CHUNK_SIZE 100

int llopen(char *porta);
void sendFile(int fd, unsigned int fileSize, char *fileName, char *fileContent);
void sendControlPacket(int fd, int packetC, unsigned int fileSize, char *fileName);
void sendDataPacket(int fd, char *packet, int dataChars, unsigned char n);
int openFile(char *fileName, char **fileContent);
int llclose(int fd);
int llwrite(int fd, char * buffer, int length);
