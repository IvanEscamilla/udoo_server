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
#define acelerometro  0x01
#define magnetometro  0x02
#define giroscopio 	  0x03
#define todos 		  0xFF
#define error 		  0xFE

//Eje a leer
#define eje_x  		  0x01
#define eje_y  		  0x02
#define eje_z 	  	  0x03
#define eje_xyz 	  0x04


typedef int bool;
#define true 1
#define false 0

typedef struct cCommand {
   int  SOF;
   int  Sensor;
   int  Eje;
   int  CS;
} clientCommand;

typedef struct rCommand {
   unsigned char  SOF;
   unsigned char  Sensor;
   unsigned char  dataLength;
   short data[9];
   unsigned char  CS;
} responseCommand;

pthread_t threadId;
pthread_mutex_t lock;

static void  *vfClientThread(void* vpArgs);

int main(int argc, char *argv[])
{
	/*Se obtiene el puerto al que escuchará el servidor pasado por parametro*/
    int Puerto = atoi(argv[1]);
    int socketFd;
    int bindFd;
    int listenFd;
    int client;
	
	/*Configurando Magnetometro y acelerometro*/	
	if(FXOS8700CQ_Init() < 0)
	{
		printf("\n Inicialización Acelerometro y magnetometro fallida!\n");
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

    printf("Abriendo Puerto = %i\n\n",Puerto);
	
	/*Creando un socket de tipo SOCK_STREAM e IPv4 fd guardado en la var */
    socketFd = socket(AF_INET, SOCK_STREAM, 0);

    if (socketFd < 0)
    {
       printf("Error en el socket \n");
       exit(EXIT_FAILURE);
    }
	
	printf("Socket Creado!\n");
	/*Configurnado Socket*/
    socketOptions.sin_family = AF_INET; //Familia AF_INET
    socketOptions.sin_port = htons(Puerto); // Numero de puerto
    socketOptions.sin_addr.s_addr = htons(INADDR_ANY); // direccion socket, escuchando en todas las interfases
	
	/*Nombrando al socket*/
    bindFd = bind(socketFd,(struct sockaddr *)&socketOptions, sizeof(struct sockaddr_in));
	printf("Ruta del socket: %i \n\n",socketOptions.sin_addr.s_addr);
    
	if (bindFd < 0)
    {
       printf("Error del bind \n");
       exit(EXIT_FAILURE);
    }
	
	printf("Socket Asignado!\n");
   	/*Escucar conexiones en el socket*/
    listenFd = listen(socketFd, LISTEN_BACKLOG);

    if(listenFd < 0)
    {
       printf("Error en el listen \n");
       exit(EXIT_FAILURE);
    }
	
	printf("Socket Listo para asignar clientes!...\n");
    addrlen = sizeof(struct sockaddr_in);
	
    for(;;)
    {
		/*En espera de un cliente*/
		printf("Esperando cliente...\n");    
     	client = accept(socketFd,(struct sockaddr *)&socketOptions,&addrlen);
		printf("Cliente coonectado creando hilo de conexión...\n");
		/*Creando Hilo vfClientThread mandandole por parametro el socket del cliente para recibir los mensajes del usuario conectado*/
     	pthread_create(&threadId,NULL,&vfClientThread,(void *)&client);
     	printf("socket numero: %i creado satisfactoriamente, ejecutando Hilo...\n",client);
    }  
   	
	pthread_mutex_destroy(&lock);
	close(client); 
}

static void *vfClientThread(void* vpArgs)
{
	/*Variable control de vida del thread*/
	bool closeSocket = false;
	/*Variable length del mensaje recibido*/
  	int msgLenght;
	/*Buffer*/
  	char *buffer;
	/*Obtener socket del cliente*/
  	int socket = *((int *)vpArgs);
	/*reservando espacio de memoria para el buffer*/
 	buffer = (char *)malloc(MAXLENGHT);
  	if (buffer==NULL)
	{
		printf("No se pudo reservar memoria para el buffer termianando hilo...\n\n");
		pthread_exit(NULL);	
	}

	while(closeSocket == false)
	{
		/*Limpia Buffer*/
		memset(buffer,'\0',MAXLENGHT);
		/*Esperando algun mensaje*/
		printf("Esperando mensaje del cliente...\n");
		msgLenght = recv(socket, buffer, MAXLENGHT,0);
		/*nterrupted by a signal or error ocurred*/
		if(msgLenght <= 0) 
		{
			closeSocket = true;
		}
		else
		{  
			pthread_mutex_lock(&lock);
			
			int checksum;
			clientCommand command;
			responseCommand response = {0, 0, 0, {0,0,0,0,0,0,0,0,0}, 0};
			SRAWDATA sAccRawData;
			SRAWDATA sMagRawData;
			SGYRORAWDATA sGyroRawData;

			/*Response SOF*/
			response.SOF = 0xaa;
			
			/*Imprime comando recivido del cliente*/
			printf("SOF:   \"%#2x\"\n",buffer[0]);
			printf("Sensor:\"%#2x\"\n",buffer[1]);
			printf("Eje:   \"%#2x\"\n",buffer[2]);
			printf("CS:    \"%#2x\"\n\n",buffer[3]);
			
			/*Almacenando valores*/
			command.SOF = (int)buffer[0];
			command.Sensor = (int)buffer[1];
			command.Eje = (int)buffer[2];
			command.CS = (int)buffer[3];
			
			checksum = command.SOF + command.Sensor + command.Eje;
			/*Validando Checksum*/
			if(checksum == command.CS)
			{
				printf("Interpretando mensaje recibido\n");
				switch(command.Sensor)
				{
					case acelerometro:
					{
						response.Sensor = acelerometro;
						printf("Leyendo datos del acelerometro...\n");
						ReadAccelMagnData(&sAccRawData, &sMagRawData);
						switch(command.Eje)
						{
							case eje_x:
							{
								printf("Leyendo Eje x del acelerometro...\n");
								printf("X: %i\n",sAccRawData.x);
								response.dataLength = 1;
								response.data[0] = sAccRawData.x;

							}break;
							case eje_y:
							{
								printf("Leyendo Eje y del acelerometro...\n");
								printf("Y: %i\n",sAccRawData.y);
								response.dataLength = 1;
								response.data[0] = sAccRawData.y;

							}break;
							case eje_z:
							{
								printf("Leyendo Eje z del acelerometro...\n");
								printf("Z: %i\n",sAccRawData.z);
								response.dataLength = 1;
								response.data[0] = sAccRawData.z;

							}break;
							case eje_xyz:
							{
								printf("Leyendo Eje x, y, z del acelerometro...\n");
								printf("X: %i\n",sAccRawData.x);
								printf("Y: %i\n",sAccRawData.y);
								printf("Z: %i\n\n",sAccRawData.z);
								response.dataLength = 3;
								response.data[0] = sAccRawData.x;
								response.data[1] = sAccRawData.y;
								response.data[2] = sAccRawData.z;

							}break;
						}

					}break;
					case magnetometro:
					{
						response.Sensor = magnetometro;
						printf("Leyendo datos del magnetometro...\n");
						ReadAccelMagnData(&sAccRawData, &sMagRawData);
						switch(command.Eje)
						{
							case eje_x:
							{
								printf("Leyendo Eje x del magnetometro...\n");
								printf("X: %i\n",sMagRawData.x);
								response.dataLength = 1;
								response.data[0] = sMagRawData.x;

							}break;
							case eje_y:
							{
								printf("Leyendo Eje y del magnetometro...\n");
								printf("Y: %i\n",sMagRawData.y);
								response.dataLength = 1;
								response.data[0] = sMagRawData.y;


							}break;
							case eje_z:
							{
								printf("Leyendo Eje z del magnetometro...\n");
								printf("Z: %i\n",sMagRawData.z);
								response.dataLength = 1;
								response.data[0] = sMagRawData.z;


							}break;
							case eje_xyz:
							{
								printf("Leyendo Eje x, y, z del magnetometro...\n");
								printf("X: %i\n",sMagRawData.x);
								printf("Y: %i\n",sMagRawData.y);
								printf("Z: %i\n\n",sMagRawData.z);
								response.dataLength = 3;
								response.data[0] = sMagRawData.x;
								response.data[1] = sMagRawData.y;
								response.data[2] = sMagRawData.z;

							}break;
						}

					}break;
					case giroscopio:
					{
						response.Sensor = giroscopio;
						printf("Leyendo datos del giroscopio...\n");
						ReadGyroData(&sGyroRawData);
						switch(command.Eje)
						{
							case eje_x:
							{
								printf("Leyendo Eje x del giroscopio...\n");
								printf("X: %i\n",sGyroRawData.x);
								response.dataLength = 1;
								response.data[0] = sGyroRawData.x;

							}break;
							case eje_y:
							{
								printf("Leyendo Eje y del giroscopio...\n");
								printf("Y: %i\n",sGyroRawData.y);
								response.dataLength = 1;
								response.data[0] = sGyroRawData.y;


							}break;
							case eje_z:
							{
								printf("Leyendo Eje z del giroscopio...\n");
								printf("Z: %i\n",sGyroRawData.z);
								response.dataLength = 1;
								response.data[0] = sGyroRawData.z;


							}break;
							case eje_xyz:
							{
								printf("Leyendo Eje x, y, z del giroscopio...\n");
								printf("X: %i\n",sGyroRawData.x);
								printf("Y: %i\n",sGyroRawData.y);
								printf("Z: %i\n\n",sGyroRawData.z);
								response.dataLength = 3;
								response.data[0] = sGyroRawData.x;
								response.data[1] = sGyroRawData.y;
								response.data[2] = sGyroRawData.z;

							}break;
						}

					}break;
					case todos:
					{
						response.Sensor = todos;
						printf("Leyendo datos de todos los sensores...\n");
						ReadAccelMagnData(&sAccRawData, &sMagRawData);
						ReadGyroData(&sGyroRawData);
						switch(command.Eje)
						{
							case eje_x:
							{
								printf("Leyendo Eje x de todos los sensores...\n");
								printf("Acc X: %i\n",sAccRawData.x);
								printf("Mag X: %i\n",sMagRawData.x);
								printf("Gyr X: %i\n",sGyroRawData.x);
								response.dataLength = 3;
								response.data[0] = sAccRawData.x;
								response.data[1] = sMagRawData.x;
								response.data[2] = sGyroRawData.x;


							}break;
							case eje_y:
							{
								printf("Leyendo Eje y de todos los sensores...\n");
								printf("Acc Y: %i\n",sAccRawData.y);
								printf("Mag Y: %i\n",sMagRawData.y);
								printf("Gyr Y: %i\n",sGyroRawData.y);
								response.dataLength = 3;
								response.data[0] = sAccRawData.y;
								response.data[1] = sMagRawData.y;
								response.data[2] = sGyroRawData.y;


							}break;
							case eje_z:
							{
								printf("Leyendo Eje z de todos los sensores...\n");
								printf("Acc Z: %i\n",sAccRawData.z);
								printf("Mag Z: %i\n",sMagRawData.z);
								printf("Gyr Z: %i\n",sGyroRawData.z);
								response.dataLength = 3;
								response.data[0] = sAccRawData.z;
								response.data[1] = sMagRawData.z;
								response.data[2] = sGyroRawData.z;


							}break;
							case eje_xyz:
							{
								printf("Leyendo Eje x, y, z de todos los sensores...\n");
								printf("Acc X: %i\n",sAccRawData.x);
								printf("Acc Y: %i\n",sAccRawData.y);
								printf("Acc Z: %i\n\n",sAccRawData.z);
								response.dataLength = 9;
								response.data[0] = sAccRawData.x;
								response.data[1] = sAccRawData.y;
								response.data[2] = sAccRawData.z;

								printf("Mag X: %i\n",sMagRawData.x);
								printf("Mag Y: %i\n",sMagRawData.y);
								printf("Mag Z: %i\n\n",sMagRawData.z);
								response.data[3] = sMagRawData.x;
								response.data[4] = sMagRawData.y;
								response.data[5] = sMagRawData.z;

								printf("Gyr X: %i\n",sGyroRawData.x);
								printf("Gyr Y: %i\n",sGyroRawData.y);
								printf("Gyr Z: %i\n\n",sGyroRawData.z);
								response.data[6] = sGyroRawData.x;
								response.data[7] = sGyroRawData.y;
								response.data[8] = sGyroRawData.z;

							}break;
						}

					}break;
				}
				
			}
			else
			{

				response.Sensor = error;
				response.dataLength = 0;
				response.CS 		= 0xFF;
				printf("Error en el mensaje checksum fail...\n\n");
			}
			
			printf("El tamaño es de %i\n", sizeof(response));
			printf("SOF:    	int val: %i  hex val: %#2x \n\n", response.SOF, response.SOF);
			printf("Sensor: 	int val: %i  hex val: %#2x \n\n", response.Sensor, response.Sensor);
			printf("dataLength: int val: %i  hex val: %#2x \n\n", response.dataLength, response.dataLength);
			printf("data[0]: 	int val: %i  hex val: %#2x \n\n", response.data[0], response.data[0]);
			printf("data[1]: 	int val: %i  hex val: %#2x \n\n", response.data[1], response.data[1]);
			printf("data[2]: 	int val: %i  hex val: %#2x \n\n", response.data[2], response.data[2]);
			printf("data[3]: 	int val: %i  hex val: %#2x \n\n", response.data[3], response.data[3]);
			printf("data[4]: 	int val: %i  hex val: %#2x \n\n", response.data[4], response.data[4]);
			printf("data[5]: 	int val: %i  hex val: %#2x \n\n", response.data[5], response.data[5]);
			printf("data[6]: 	int val: %i  hex val: %#2x \n\n", response.data[6], response.data[6]);
			printf("data[7]: 	int val: %i  hex val: %#2x \n\n", response.data[7], response.data[7]);
			printf("data[8]: 	int val: %i  hex val: %#2x \n\n", response.data[8], response.data[8]);
			printf("CS: 		int val: %i  hex val: %#2x \n\n", response.CS, response.CS);

			/*Response to client*/
			if(write(socket, &response, sizeof(response)) <= 0)
			{
				printf("Error al enviar mensaje\n");
			}

		    pthread_mutex_unlock(&lock);


		}

	}
	/*Cerrando Socket*/
	close(socket);
	printf("Socket #%i Cerrado\n", socket);
	/*Liberando memeria al pool comun*/
	free(buffer);
	/*borrar asignacion de addr del puntero*/
	buffer = NULL;
	pthread_exit(NULL);
}
