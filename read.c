/*Non-Canonical Input Processing*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>

#define BAUDRATE 5
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

int fd, res, c = 0;
char readBuf[255], writeBuf[255];

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
    res = read(fd,readBuf,1);
	printf("READ PACKET\n");
    if(readBuf[0]==FLAG) //FLAG
	{
		res=read(fd,readBuf,1);
		if(readBuf[0]==A) //ADDRESS
		{
			res=read(fd,readBuf,1);
			int readC = readBuf[0]; //C
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
    return 0;
}
