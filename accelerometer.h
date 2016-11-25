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


int FXOS8700CQ_Init();
int ReadAccelMagnData(SRAWDATA *pAccelData, SRAWDATA *pMagnData);
