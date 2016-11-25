#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <pthread.h>
#include <string.h>
#include "accelerometer.h"
#include "gyroscope.h"

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


typedef int bool;
#define TRUE 			1
#define FALSE 			0

typedef struct tClient {
   int  SOF;
   int  Sensor;
   int  Eje;
   int  CS;
} tClientCommand;

typedef struct tResponse {
   unsigned char  SOF;
   unsigned char  Sensor;
   unsigned char  dataLength;
   unsigned char  CS;
   short data[9];
} tResponseCommand;

pthread_t pThreadId;
pthread_mutex_t lock;

static void  *vfnClientThread(void* vpArgs);

int main(int argc, char *argv[])
{
	/*Se obtiene el puerto al que escuchará el servidor pasado por parametro*/
    int iPuerto = atoi(argv[1]);
    int iSocketFd;
    int iBindFd;
    int iListenFd;
    int iClient;
	
	/*Configurando Magnetometro y Acelerometro*/	
	if(FXOS8700CQ_Init() < 0)
	{
		printf("\n Inicialización Acelerometro y Magnetometro fallida!\n");
        exit(EXIT_FAILURE);	
	}

	/*Configurando Giroscopio*/	
	if(FXAS21002_Init() < 0)
	{
		printf("\n Inicialización Giroscopio fallida!\n");
        exit(EXIT_FAILURE);	
	}


	if (pthread_mutex_init(&lock, NULL) != 0)
	{
		printf("\n Inicialización mutex fallida!\n");
        exit(EXIT_FAILURE);
	}

    struct sockaddr_in socketOptions;
    socklen_t addrlen;

    printf("Abriendo Puerto = %i\n\n",iPuerto);
	
	/*Creando un socket de tipo SOCK_STREAM e IPv4 fd guardado en la var */
    iSocketFd = socket(AF_INET, SOCK_STREAM, 0);

    if (iSocketFd < 0)
    {
       printf("Error en el socket \n");
       exit(EXIT_FAILURE);
    }
	
	printf("Socket Creado!\n");
	/*Configurnado Socket*/
    socketOptions.sin_family = AF_INET; //Familia AF_INET
    socketOptions.sin_port = htons(iPuerto); // Numero de puerto
    socketOptions.sin_addr.s_addr = htons(INADDR_ANY); // direccion socket, escuchando en todas las interfases
	
	/*Nombrando al socket*/
    iBindFd = bind(iSocketFd,(struct sockaddr *)&socketOptions, sizeof(struct sockaddr_in));
	printf("Ruta del socket: %i \n\n",socketOptions.sin_addr.s_addr);
    
	if (iBindFd < 0)
    {
       printf("Error del bind \n");
       exit(EXIT_FAILURE);
    }
	
	printf("Socket Asignado!\n");
   	/*Escucar conexiones en el socket*/
    iListenFd = listen(iSocketFd, LISTEN_BACKLOG);

    if(iListenFd < 0)
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
     	iClient = accept(iSocketFd,(struct sockaddr *)&socketOptions,&addrlen);
		printf("cliente coonectado creando hilo de conexión...\n");
		/*Creando Hilo vfnClientThread mandandole por parametro el socket del Cliente para recibir los mensajes del usuario conectado*/
     	pthread_create(&pThreadId,NULL,&vfnClientThread,(void *)&iClient);
     	printf("socket numero: %i creado satisfactoriamente, ejecutando Hilo...\n",iClient);
    }  
   	
	pthread_mutex_destroy(&lock);
	close(iClient); 
}

