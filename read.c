/*Non-Canonical Input Processing*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#define BAUDRATE B9600
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FLAG 0x7E
#define A 0x03
#define C_SET 0x03
#define C_UA 0x07
#define C_RR 0x05
#define C_REJ 0x01
#define C_DISC 0x0B
#define FALSE 0
#define TRUE 1

#define CHUNK_SIZE 1024

int fd, res, c = 0, n = 0;
unsigned int fileSize;
char readBuf[1], writeBuf[255], chunkBuf[CHUNK_SIZE+5], fileName[255]; //TEMP FIX: GUARDA BCC2
FILE * pFile;

void sendControlFrame(int c)
{
	writeBuf[0]=FLAG;
    writeBuf[1]=A;
    writeBuf[2]=c;
    writeBuf[3]=writeBuf[1]^writeBuf[2];
    writeBuf[4]=FLAG;
	printf("SENDING COMMAND 0x%x\n", c);
	write(fd, writeBuf, 5);
}

void setConnection()
{
	printf("AWAITING CONNECTION\n");
	if(readFrame() != C_SET)
	{
		printf("\tSET FRAME MALFORMED, QUITING\n");
		exit(1);
	}
	else
	{
		sendControlFrame(C_UA);
		printf("CONNECTION ESTABLISHED\n");
	}
}

int readFrame()
{
    printf("WAITING FOR FRAME\n");
    return frameStateMachine(); //estado inicial
}

int frameStateMachine()
{
	int i = 0, bcc2 = 0, state = 0;
    unsigned char readByte, readC;
    while(1)
    {
		read(fd, readBuf, 1);
		readByte = readBuf[0];
		switch(state)
		{
		case 0: //FLAG
			if(readByte == FLAG)
				state=1;
			break;
		case 1: //A
			if(readByte == A)
				state=2;
			else if(readByte != FLAG)
				state = 0;
			break;
		case 2: //C
			readC = readByte;
			if(readC == C_SET || readC == C_UA || readC == C_DISC || readC == 0 || readC == 1)
				state = 3;
			else if(readByte == FLAG)
				state = 1;
			else
				state = 0;
			break;
		case 3: // BCC1
			if(readByte == A ^ readC)
				state = 4;
			else if(readByte == FLAG)
							state = 1;
					else
							state = 0;
			break;
		case 4:
			if(readByte == FLAG)
			{
				if(readC == 0 || readC == 1)
                {
					if(readC == c) //NOT DUPLICATE
					{
                        if(chunkBuf[i-1] == bcc2)
                        {
                            c = !c;
                            packetStateMachine();
                        }
                        else
                            return -1; //reject
					}
                }
				return readC;
			}
			else
			{
				//printf("\t%x - %c\n", readByte, readByte);
				//printf("I: %i\n", i);
				chunkBuf[i] = readByte; //construir chunk (para escrita no ficheiro)
				if(i>0)
					bcc2^=chunkBuf[i-1]; //to avoid reading BCC2
				i++;
			}
			break;
		}
    }
}

int packetStateMachine()
{
    printf("GOT DATA FRAME\n");
    unsigned char c = chunkBuf[0];
    switch(c)
    {
        case 0:
        {
            printf("\tDATA PACKET\n");
            if(chunkBuf[1]==n)
            {
                unsigned char l2 = chunkBuf[2], l1 = chunkBuf[3];
                int k = 256*l2+l1;
                //printf("\tK -> %i\n", k);
                fwrite(chunkBuf+4, sizeof(char), k, pFile); //escreve para ficheiro
                if(n==255)
                    n=0;
                else
                    n++;
            }
            break;
        }
        case 1:
        {
            printf("\tSTART PACKET\n");
            unsigned char t1 = chunkBuf[1], l1 = chunkBuf[2], i, tmp = l1;
            for(i=3;i<l1+3;++i)
                fileSize |= (((unsigned char)chunkBuf[i]) << (8*--tmp));
                    
            unsigned char t2 = chunkBuf[i++], l2 = chunkBuf[i++], j=i;
            for(;i<l2+j;i++)
                fileName[i-j]=chunkBuf[i];
            
            //printf("\t%s\n", fileName);
            
            break;
        }
        case 2:
        {
            printf("\tEND PACKET\n");
            if(fileSize != ftell(pFile)) //compara tamanho do ficheiro do pacote inicial com o realmente escrito para o ficheiro
                printf("\nERROR TRANSFERING: WRONG FILE SIZE\n");
            
            break;
        }
    }
    return 0;
}

int main(int argc, char** argv)
{
    struct termios oldtio,newtio;

    if ( (argc < 2) || 
  	     ((strcmp("/dev/ttyS0", argv[1])!=0) && 
  	      (strcmp("/dev/ttyS1", argv[1])!=0) )) {
      printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
      exit(1);
    }


  /*
    Open serial port device for reading and writing and not as controlling tty
    because we don't want to get killed if linenoise sends CTRL-C.
  */
  
    
    fd = open(argv[1], O_RDWR | O_NOCTTY );
    if (fd <0) {perror(argv[1]); exit(-1); }

    if ( tcgetattr(fd,&oldtio) == -1) { /* save current port settings */
      perror("tcgetattr");
      exit(-1);
    }

    bzero(&newtio, sizeof(newtio));
    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    /* set input mode (non-canonical, no echo,...) */
    newtio.c_lflag = 0;

    newtio.c_cc[VTIME]    = 0;   /* inter-character timer unused */
    newtio.c_cc[VMIN]     = 1;   /* blocking read until 5 chars received */



  /* 
    VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a 
    leitura do(s) próximo(s) caracter(es)
  */



    tcflush(fd, TCIOFLUSH);

    if ( tcsetattr(fd,TCSANOW,&newtio) == -1) {
      perror("tcsetattr");
      exit(-1);
    }

    printf("New termios structure set\n");
	
	
	//Open file for writing
	pFile = fopen ( "tmp" , "wb" );
	if (pFile==NULL) {fputs ("File error",stderr); exit (1);}
	
	setConnection();
	
	//sleep(1);
	
	while(1)
	{
		int readC = readFrame();
		if(readC == C_DISC)
		{
			printf("DISCONNECT REQUEST RECEIVED\n"); //FALTAM CENAS
			sendControlFrame(C_DISC);//FALTAM TIMEOUTS E O ADDRESS ESTÁ MAL!!
			if(readFrame() == C_UA)
				break;
		}
		else if(readC == -1) //invalid bcc2
		{
			printf("\nINVALID DATA, SENDING REJECT\n");
			sendControlFrame(C_REJ ^ c);
		}
        else
			sendControlFrame(C_RR ^ c);
	}
	printf("\nALL DONE\n");
	sleep(1);

    tcsetattr(fd,TCSANOW,&oldtio);
    close(fd);
	fclose(pFile);
    rename("tmp", fileName); //rename file
    return 0;
}
