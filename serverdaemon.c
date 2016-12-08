#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <pthread.h>
#include <string.h>
#include "accelerometer.h"
#include "gyroscope.h"
#include <termios.h>

#define LISTEN_BACKLOG 50 //limitando a 50 conexiones en background
#define MAXLENGHT  100 //100 bytes Max por trama
//Sensor a leer
#define ACELEROMETRO  0x01
#define MAGNETOMETRO  0x02
#define GIROSCOPIO 	  0x03
#define TODOS 		  0xFF
#define ERROR 		  0xFE

//Eje a leer
#define EJE_X  		  0x01
#define EJE_Y  		  0x02
#define EJE_Z 	  	  0x03
#define EJE_XYZ 	  0x04


typedef int8_t bool;
#define TRUE 			1
#define FALSE 			0

typedef struct tClient {
   uint8_t  SOF;
   uint8_t  Potencia;
   uint8_t  Angulo;
   uint8_t  CS;
} SCLIENTCOMMAND;

typedef struct tResponse {
   uint8_t  SOF;
   uint8_t  status;
   uint8_t  CS;
} SRESPONSECOMMAND;

#define WAIST 		0
#define SHOULDER	1
#define ELBOW 		2
#define WRIST 		3
#define GRIPPER 	4


#define FORWARD	 	0
#define BACKWARD	1

typedef struct tKinetis {
   uint8_t  SOF;
   uint8_t  leftPower;
   uint8_t  leftDir;
   uint8_t  rightPower;
   uint8_t  rightDir;
   uint8_t  CS;
} SKINETISCOMMAND;

/*UART*/
char *portname = "/dev/ttymxc5";
int  fdUart;

pthread_t gpThreadId;
pthread_mutex_t gpLock;

int set_interface_attribs (int fd, int speed, int parity);
void set_blocking (int fd, int should_block);
static void  *vfnClientThread(void* vpArgs);
uint8_t bfnChecksum(void *vpBlock, uint8_t bSize);
uint16_t wfnMaps(uint16_t wX, uint16_t wInMin, uint16_t wInMax, uint16_t wOutMin, uint16_t wOutMax);

int main(int argc, char *argv[])
{
	/*Se obtiene el puerto al que escuchará el servidor pasado por parametro*/
    int32_t dwPuerto = atoi(argv[1]);
    int32_t dwSocketFd;
    int32_t dwBindFd;
    int32_t dwListenFd;
    int32_t dwClient[LISTEN_BACKLOG];
	int8_t	bClientCounter = 0;

	 /*Init UART*/
    fdUart = open (portname, O_RDWR | O_NOCTTY | O_SYNC);
    if (fdUart < 0)
    {
            printf("\n Inicialización UART fallida!\n");
            exit(EXIT_FAILURE);
    }

    set_interface_attribs (fdUart, B115200, PARENB);  // set speed to 115,200 bps, 8n1 (even parity)
    set_blocking (fdUart, 1);                // set no blocking

	if (pthread_mutex_init(&gpLock, NULL) != 0)
	{
		printf("\n Inicialización mutex fallida!\n");
        exit(EXIT_FAILURE);
	}

    struct sockaddr_in socketOptions;
    socklen_t addrlen;

    printf("Abriendo Puerto = %i\n\n",dwPuerto);
	
	/*Creando un socket de tipo SOCK_STREAM e IPv4 fd guardado en la var */
    dwSocketFd = socket(AF_INET, SOCK_STREAM, 0);

    if (dwSocketFd < 0)
    {
       printf("Error en el socket \n");
       exit(EXIT_FAILURE);
    }
	
	printf("Socket Creado!\n");
	/*Configurnado Socket*/
    socketOptions.sin_family = AF_INET; //Familia AF_INET
    socketOptions.sin_port = htons(dwPuerto); // Numero de puerto
    socketOptions.sin_addr.s_addr = htons(INADDR_ANY); // direccion socket, escuchando en todas las interfases
	
	/*Nombrando al socket*/
    dwBindFd = bind(dwSocketFd,(struct sockaddr *)&socketOptions, sizeof(struct sockaddr_in));
	printf("Ruta del socket: %i \n\n",socketOptions.sin_addr.s_addr);
    
	if (dwBindFd < 0)
    {
       printf("Error del bind \n");
       exit(EXIT_FAILURE);
    }
	
	printf("Socket Asignado!\n");
   	/*Escucar conexiones en el socket*/
    dwListenFd = listen(dwSocketFd, LISTEN_BACKLOG);

    if(dwListenFd < 0)
    {
       printf("Error en el listen \n");
       exit(EXIT_FAILURE);
    }
	
	printf("Socket Listo para asignar Clientes!...\n");
    addrlen = sizeof(struct sockaddr_in);
	
    for(;;)
    {
		/*En espera de un Cliente*/
		printf("Esperando cliente...\n");    
     	dwClient[bClientCounter++] = accept(dwSocketFd,(struct sockaddr *)&socketOptions,&addrlen);
		printf("cliente coonectado creando hilo de conexión...\n");
		/*Creando Hilo vfnClientThread mandandole por parametro el socket del Cliente para recibir los mensajes del usuario conectado*/
     	pthread_create(&gpThreadId,NULL,&vfnClientThread,(void *)&dwClient[bClientCounter - 1]);
     	printf("socket numero: %i creado satisfactoriamente, ejecutando Hilo...\n",dwClient[bClientCounter - 1]);
    }  
   	
	pthread_mutex_destroy(&gpLock);
	uint8_t i;
	for (i = 0; i < bClientCounter; i++)
	{
		close(dwClient[i]);
	} 
}

