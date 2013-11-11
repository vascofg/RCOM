#include "read.h"

//Maquina de estados para um trama de dados e respectivo pacote
int packetStateMachine(char *buffer)
{
    unsigned char c = buffer[0];
    switch(c)
    {
        case 0:
        {
            printf("\tDATA PACKET\n");
            if(buffer[1]==n) //se não for um pacote duplicado, grava para ficheiro
            {
                unsigned char l2 = buffer[2], l1 = buffer[3];
                int k = 256*l2+l1;
                fwrite(buffer+4, sizeof(char), k, pFile); //escreve para ficheiro
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
            unsigned char t1 = buffer[1], l1 = buffer[2], i, tmp = l1;
            for(i=3;i<l1+3;++i)
                fileSize |= (((unsigned char)buffer[i]) << (8*--tmp));
                    
            unsigned char t2 = buffer[i++], l2 = buffer[i++], j=i;
            for(;i<l2+j;i++)
                fileName[i-j]=buffer[i];
            
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

int llopen(char *porta)
{
	return setConnection(porta, 1); //reader
}


int llclose(int fd)
{
	return disconnect(fd, 1); //1 - reader
}

int llread(int fd, char *buffer)
{
	return readData(fd, buffer);
}

int main(int argc, char** argv)
{
	if (argc < 2){
      printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
      exit(1);
    }
	//Open file for writing
	pFile = fopen ( "tmp" , "wb" );
	if (pFile==NULL) {fputs ("File error",stderr); exit (1);}
	
	int fd = llopen(argv[1]);
	if(fd<0)
		return -1;
		
	char chunkBuf[CHUNK_SIZE+5];
	
	while(1)
	{
		int res = llread(fd, chunkBuf);
		if(res == 0) //disconnect request received
		{
			if(llclose(fd) < 0)
				return -1;
			break;
		}
		else if(res != -1) //not invalidBCC
			packetStateMachine(chunkBuf);
	}
	printf("\nALL DONE\n");
	//sleep(1);

	fclose(pFile);
    rename("tmp", fileName); //rename file
    return 0;
}
