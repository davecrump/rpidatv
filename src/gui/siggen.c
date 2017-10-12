#include <linux/input.h>
#include <string.h>



#include <signal.h>
#include <stdio.h>
#include "VG/openvg.h"
#include "VG/vgu.h"
#include "fontinfo.h"
#include "shapes.h"
#include <pthread.h>
#include <fftw3.h>
#include <math.h>

#include "rpidatvtouch.h"

// This file is dure to provide Signal Generator Functionality in the future

// Proof of concept code
void siggenmain(void)
{
  char IP[256];
  GetIPAddr(IP);
  printf(IP);
  printf("IP Address printed from siggen.c\n");  
}