static void *vfnClientThread(void* vpArgs)
{
	/*Variable control de vida del thread*/
	bool bCloseSocket = FALSE;
	/*Variable length del mensaje recibido*/
  	int32_t dwMsgLenght;
	/*Buffer*/
  	int8_t *bpBuffer;
	/*Obtener socket del cliente*/
  	int32_t dwSocket = *((int32_t *)vpArgs);
	/*reservando espacio de memoria para el buffer*/
 	bpBuffer = (int8_t *)malloc(MAXLENGHT);

  	if (bpBuffer==NULL)
	{
		printf("No se pudo reservar memoria para el buffer termianando hilo...\n\n");
		pthread_exit(NULL);	
	}

	while(bCloseSocket == FALSE)
	{
		/*Limpia Buffer*/
		memset(bpBuffer,'\0',MAXLENGHT);
		/*Esperando algun mensaje*/
		printf("Esperando mensaje del cliente...\n");
		dwMsgLenght = recv(dwSocket, bpBuffer, MAXLENGHT,0);
		/*nterrupted by a signal or Error ocurred*/
		if(dwMsgLenght <= 0) 
		{
			bCloseSocket = TRUE;
		}
		else
		{  
			pthread_mutex_lock(&gpLock);
			
			char *ansBuf = malloc(3 * sizeof(char));
			SCLIENTCOMMAND *tCommand = malloc(sizeof *tCommand); 
			SRESPONSECOMMAND *tResponse = malloc(sizeof *tResponse);
			SKINETISCOMMAND *tKinetis = malloc(sizeof *tKinetis);
			uint8_t bChecksum;

			/*Response SOF
				Udoo = 0xa
				Cel  = 0x1
				Kl25 = 0x2
			*/

			tResponse->SOF = 0xa1;
			tKinetis->SOF = 0xa2;
			
			/*Imprime comando recivido del cliente*/
			printf("SOF:   		\"%#2x\"\n",(uint8_t)bpBuffer[0]);
			printf("Potencia:	\"%i\"\n",(uint8_t)bpBuffer[1]);
			printf("Angulo:   	\"%i\"\n",(uint8_t)bpBuffer[2]);
			printf("CS:    		\"%#2x\"\n\n",(uint8_t)bpBuffer[3]);
			
			/*Almacenando valores recibidos por el celular*/
			tCommand->Potencia 	= (uint8_t)bpBuffer[1];
			tCommand->Angulo 	= (uint8_t)bpBuffer[2];
			tCommand->CS 		= (uint8_t)bpBuffer[3];
			
			bChecksum = bfnChecksum((void *)bpBuffer, 3);
			printf("Checksum: %i\n",bChecksum);
			printf("CS: %i\n\n", tCommand->CS);

			// tKinetis->leftPower  = 100;//(uint8_t)bpBuffer[1];
			// tKinetis->leftDir    = FORWARD;//(uint8_t)bpBuffer[2];
			// tKinetis->rightPower = 100;//(uint8_t)bpBuffer[3];
			// tKinetis->rightDir	 = FORWARD;
			// tKinetis->CS 		 = 0x72;//(uint8_t)bpBuffer[4];

			// write(fdUart, tKinetis, 6);
			
			/*Validando Checksum*/
			if(bChecksum == tCommand->CS)
			{
				uint16_t Angulo = wfnMaps(tCommand->Angulo,1,255,1,360);
				printf("Angulo desmapeado:		%i\n\n",(uint16_t)Angulo);

				if(Angulo >= 0 && Angulo <= 180)
				{
					/*Esta en el primer o segundo cuadrante*/
					tKinetis->leftDir = FORWARD;
					tKinetis->rightDir = FORWARD;					
					if(Angulo >= 0 && Angulo <= 90)
					{
						/*Primer Cuadrante*/
						uint8_t atenuacion = (uint8_t)wfnMaps(Angulo,1,90,100,1);
						tKinetis->leftPower = tCommand->Potencia;
						tKinetis->rightPower = tCommand->Potencia - atenuacion;

					}
					else
					{
						/*Segundo Cuadrante*/
						uint8_t atenuacion = (uint8_t)wfnMaps(Angulo,90,180,1,100);
						tKinetis->rightPower = tCommand->Potencia;
						tKinetis->leftPower = tCommand->Potencia - atenuacion;

					}
				}
				else
				{
					/*Esta en el tercer o cuarto cuadrante*/
					tKinetis->leftDir = BACKWARD;
					tKinetis->rightDir = BACKWARD;
					if(Angulo >= 180 && Angulo <= 270)
					{
						/*Tercer Cuadrante*/
						uint8_t atenuacion = (uint8_t)wfnMaps(Angulo,180,270,100,1);
						tKinetis->leftPower = tCommand->Potencia - atenuacion;
						tKinetis->rightPower = tCommand->Potencia;

					}
					else
					{
						/*Cuarto Cuadrante*/
						uint8_t atenuacion = (uint8_t)wfnMaps(Angulo,270,360,1,100);
						tKinetis->rightPower = tCommand->Potencia - atenuacion;
						tKinetis->leftPower = tCommand->Potencia;

					}

				}

				tKinetis->CS = bfnChecksum((void *)tKinetis, 5);
				printf("leftPower:	\"%i\"\n",(uint8_t)tKinetis->leftPower);
				printf("leftDir:   	\"%i\"\n",(uint8_t)tKinetis->leftDir);
				printf("rightPower: \"%i\"\n\n",(uint8_t)tKinetis->rightPower);
				printf("rightPower: \"%i\"\n\n",(uint8_t)tKinetis->rightDir);

				if(write(fdUart, tKinetis, 6) <= 0)
				{
					printf("Error al enviar mensaje a KL25\n");
					tResponse->status = ERROR;
					tResponse->CS 		= 255;
				}
				else
				{
					// usleep (15000);
	    //             bytesReaded = read(fdUart,(void *)ansBuf,3);
					// bChecksum = bfnChecksum((void *)ansBuf, bytesReaded);
					// if(bChecksum == ansBuf[2])
					// {
					// 	printf("Data Received Kl25: %01x : %01x : %01x\n\n",(uint8_t)ansBuf[0], (uint8_t)ansBuf[1], (uint8_t)ansBuf[2]);
					// 	tResponse->status = (uint8_t)ansBuf[1];
					// 	tResponse->CS 	  = bfnChecksum((void *)tResponse, 3);

					// }
					// else
					// {
					// 	printf("Error al recibir mensaje de KL25\n");
					// 	tResponse->status = ERROR;
					// 	tResponse->CS 		= 255;
					// }
				}
			}
			else
			{
				tResponse->status = 0;
				tResponse->CS 		= 255;
				printf("Error en el mensaje Checksum fail...\n\n");
			}
			
			/*Response to client*/
			if(write(dwSocket, tResponse, sizeof(SRESPONSECOMMAND)) <= 0)
			{
				printf("Error al enviar mensaje\n");
			}

			/*Liberando memeria al pool comun*/
			free(tCommand);
			free(tResponse);
			free(tKinetis);
			free(ansBuf);

			/*borrar asignacion de addr del puntero*/
			tCommand = NULL;
			tResponse = NULL;
			tKinetis = NULL;
			ansBuf = NULL;

		    pthread_mutex_unlock(&gpLock);


		}

	}
	/*Cerrando Socket*/
	close(dwSocket);
	printf("Socket #%i Cerrado\n", dwSocket);
	/*Liberando memeria al pool comun*/
	free(bpBuffer);
	/*borrar asignacion de addr del puntero*/
	bpBuffer = NULL;
	pthread_exit(NULL);
}

