 #include "accelerometer.h"

/*
	FXOS8700CQ I2C address
	is mounted to i2c-3 module of linux kernel
*/

#define FXOS8700CQ_SLAVE_ADDR 		0x1E // with pins SA0=0, SA1=0

// FXOS8700CQ internal register addresses
#define FXOS8700CQ_STATUS					0x00
#define FXOS8700CQ_WHOAMI 					0x0D
#define FXOS8700CQ_XYZ_DATA_CFG 			0x0E
#define FXOS8700CQ_CTRL_REG1 				0x2A
#define FXOS8700CQ_M_CTRL_REG1 				0x5B
#define FXOS8700CQ_M_CTRL_REG2 				0x5C
#define FXOS8700CQ_WHOAMI_VAL 				0xC7
/*XYZ REGISTERS OF ACC*/
#define FXOS8700CQ_XYZ_ACC_START_REGISTER 	0x01
#define FXOS8700CQ_X_MSB_ACC_REGISTER 		1
#define FXOS8700CQ_X_LSB_ACC_REGISTER 		2
#define FXOS8700CQ_Y_MSB_ACC_REGISTER 		3
#define FXOS8700CQ_Y_LSB_ACC_REGISTER 		4
#define FXOS8700CQ_Z_MSB_ACC_REGISTER 		5
#define FXOS8700CQ_Z_LSB_ACC_REGISTER 		6
/*XYZ REGISTERS OF MAG*/
#define FXOS8700CQ_XYZ_MAG_START_REGISTER 	0x33
#define FXOS8700CQ_X_MSB_MAG_REGISTER 		7
#define FXOS8700CQ_X_LSB_MAG_REGISTER 		8
#define FXOS8700CQ_Y_MSB_MAG_REGISTER 		9
#define FXOS8700CQ_Y_LSB_MAG_REGISTER 		10
#define FXOS8700CQ_Z_MSB_MAG_REGISTER 		11
#define FXOS8700CQ_Z_LSB_MAG_REGISTER 		12

/*
	The reference driver code shown in block read of the FXOS8700CQ
	status byte and three 16-bit accelerometer channels plus three 
	16-bit magnetometer channels for a total of 13 bytes in a single 
	I2C read operation.
*/

// number of bytes to be read from the FXOS8700CQ
#define FXOS8700CQ_FULL_READ_LEN 		13 // 0x00 to 0x38 = 56 bytes
#define FXOS8700CQ_ACC_READ_LEN 		6 // plus 6 channels = 6 bytes
#define FXOS8700CQ_MAG_READ_LEN 		6 // plus 6 channels = 6 bytes

int accFd;
char *buffer;

/*
	function configures FXOS8700CQ combination accelerometer and 
	magnetometer sensor 
*/

