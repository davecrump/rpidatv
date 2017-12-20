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

// uint32_t registers[6] =  {0x4580A8, 0x80080C9, 0x4E42, 0x4B3, 0xBC803C, 0x580005};


/******************************************************************************
 * @brief Sets the attenuation level on the external attenuator.
 *
 * @param "PE4312" or "HMC1119", then attenuation in dB (float)
 *
 * @return 1 if attenuation is out of bounds, otherwise return 0
*******************************************************************************/

int main(int argc, char *argv[])
{
  // Kick Wiring Pi into life
  if (wiringPiSetup() == -1);

  // set parameter defaults
  // uint32_t adf4350_requested_frequency = 1255000000;
  // uint32_t adf4350_requested_ref_freq = 25000000;
  // uint16_t adf4350_requested_power = 0;

  // Check that two parameters have been supplied
  if (argc != 3)
  {
    printf("ERROR: specify attenuator type and attenuation level\n");
    return 1;
  }
  // Check attenuator type
  else if (strcmp(argv[1], "PE4312") == 0)
  {
    printf("DEBUG: PE4312 specified\n");
    return 0;
  }
  else if (strcmp(argv[1], "HMC1119") == 0)
  {
    printf("DEBUG: HMC1119 specified\n"); 
    return 0;
  }
  else
  {
    printf("ERROR: attenuator type '%s' not recognised\n", argv[1]);
    return 1;
  }
}

