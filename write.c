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

#define BAUDRATE B38400
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FLAG 0x7E
#define A 0x03
#define C_SET 0x03
#define C_UA 0x07
#define FALSE 0
#define TRUE 1

#define MAX_RETRIES 3 /* Número de envios adicionais a efectuar em caso de não haver resposta */

volatile int STOP=FALSE;

int fd, conta = 0, writeBufLen, res;
char writeBuf[255], readBuf[255];

void readResponse()
{
	res = read(fd,readBuf,1); //parado aqui
	printf("READ RESPONSE\n");
	alarm(0); //cancelar alarmes pendentes
	conta = 0; //reinicia contador de retries
    while(STOP==FALSE)
    {
        res = read(fd,readBuf,1);
        if(readBuf[0] == FLAG)
           STOP = TRUE;
    }
	STOP = FALSE;
}

void sendTrama()
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

void setConnection()
{
	writeBuf[0]=FLAG;
	writeBuf[1]=A;
	writeBuf[2]=C_SET;
	writeBuf[3]=writeBuf[1]^writeBuf[2];
	writeBuf[4]=FLAG;
	writeBufLen=5;
	printf("WRITING SET\n");
	
    (void) signal(SIGALRM, sendTrama);
    sendTrama();
	readResponse(); //aguarda OK
}

int main(int argc, char** argv)
{
    struct termios oldtio,newtio;
  
    int i, sum = 0, speed = 0;
    
    if (argc < 2){
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


   /* printf("WRITING PACKET\n");
    res = write(fd,buf,5);
    printf("%d bytes written\n", res);*/
    
	setConnection();
	
	//Trama de teste
	writeBuf[0]=FLAG;
	writeBuf[1]=A;
	writeBuf[2]=0; //o que mandar aqui?
	writeBuf[3]=writeBuf[1]^writeBuf[2];
	writeBuf[4]='O';
	writeBuf[5]='L';
	writeBuf[6]='A';
	writeBuf[7]=0; //XOR COM TODOS OS BYTES ENVIADOS
	writeBuf[8]=FLAG;
	writeBufLen = 9;
	
	printf("WRITING TRAMA\n");
	sendTrama();
	readResponse(); //aguarda OK
	
	printf("\nALL DONE\n");
	sleep(1);
    
    if ( tcsetattr(fd,TCSANOW,&oldtio) == -1) {
      perror("tcsetattr");
      exit(-1);
    }

    close(fd);
    return 0;
}

