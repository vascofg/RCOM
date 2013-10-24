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

int fd, res, c = 0;
char readBuf[1], writeBuf[255], chunkBuf[CHUNK_SIZE+1]; //TEMP FIX: GUARDA BCC2
FILE * pFile;

void sendControlTrama(int c)
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
	if(readTrama() != C_SET)
	{
		printf("\tSET TRAMA MALFORMED, QUITING\n");
		exit(1);
	}
	else
	{
		sendControlTrama(C_UA);
		printf("CONNECTION ESTABLISHED\n");
	}
}

int readTrama()
{
    printf("WAITING FOR PACKET\n");
    //res = read(fd,readBuf,1);
    return stateMachine(0); //estado inicial
    /*printf("READ PACKET\n");
	printf("FLAG -> %x\n", readBuf[0]);
    if(readBuf[0]==FLAG) //FLAG
	{
		res=read(fd,readBuf,1);
		if(readBuf[0]==A) //ADDRESS
		{
			res=read(fd,readBuf,1);
			int readC = readBuf[0]; //C
			printf("Read C -> %i\n", readC);
			res=read(fd,readBuf,1);
			if(readBuf[0]==A^readC) //BCC1
			{
				while (1) {
					res=read(fd,readBuf,1);
					if(readBuf[0]!=FLAG)
						printf("\t%x - %c\n", readBuf[0], readBuf[0]); //FALTA VERIFICAÇÃO DO BCC2
					else
					{
						if(readC == c) //VALID INFORMATION TRAMA
							c = !c;
						return readC;
					}
				}
			}	
		}
		//printf("%x\n",readBuf[0]); //FLAG
	}
	return -1; //MALFORMED
	*/
	
}

int stateMachine(int state)
{
	int i = 0;
    char readByte, readC;
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
					if(readC == c) //VALID INFORMATION TRAMA
					{
						c = !c;
						fwrite(chunkBuf, sizeof(char), i, pFile); //ESTÁ A GUARDAR BCC2
					}
				return readC;
			}
			else
			{
				//printf("\t%x - %c\n", readByte, readByte);
				//printf("I: %i\n", i);
				chunkBuf[i++] = readByte; //construir chunk (para escrita no ficheiro)
			}
			break;
		}
    }
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
	
	
	pFile = fopen ( "tmp" , "wb" );
	if (pFile==NULL) {fputs ("File error",stderr); exit (1);}
	
	setConnection();
	
	//sleep(1);
	
	while(1)
	{
		int readC = readTrama();
		if(readC == !c) //JÁ TROCOU
			sendControlTrama(C_RR ^ c);
		else if(readC == C_DISC)
		{
			printf("\nDISCONNECT REQUEST RECEIVED\n"); //FALTAM CENAS
			sendControlTrama(C_DISC);//FALTAM TIMEOUTS E O ADDRESS ESTÁ MAL!!
			if(readTrama() == C_UA)
				break;
		}
		else
		{
			printf("\tDATA TRAMA MALFORMED, WAITING...");
			sendControlTrama(C_REJ ^ c);
		}
	}
	printf("\nALL DONE\n");
	sleep(1);

  /* 
    O ciclo WHILE deve ser alterado de modo a respeitar o indicado no guião 
  */

    tcsetattr(fd,TCSANOW,&oldtio);
    close(fd);
	fclose(pFile);
    return 0;
}
