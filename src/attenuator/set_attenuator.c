#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <wiringPi.h>
#include "pe4312.h"


/******************************************************************************
 * @brief Sets the attenuation level on the external attenuator
 * @param attenuator-type "PE4312" or "HMC1119"
 * @param level Attenuation level in dB (float)
 * @return 1 if invalid attenuator type or level is out of bounds, 0 otherwise
*******************************************************************************/

int main(int argc, char *argv[])
{
  // Kick Wiring Pi into life
  if (wiringPiSetup() == -1);

  // Check that two parameters have been supplied
  if (argc != 3)
  {
    printf("ERROR: specify attenuator type and attenuation level\n");
    return 1;
  }
  // Check attenuator type
  else if (strcmp(argv[1], "PE4312") == 0)
  {
    // printf("DEBUG: PE4312 specified\n");
    int rc = pe4312_set_level(atof(argv[2]));
    printf("DEBUG: rc = %d\n", rc);
    return rc;
  }
  else if (strcmp(argv[1], "HMC1119") == 0)
  {
    printf("DEBUG: HMC1119 specified\n"); 
    // int rc = pe4312_set_level(atof(argv[2]));
    // printf("DEBUG: rc = %d\n", rc);
    // return rc;
    return 0;
  }
  else
  {
    printf("ERROR: attenuator type '%s' not recognised\n", argv[1]);
    return 1;
  }
}

