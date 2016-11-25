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
   uint8_t  Sensor;
   uint8_t  Eje;
   uint8_t  CS;
} SCLIENTCOMMAND;

typedef struct tResponse {
   uint8_t  SOF;
   uint8_t  Sensor;
   uint8_t  dataLength;
   uint8_t  CS;
   int16_t data[9];
} SRESPONSECOMMAND;

pthread_t gpThreadId;
pthread_mutex_t gpLock;

static void  *vfnClientThread(void* vpArgs);

int main(int argc, char *argv[])
{
	/*Se obtiene el puerto al que escuchará el servidor pasado por parametro*/
    int32_t dwPuerto = atoi(argv[1]);
    int32_t dwSocketFd;
    int32_t dwBindFd;
    int32_t dwListenFd;
    int32_t dwClient;
	
	/*Configurando Magnetometro y Acelerometro*/	
	if(FXOS8700CQ_Init() < 0)
	{
		printf("\n Inicialización Acelerometro y Magnetometro fallida!\n");
        exit(EXIT_FAILURE);	
	}

	/*Configurando Giroscopio*/	
	if(dwfnFXAS21002Init() < 0)
	{
		printf("\n Inicialización Giroscopio fallida!\n");
        exit(EXIT_FAILURE);	
	}


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
     	dwClient = accept(dwSocketFd,(struct sockaddr *)&socketOptions,&addrlen);
		printf("cliente coonectado creando hilo de conexión...\n");
		/*Creando Hilo vfnClientThread mandandole por parametro el socket del Cliente para recibir los mensajes del usuario conectado*/
     	pthread_create(&gpThreadId,NULL,&vfnClientThread,(void *)&dwClient);
     	printf("socket numero: %i creado satisfactoriamente, ejecutando Hilo...\n",dwClient);
    }  
   	
	pthread_mutex_destroy(&gpLock);
	close(dwClient); 
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
			
			uint8_t bChecksum;
			SCLIENTCOMMAND tCommand;
			SRESPONSECOMMAND tResponse = {0, 0, 0, 0, {0,0,0,0,0,0,0,0,0}};
			SRAWDATA tAccRawData;
			SRAWDATA tMagRawData;
			SRAWDATA tGyroRawData;

			/*Response SOF*/
			tResponse.SOF = 0xaa;
			
			/*Imprime comando recivido del cliente*/
			printf("SOF:   \"%#2x\"\n",(uint8_t)bpBuffer[0]);
			printf("Sensor:\"%#2x\"\n",(uint8_t)bpBuffer[1]);
			printf("Eje:   \"%#2x\"\n",(uint8_t)bpBuffer[2]);
			printf("CS:    \"%#2x\"\n\n",(uint8_t)bpBuffer[3]);
			
			/*Almacenando valores*/
			tCommand.SOF = (uint8_t)bpBuffer[0];
			tCommand.Sensor = (uint8_t)bpBuffer[1];
			tCommand.Eje = (uint8_t)bpBuffer[2];
			tCommand.CS = (uint8_t)bpBuffer[3];
			
			bChecksum = tCommand.SOF + tCommand.Sensor + tCommand.Eje;
			printf("Checksum: %i\n",bChecksum);
			printf("CS: %i\n\n", tCommand.CS);
			/*Validando Checksum*/
			if(bChecksum == tCommand.CS)
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
						dwfnReadGyroData(&tGyroRawData);
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
						dwfnReadGyroData(&tGyroRawData);
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

				int32_t dwCs =	(int32_t)tResponse.SOF + (int32_t)tResponse.Sensor + (int32_t)tResponse.dataLength + (int32_t)tResponse.data[0] + (int32_t)tResponse.data[1] + (int32_t)tResponse.data[2] + (int32_t)tResponse.data[3] + (int32_t)tResponse.data[4] + (int32_t)tResponse.data[5] + (int32_t)tResponse.data[6] + (int32_t)tResponse.data[7] + (int32_t)tResponse.data[8];
				tResponse.CS = (uint8_t) dwCs;

				printf("El tamaño es de %i\n", sizeof(tResponse));
				printf("SOF:    	int val: %i 	hex val: %#2x		size: %i \n",  tResponse.SOF, (uint8_t)tResponse.SOF, sizeof(tResponse.SOF));
				printf("Sensor: 	int val: %i 	hex val: %#2x		size: %i \n",  tResponse.Sensor, (uint8_t)tResponse.Sensor, sizeof(tResponse.Sensor));
				printf("dataLength:	int val: %i 	hex val: %#2x		size: %i \n",  tResponse.dataLength, (uint8_t)tResponse.dataLength, sizeof(tResponse.dataLength));
				printf("CS: 		int val: %i 	hex val: %#2x		size: %i \n",  tResponse.CS, (uint8_t)tResponse.CS, sizeof(tResponse.CS));
				printf("data[0]: 	int val: %i 	hex val: %#hx		size: %i \n",  tResponse.data[0], (int16_t)tResponse.data[0], sizeof(tResponse.data[0]));
				printf("data[1]: 	int val: %i 	hex val: %#hx		size: %i \n",  tResponse.data[1], (int16_t)tResponse.data[1], sizeof(tResponse.data[1]));
				printf("data[2]: 	int val: %i 	hex val: %#hx		size: %i \n",  tResponse.data[2], (int16_t)tResponse.data[2], sizeof(tResponse.data[2]));
				printf("data[3]: 	int val: %i 	hex val: %#hx		size: %i \n",  tResponse.data[3], (int16_t)tResponse.data[3], sizeof(tResponse.data[3]));
				printf("data[4]: 	int val: %i 	hex val: %#hx		size: %i \n",  tResponse.data[4], (int16_t)tResponse.data[4], sizeof(tResponse.data[4]));
				printf("data[5]: 	int val: %i 	hex val: %#hx		size: %i \n",  tResponse.data[5], (int16_t)tResponse.data[5], sizeof(tResponse.data[5]));
				printf("data[6]: 	int val: %i 	hex val: %#hx		size: %i \n",  tResponse.data[6], (int16_t)tResponse.data[6], sizeof(tResponse.data[6]));
				printf("data[7]: 	int val: %i 	hex val: %#hx		size: %i \n",  tResponse.data[7], (int16_t)tResponse.data[7], sizeof(tResponse.data[7]));
				printf("data[8]: 	int val: %i 	hex val: %#hx		size: %i \n",  tResponse.data[8], (int16_t)tResponse.data[8], sizeof(tResponse.data[8]));

				
			}
			else
			{

				tResponse.Sensor = ERROR;
				tResponse.dataLength = 0;
				tResponse.CS 		= 255;
				printf("Error en el mensaje Checksum fail...\n\n");
			}
			
			/*Response to client*/
			if(write(dwSocket, &tResponse, sizeof(tResponse)) <= 0)
			{
				printf("Error al enviar mensaje\n");
			}

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
