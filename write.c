/*Non-Canonical Input Processing*/

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
#define C_SET 0x03
#define C_UA 0x07
#define C_RR 0x05
#define C_REJ 0x01
#define C_DISC 0x0B
#define FALSE 0
#define TRUE 1

#define MAX_RETRIES 3 /* N�mero de envios adicionais a efectuar em caso de n�o haver resposta */
#define CHUNK_SIZE 1024


int fd, conta = 0, writeBufLen, res, c=0;
unsigned int fileSize;
char writeBuf[CHUNK_SIZE+10], chunkBuf[CHUNK_SIZE+4], readBuf[255], *fileContent, fileName[255];

int readResponse()
{
	res = read(fd,readBuf,1); //parado aqui
    if(readBuf[0]==FLAG) //FLAG
	{
		res=read(fd,readBuf,1);
		if(readBuf[0]==A) //ADDRESS
		{
			res=read(fd,readBuf,1); //C
			int readC = readBuf[0];
			res=read(fd,readBuf,1);
			if(readBuf[0]==A^readC) //BCC1
			{
				res=read(fd,readBuf,1);
				if(readBuf[0]==FLAG) //FLAG
				{
					alarm(0); //cancelar alarmes pendentes (ATENÇÂO A RESPOSTAS INVÁLIDAS, NÃO ESPERADAS)
					conta = 0; //reinicia contador de retries
					return readC;
				}
			}
		}
	}
	return -1; //ERROR
}

void sendFrame()
{
	if(conta++ <= MAX_RETRIES)
	{
		write(fd, writeBuf, writeBufLen);
		printf("\tSENDING PACKET\n");
		alarm(3);
	}
	else
	{
		printf("CONNECTION LOST... GIVING UP\n");
		exit(1);
	}
}


void sendControlFrame(int c)
{
	writeBuf[0]=FLAG;
    writeBuf[1]=A;
    writeBuf[2]=c;
    writeBuf[3]=writeBuf[1]^writeBuf[2];
    writeBuf[4]=FLAG;
	writeBufLen=5;
	printf("SENDING COMMAND 0x%x\n", c);
	write(fd, writeBuf, writeBufLen);  
}

void disconnect()
{
	writeBuf[0]=FLAG;
	writeBuf[1]=A;
	writeBuf[2]=C_DISC;
	writeBuf[3]=writeBuf[1]^writeBuf[2];
	writeBuf[4]=FLAG;
	writeBufLen=5;
	printf("DISCONNECTING\n");
	
    (void) signal(SIGALRM, sendFrame);
    sendFrame();
	int readC = readResponse();
	if(readC != C_DISC)
		disconnect(); //wrong response, send disc again
	else
		sendControlFrame(C_UA);
}

void setConnection()
{
	writeBuf[0]=FLAG;
	writeBuf[1]=A;
	writeBuf[2]=C_SET;
	writeBuf[3]=writeBuf[1]^writeBuf[2];
	writeBuf[4]=FLAG;
	writeBufLen=5;
	printf("WRITING SET\n");
	
    (void) signal(SIGALRM, sendFrame);
    sendFrame();
	if(readResponse() != C_UA)
	{
		printf("\tWRONG SET RESPONSE, QUITING\n");
		exit(1);
	}
	else
		printf("CONNECTION ESTABLISHED!\n");
}

void sendData()
{
	unsigned int i, n=0;
	//pacote de controlo start
	sendControlPacket(1);
	
	for(i=0;i<(fileSize/CHUNK_SIZE);i++) //whole chunks
	{
		memcpy(chunkBuf+4, &fileContent[i*CHUNK_SIZE], CHUNK_SIZE); //ADD 4 to accommodate packet information
		sendDataPacket(chunkBuf, CHUNK_SIZE, n);
		if(n==255)
			n=0;
		else
			n++;
	}
	
	//last bytes
	if(fileSize%CHUNK_SIZE>0)
	{
		memcpy(chunkBuf+4, &fileContent[i*CHUNK_SIZE], fileSize%CHUNK_SIZE); //ADD 4 to accommodate packet information
		sendDataPacket(chunkBuf, fileSize%CHUNK_SIZE, n);
	}
    
    //pacote de controlo end
	sendControlPacket(2);
}