int FXOS8700CQ_Init()
{
	char *modulePath = "/dev/i2c-3";
	uint8_t databyte[2];

	if ((accFd = open(modulePath, O_RDWR)) < 0) 
	{
		/* ERROR HANDLING: you can check errno to see what went wrong */
		perror("Failed to open the i2c bus");
		return (I2C_ERROR);
	}

	// open comunication to FXOS8700CQ
	if (ioctl(accFd, I2C_SLAVE_FORCE, FXOS8700CQ_SLAVE_ADDR) < 0)
	{
		buffer = strerror(errno);
        printf("%s\n\n", buffer);
		perror("Failed to open comunication to FXOS8700CQ");
		return (I2C_ERROR);
	}

	// write 0000 0000 = 0x00 to accelerometer control register 1 to place FXOS8700CQ into
	// standby
	// [7-1] = 0000 000
	// [0]: active=0
	databyte[0]	= FXOS8700CQ_CTRL_REG1;
	databyte[1] = 0x00;

	if(write( accFd, &databyte, 2) <= 0)
	{
		buffer = strerror(errno);
        printf("%s\n\n", buffer);
		perror("Failed to write to accelerometer control register 1 to place FXOS8700CQ into standby");
		return (I2C_ERROR);
	}

	// write 0001 1111 = 0x1F to magnetometer control register 1
	// [7]: m_acal=0: auto calibration disabled
	// [6]: m_rst=0: no one-shot magnetic reset
	// [5]: m_ost=0: no one-shot magnetic measurement
	// [4-2]: m_os=111=7: 8x oversampling (for 200Hz) to reduce magnetometer noise
	// [1-0]: m_hms=11=3: select hybrid mode with accel and magnetometer active
	databyte[0]	= FXOS8700CQ_M_CTRL_REG1;
	databyte[1] = 0x1F;
	if(write( accFd, &databyte, 2) <= 0)
	{
		buffer =  strerror(errno);
        printf("%s\n\n", buffer);
		perror("Failed to write to magnetometer control register 1");
		return (I2C_ERROR);
	}
	// write 0010 0000 = 0x20 to magnetometer control register 2
	// [7]: reserved
	// [6]: reserved
	// [5]: hyb_autoinc_mode=1 to map the magnetometer registers to follow the accelerometer registers
	// [4]: m_maxmin_dis=0 to retain default min/max latching even though not used
	// [3]: m_maxmin_dis_ths=0
	// [2]: m_maxmin_rst=0
	// [1-0]: m_rst_cnt=00 to enable magnetic reset each cycle
	databyte[0]	= FXOS8700CQ_M_CTRL_REG2;
	databyte[1] = 0x20;
	if(write( accFd, &databyte, 2) <= 0)
	{
		buffer =  strerror(errno);
        printf("%s\n\n", buffer);
		perror("Failed to write to magnetometer control register 2");
		return (I2C_ERROR);
	}
	
	// write 0000 0001= 0x01 to XYZ_DATA_CFG register
	// [7]: reserved
	// [6]: reserved
	// [5]: reserved
	// [4]: hpf_out=0
	// [3]: reserved
	// [2]: reserved
	// [1-0]: fs=01 for accelerometer range of +/-4g range with 0.488mg/LSB
	databyte[0]	= FXOS8700CQ_XYZ_DATA_CFG;
	databyte[1] = 0x01;
	if(write( accFd, &databyte, 2) <= 0)
	{
		buffer =  strerror(errno);
        printf("%s\n\n", buffer);
		perror("Failed to write 0x01 to XYZ_DATA_CFG register to get accelerometer range of +/-4g");
		return (I2C_ERROR);
	}

	// write 0000 1101 = 0x0D to accelerometer control register 1
	// [7-6]: aslp_rate=00
	// [5-3]: dr=001 for 200Hz data rate (when in hybrid mode)
	// [2]: lnoise=1 for low noise mode
	// [1]: f_read=0 for normal 16 bit reads
	// [0]: active=1 to take the part out of standby and enable sampling
	databyte[0]	= FXOS8700CQ_CTRL_REG1;
	databyte[1] = 0x0D;
	if(write( accFd, &databyte, 2) <= 0)
	{
		buffer =  strerror(errno);
        printf("%s\n\n", buffer);
		perror("Failed to write 0x0D to accelerometer control register 1 to take the part out of standby and enable sampling");
		return (I2C_ERROR);
	}
	// normal return
	return (I2C_OK);
}