static void *vfnClientThread(void* vpArgs)
{
	/*Variable control de vida del thread*/
	bool bCloseSocket = FALSE;
	/*Variable length del mensaje recibido*/
  	int iMsgLenght;
	/*Buffer*/
  	char *cpBuffer;
	/*Obtener socket del cliente*/
  	int iSocket = *((int *)vpArgs);
	/*reservando espacio de memoria para el buffer*/
 	cpBuffer = (char *)malloc(MAXLENGHT);

  	if (cpBuffer==NULL)
	{
		printf("No se pudo reservar memoria para el buffer termianando hilo...\n\n");
		pthread_exit(NULL);	
	}

	while(bCloseSocket == FALSE)
	{
		/*Limpia Buffer*/
		memset(cpBuffer,'\0',MAXLENGHT);
		/*Esperando algun mensaje*/
		printf("Esperando mensaje del cliente...\n");
		iMsgLenght = recv(iSocket, cpBuffer, MAXLENGHT,0);
		/*nterrupted by a signal or Error ocurred*/
		if(iMsgLenght <= 0) 
		{
			bCloseSocket = TRUE;
		}
		else
		{  
			pthread_mutex_lock(&lock);
			
			unsigned char ucChecksum;
			tClientCommand tCommand;
			tResponseCommand tResponse = {0, 0, 0, 0, {0,0,0,0,0,0,0,0,0}};
			SRAWDATA tAccRawData;
			SRAWDATA tMagRawData;
			SGYRORAWDATA tGyroRawData;

			/*Response SOF*/
			tResponse.SOF = 0xaa;
			
			/*Imprime comando recivido del cliente*/
			printf("SOF:   \"%#2x\"\n",cpBuffer[0]);
			printf("Sensor:\"%#2x\"\n",cpBuffer[1]);
			printf("Eje:   \"%#2x\"\n",cpBuffer[2]);
			printf("CS:    \"%#2x\"\n\n",cpBuffer[3]);
			
			/*Almacenando valores*/
			tCommand.SOF = (int)cpBuffer[0];
			tCommand.Sensor = (int)cpBuffer[1];
			tCommand.Eje = (int)cpBuffer[2];
			tCommand.CS = (int)cpBuffer[3];
			
			ucChecksum = tCommand.SOF + tCommand.Sensor + tCommand.Eje;
			printf("Checksum: %i\n",ucChecksum);
			printf("CS: %i\n\n", tCommand.CS);
			/*Validando Checksum*/
			if(ucChecksum == tCommand.CS)
			{
				printf("Interpretando mensaje recibido\n");
				switch(tCommand.Sensor)
				{
					case ACELEROMETRO:
					{
						tResponse.Sensor = ACELEROMETRO;
						printf("Leyendo datos del Acelerometro...\n");
						ReadAccelMagnData(&tAccRawData, &tMagRawData);
						switch(tCommand.Eje)
						{
							case EJE_X:
							{
								printf("Leyendo Eje x del Acelerometro...\n");
								printf("X: %i\n",tAccRawData.x);
								tResponse.dataLength = 1;
								tResponse.data[0] = tAccRawData.x;

							}break;
							case EJE_Y:
							{
								printf("Leyendo Eje y del Acelerometro...\n");
								printf("Y: %i\n",tAccRawData.y);
								tResponse.dataLength = 1;
								tResponse.data[0] = tAccRawData.y;

							}break;
							case EJE_Z:
							{
								printf("Leyendo Eje z del Acelerometro...\n");
								printf("Z: %i\n",tAccRawData.z);
								tResponse.dataLength = 1;
								tResponse.data[0] = tAccRawData.z;

							}break;
							case EJE_XYZ:
							{
								printf("Leyendo Eje x, y, z del Acelerometro...\n");
								printf("X: %i\n",tAccRawData.x);
								printf("Y: %i\n",tAccRawData.y);
								printf("Z: %i\n\n",tAccRawData.z);
								tResponse.dataLength = 3;
								tResponse.data[0] = tAccRawData.x;
								tResponse.data[1] = tAccRawData.y;
								tResponse.data[2] = tAccRawData.z;

							}break;
						}

					}break;
					case MAGNETOMETRO:
					{
						tResponse.Sensor = MAGNETOMETRO;
						printf("Leyendo datos del magnetometro...\n");
						ReadAccelMagnData(&tAccRawData, &tMagRawData);
						switch(tCommand.Eje)
						{
							case EJE_X:
							{
								printf("Leyendo Eje x del magnetometro...\n");
								printf("X: %i\n",tMagRawData.x);
								tResponse.dataLength = 1;
								tResponse.data[0] = tMagRawData.x;

							}break;
							case EJE_Y:
							{
								printf("Leyendo Eje y del magnetometro...\n");
								printf("Y: %i\n",tMagRawData.y);
								tResponse.dataLength = 1;
								tResponse.data[0] = tMagRawData.y;


							}break;
							case EJE_Z:
							{
								printf("Leyendo Eje z del magnetometro...\n");
								printf("Z: %i\n",tMagRawData.z);
								tResponse.dataLength = 1;
								tResponse.data[0] = tMagRawData.z;


							}break;
							case EJE_XYZ:
							{
								printf("Leyendo Eje x, y, z del magnetometro...\n");
								printf("X: %i\n",tMagRawData.x);
								printf("Y: %i\n",tMagRawData.y);
								printf("Z: %i\n\n",tMagRawData.z);
								tResponse.dataLength = 3;
								tResponse.data[0] = tMagRawData.x;
								tResponse.data[1] = tMagRawData.y;
								tResponse.data[2] = tMagRawData.z;

							}break;
						}

					}break;
					case GIROSCOPIO:
					{
						tResponse.Sensor = GIROSCOPIO;
						printf("Leyendo datos del giroscopio...\n");
						ReadGyroData(&tGyroRawData);
						switch(tCommand.Eje)
						{
							case EJE_X:
							{
								printf("Leyendo Eje x del giroscopio...\n");
								printf("X: %i\n",tGyroRawData.x);
								tResponse.dataLength = 1;
								tResponse.data[0] = tGyroRawData.x;

							}break;
							case EJE_Y:
							{
								printf("Leyendo Eje y del giroscopio...\n");
								printf("Y: %i\n",tGyroRawData.y);
								tResponse.dataLength = 1;
								tResponse.data[0] = tGyroRawData.y;


							}break;
							case EJE_Z:
							{
								printf("Leyendo Eje z del giroscopio...\n");
								printf("Z: %i\n",tGyroRawData.z);
								tResponse.dataLength = 1;
								tResponse.data[0] = tGyroRawData.z;


							}break;
							case EJE_XYZ:
							{
								printf("Leyendo Eje x, y, z del giroscopio...\n");
								printf("X: %i\n",tGyroRawData.x);
								printf("Y: %i\n",tGyroRawData.y);
								printf("Z: %i\n\n",tGyroRawData.z);
								tResponse.dataLength = 3;
								tResponse.data[0] = tGyroRawData.x;
								tResponse.data[1] = tGyroRawData.y;
								tResponse.data[2] = tGyroRawData.z;

							}break;
						}

					}break;
					case TODOS:
					{
						tResponse.Sensor = TODOS;
						printf("Leyendo datos de todos los sensores...\n");
						ReadAccelMagnData(&tAccRawData, &tMagRawData);
						ReadGyroData(&tGyroRawData);
						switch(tCommand.Eje)
						{
							case EJE_X:
							{
								printf("Leyendo Eje x de todos los sensores...\n");
								printf("Acc X: %i\n",tAccRawData.x);
								printf("Mag X: %i\n",tMagRawData.x);
								printf("Gyr X: %i\n",tGyroRawData.x);
								tResponse.dataLength = 3;
								tResponse.data[0] = tAccRawData.x;
								tResponse.data[1] = tMagRawData.x;
								tResponse.data[2] = tGyroRawData.x;


							}break;
							case EJE_Y:
							{
								printf("Leyendo Eje y de todos los sensores...\n");
								printf("Acc Y: %i\n",tAccRawData.y);
								printf("Mag Y: %i\n",tMagRawData.y);
								printf("Gyr Y: %i\n",tGyroRawData.y);
								tResponse.dataLength = 3;
								tResponse.data[0] = tAccRawData.y;
								tResponse.data[1] = tMagRawData.y;
								tResponse.data[2] = tGyroRawData.y;


							}break;
							case EJE_Z:
							{
								printf("Leyendo Eje z de todos los sensores...\n");
								printf("Acc Z: %i\n",tAccRawData.z);
								printf("Mag Z: %i\n",tMagRawData.z);
								printf("Gyr Z: %i\n",tGyroRawData.z);
								tResponse.dataLength = 3;
								tResponse.data[0] = tAccRawData.z;
								tResponse.data[1] = tMagRawData.z;
								tResponse.data[2] = tGyroRawData.z;


							}break;
							case EJE_XYZ:
							{
								printf("Leyendo Eje x, y, z de todos los sensores...\n");
								printf("Acc X: %i\n",tAccRawData.x);
								printf("Acc Y: %i\n",tAccRawData.y);
								printf("Acc Z: %i\n\n",tAccRawData.z);
								tResponse.dataLength = 9;
								tResponse.data[0] = tAccRawData.x;
								tResponse.data[1] = tAccRawData.y;
								tResponse.data[2] = tAccRawData.z;

								printf("Mag X: %i\n",tMagRawData.x);
								printf("Mag Y: %i\n",tMagRawData.y);
								printf("Mag Z: %i\n\n",tMagRawData.z);
								tResponse.data[3] = tMagRawData.x;
								tResponse.data[4] = tMagRawData.y;
								tResponse.data[5] = tMagRawData.z;

								printf("Gyr X: %i\n",tGyroRawData.x);
								printf("Gyr Y: %i\n",tGyroRawData.y);
								printf("Gyr Z: %i\n\n",tGyroRawData.z);
								tResponse.data[6] = tGyroRawData.x;
								tResponse.data[7] = tGyroRawData.y;
								tResponse.data[8] = tGyroRawData.z;

							}break;
						}

					}break;
				}

				/*Calculando Checksum*/

				int cs =	(int)tResponse.SOF + (int)tResponse.Sensor + (int)tResponse.dataLength + (int)tResponse.data[0] + (int)tResponse.data[1] + (int)tResponse.data[2] + (int)tResponse.data[3] + (int)tResponse.data[4] + (int)tResponse.data[5] + (int)tResponse.data[6] + (int)tResponse.data[7] + (int)tResponse.data[8];
				tResponse.CS = (unsigned char) cs;

				printf("El tamaño es de %i\n", sizeof(tResponse));
				printf("SOF:    	int val: %i  hex val: %#2x 	size: %i \n", tResponse.SOF, tResponse.SOF, sizeof(tResponse.SOF));
				printf("Sensor: 	int val: %i  hex val: %#2x 	size: %i \n", tResponse.Sensor, tResponse.Sensor, sizeof(tResponse.Sensor));
				printf("dataLength:	int val: %i  hex val: %#2x 	size: %i \n", tResponse.dataLength, tResponse.dataLength, sizeof(tResponse.dataLength));
				printf("data[0]: 	int val: %i  hex val: %#2x 	size: %i \n", tResponse.data[0], tResponse.data[0], sizeof(tResponse.data[0]));
				printf("data[1]: 	int val: %i  hex val: %#2x 	size: %i \n", tResponse.data[1], tResponse.data[1], sizeof(tResponse.data[1]));
				printf("data[2]: 	int val: %i  hex val: %#2x 	size: %i \n", tResponse.data[2], tResponse.data[2], sizeof(tResponse.data[2]));
				printf("data[3]: 	int val: %i  hex val: %#2x 	size: %i \n", tResponse.data[3], tResponse.data[3], sizeof(tResponse.data[3]));
				printf("data[4]: 	int val: %i  hex val: %#2x 	size: %i \n", tResponse.data[4], tResponse.data[4], sizeof(tResponse.data[4]));
				printf("data[5]: 	int val: %i  hex val: %#2x 	size: %i \n", tResponse.data[5], tResponse.data[5], sizeof(tResponse.data[5]));
				printf("data[6]: 	int val: %i  hex val: %#2x 	size: %i \n", tResponse.data[6], tResponse.data[6], sizeof(tResponse.data[6]));
				printf("data[7]: 	int val: %i  hex val: %#2x 	size: %i \n", tResponse.data[7], tResponse.data[7], sizeof(tResponse.data[7]));
				printf("data[8]: 	int val: %i  hex val: %#2x 	size: %i \n", tResponse.data[8], tResponse.data[8], sizeof(tResponse.data[8]));
				printf("CS: 		int val: %i  hex val: %#2x 	size: %i \n", tResponse.CS, tResponse.CS, sizeof(tResponse.CS));

				
			}
			else
			{

				tResponse.Sensor = ERROR;
				tResponse.dataLength = 0;
				tResponse.CS 		= 255;
				printf("Error en el mensaje ucChecksum fail...\n\n");
			}
			
			/*Response to client*/
			if(write(iSocket, &tResponse, sizeof(tResponse)) <= 0)
			{
				printf("Error al enviar mensaje\n");
			}

		    pthread_mutex_unlock(&lock);


		}

	}
	/*Cerrando Socket*/
	close(iSocket);
	printf("Socket #%i Cerrado\n", iSocket);
	/*Liberando memeria al pool comun*/
	free(cpBuffer);
	/*borrar asignacion de addr del puntero*/
	cpBuffer = NULL;
	pthread_exit(NULL);
}
