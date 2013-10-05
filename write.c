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
#define MODEMDEVICE "/dev/ttyS4"
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FLAG 0x7E
#define A 0x03
#define C_SET 0x03
#define C_UA 0x07
#define FALSE 0
#define TRUE 1

volatile int STOP=FALSE;

void send_trama(int fd, char buf[255])
{
    int res;
    printf("WRITING PACKET\n");
    res = write(fd,buf,strlen(buf)+1);
    printf("%d bytes written\n", res);
}

int fd, flag = 0, conta = 0;

void send_set()
{
    if(!flag)
    {
        char buf[255];
        buf[0]=FLAG;
        buf[1]=A;
        buf[2]=C_SET;
        buf[3]=A^C_SET;
        buf[4]=FLAG;
        printf("WRITING SET\n");
        write(fd, buf, 5);
        conta++;
        if(conta < 4)
            alarm(3);
    }
}

int main(int argc, char** argv)
{
    int c, res;
    struct termios oldtio,newtio;
    char buf[255], buf2[255];
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
    
    (void) signal(SIGALRM, send_set);
    send_set();
    printf("READING UA\n");
    res = read(fd,buf2,1); //parado aqui
    flag = 1;
    printf("%x\n", buf2[0]);
    while(STOP==FALSE)
    {
        res = read(fd,buf2,1);
        printf("%x\n", buf2[0]);
        if(buf2[0] == FLAG)
           STOP = TRUE;
    }
    
    if ( tcsetattr(fd,TCSANOW,&oldtio) == -1) {
      perror("tcsetattr");
      exit(-1);
    }



    close(fd);
    return 0;
}