uint8_t bfnChecksum(void *vpBlock, uint8_t bSize)
{
	uint8_t result = 0;
	uint8_t *bpData;
	bpData = vpBlock;

	while(bSize--) result += *bpData++;

	return result;
}


int set_interface_attribs (int fd, int speed, int parity)
{
        struct termios tty;
        memset (&tty, 0, sizeof tty);
        if (tcgetattr (fd, &tty) != 0)
        {
            printf("\n Inicialización UART fallida!\n");
            exit(EXIT_FAILURE);
        }
 
        cfsetospeed (&tty, speed);
        cfsetispeed (&tty, speed);
 
        tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
        // disable IGNBRK for mismatched speed tests; otherwise receive break
        // as \000 chars
        tty.c_iflag &= ~IGNBRK;         // disable break processing
        tty.c_lflag = 0;                // no signaling chars, no echo,
                                        // no canonical processing
        tty.c_oflag = 0;                // no remapping, no delays
        tty.c_cc[VMIN]  = 0;            // read doesn't block
        tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout
 
        tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl
 
        tty.c_cflag |= (CLOCAL | CREAD);// ignore modem controls,
                                        // enable reading
        tty.c_cflag &= ~(PARENB | PARODD);      // shut off parity
        tty.c_cflag |= parity;
        tty.c_cflag &= ~CSTOPB;
        tty.c_cflag &= ~CRTSCTS;
 
        if (tcsetattr (fd, TCSANOW, &tty) != 0)
        {
            printf("\n Inicialización UART fallida!\n");
            exit(EXIT_FAILURE);
        }
 
        return 0;
}


uint16_t wfnMaps(uint16_t wX, uint16_t wInMin, uint16_t wInMax, uint16_t wOutMin, uint16_t wOutMax)
{
  return (uint16_t)((wX - wInMin) * ((wOutMax - wOutMin) / (wInMax - wInMin)) + wOutMin);
}
 
void set_blocking (int fd, int should_block)
{
        struct termios tty;
        memset (&tty, 0, sizeof tty);
        if (tcgetattr (fd, &tty) != 0)
        {
			printf("\n Inicialización UART fallida!\n");
            exit(EXIT_FAILURE);
        }
 
        tty.c_cc[VMIN]  = should_block ? 1 : 0;
        tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout
 
        if (tcsetattr (fd, TCSANOW, &tty) != 0)
        {
            printf("\n Inicialización UART fallida!\n");
            exit(EXIT_FAILURE);
        }
 
}