#pragma once

#include <stdio.h>
#include <stdlib.h>

#define CHUNK_SIZE 1024

int n = 0;

unsigned int fileSize;

char fileName[255];
FILE * pFile;

int packetStateMachine(char *buffer);
int llopen(char *porta);
int llclose(int fd);
int llread(int fd, char *buffer);