/**************************************************************************//***
 *  @file    hmc1119.c
 *  @author  Ray M0DHP
 *  @date    2017-12-22  
 *  @version 0.1
*******************************************************************************/

#include <unistd.h>
#include <stdio.h>
#include <math.h>
#include <wiringPi.h>
#include "hmc1119.h"


/***************************************************************************//**
 *  @brief Sets the attenuation level on the HMC1119 attenuator
 *  
 *  @param level Attenuation level in dB (positive float) e.g. "12.5"
 *
 *  @return 1 if level is out of bounds, 0 otherwise
*******************************************************************************/

int hmc1119_set_level(float level)
{
  if (level >= HMC1119_MIN_ATTENUATION && level <= HMC1119_MAX_ATTENUATION)
  {
    uint8_t integer_level = round(level * 4.0);
    printf("DEBUG: setting %s attenuation level to %.2f dB\n", HMC1119_DISPLAY_NAME, integer_level / 4.0);

    // Nominate pins using WiringPi numbers

    const uint8_t LE_1119_GPIO = 16;
    const uint8_t CLK_1119_GPIO = 21;
    const uint8_t DATA_1119_GPIO = 22;

    // Set all nominated pins to outputs

    pinMode(LE_1119_GPIO, OUTPUT);
    pinMode(CLK_1119_GPIO, OUTPUT);
    pinMode(DATA_1119_GPIO, OUTPUT);

    // Set idle conditions

    usleep(10);
    digitalWrite(LE_1119_GPIO, LOW);
    digitalWrite(CLK_1119_GPIO, LOW);

    // Shift out data 

    int8_t bit;
    for (bit = 6; bit >= 0; bit--)
    {
      digitalWrite(DATA_1119_GPIO, (integer_level >> bit) & 0x01);
      usleep(10);
      digitalWrite(CLK_1119_GPIO, HIGH);
      usleep(10);
      digitalWrite(CLK_1119_GPIO, LOW);
      usleep(10);
    }

    // Latch data

    digitalWrite(LE_1119_GPIO, HIGH);
    usleep(10);
    digitalWrite(LE_1119_GPIO, LOW);
    usleep(10);

    return 0;
  }
  else
  {
    printf("ERROR: level %.3f dB is outside limits for %s attenuator\n", level, HMC1119_DISPLAY_NAME);
    return 1;
  }
}

