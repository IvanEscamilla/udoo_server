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

typedef int bool;
#define true 1
#define false 0

typedef struct cCommand {
   int  SOF;
   int  Sensor;
   int  Eje;
   int  CS;
} clientCommand;

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
			
			SRAWDATA sAccRawData;
			SRAWDATA sMagRawData;
			SGYRORAWDATA sGyroRawData;

			
			ReadAccelMagnData(&sAccRawData, &sMagRawData);
			ReadGyroData(&sGyroRawData);

			/*Print Acc Data Raw*/
			printf("Acc Raw Data\n");
			printf("X: %i\n",sAccRawData.x);
			printf("Y: %i\n",sAccRawData.y);
			printf("Z: %i\n\n",sAccRawData.z);

			/*Print Mag Data Raw*/
			printf("Mag Raw Data\n");
			printf("X: %i\n",sMagRawData.x);
			printf("Y: %i\n",sMagRawData.y);
			printf("Z: %i\n\n",sMagRawData.z);

			/*Print Gyro Data Raw*/
			printf("Gyro Raw Data\n");
			printf("X: %i\n",sGyroRawData.x);
			printf("Y: %i\n",sGyroRawData.y);
			printf("Z: %i\n\n",sGyroRawData.z);

			int checksum;
			clientCommand command;
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
				
			}
			else
			{
				printf("Error en el mensaje checksum fail...\n\n");
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