void sendControlPacket(int packetC)
{
	char controlPacket[255];
	controlPacket[0]=packetC; //C
	controlPacket[1]=0; //T1 - tamanho do ficheiro
	int i=3, currentByte, significant = 0;
	for(currentByte=sizeof(fileSize);currentByte>0;currentByte--)
	{
		unsigned char byte = (fileSize >> (currentByte-1)*8) & 0xff; //passa bytes individuais de fileSize pela ordem msb -> lsb para controlPacket
		if(!significant)
			if(byte!=0)
				significant = 1;
		if(significant)
			controlPacket[i++]=byte; //V1
	}
	controlPacket[2]=i-3; //L1
	controlPacket[i++] = 1; //T2 - nome do ficheiro
	controlPacket[i++] = strlen(fileName); //L2
	int j;
	for(j=0;j<strlen(fileName);j++)
		controlPacket[i++] = fileName[j]; //V2
		
	printf("WRITING CONTROL PACKET\n");
	sendDataFrame(controlPacket, i);
}

void sendDataPacket(char *packet, int numchars, unsigned char n)
{
	packet[0]=0;
	packet[1]=n;
	packet[2]=numchars/256;
	packet[3]=numchars%256;
    printf("WRITING DATA PACKET\n");
	sendDataFrame(packet, numchars+4);
}

void sendDataFrame(char *packet, int numchars)
{
	writeBuf[0]=FLAG;
	writeBuf[1]=A;
	writeBuf[2]=c;
	writeBuf[3]=writeBuf[1]^writeBuf[2];
	int i, bcc2 = 0;
	for(i=4;i<numchars+4;++i) //APPEND
	{
		//printf("chunkBuf[%i] -> %c\n", i-4, packet[i-4]);
		writeBuf[i] = packet[i-4];
		bcc2 ^= writeBuf[i];
	}
	writeBuf[i++]=bcc2; //XOR COM TODOS OS BYTES ENVIADOS
	writeBuf[i++]=FLAG;
	writeBufLen = i;
	
	printf("WRITING DATA FRAME\n");
	sendFrame();
	
	if(readResponse() != C_RR ^ !c) //aguarda RR
	{
		printf("\tREJECT! RESENDING!!!\n");
		sendDataFrame(packet, numchars); //reenvia
	}
	else
	{
		c = !c;
		printf("\tRECEIVER READY!\n");
	}
}

int openFile(char *fileName)
{
	FILE * pFile;
	long lSize;
	size_t result;

	pFile = fopen ( fileName , "rb" );
	if (pFile==NULL) {fputs ("File error",stderr); exit (1);}

	// obtain file size:
	fseek (pFile , 0 , SEEK_END);
	lSize = ftell (pFile);
	rewind (pFile);

	// allocate memory to contain the whole file:
	fileContent = (char*) malloc (sizeof(char)*lSize);
	if (fileContent == NULL) {fputs ("Memory error",stderr); exit (2);}

	// copy the file into the buffer:
	result = fread (fileContent,1,lSize,pFile);
	if (result != lSize) {fputs ("Reading error",stderr); exit (3);}

	/* the whole file is now loaded in the memory buffer. */

	// terminate
	fclose (pFile);
	return lSize;
}

int main(int argc, char** argv)
{
    struct termios oldtio,newtio;
  
    int sum = 0, speed = 0;
    
    if (argc < 3){
      printf("Usage:\tnserial SerialPort FileName\n\tex: nserial /dev/ttyS1 text.txt\n");
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
    leitura do(s) pr�ximo(s) caracter(es)
  */


    tcflush(fd, TCIOFLUSH);

    if ( tcsetattr(fd,TCSANOW,&newtio) == -1) {
      perror("tcsetattr");
      exit(-1);
    }

    printf("New termios structure set\n");

	strcpy(fileName, argv[2]);

	fileSize = openFile(fileName);
	
	printf("FILESIZE: %i\n", fileSize);
    
	(void) signal(SIGALRM, sendFrame);
	setConnection();
	
	//while(1)
		sendData();
	
	disconnect();
	
	printf("\nALL DONE\n");
	sleep(1);
    
    if ( tcsetattr(fd,TCSANOW,&oldtio) == -1) {
      perror("tcsetattr");
      exit(-1);
    }

    close(fd);
    return 0;
}