// read status and the three channels of accelerometer and magnetometer data from
// FXOS8700CQ (13 bytes)
int ReadAccelMagnData(SRAWDATA *pAccelData, SRAWDATA *pMagnData)
{
	uint8_t Buffer[FXOS8700CQ_FULL_READ_LEN];
	//uint8_t accBuffer[FXOS8700CQ_ACC_READ_LEN]; // read buffer
	//uint8_t magBuffer[FXOS8700CQ_MAG_READ_LEN]; // read buffer
	//uint8_t addrToRead;
	
	// read FXOS8700CQ_FULL_READ_LEN=56 bytes (status byte and the six channels of data)
	if (read( accFd, Buffer, FXOS8700CQ_FULL_READ_LEN) != FXOS8700CQ_FULL_READ_LEN) 
	{
        buffer =  strerror(errno);
        printf("%s\n\n", buffer);
		return I2C_ERROR;
    } 
	else
	{

		// copy the 14 bit accelerometer byte data into 16 bit words
		pAccelData->x = (int16_t)(((Buffer[FXOS8700CQ_X_MSB_ACC_REGISTER] << 8) | Buffer[FXOS8700CQ_X_LSB_ACC_REGISTER]));
		pAccelData->y = (int16_t)(((Buffer[FXOS8700CQ_Y_MSB_ACC_REGISTER] << 8) | Buffer[FXOS8700CQ_Y_LSB_ACC_REGISTER]));
		pAccelData->z = (int16_t)(((Buffer[FXOS8700CQ_Z_MSB_ACC_REGISTER] << 8) | Buffer[FXOS8700CQ_Z_LSB_ACC_REGISTER]));

		// copy the magnetometer byte data into 16 bit words
		pMagnData->x = (Buffer[FXOS8700CQ_X_MSB_MAG_REGISTER] << 8) | Buffer[FXOS8700CQ_X_LSB_MAG_REGISTER];
		pMagnData->y = (Buffer[FXOS8700CQ_Y_MSB_MAG_REGISTER] << 8) | Buffer[FXOS8700CQ_Y_LSB_MAG_REGISTER];
		pMagnData->z = (Buffer[FXOS8700CQ_Z_MSB_MAG_REGISTER] << 8) | Buffer[FXOS8700CQ_Z_LSB_MAG_REGISTER];

	}
	
	/*
	//Read acc data from 0x01 addr to 0x06
	addrToRead = FXOS8700CQ_XYZ_ACC_START_REGISTER;

	if(write( accFd, &addrToRead, 1) <= 0)
	{
		buffer =  strerror(errno);
        printf("%s\n\n", buffer);
		perror("Failed to write the start addres to read");
		return (I2C_ERROR);
	}

	// read FXOS8700CQ_READ_LEN=6 bytes (status byte and the six channels of data)
	if (read( accFd, accBuffer, FXOS8700CQ_ACC_READ_LEN) != FXOS8700CQ_ACC_READ_LEN) 
	{

        buffer =  strerror(errno);
        printf("%s\n\n", buffer);
		return I2C_ERROR;

    } 
	else
	{

		// copy the 14 bit accelerometer byte data into 16 bit words
		pAccelData->x = (int16_t)(((accBuffer[0] << 8) | accBuffer[1]));
		pAccelData->y = (int16_t)(((accBuffer[2] << 8) | accBuffer[3]));
		pAccelData->z = (int16_t)(((accBuffer[4] << 8) | accBuffer[5]));

	}

	//Read mag data from 0x33 addr to 0x38
	addrToRead = FXOS8700CQ_XYZ_MAG_START_REGISTER;

	if(write( accFd, &addrToRead, 1) <= 0)
	{
		buffer =  strerror(errno);
        printf("%s\n\n", buffer);
		perror("Failed to write the start addres to read");
		return (I2C_ERROR);
	}

	// read FXOS8700CQ_MAG_READ_LEN=6 bytes (status byte and the six channels of data)
	if (read( accFd, magBuffer, FXOS8700CQ_MAG_READ_LEN) != FXOS8700CQ_MAG_READ_LEN) 
	{

        buffer =  strerror(errno);
        printf("%s\n\n", buffer);
		return I2C_ERROR;

    } 
	else
	{
		// copy the magnetometer byte data into 16 bit words
		pMagnData->x = (magBuffer[0] << 8) | magBuffer[1];
		pMagnData->y = (magBuffer[2] << 8) | magBuffer[3];
		pMagnData->z = (magBuffer[4] << 8) | magBuffer[5];

	}*/

	// normal return
	return I2C_OK;
}
