#include "write.h"

//Trata de iniciar a conexao entre o transmitter e o receiver
int llopen(char *porta)
{
	return setConnection(porta, 0); //0 - writer
}

//Trata do envio de dados, no devido formato 
void sendFile(int fd, unsigned int fileSize, char *fileName, char *fileContent)
{
	unsigned int i, n=0;
	//pacote de controlo start
	sendControlPacket(fd, 1, fileSize, fileName);
	
	char chunkBuf[CHUNK_SIZE+4]; //tamanho do chunk a enviar mais cabeçalho
	
	for(i=0;i<(fileSize/CHUNK_SIZE);i++) //whole chunks
	{
		memcpy(chunkBuf+4, &fileContent[i*CHUNK_SIZE], CHUNK_SIZE); //ADD 4 to accommodate packet information
		sendDataPacket(fd, chunkBuf, CHUNK_SIZE, n);
		if(n==255)
			n=0;
		else
			n++;
	}
	
	//last bytes
	if(fileSize%CHUNK_SIZE>0)
	{
		memcpy(chunkBuf+4, &fileContent[i*CHUNK_SIZE], fileSize%CHUNK_SIZE); //ADD 4 to accommodate packet information
		sendDataPacket(fd, chunkBuf, fileSize%CHUNK_SIZE, n);
	}
    
    //pacote de controlo end
	sendControlPacket(fd, 2, fileSize, fileName);
}

//Constroi e envia trama de dados com um pacote de controlo
void sendControlPacket(int fd, int packetC, unsigned int fileSize, char *fileName)
{
	char controlPacket[255];
	controlPacket[0]=packetC; //C
	controlPacket[1]=0; //T1 - tamanho do ficheiro
	int i=3, currentByte, significant = 0;
	
	for(currentByte=sizeof(fileSize);currentByte>0;currentByte--) //percorre inteiro byte a byte do mais significativo para o menos
	{
		unsigned char byte = (fileSize >> (currentByte-1)*8) & 0xff; //passa bytes individuais(octetos) para controlPacket
		
		if(!significant) //enquanto forem não significativos
			if(byte!=0) //se byte actual não for zero
				significant = 1; //todos os seguintes são significativos
		if(significant) //para todos os bytes significativos, enviar
			controlPacket[i++]=byte; //V1
	}
	
	controlPacket[2]=i-3; //L1
	controlPacket[i++] = 1; //T2 - nome do ficheiro
	controlPacket[i++] = strlen(fileName); //L2
	
	int j;	
	for(j=0;j<strlen(fileName);j++) //escreve caracteres individuais do nome do ficheiro
		controlPacket[i++] = fileName[j]; //V2
		
	printf("WRITING CONTROL PACKET\n");
	llwrite(fd, controlPacket, i);
}

//Envia trama de dados com um pacote de dados
void sendDataPacket(int fd, char *packet, int dataChars, unsigned char n)
{
	//cabeçalho do pacote
	packet[0]=0;
	packet[1]=n;
	packet[2]=dataChars/256;
	packet[3]=dataChars%256;
    printf("WRITING DATA PACKET\n");
	llwrite(fd, packet, dataChars+4); //por causa do cabeçalho
}

int llwrite(int fd, char * buffer, int length)
{
	return sendData(fd, buffer, length);
}

//Abre ficheiro para leitura em binario, devolvendo o tamanho em bytes do ficheiro
int openFile(char *fileName, char **fileContent)
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
	*fileContent = (char*) malloc (sizeof(char)*lSize);
	if (*fileContent == NULL) {fputs ("Memory error",stderr); exit (2);}

	// copy the file into the buffer:
	result = fread (*fileContent,1,lSize,pFile);
	if (result != lSize) {fputs ("Reading error",stderr); exit (3);}

	/* the whole file is now loaded in the memory buffer. */

	// terminate
	fclose (pFile);
	return lSize;
}

int llclose(int fd)
{
	return disconnect(fd, 0); //0 - writer
}

int main(int argc, char** argv)
{
	if (argc < 3){
      printf("Usage:\tnserial SerialPort FileName\n\tex: nserial /dev/ttyS1 text.txt\n");
      exit(1);
    }
	
	unsigned int fileSize;
	char *fileContent, fileName[255];
	
	
	strcpy(fileName, argv[2]);
	
	fileSize = openFile(fileName, &fileContent);
	
	printf("FILESIZE: %i\n", fileSize);
	
	int fd = llopen(argv[1]);
	if(fd<0)
		return -1;
	sendFile(fd, fileSize, fileName, fileContent);
	
	printf("\nALL DONE\n");
	
	if(llclose(fd) < 0)
		return -1;
	
    return 0;
}

