 #include "gyroscope.h"

/*
	FXAS21002 I2C address
	is mounted to i2c-3 module of linux kernel
*/

#define FXAS21002_SLAVE_ADDR 				0x20 // with pins SA0=0, SA1=0

// FXAS21002 internal register addresses
#define FXAS21002_STATUS					0x00
#define FXAS21002_WHOAMI 					0x0C
#define FXAS21002_CTRL_REG0 				0x0D
#define FXAS21002_CTRL_REG1 				0x13
#define FXAS21002_CTRL_REG2 				0x14
#define FXAS21002_CTRL_REG3 				0x15
#define FXAS21002_WHOAMI_VAL 				0xD7
/*XYZ REGISTERS OF ACC*/
#define FXAS21002_XYZ_START_REGISTER 		0x01
#define FXAS21002_X_MSB_REGISTER	 		1
#define FXAS21002_X_LSB_REGISTER	 		2
#define FXAS21002_Y_MSB_REGISTER	 		3
#define FXAS21002_Y_LSB_REGISTER	 		4
#define FXAS21002_Z_MSB_REGISTER	 		5
#define FXAS21002_Z_LSB_REGISTER	 		6

/*
	The reference driver code shown in block read of the FXAS21002
	status byte and three 16-bit gyroscope channels for a total of 
	7 bytes in a single I2C read operation.
*/

// number of bytes to be read from the FXAS21002
#define FXAS21002_FULL_READ_LEN 	7 // 0x00 to 0x06 = 7 bytes

int32_t gbGyroFd;
int8_t *gbpBuffer;

/*
	function configures FXAS21002 combination accelerometer and 
	magnetometer sensor 
*/

int32_t dwfnFXAS21002Init()
{
	int8_t *bpModulePath = "/dev/i2c-3";
	uint8_t bDatabyte[2];

	if ((gbGyroFd = open(bpModulePath, O_RDWR)) < 0) 
	{
		/* ERROR HANDLING: you can check errno to see what went wrong */
		perror("Failed to open the i2c bus");
		return (I2C_ERROR);
	}

	// open comunication to FXAS21002
	if (ioctl(gbGyroFd, I2C_SLAVE_FORCE, FXAS21002_SLAVE_ADDR) < 0)
	{
		gbpBuffer = strerror(errno);
        printf("%s\n\n", gbpBuffer);
		perror("Failed to open comunication to FXAS21002");
		return (I2C_ERROR);
	}

	// write 0001 0110 =  0x16 to accelerometer control register 1 to place FXAS21002 into
	// active
	// [7]: 0: Not used
	// [6]: 0: Device reset not triggered/completed
	// [5]: 0: Self-Test disabled					D   B  	 ODR		
	// [4:2]: 101: Output Data Rate selection -> 	5 	101	 25 (Hz)
	// [1]: 1: Standby/Active mode selection
	// [0]: 0: Standby/Ready mode selection
	
	bDatabyte[0]	= FXAS21002_CTRL_REG1;
	bDatabyte[1] = 0x16;

	if(write( gbGyroFd, &bDatabyte, 2) <= 0)
	{
		gbpBuffer = strerror(errno);
        printf("%s\n\n", gbpBuffer);
		perror("Failed to write to accelerometer control register 1 to place FXAS21002 into standby");
		return (I2C_ERROR);
	}

	// normal return
	return (I2C_OK);
}

// read status and the three channels of accelerometer and magnetometer data from
// FXAS21002 (6 bytes)
int32_t dwfnReadGyroData(SRAWDATA *pGyroData)
{
	uint8_t bBuffer[FXAS21002_FULL_READ_LEN];
	
	// read FXAS21002_FULL_READ_LEN = 13 bytes (status byte and the six channels of data)
	if (read( gbGyroFd, bBuffer, FXAS21002_FULL_READ_LEN) != FXAS21002_FULL_READ_LEN) 
	{
        gbpBuffer =  strerror(errno);
        printf("%s\n\n", gbpBuffer);
		return I2C_ERROR;
    } 
	else
	{
		// copy the 16 bit gyroscope byte data into 16 bit words
		pGyroData->x = (int16_t)(((bBuffer[FXAS21002_X_MSB_REGISTER] << 8) | bBuffer[FXAS21002_X_LSB_REGISTER]));
		pGyroData->y = (int16_t)(((bBuffer[FXAS21002_Y_MSB_REGISTER] << 8) | bBuffer[FXAS21002_Y_LSB_REGISTER]));
		pGyroData->z = (int16_t)(((bBuffer[FXAS21002_Z_MSB_REGISTER] << 8) | bBuffer[FXAS21002_Z_LSB_REGISTER]));

	}

	// normal return
	return I2C_OK;
}
