#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define I2C_ERROR	-1
#define I2C_OK		 1

#ifndef SRAWDATA
typedef struct
{
	int16_t x;
	int16_t y;
	int16_t z;
} SRAWDATA;
#endif

int32_t dwfnFXAS21002Init();
int32_t dwfnReadGyroData(SRAWDATA *pGyroData);
