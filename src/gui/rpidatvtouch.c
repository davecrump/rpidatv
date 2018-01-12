// rpidatvtouch.c
/*
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

  shapedemo: testbed for OpenVG APIs
  by Anthony Starks (ajstarks@gmail.com)

  Initial code by Evariste F5OEO
  Rewitten by Dave, G8GKQ
*/
//
#include <linux/input.h>
#include <string.h>
#include "touch.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>

#include "VG/openvg.h"
#include "VG/vgu.h"
#include "fontinfo.h"
#include "shapes.h"

#include <pthread.h>
#include <fftw3.h>
#include <math.h>
#include <wiringPi.h>

#define KWHT  "\x1B[37m"
#define KYEL  "\x1B[33m"

#define PATH_CONFIG "/home/pi/rpidatv/scripts/rpidatvconfig.txt"
#define PATH_TOUCHCAL "/home/pi/rpidatv/scripts/touchcal.txt"

char ImageFolder[]="/home/pi/rpidatv/image/";

int fd=0;
int wscreen, hscreen;
float scaleXvalue, scaleYvalue; // Coeff ratio from Screen/TouchArea
int wbuttonsize;
int hbuttonsize;

typedef struct {
	int r,g,b;
} color_t;

typedef struct {
	char Text[255];
	color_t  Color;
} status_t;

#define MAX_STATUS 10
typedef struct {
	int x,y,w,h;                   // Position and size
	status_t Status[MAX_STATUS];   // Array of text and required colour for each status
	int IndexStatus;               // The number of valid status definitions.  0 = do not display
	int NoStatus;                  // This is the active status (colour and text)
} button_t;

// 	int LastEventTime; Was part of button_t.  No longer used

#define MAX_BUTTON 300
int IndexButtonInArray=0;
button_t ButtonArray[MAX_BUTTON];
#define TIME_ANTI_BOUNCE 500
int CurrentMenu=1;

//GLOBAL PARAMETERS

int fec;
int SR;
char ModeInput[255];
char freqtxt[255];
char ModeAudio[255];
char ModeOutput[255];
char ModeSTD[255];
char ModeOP[255];
char Caption[255];
int  scaledX, scaledY;
VGfloat CalShiftX = 0;
VGfloat CalShiftY = 0;
float CalFactorX = 1.0;
float CalFactorY = 1.0;
int GPIO_PTT = 29;
char ScreenState[255] = "NormalMenu";  // NormalMenu SpecialMenu TXwithMenu TXwithImage RXwithImage VideoOut SnapView VideoView Snap SigGen
char MenuTitle[30][127];

// Values for buttons
// May be over-written by values from from rpidatvconfig.txt:

int TabSR[5]={125,333,1000,2000,4000};
char SRLabel[5][255]={"SR 125","SR 333","SR1000","SR2000","SR4000"};
int TabFec[5]={1,2,3,5,7};
char TabModeInput[10][255]={"CAMMPEG-2","CAMH264","PATERNAUDIO","ANALOGCAM","CARRIER","CONTEST","IPTSIN","ANALOGMPEG-2", "CARDMPEG-2", "CAMHDMPEG-2"};
char TabFreq[5][255]={"71","146.5","437","1249","1255"};
char FreqLabel[5][255]={" 71 MHz ","146.5 MHz","437 MHz ","1249 MHz","1255 MHz"};
char TabModeAudio[3][255]={"mic","auto","video"};
char TabModeSTD[2][255]={"6","0"};
char TabModeOP[10][255]={"IQ","QPSKRF","DATVEXPRESS","BATC","COMPVID"," "," "," "," ","DTX1"};
int Inversed=0;//Display is inversed (Waveshare=1)

pthread_t thfft,thbutton,thview;

// Function Prototypes

void Start_Highlights_Menu1();
void Start_Highlights_Menu2();
void Start_Highlights_Menu3();
void MsgBox4(const char *, const char *, const char *, const char *);
void wait_touch();
int getTouchSample(int *, int *, int *);
void TransformTouchMap(int, int);

/***************************************************************************//**
 * @brief Looks up the value of Param in PathConfigFile and sets value
 *        Used to look up the configuration from rpidatvconfig.txt
 *
 * @param PatchConfigFile (str) the name of the configuration text file
 * @param Param the string labeling the parameter
 * @param Value the looked-up value of the parameter
 *
 * @return void
*******************************************************************************/

void GetConfigParam(char *PathConfigFile,char *Param, char *Value)
{
	char * line = NULL;
	size_t len = 0;
	int read;
	FILE *fp=fopen(PathConfigFile,"r");
	if(fp!=0)
	{
		while ((read = getline(&line, &len, fp)) != -1)
		{
			if(strncmp (line,Param,strlen(Param)) == 0)
			{
				strcpy(Value,line+strlen(Param)+1);
				char *p;
				if((p=strchr(Value,'\n'))!=0) *p=0; //Remove \n
				break;
			}
			//strncpy(Value,line+strlen(Param)+1,strlen(line)-strlen(Param)-1-1/* pour retour chariot*/);
	    	}
	}
	else
		printf("Config file not found \n");
	fclose(fp);

}

/***************************************************************************//**
 * @brief sets the value of Param in PathConfigFile froma program variable
 *        Used to store the configuration in rpidatvconfig.txt
 *
 * @param PatchConfigFile (str) the name of the configuration text file
 * @param Param the string labeling the parameter
 * @param Value the looked-up value of the parameter
 *
 * @return void
*******************************************************************************/

void SetConfigParam(char *PathConfigFile,char *Param,char *Value)
{
  char * line = NULL;
  size_t len = 0;
  int read;
  char Command[255];
  char BackupConfigName[255];
  strcpy(BackupConfigName,PathConfigFile);
  strcat(BackupConfigName,".bak");
  FILE *fp=fopen(PathConfigFile,"r");
  FILE *fw=fopen(BackupConfigName,"w+");
  if(fp!=0)
  {
    while ((read = getline(&line, &len, fp)) != -1)
    {
      if(strncmp (line,Param,strlen(Param)) == 0)
      {
        fprintf(fw,"%s=%s\n",Param,Value);
      }
      else
        fprintf(fw,line);
    }
    fclose(fp);
    fclose(fw);
    sprintf(Command,"cp %s %s",BackupConfigName,PathConfigFile);
    system(Command);
  }
  else
  {
    printf("Config file not found \n");
    fclose(fp);
    fclose(fw);
  }
}


/***************************************************************************//**
 * @brief Looks up the card number for the RPi Audio Card
 *
 * @param card (str) as a single character string with no <CR>
 *
 * @return void
*******************************************************************************/

void GetPiAudioCard(char card[256])
{
  FILE *fp;

  /* Open the command for reading. */
  fp = popen("aplay -l | grep bcm2835 | head -1 | cut -c6-6", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(card, 7, fp) != NULL)
  {
    sprintf(card, "%d", atoi(card));
  }

  /* close */
  pclose(fp);
}


/***************************************************************************//**
 * @brief Looks up the current IPV4 address
 *
 * @param IPAddress (str) IP Address to be passed as a string
 *
 * @return void
*******************************************************************************/

void GetIPAddr(char IPAddress[256])
{
  FILE *fp;

  /* Open the command for reading. */
  fp = popen("ifconfig | grep -Eo \'inet (addr:)?([0-9]*\\.){3}[0-9]*\' | grep -Eo \'([0-9]*\\.){3}[0-9]*\' | grep -v \'127.0.0.1\' | head -1", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(IPAddress, 16, fp) != NULL)
  {
    //printf("%s", IPAddress);
  }

  /* close */
  pclose(fp);
}

/***************************************************************************//**
 * @brief Looks up the current Software Version
 *
 * @param SVersion (str) IP Address to be passed as a string
 *
 * @return void
*******************************************************************************/

void GetSWVers(char SVersion[256])
{
  FILE *fp;

  /* Open the command for reading. */
  fp = popen("cat /home/pi/rpidatv/scripts/installed_version.txt", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(SVersion, 16, fp) != NULL)
  {
    printf("%s", SVersion);
  }

  /* close */
  pclose(fp);
}

/***************************************************************************//**
 * @brief Looks up the GPU Temp
 *
 * @param GPUTemp (str) GPU Temp to be passed as a string max 20 char
 *
 * @return void
*******************************************************************************/

void GetGPUTemp(char GPUTemp[256])
{
  FILE *fp;

  /* Open the command for reading. */
  fp = popen("/opt/vc/bin/vcgencmd measure_temp", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(GPUTemp, 20, fp) != NULL)
  {
    printf("%s", GPUTemp);
  }

  /* close */
  pclose(fp);
}

/***************************************************************************//**
 * @brief Looks up the CPU Temp
 *
 * @param CPUTemp (str) CPU Temp to be passed as a string
 *
 * @return void
*******************************************************************************/

void GetCPUTemp(char CPUTemp[256])
{
  FILE *fp;

  /* Open the command for reading. */
  fp = popen("cat /sys/class/thermal/thermal_zone0/temp", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(CPUTemp, 20, fp) != NULL)
  {
    printf("%s", CPUTemp);
  }

  /* close */
  pclose(fp);
}

/***************************************************************************//**
 * @brief Checks the CPU Throttling Status
 *
 * @param Throttled (str) Throttle status to be passed as a string
 *
 * @return void
*******************************************************************************/

void GetThrottled(char Throttled[256])
{
  FILE *fp;

  /* Open the command for reading. */
  fp = popen("vcgencmd get_throttled", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(Throttled, 20, fp) != NULL)
  {
    printf("%s", Throttled);
  }

  /* close */
  pclose(fp);
}


/***************************************************************************//**
 * @brief Reads the input source from rpidatvconfig.txt
 *        and determines coding and video source
 * @param sets strings: coding, source
 *
 * @return void
*******************************************************************************/

void ReadModeInput(char coding[256], char vsource[256])
{
  char ModeInput[256];
  GetConfigParam(PATH_CONFIG,"modeinput", ModeInput);

  strcpy(coding, "notset");
  strcpy(vsource, "notset");

  if (strcmp(ModeInput, "CAMH264") == 0) 
  {
    strcpy(coding, "H264");
    strcpy(vsource, "RPi Camera");
  } 
  else if (strcmp(ModeInput, "CAMMPEG-2") == 0)
  {
    strcpy(coding, "MPEG-2");
    strcpy(vsource, "RPi Camera");
  }
  else if (strcmp(ModeInput, "FILETS") == 0)
  {
    strcpy(coding, "Native");
    strcpy(vsource, "TS File");
  }
  else if (strcmp(ModeInput, "PATERNAUDIO") == 0)
  {
    strcpy(coding, "H264");
    strcpy(vsource, "Test Card");
  }
  else if (strcmp(ModeInput, "CARRIER") == 0)
  {
    strcpy(coding, "DC");
    strcpy(vsource, "Plain Carrier");
  }
  else if (strcmp(ModeInput, "TESTMODE") == 0)
  {
    strcpy(coding, "Square Wave");
    strcpy(vsource, "Test");
  }
  else if (strcmp(ModeInput, "PATERNAUDIO") == 0)
  {
    strcpy(coding, "H264");
    strcpy(vsource, "Test Card");
  }
  else if (strcmp(ModeInput, "IPTSIN") == 0)
  {
    strcpy(coding, "Native");
    strcpy(vsource, "IP Transport Stream");
  }
  else if (strcmp(ModeInput, "ANALOGCAM") == 0)
  {
    strcpy(coding, "H264");
    strcpy(vsource, "Ext Video Input");
  }
  else if (strcmp(ModeInput, "VNC") == 0)
  {
    strcpy(coding, "H264");
    strcpy(vsource, "VNC");
  }
  else if (strcmp(ModeInput, "DESKTOP") == 0)
  {
    strcpy(coding, "H264");
    strcpy(vsource, "Screen");
  }
  else if (strcmp(ModeInput, "CONTEST") == 0)
  {
    strcpy(coding, "H264");
    strcpy(vsource, "Contest Numbers");
  }
  else if (strcmp(ModeInput, "ANALOGMPEG-2") == 0)
  {
    strcpy(coding, "MPEG-2");
    strcpy(vsource, "Ext Video Input");
  }
  else if (strcmp(ModeInput, "CARDMPEG-2") == 0)
  {
    strcpy(coding, "MPEG-2");
    strcpy(vsource, "Static Test Card");
  }
  else if (strcmp(ModeInput, "CAMHDMPEG-2") == 0)
  {
    strcpy(coding, "MPEG-2");
    strcpy(vsource, "RPi Cam HD");
  }
  else
  {
    strcpy(coding, "notset");
    strcpy(vsource, "notset");
  }
}

/***************************************************************************//**
 * @brief Reads the output mode from rpidatvconfig.txt
 *        and determines the user-friendkly string for display
 * @param sets strings: Moutput
 *
 * @return void
*******************************************************************************/

void ReadModeOutput(char Moutput[256])
{
  char ModeOutput[256];
  GetConfigParam(PATH_CONFIG,"modeoutput", ModeOutput);

  strcpy(Moutput, "notset");

  if (strcmp(ModeOutput, "IQ") == 0) 
  {
    strcpy(Moutput, "Filter-modulator Board");
  } 
  else if (strcmp(ModeOutput, "QPSKRF") == 0) 
  {
    strcpy(Moutput, "Ugly mode for testing");
  } 
  else if (strcmp(ModeOutput, "BATC") == 0) 
  {
    strcpy(Moutput, "BATC Streaming");
  } 
  else if (strcmp(ModeOutput, "DIGITHIN") == 0) 
  {
    strcpy(Moutput, "DigiThin Board");
  } 
  else if (strcmp(ModeOutput, "DTX1") == 0) 
  {
    strcpy(Moutput, "DTX-1 Modulator");
  } 
  else if (strcmp(ModeOutput, "DATVEXPRESS") == 0) 
  {
    strcpy(Moutput, "DATV Express by USB");
  } 
  else if (strcmp(ModeOutput, "IP") == 0) 
  {
    strcpy(Moutput, "IP Stream");
  } 
  else if (strcmp(ModeOutput, "COMPVID") == 0) 
  {
    strcpy(Moutput, "Composite Video");
  } 
  else
  {
    strcpy(Moutput, "notset");
  }
}


/***************************************************************************//**
 * @brief Looks up the SD Card Serial Number
 *
 * @param SerNo (str) Serial Number to be passed as a string
 *
 * @return void
*******************************************************************************/

void GetSerNo(char SerNo[256])
{
  FILE *fp;

  /* Open the command for reading. */
  fp = popen("cat /sys/block/mmcblk0/device/serial", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(SerNo, 20, fp) != NULL)
  {
    printf("%s", SerNo);
  }

  /* close */
  pclose(fp);
}

/***************************************************************************//**
 * @brief Looks up the Audio Input Devices
 *
 * @param DeviceName1 and DeviceName2 (str) First 40 char of device names
 *
 * @return void
*******************************************************************************/

void GetDevices(char DeviceName1[256], char DeviceName2[256])
{
  FILE *fp;
  char arecord_response_line[256];
  int card = 1;

  /* Open the command for reading. */
  fp = popen("arecord -l", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(arecord_response_line, 250, fp) != NULL)
  {
    if (arecord_response_line[0] == 'c')
    {
      if (card == 2)
      {
        strcpy(DeviceName2, arecord_response_line);
        card = card + 1;
      }
      if (card == 1)
      {
        strcpy(DeviceName1, arecord_response_line);
        card = card + 1;
      }
    }
    printf("%s", arecord_response_line);
  }
  /* close */
  pclose(fp);
}

/***************************************************************************//**
 * @brief Looks up the USB Video Input Device address
 *
 * @param DeviceName1 and DeviceName2 (str) First 40 char of device names
 *
 * @return void
*******************************************************************************/

void GetUSBVidDev(char VidDevName[256])
{
  FILE *fp;
  char response_line[256];

  /* Open the command for reading. */
  fp = popen("v4l2-ctl --list-devices 2> /dev/null | sed -n '/usb/,/dev/p' | grep 'dev' | tr -d '\t'", "r");
  if (fp == NULL)
  {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(response_line, 250, fp) != NULL)
  {
    if (strlen(response_line) <= 1)
    {
        strcpy(VidDevName, "nil");
    }
    else
    {
      strcpy(VidDevName, response_line);
    }
  }

  /* close */
  pclose(fp);
}


/***************************************************************************//**
 * @brief Reads the Presets from rpidatvconfig.txt and formats them for
 *        Display and switching
 *
 * @param None.  Works on global variables
 *
 * @return void
*******************************************************************************/

void ReadPresets()
{
  int n;
  char Param[255];
  char Value[255];
  char SRTag[5][255]={"psr1","psr2","psr3","psr4","psr5"};
  char FreqTag[5][255]={"pfreq1","pfreq2","pfreq3","pfreq4","pfreq5"};
  char SRValue[5][255];
  //char FreqValue[5];
  int len;

  // Read SRs
  for( n = 0; n < 5; n = n + 1)
  {
    strcpy(Param, SRTag[ n ]);
    GetConfigParam(PATH_CONFIG,Param,Value);
    strcpy(SRValue[ n ], Value);
    TabSR[n] = atoi(SRValue[n]);
    if (TabSR[n] > 999)
    {
      strcpy(SRLabel[n], "SR");
      strcat(SRLabel[n], SRValue[n]);
    }
    else
    {
      strcpy(SRLabel[n], "SR ");
      strcat(SRLabel[n], SRValue[n]);
    }
  //printf("Read Presets\n");
  //printf("Value=%s %s\n",SRValue[ n ],"SR");
  }

  // Read Frequencies
  for( n = 0; n < 5; n = n + 1)
  {
    strcpy(Param, FreqTag[ n ]);
    GetConfigParam(PATH_CONFIG,Param,Value);
    strcpy(TabFreq[ n ], Value);
    len  = strlen(TabFreq[n]);
    switch (len)
    {
      case 2:
        strcpy(FreqLabel[n], " ");
        strcat(FreqLabel[n], TabFreq[n]);
        strcat(FreqLabel[n], " MHz ");
        break;
      case 3:
        strcpy(FreqLabel[n], TabFreq[n]);
        strcat(FreqLabel[n], " MHz ");
        break;
      case 4:
        strcpy(FreqLabel[n], TabFreq[n]);
        strcat(FreqLabel[n], " MHz");
        break;
      case 5:
        strcpy(FreqLabel[n], TabFreq[n]);
        strcat(FreqLabel[n], "MHz");
        break;
      default:
        strcpy(FreqLabel[n], TabFreq[n]);
        strcat(FreqLabel[n], " M");
        break;
    }
  }
}

/***************************************************************************//**
 * @brief Checks for the presence on an RTL-SDR
 *        
 * @param None
 *
 * @return 0 if present, 1 if not present
*******************************************************************************/

int CheckRTL()
{
  char RTLStatus[256];
  FILE *fp;

  /* Open the command for reading. */
  fp = popen("/home/pi/rpidatv/scripts/check_rtl.sh", "r");
  if (fp == NULL)
  {
    printf("Failed to run command\n" );
    exit(1);
  }
  /* Read the output a line at a time - output it. */
  while (fgets(RTLStatus, sizeof(RTLStatus)-1, fp) != NULL)
  {
  }
  if (RTLStatus[0] == '0')
  {
    printf("RTL Detected\n" );
    return(0);
  }
  else
  {
    printf("No RTL Detected\n" );
    return(1);
  }

  /* close */
  pclose(fp);
}


int mymillis()
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (tv.tv_sec) * 1000 + (tv.tv_usec)/1000;
}

void ReadTouchCal()
{
  char Param[255];
  char Value[255];

  // Read CalFactors
  strcpy(Param, "CalFactorX");
  GetConfigParam(PATH_TOUCHCAL,Param,Value);
  CalFactorX = strtof(Value, 0);
  printf("Starting with CalfactorX = %f\n", CalFactorX);
  strcpy(Param, "CalFactorY");
  GetConfigParam(PATH_TOUCHCAL,Param,Value);
  CalFactorY = strtof(Value, 0);
  printf("Starting with CalfactorY = %f\n", CalFactorY);

  // Read CalShifts
  strcpy(Param, "CalShiftX");
  GetConfigParam(PATH_TOUCHCAL,Param,Value);
  CalShiftX = strtof(Value, 0);
  printf("Starting with CalShiftX = %f\n", CalShiftX);
  strcpy(Param, "CalShiftY");
  GetConfigParam(PATH_TOUCHCAL,Param,Value);
  CalShiftY = strtof(Value, 0);
  printf("Starting with CalShiftY = %f\n", CalShiftY);
}

void touchcal()
{
  VGfloat cross_x;                 // Position of white cross
  VGfloat cross_y;                 // Position of white cross
  int n;                           // Loop counter
  int rawX, rawY, rawPressure;     // Raw touch position
  int lowerY = 0, higherY = 0;     // Screen referenced touch average
  int leftX = 0, rightX = 0;       // Screen referenced touch average
  int touchposX[8];                // Screen referenced uncorrected touch position
  int touchposY[8];                // Screen referenced uncorrected touch position
  int correctedX;                  // Screen referenced corrected touch position
  int correctedY;                  // Screen referenced corrected touch position
  char Param[255];                 // Parameter name for writing to Calibration File
  char Value[255];                 // Value for writing to Calibration File

  MsgBox4("TOUCHSCREEN CALIBRATION", "Touch the screen on each cross", "Screen will be recalibrated after 8 touches", "Touch screen to start");
  wait_touch();

  for (n = 1; n < 9; n = n + 1 )
  {
    BackgroundRGB(0,0,0,255);
    Fill(255, 255, 255, 1); 
    WindowClear();
    StrokeWidth(3);
    Stroke(255, 255, 255, 0.8);    // White lines

    // Draw Frame of 3 points deep around screen inner
    Line(0,         hscreen, wscreen-1, hscreen);
    Line(0,         1,       wscreen-1, 1      );
    Line(0,         1,       0,         hscreen);
    Line(wscreen-1, 1,       wscreen-1, hscreen);

    // Calculate cross centres for 10% and 90% crosses
    if ((n == 1) || (n ==5) || (n == 4) || (n == 8))
    {
      cross_y = (hscreen * 9) / 10;
    }
    else
    {
      cross_y = hscreen / 10;
    }
    if ((n == 3) || (n ==4) || (n == 7) || (n == 8))
    {
      cross_x = (wscreen * 9) / 10;
    }
    else
    {
      cross_x = wscreen / 10;
    }

    // Draw cross
    Line(cross_x-wscreen/20, cross_y, cross_x+wscreen/20, cross_y);
    Line(cross_x, cross_y-hscreen/20, cross_x, cross_y+hscreen/20);

    //Send to Screen
    End();

    // Wait here until screen touched
    while(getTouchSample(&rawX, &rawY, &rawPressure)==0)
    {
      usleep(100000);
    }

    // Transform touch to display coordinate system
    // Results returned in scaledX and scaledY (globals)
    TransformTouchMap(rawX, rawY);

    printf("x=%d y=%d scaledX=%d scaledY=%d\n ",rawX,rawY,scaledX,scaledY);

    if ((n == 1) || (n ==5) || (n == 4) || (n == 8))
    {
      if (scaledY < hscreen/2)  // gross error
      {
        StrokeWidth(0);
        MsgBox4("Last touch was too far", "from the cross", "Touch Screen to continue", "and then try again");
        wait_touch();
        return;
      }
      higherY = higherY + scaledY;
    }
    else
    {
     if (scaledY > hscreen/2)  // gross error
      {
        StrokeWidth(0);
        MsgBox4("Last touch was too far", "from the cross", "Touch Screen to continue", "and then try again");
        wait_touch();
        return;
      }
      lowerY = lowerY + scaledY;
    }
    if ((n == 3) || (n ==4) || (n == 7) || (n == 8))
    {
     if (scaledX < wscreen/2)  // gross error
      {
        StrokeWidth(0);
        MsgBox4("Last touch was too far", "from the cross", "Touch Screen to continue", "and then try again");
        wait_touch();
        return;
      }
      rightX = rightX + scaledX;
    }
    else
    {
     if (scaledX > wscreen/2)  // gross error
      {
        StrokeWidth(0);
        MsgBox4("Last touch was too far", "from the cross", "Touch Screen to continue", "and then try again");
        wait_touch();
        return;
      }
      leftX = leftX + scaledX;
    }

    // Save touch position for display
    touchposX[n] = scaledX;
    touchposY[n] = scaledY;
  }
    
  // Average out touches
  higherY = higherY/4; // higherY is the height of the upper horixontal line
  lowerY = lowerY/4;   // lowerY is the height of the lower horizontal line
  rightX = rightX/4;   // rightX is the left-right pos of the right hand vertical line
  leftX = leftX/4;     // leftX is the left-right pos of the left hand vertical line

  // Now calculate the global calibration factors
  CalFactorX = (0.8 * wscreen)/(rightX-leftX);
  CalFactorY = (0.8 * hscreen)/(higherY-lowerY);
  CalShiftX= wscreen/10 - leftX * CalFactorX;
  CalShiftY= hscreen/10 - lowerY * CalFactorY;

  // Save them to the calibration file

  // Save CalFactors
  snprintf(Value, 10, "%.4f", CalFactorX);
  strcpy(Param, "CalFactorX");
  SetConfigParam(PATH_TOUCHCAL,Param,Value);
  snprintf(Value, 10, "%.4f", CalFactorY);
  strcpy(Param, "CalFactorY");
  SetConfigParam(PATH_TOUCHCAL,Param,Value);

  // Save CalShifts
  snprintf(Value, 10, "%.1f", CalShiftX);
  strcpy(Param, "CalShiftX");
  SetConfigParam(PATH_TOUCHCAL,Param,Value);
  snprintf(Value, 10, "%.1f", CalShiftY);
  strcpy(Param, "CalShiftY");
  SetConfigParam(PATH_TOUCHCAL,Param,Value);

  //printf("Left = %d, right = %d \n", leftX, rightX);
  //printf("Lower  = %d, upper = %d \n", lowerY, higherY);
  //printf("CalFactorX = %lf \n", CalFactorX);
  //printf("CalFactorY = %lf \n", CalFactorY);
  //printf("CalShiftX = %lf \n", CalShiftX);
  //printf("CalShiftY = %lf \n", CalShiftY);

  // Draw crosses in red where the touches were
  // and in green where the corrected touches are
  
  BackgroundRGB(0,0,0,255);
  Fill(255, 255, 255, 1); 
  WindowClear();
  StrokeWidth(3);
  Stroke(255, 255, 255, 0.8);

  VGfloat th = TextHeight(SansTypeface, 25);
  TextMid(wscreen/2, hscreen/2+th, "Red crosses are before calibration,", SansTypeface, 25);
  TextMid(wscreen/2, hscreen/2-th, "Green crosses after calibration", SansTypeface, 25);
  TextMid(wscreen/2, hscreen/24, "Touch Screen to Continue", SansTypeface, 25);

  // Draw Frame
  Line(0,         hscreen, wscreen-1, hscreen);
  Line(0,         1,       wscreen-1, 1      );
  Line(0,         1,       0,         hscreen);
  Line(wscreen-1, 1,       wscreen-1, hscreen);

  // Draw the crosses
  for (n = 1; n < 9; n = n + 1 )
  {
    // Calculate cross centres
    if ((n == 1) || (n == 4) || (n == 5) || (n == 8))
    {
      cross_y = (hscreen * 9) / 10;
    }
    else
    {
      cross_y = hscreen / 10;
    }
    if ((n == 3) || (n == 4) || (n == 7) || (n == 8))
    {
      cross_x = (wscreen * 9) / 10;
    }
    else
    {
      cross_x = wscreen / 10;
    }

    // Draw reference cross in white
    Stroke(255, 255, 255, 0.8);
    Line(cross_x-wscreen/20, cross_y, cross_x+wscreen/20, cross_y);
    Line(cross_x, cross_y-hscreen/20, cross_x, cross_y+hscreen/20);

    // Draw uncorrected touch cross in red
    Stroke(255, 0, 0, 0.8);
    Line(touchposX[n]-wscreen/40, touchposY[n]-hscreen/40, touchposX[n]+wscreen/40, touchposY[n]+hscreen/40);
    Line(touchposX[n]+wscreen/40, touchposY[n]-hscreen/40, touchposX[n]-wscreen/40, touchposY[n]+hscreen/40);

    // Draw corrected touch cross in green
    correctedX = touchposX[n] * CalFactorX;
    correctedX = correctedX + CalShiftX;
    correctedY = touchposY[n] * CalFactorY;
    correctedY = correctedY + CalShiftY;
    Stroke(0, 255, 0, 0.8);
    Line(correctedX-wscreen/40, correctedY-hscreen/40, correctedX+wscreen/40, correctedY+hscreen/40);
    Line(correctedX+wscreen/40, correctedY-hscreen/40, correctedX-wscreen/40, correctedY+hscreen/40);
  }

  //Send to Screen
  End();
  wait_touch();

  // Set screen back to normal
  BackgroundRGB(0,0,0,255);
  StrokeWidth(0);
}

void TransformTouchMap(int x, int y)
{
  // This function takes the raw (0 - 4095 on each axis) touch data x and y
  // and transforms it to approx 0 - wscreen and 0 - hscreen in globals scaledX 
  // and scaledY prior to final correction by CorrectTouchMap  

  int shiftX, shiftY;
  double factorX, factorY;
  char Param[255];
  char Value[255];

  // Adjust registration of touchscreen for Waveshare
  shiftX=30; // move touch sensitive position left (-) or right (+).  Screen is 700 wide
  shiftY=-5; // move touch sensitive positions up (-) or down (+).  Screen is 480 high

  factorX=-0.4;  // expand (+) or contract (-) horizontal button space from RHS. Screen is 5.6875 wide
  factorY=-0.3;  // expand or contract vertical button space.  Screen is 8.53125 high

  // Switch axes for normal and waveshare displays
  if(Inversed==0) // Tontec35 or Element14_7
  {
    scaledX = x/scaleXvalue;
    scaledY = hscreen-y/scaleYvalue;
  }
  else //Waveshare (inversed)
  {
    scaledX = shiftX+wscreen-y/(scaleXvalue+factorX);

    strcpy(Param,"display");  //Check for Waveshare 4 inch
    GetConfigParam(PATH_CONFIG,Param,Value);
    if(strcmp(Value,"Waveshare4")!=0)
    {
      scaledY = shiftY+hscreen-x/(scaleYvalue+factorY);
    }
    else  // Waveshare 4 inch display so flip vertical axis
    {
      scaledY = shiftY+x/(scaleYvalue+factorY); // Vertical flip for 4 inch screen
    }
  }
}

void CorrectTouchMap()
{
  // This function takes the approx touch data and applies the calibration correction
  // It works directly on the globals scaledX and scaledY based on the constants read
  // from the ScreenCal.txt file during initialisation

  scaledX = scaledX * CalFactorX;
  scaledX = scaledX + CalShiftX;

  scaledY = scaledY * CalFactorY;
  scaledY = scaledY + CalShiftY;
}

int IsButtonPushed(int NbButton,int x,int y)
{
  //int  scaledX, scaledY;

  TransformTouchMap(x,y);  // Sorts out orientation and approx scaling of the touch map

  CorrectTouchMap();       // Calibrates each individual screen

  //printf("x=%d y=%d scaledx %d scaledy %d sxv %f syv %f Button %d\n",x,y,scaledX,scaledY,scaleXvalue,scaleYvalue, NbButton);

  int margin=10;  // was 20

  if((scaledX<=(ButtonArray[NbButton].x+ButtonArray[NbButton].w-margin))&&(scaledX>=ButtonArray[NbButton].x+margin) &&
    (scaledY<=(ButtonArray[NbButton].y+ButtonArray[NbButton].h-margin))&&(scaledY>=ButtonArray[NbButton].y+margin))
  {
    // ButtonArray[NbButton].LastEventTime=mymillis(); No longer used
    return 1;
  }
  else
  {
    return 0;
  }
}

int IsMenuButtonPushed(int x,int y)
{
  int  i, NbButton, cmo;
  NbButton = -1;
  cmo = 25 * (CurrentMenu - 1); // Current Menu Button number Offset

  TransformTouchMap(x,y);       // Sorts out orientation and approx scaling of the touch map
  CorrectTouchMap();            // Calibrates each individual screen

  //printf("x=%d y=%d scaledx %d scaledy %d sxv %f syv %f Button %d\n",x,y,scaledX,scaledY,scaleXvalue,scaleYvalue, NbButton);

  int margin=10;  // was 20

  // For each button in the current Menu, check if it has been pushed.
  // If it has been pushed, return the button number.  If nothing valid has been pushed return -1
  // If it has been pushed, do something with the last event time

  for (i = 0; i <=23; i++)
  {
    if (ButtonArray[i + cmo].IndexStatus > 0)  // If button has been defined
    {
      if  ((scaledX <= (ButtonArray[i + cmo].x + ButtonArray[i + cmo].w - margin))
        && (scaledX >= ButtonArray[i + cmo].x + margin)
        && (scaledY <= (ButtonArray[i + cmo].y + ButtonArray[i + cmo].h - margin))
        && (scaledY >= ButtonArray[i + cmo].y + margin))  // and touched
      {
        // ButtonArray[NbButton].LastEventTime=mymillis(); No longer used
        NbButton = i;          // Set the button number to return
        break;                 // Break out of loop as button has been found
      }
    }
  }
  return NbButton;
}

int InitialiseButtons()
{
  // Writes 0 to IndexStatus of each button to signify that it should not
  // be displayed.  As soon as a status (text and color) is added, IndexStatus > 0
  int i;
  for (i = 0; i <= MAX_BUTTON; i = i + 1)
  {
    ButtonArray[i].IndexStatus = 0;
  }
  return 1;
}

int AddButton(int x,int y,int w,int h)
{
	button_t *NewButton=&(ButtonArray[IndexButtonInArray]);
	NewButton->x=x;
	NewButton->y=y;
	NewButton->w=w;
	NewButton->h=h;
	NewButton->NoStatus=0;
	NewButton->IndexStatus=0;
	// NewButton->LastEventTime=mymillis();  No longer used
	return IndexButtonInArray++;
}

int CreateButton(int MenuIndex, int ButtonPosition)
{
  // Provide Menu number (int 1 - 20), Button Position (0 bottom left, 23 top right)
  // return button number

  // Calculate button index
  int ButtonIndex = (MenuIndex - 1) * 25 + ButtonPosition;
  
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;

  if (ButtonPosition < 20)  // Bottom 4 rows
  {
    x = (ButtonPosition % 5) * wbuttonsize + 20;  // % operator gives the remainder of the division
    y = (ButtonPosition / 5) * hbuttonsize + 20;
    w = wbuttonsize * 0.9;
    h = hbuttonsize * 0.9;
  }
  else if ((ButtonPosition == 20) || (ButtonPosition == 21))  // TX and RX buttons
  {
    x = (ButtonPosition % 5) * wbuttonsize *1.7 + 20;    // % operator gives the remainder of the division
    y = (ButtonPosition / 5) * hbuttonsize + 20;
    w = wbuttonsize * 1.2;
    h = hbuttonsize * 1.2;
  }
  else if ((ButtonPosition == 22) || (ButtonPosition == 23)) //Menu Up and Menu down buttons
  {
    x = ((ButtonPosition + 1) % 5) * wbuttonsize + 20;  // % operator gives the remainder of the division
    y = (ButtonPosition / 5) * hbuttonsize + 20;
    w = wbuttonsize * 0.9;
    h = hbuttonsize * 1.2;
  }

  button_t *NewButton=&(ButtonArray[ButtonIndex]);
  NewButton->x=x;
  NewButton->y=y;
  NewButton->w=w;
  NewButton->h=h;
  NewButton->NoStatus=0;
  NewButton->IndexStatus=0;

  return (ButtonIndex);
}

int AddButtonStatus(int ButtonIndex,char *Text,color_t *Color)
{
	button_t *Button=&(ButtonArray[ButtonIndex]);
	strcpy(Button->Status[Button->IndexStatus].Text,Text);
	Button->Status[Button->IndexStatus].Color=*Color;
	return Button->IndexStatus++;
}

void DrawButton(int ButtonIndex)
{
  button_t *Button=&(ButtonArray[ButtonIndex]);

  Fill(Button->Status[Button->NoStatus].Color.r, Button->Status[Button->NoStatus].Color.g, Button->Status[Button->NoStatus].Color.b, 1);
  Roundrect(Button->x,Button->y,Button->w,Button->h, Button->w/10, Button->w/10);
  Fill(255, 255, 255, 1);				   // White text

  char label[256];
  strcpy(label, Button->Status[Button->NoStatus].Text);
  char find = '^';                                  // Line separator is ^
  const char *ptr = strchr(label, find);            // pointer to ^ in string
  if(ptr)                                           // if ^ found then 2 lines
  {
    int index = ptr - label;                        // Position of ^ in string
    char line1[15];
    char line2[15];
    snprintf(line1, index+1, label);                // get text before ^
    snprintf(line2, strlen(label) - index, label + index + 1);  // and after ^
    TextMid(Button->x+Button->w/2, Button->y+Button->h*11/16, line1, SerifTypeface, Button->w/strlen(line1));	
    TextMid(Button->x+Button->w/2, Button->y+Button->h* 3/16, line2, SerifTypeface, Button->w/strlen(line2));	
  }
  else                                              // One line only
  {
    TextMid(Button->x+Button->w/2, Button->y+Button->h/2, label, SerifTypeface, Button->w/strlen(label));	
  }
}

void SetButtonStatus(int ButtonIndex,int Status)
{
  button_t *Button=&(ButtonArray[ButtonIndex]);
  Button->NoStatus=Status;
}

int GetButtonStatus(int ButtonIndex)
{
  button_t *Button=&(ButtonArray[ButtonIndex]);
  return Button->NoStatus;
}

void GetNextPicture(char *PictureName)
{

	DIR           *d;
 	struct dirent *dir;

  	d = opendir(ImageFolder);
  	if (d)
  	{
    	while ((dir = readdir(d)) != NULL)
    	{
		if (dir->d_type == DT_REG)
		{
			size_t len = strlen(dir->d_name);
    			if( len > 4 && strcmp(dir->d_name + len - 4, ".jpg") == 0)
			{
    				 printf("%s\n", dir->d_name);

				strncpy(PictureName,dir->d_name,strlen(dir->d_name)-4);
				break;
			}
  		}
      	}

    	closedir(d);
	}
}

int openTouchScreen(int NoDevice)
{
  char sDevice[255];

  sprintf(sDevice,"/dev/input/event%d",NoDevice);
  if(fd!=0) close(fd);
  if ((fd = open(sDevice, O_RDONLY)) > 0)
  {
    return 1;
  }
  else
    return 0;
}

/*
Input device name: "ADS7846 Touchscreen"
Supported events:
  Event type 0 (Sync)
  Event type 1 (Key)
    Event code 330 (Touch)
  Event type 3 (Absolute)
    Event code 0 (X)
     Value      0
     Min        0
     Max     4095
    Event code 1 (Y)
     Value      0
     Min        0
     Max     4095
    Event code 24 (Pressure)
     Value      0
     Min        0
     Max      255
*/

int getTouchScreenDetails(int *screenXmin,int *screenXmax,int *screenYmin,int *screenYmax)
{
	//unsigned short id[4];
        unsigned long bit[EV_MAX][NBITS(KEY_MAX)];
        char name[256] = "Unknown";
        int abs[6] = {0};

        ioctl(fd, EVIOCGNAME(sizeof(name)), name);
        printf("Input device name: \"%s\"\n", name);

        memset(bit, 0, sizeof(bit));
        ioctl(fd, EVIOCGBIT(0, EV_MAX), bit[0]);
        printf("Supported events:\n");

        int i,j,k;
	int IsAtouchDevice=0;
        for (i = 0; i < EV_MAX; i++)
                if (test_bit(i, bit[0])) {
                        printf("  Event type %d (%s)\n", i, events[i] ? events[i] : "?");
                        if (!i) continue;
                        ioctl(fd, EVIOCGBIT(i, KEY_MAX), bit[i]);
                        for (j = 0; j < KEY_MAX; j++){
                                if (test_bit(j, bit[i])) {
                                        printf("    Event code %d (%s)\n", j, names[i] ? (names[i][j] ? names[i][j] : "?") : "?");
	if(j==330) IsAtouchDevice=1;
                                        if (i == EV_ABS) {
                                                ioctl(fd, EVIOCGABS(j), abs);
                                                for (k = 0; k < 5; k++)
                                                        if ((k < 3) || abs[k]){
                                                                printf("     %s %6d\n", absval[k], abs[k]);
                                                                if (j == 0){
                                                                        if ((strcmp(absval[k],"Min  ")==0)) *screenXmin =  abs[k];
                                                                        if ((strcmp(absval[k],"Max  ")==0)) *screenXmax =  abs[k];
                                                                }
                                                                if (j == 1){
                                                                        if ((strcmp(absval[k],"Min  ")==0)) *screenYmin =  abs[k];
                                                                        if ((strcmp(absval[k],"Max  ")==0)) *screenYmax =  abs[k];
                                                                }
                                                        }
                                                }

                                        }
                               }
                        }

return IsAtouchDevice;
}


int getTouchSample(int *rawX, int *rawY, int *rawPressure)
{
	int i;
        /* how many bytes were read */
        size_t rb;
        /* the events (up to 64 at once) */
        struct input_event ev[64];
	//static int Last_event=0; //not used?
	rb=read(fd,ev,sizeof(struct input_event)*64);
	*rawX=-1;*rawY=-1;
	int StartTouch=0;
        for (i = 0;  i <  (rb / sizeof(struct input_event)); i++){
              if (ev[i].type ==  EV_SYN)
		{
                         //printf("Event type is %s%s%s = Start of New Event\n",KYEL,events[ev[i].type],KWHT);
		}
                else if (ev[i].type == EV_KEY && ev[i].code == 330 && ev[i].value == 1)
		{
			StartTouch=1;
                        //printf("Event type is %s%s%s & Event code is %sTOUCH(330)%s & Event value is %s1%s = Touch Starting\n", KYEL,events[ev[i].type],KWHT,KYEL,KWHT,KYEL,KWHT);
		}
                else if (ev[i].type == EV_KEY && ev[i].code == 330 && ev[i].value == 0)
		{
			//StartTouch=0;
			//printf("Event type is %s%s%s & Event code is %sTOUCH(330)%s & Event value is %s0%s = Touch Finished\n", KYEL,events[ev[i].type],KWHT,KYEL,KWHT,KYEL,KWHT);
		}
                else if (ev[i].type == EV_ABS && ev[i].code == 0 && ev[i].value > 0){
                        //printf("Event type is %s%s%s & Event code is %sX(0)%s & Event value is %s%d%s\n", KYEL,events[ev[i].type],KWHT,KYEL,KWHT,KYEL,ev[i].value,KWHT);
			*rawX = ev[i].value;
		}
                else if (ev[i].type == EV_ABS  && ev[i].code == 1 && ev[i].value > 0){
                        //printf("Event type is %s%s%s & Event code is %sY(1)%s & Event value is %s%d%s\n", KYEL,events[ev[i].type],KWHT,KYEL,KWHT,KYEL,ev[i].value,KWHT);
			*rawY = ev[i].value;
		}
                else if (ev[i].type == EV_ABS  && ev[i].code == 24 && ev[i].value > 0){
                        //printf("Event type is %s%s%s & Event code is %sPressure(24)%s & Event value is %s%d%s\n", KYEL,events[ev[i].type],KWHT,KYEL,KWHT,KYEL,ev[i].value,KWHT);
			*rawPressure = ev[i].value;
		}
		if((*rawX!=-1)&&(*rawY!=-1)&&(StartTouch==1))
		{
			/*if(Last_event-mymillis()>500)
			{
				Last_event=mymillis();
				return 1;
			}*/
			//StartTouch=0;
			return 1;
		}

	}
	return 0;
}

void ShowTitle()
{
  // Initialise and calculate the text display
  //BackgroundRGB(0,0,0,255);  // Black background
  if (CurrentMenu == 1)
  {
    Fill(0, 0, 0, 1);    // Black text
  }
  else
  {
    Fill(255, 255, 255, 1);    // White text
  }
  Fontinfo font = SansTypeface;
  int pointsize = 20;
  VGfloat txtht = TextHeight(font, pointsize);
  VGfloat txtdp = TextDepth(font, pointsize);
  VGfloat linepitch = 1.1 * (txtht + txtdp);
  VGfloat linenumber = 1.0;
  VGfloat tw;

  // Display Text
  tw = TextWidth(MenuTitle[CurrentMenu], font, pointsize);
  Text(wscreen / 2.0 - (tw / 2.0), hscreen - linenumber * linepitch, MenuTitle[CurrentMenu], font, pointsize);
}

void UpdateWindow()
// Paint each defined button and the title on the current Menu
{
  int i;
  int first;
  int last;
  if (CurrentMenu <= 20)  // Calculate button number for 20 standard 24-button menus
  {
    first = (CurrentMenu - 1) * 25;
    last = first + 23;
  }
  else                   // Calculate button number for 10 non-standard 50-button menus
  {
    first = 500 + (CurrentMenu - 21) * 50;
    last = first + 49;
  }

  if ((first > MAX_BUTTON) || ( last > MAX_BUTTON))  // Gross error check
  {
    printf("Button Calulation Error in UpdateWindow \n");
    first = 0;
    last = 0;
  } 

  for(i=first;i<=last;i++)
  {
    if (ButtonArray[i].IndexStatus > 0)  // If button needs to be drwan
    {
      DrawButton(i);                     // Draw the button
    }
  }
  ShowTitle();
  End();                      // Write the drawn buttons to the screen
}

void SelectInGroup(int StartButton,int StopButton,int NoButton,int Status)
{
  int i;
  for(i = StartButton ; i <= StopButton ; i++)
  {
    if(i == NoButton)
      SetButtonStatus(i, Status);
    else
      SetButtonStatus(i, 0);
  }
}

void SelectFreq(int NoButton)  //Frequency
{
  SelectInGroup(0, 4, NoButton, 1);
  strcpy(freqtxt, TabFreq[NoButton - 0]);
  char Param[] = "freqoutput";
  printf("************** Set Frequency = %s\n",freqtxt);
  SetConfigParam(PATH_CONFIG, Param, freqtxt);

  // Set the Band (and filter) Switching
  system ("sudo /home/pi/rpidatv/scripts/ctlfilter.sh");
  // And wait for it to finish using rpidatvconfig.txt
  usleep(100000);
}

void SelectSR(int NoButton)  // Symbol Rate
{
  SelectInGroup(5, 9, NoButton, 1);
  SR = TabSR[NoButton - 5];
  char Param[] = "symbolrate";
  char Value[255];
  sprintf(Value, "%d", SR);
  printf("************** Set SR = %s\n",Value);
  SetConfigParam(PATH_CONFIG, Param, Value);
}

void SelectFec(int NoButton)  // FEC
{
  SelectInGroup(10, 14 ,NoButton ,1);
  fec = TabFec[NoButton - 10];
  char Param[]="fec";
  char Value[255];
  sprintf(Value, "%d", fec);
  printf("************** Set FEC = %s\n",Value);
  SetConfigParam(PATH_CONFIG, Param, Value);
}

void SelectSource(int NoButton,int Status)  //Input mode
{
  SelectInGroup(15, 19, NoButton, Status);
  SelectInGroup(25 + 15, 25 + 18, NoButton, Status);
  SelectInGroup(25 + 2, 25 + 2, NoButton, Status);
  strcpy(ModeInput,TabModeInput[NoButton-15]);
  printf("************** Set Input Mode = %s\n",ModeInput);
  char Param[]="modeinput";
  SetConfigParam(PATH_CONFIG,Param,ModeInput);

  // Load the Pi Cam driver for CAMMPEG-2 modes
  if((strcmp(ModeInput,"CAMMPEG-2")==0)||(strcmp(ModeInput,"CAMHDMPEG-2")==0))
  {
    system("sudo modprobe bcm2835_v4l2");
  }
  // Replace Contest Numbers with BATC Logo
  system("sudo fbi -T 1 -noverbose -a \"/home/pi/rpidatv/scripts/images/BATC_Black.png\" >/dev/null 2>/dev/null");
  system("(sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &");
}

void SelectPTT(int NoButton,int Status)  // TX/RX
{
  SelectInGroup(20, 21, NoButton, Status);
}

void SelectCaption(int NoButton,int Status)  // Caption on or off
{
  char Param[]="caption";
  char Value[255];
  char Feedback[7];
  strcpy(Param,"caption");
  GetConfigParam(PATH_CONFIG, Param, Value);
  if(strcmp(Value, "on") == 0)
  {
    Status=0;
    SetConfigParam(PATH_CONFIG,Param,"off");
    strcpy(Feedback, "off");
  }
  else
  {
    Status=1;
    SetConfigParam(PATH_CONFIG,Param,"on");
    strcpy(Feedback, "on");
  }
  SelectInGroup(25 + 3, 25 + 3, 25 + NoButton, Status);
  printf("************** Set Caption %s \n", Feedback);
}

void SelectSTD(int NoButton,int Status)  // PAL or NTSC
{
  char USBVidDevice[255];
  char Param[255];
  char SetStandard[255];

  SelectInGroup(25 + 8, 25 + 9, 25 + NoButton, Status);
  strcpy(ModeSTD, TabModeSTD[NoButton - 8]);
  printf("************** Set Input Standard = %s\n", ModeSTD);
  strcpy(Param, "analogcamstandard");
  SetConfigParam(PATH_CONFIG,Param,ModeSTD);

  // Now Set the Analog Capture (input) Standard
  GetUSBVidDev(USBVidDevice);
  if (strlen(USBVidDevice) == 12)  // /dev/video* with a new line
  {
    USBVidDevice[strcspn(USBVidDevice, "\n")] = 0;  //remove the newline
    strcpy(SetStandard, "v4l2-ctl -d ");
    strcat(SetStandard, USBVidDevice);
    strcat(SetStandard, " --set-standard=");
    strcat(SetStandard, ModeSTD);
    printf(SetStandard);
    system(SetStandard);
  }
}

void SelectAudio(int NoButton,int Status)  // Audio Input
{
  SelectInGroup(25 + 5, 25 + 7, 25 + NoButton, Status);
  strcpy(ModeAudio,TabModeAudio[NoButton - 5]);
  printf("************** Set Audio Input = %s\n",ModeAudio);
  char Param[]="audio";
  SetConfigParam(PATH_CONFIG,Param,ModeAudio);
}

void SelectOP(int NoButton,int Status)  //Output mode
{
  SelectInGroup(25 + 10, 25 + 14, 25 + NoButton, Status);
  SelectInGroup(25 + 19, 25 + 19, 25 + NoButton, Status);
  strcpy(ModeOP, TabModeOP[NoButton -10]);
  printf("************** Set Output Mode = %s\n",ModeOP);
  char Param[]="modeoutput";
  SetConfigParam(PATH_CONFIG, Param, ModeOP);
}

void SelectSource2(int NoButton,int Status)  //Input mode
{
  SelectInGroup(15, 19, 25 + NoButton, Status);
  SelectInGroup(25 + 15, 25 + 18, 25 + NoButton, Status);
  SelectInGroup(25 + 2, 25 + 2, 25 + NoButton, Status);
  if(NoButton == 2) //Cam HD
  {
    strcpy(ModeInput,TabModeInput[9]);
  }
  else
  {
    strcpy(ModeInput,TabModeInput[NoButton-10]);
  }
  printf("************** Menu 2 Set Input Mode = %s\n",ModeInput);
  char Param[]="modeinput";
  SetConfigParam(PATH_CONFIG,Param,ModeInput);

  // Load the Pi Cam driver for CAMMPEG-2 modes
  if((strcmp(ModeInput,"CAMMPEG-2")==0)||(strcmp(ModeInput,"CAMHDMPEG-2")==0))
  {
    system("sudo modprobe bcm2835_v4l2");
  }
  // Just in case
  // Replace Contest Numbers with BATC Logo
  system("sudo fbi -T 1 -noverbose -a \"/home/pi/rpidatv/scripts/images/BATC_Black.png\" >/dev/null 2>/dev/null");
  system("(sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &");
}

void TransmitStart()
{
  printf("Transmit Start\n");

  char Param[255];
  char Value[255];
  char Cmd[255];
  #define PATH_SCRIPT_A "sudo /home/pi/rpidatv/scripts/a.sh >/dev/null 2>/dev/null"

  strcpy(Param,"modeinput");
  GetConfigParam(PATH_CONFIG,Param,Value);
  strcpy(ModeInput,Value);

  // Check if MPEG-2 camera mode selected 
  if((strcmp(ModeInput,"CAMMPEG-2")==0)||(strcmp(ModeInput,"CAMHDMPEG-2")==0))
  {
    // Start the viewfinder
    finish();
    system("v4l2-ctl --overlay=1 >/dev/null 2>/dev/null");
    strcpy(ScreenState, "TXwithImage");
  }

  // Check if H264 Camera selected
  if(strcmp(ModeInput,"CAMH264") == 0)
  {
    // Start the viewfinder 
    finish();
    strcpy(ScreenState, "TXwithImage");
  }

  // Check if a desktop mode is selected; if so, display desktop
  if  ((strcmp(ModeInput,"CONTEST") == 0) 
    || (strcmp(ModeInput,"DESKTOP") == 0)
    || (strcmp(ModeInput,"PATERNAUDIO") == 0))
  {
    finish();
    strcpy(ScreenState, "TXwithImage");
  }

  // Check if non-display input mode selected.  If so, turn off response to buttons.
  if ((strcmp(ModeInput,"ANALOGCAM") == 0)
    ||(strcmp(ModeInput,"ANALOGMPEG-2") == 0)
    ||(strcmp(ModeInput,"CARRIER") == 0)
    ||(strcmp(ModeInput,"TESTMODE") == 0)
    ||(strcmp(ModeInput,"IPTSIN") == 0))
  {
     strcpy(ScreenState, "TXwithMenu");
  }

  // Check if CARDMPEG-2 selected; if so, turn off buttons and display card
  if(strcmp(ModeInput,"CARDMPEG-2")==0)
  {
    finish();
    strcpy(ScreenState, "TXwithImage");

    // Check if Caption enabled.  If so draw caption on image and display
    strcpy(Param,"caption");
    GetConfigParam(PATH_CONFIG,Param,Value);
    if(strcmp(Value,"on")==0)
    {
      strcpy(Param,"call");
      GetConfigParam(PATH_CONFIG,Param,Value);
      system("rm /home/pi/rpidatv/scripts/images/caption.png >/dev/null 2>/dev/null");
      system("rm /home/pi/rpidatv/scripts/images/tcf2.jpg >/dev/null 2>/dev/null");
      strcpy(Cmd, "convert -size 720x80 xc:transparent -fill white -gravity Center -pointsize 40 -annotate 0 \"");
      strcat(Cmd, Value);
      strcat(Cmd, "\" /home/pi/rpidatv/scripts/images/caption.png");
      system(Cmd);
      system("convert /home/pi/rpidatv/scripts/images/tcf.jpg /home/pi/rpidatv/scripts/images/caption.png -geometry +0+475 -composite /home/pi/rpidatv/scripts/images/tcf2.jpg");
      system("sudo fbi -T 1 -noverbose -a \"/home/pi/rpidatv/scripts/images/tcf2.jpg\" >/dev/null 2>/dev/null");
    }
    else 
    {
      system("sudo fbi -T 1 -noverbose -a \"/home/pi/rpidatv/scripts/images/tcf.jpg\" >/dev/null 2>/dev/null");
    }
    system("(sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &");
  }

  // Call a.sh to transmit
  system(PATH_SCRIPT_A);
}

void TransmitStop()
{
  char Param[255];
  char Value[255];
  printf("Transmit Stop\n");

  // Turn the VCO off
  system("sudo /home/pi/rpidatv/bin/adf4351 off");

  // Stop DATV Express transmitting
  char expressrx[50];
  strcpy(Param,"modeoutput");
  GetConfigParam(PATH_CONFIG,Param,Value);
  strcpy(ModeOutput,Value);
  if(strcmp(ModeOutput,"DATVEXPRESS")==0)
  {
    strcpy( expressrx, "echo \"set ptt rx\" >> /tmp/expctrl" );
    system(expressrx);
    strcpy( expressrx, "echo \"set car off\" >> /tmp/expctrl" );
    system(expressrx);
    system("sudo killall netcat >/dev/null 2>/dev/null");
  }

  // Kill the key processes as nicely as possible
  system("sudo killall rpidatv >/dev/null 2>/dev/null");
  system("sudo killall ffmpeg >/dev/null 2>/dev/null");
  system("sudo killall tcanim >/dev/null 2>/dev/null");
  system("sudo killall avc2ts >/dev/null 2>/dev/null");
  system("sudo killall netcat >/dev/null 2>/dev/null");

  // Turn the Viewfinder off
  system("v4l2-ctl --overlay=0 >/dev/null 2>/dev/null");

  // Stop the audio relay in CompVid mode
  system("sudo killall arecord >/dev/null 2>/dev/null");

  // Then pause and make sure that avc2ts has really been stopped (needed at high SRs)
  usleep(1000);
  system("sudo killall -9 avc2ts >/dev/null 2>/dev/null");

  // And make sure rpidatv has been stopped (required for brief transmit selections)
  system("sudo killall -9 rpidatv >/dev/null 2>/dev/null");

  // Ensure PTT off.  Required for carrier mode
  pinMode(GPIO_PTT, OUTPUT);
  digitalWrite(GPIO_PTT, LOW);
}

void coordpoint(VGfloat x, VGfloat y, VGfloat size, VGfloat pcolor[4]) {
  setfill(pcolor);
  Circle(x, y, size);
  setfill(pcolor);
}

fftwf_complex *fftout=NULL;
#define FFT_SIZE 256

int FinishedButton=0;

void *DisplayFFT(void * arg)
{
	FILE * pFileIQ = NULL;
	int fft_size=FFT_SIZE;
	fftwf_complex *fftin;
	fftin = (fftwf_complex *)fftwf_malloc(sizeof(fftwf_complex) * fft_size);
	fftout = (fftwf_complex *)fftwf_malloc(sizeof(fftwf_complex) * fft_size);
	fftwf_plan plan ;
	plan = fftwf_plan_dft_1d(fft_size, fftin, fftout, FFTW_FORWARD, FFTW_ESTIMATE );

	system("mkfifo fifo.iq >/dev/null 2>/dev/null");
	printf("Entering FFT thread\n");
	pFileIQ = fopen("fifo.iq", "r");

	while(FinishedButton==0)
	{
		//int Nbread; // value set later but not used
		//int log2_N=11; //FFT 1024 not used?
		//int ret; // not used?

		//Nbread=fread( fftin,sizeof(fftwf_complex),FFT_SIZE,pFileIQ);
		fread( fftin,sizeof(fftwf_complex),FFT_SIZE,pFileIQ);
		fftwf_execute( plan );

		//printf("NbRead %d %d\n",Nbread,sizeof(struct GPU_FFT_COMPLEX));

		fseek(pFileIQ,(1200000-FFT_SIZE)*sizeof(fftwf_complex),SEEK_CUR);
	}
	fftwf_free(fftin);
	fftwf_free(fftout);
  return NULL;
}

void *WaitButtonEvent(void * arg)
{
  int rawX, rawY, rawPressure;
  while(getTouchSample(&rawX, &rawY, &rawPressure)==0);
  FinishedButton=1;
  return NULL;
}

void ProcessLeandvb()
{
  #define PATH_SCRIPT_LEAN "sudo /home/pi/rpidatv/scripts/leandvbgui.sh 2>&1"
  char *line=NULL;
  size_t len = 0;
  ssize_t read;
  FILE *fp;
  VGfloat shapecolor[4];
  RGBA(255, 255, 128,1, shapecolor);

  printf("Entering LeandProcess\n");
  FinishedButton=0;

  // create Thread FFT
  pthread_create (&thfft,NULL, &DisplayFFT, NULL);

  // Create Wait Button thread
  pthread_create (&thbutton,NULL, &WaitButtonEvent, NULL);

  fp=popen(PATH_SCRIPT_LEAN, "r");
  if(fp==NULL) printf("Process error\n");

  while (((read = getline(&line, &len, fp)) != -1)&&(FinishedButton==0))
  {

        char  strTag[20];
	int NbData;
	static int Decim=0;
	sscanf(line,"%s ",strTag);
	char * token;
	static int Lock=0;
	static float SignalStrength=0;
	static float MER=0;
	static float FREQ=0;
	if((strcmp(strTag,"SYMBOLS")==0))
	{

		token = strtok(line," ");
		token = strtok(NULL," ");
		sscanf(token,"%d",&NbData);

		if(Decim%25==0)
		{
			//Start(wscreen,hscreen);
			Fill(255, 255, 255, 1);
			Roundrect(0,0,256,hscreen, 10, 10);
			BackgroundRGB(0,0,0,0);
			//Lock status
			char sLock[100];
			if(Lock==1)
			{
				strcpy(sLock,"Lock");
				Fill(0,255,0, 1);

			}
			else
			{
				strcpy(sLock,"----");
				Fill(255,0,0, 1);
			}
			Roundrect(200,0,100,50, 10, 10);
			Fill(255, 255, 255, 1);				   // White text
			Text(200, 20, sLock, SerifTypeface, 25);

			//Signal Strength
			char sSignalStrength[100];
			sprintf(sSignalStrength,"%3.0f",SignalStrength);

			Fill(255-SignalStrength,SignalStrength,0,1);
			Roundrect(350,0,20+SignalStrength/2,50, 10, 10);
			Fill(255, 255, 255, 1);				   // White text
			Text(350, 20, sSignalStrength, SerifTypeface, 25);

			//MER 2-30
			char sMER[100];
			sprintf(sMER,"%2.1fdB",MER);
			Fill(255-MER*8,(MER*8),0,1);
			Roundrect(500,0,(MER*8),50, 10, 10);
			Fill(255, 255, 255, 1);				   // White text
			Text(500,20, sMER, SerifTypeface, 25);
		}

		if(Decim%25==0)
		{
			static VGfloat PowerFFTx[FFT_SIZE];
			static VGfloat PowerFFTy[FFT_SIZE];
			StrokeWidth(2);

			Stroke(150, 150, 200, 0.8);
			int i;
			if(fftout!=NULL)
			{
			for(i=0;i<FFT_SIZE;i+=2)
			{

				PowerFFTx[i]=(i<FFT_SIZE/2)?(FFT_SIZE+i)/2:i/2;
				PowerFFTy[i]=log10f(sqrt(fftout[i][0]*fftout[i][0]+fftout[i][1]*fftout[i][1])/FFT_SIZE)*100;	
			Line(PowerFFTx[i],0,PowerFFTx[i],PowerFFTy[i]);
			//Polyline(PowerFFTx,PowerFFTy,FFT_SIZE);

			//Line(0, (i<1024/2)?(1024/2+i)/2:(i-1024/2)/2,  (int)sqrt(fftout[i][0]*fftout[i][0]+fftout[i][1]*fftout[i][1])*100/1024,(i<1024/2)?(1024/2+i)/2:(i-1024/2)/2);

			}
			//Polyline(PowerFFTx,PowerFFTy,FFT_SIZE);
			}
			//FREQ
			Stroke(0, 0, 255, 0.8);
			//Line(FFT_SIZE/2+FREQ/2/1024000.0,0,FFT_SIZE/2+FREQ/2/1024000.0,hscreen/2);
			Line(FFT_SIZE/2,0,FFT_SIZE/2,10);
			Stroke(0, 0, 255, 0.8);
			Line(0,hscreen-300,256,hscreen-300);
			StrokeWidth(10);
			Line(128+(FREQ/40000.0)*256.0,hscreen-300-20,128+(FREQ/40000.0)*256.0,hscreen-300+20);

			char sFreq[100];
			sprintf(sFreq,"%2.1fkHz",FREQ/1000.0);
			Text(0,hscreen-300+25, sFreq, SerifTypeface, 20);

		}
		if((Decim%25)==0)
		{
			int x,y;
			Decim++;
			int i;
			StrokeWidth(2);
			Stroke(255, 255, 128, 0.8);
			for(i=0;i<NbData;i++)
			{
				token=strtok(NULL," ");
				sscanf(token,"%d,%d",&x,&y);
				coordpoint(x+128, hscreen-(y+128), 5, shapecolor);

				Stroke(0, 255, 255, 0.8);
				Line(0,hscreen-128,256,hscreen-128);
				Line(128,hscreen,128,hscreen-256);

			}


			End();
			//usleep(40000);

		}
		else
			Decim++;
		/*if(Decim%1000==0)
		{
			char FileSave[255];
			FILE *File;
			sprintf(FileSave,"Snap%d_%dx%d.png",Decim,wscreen,hscreen);
			File=fopen(FileSave,"w");

			dumpscreen(wscreen,hscreen,File);
			fclose(File);
		}*/
		/*if(Decim>200)
		{
			Decim=0;
			Start(wscreen,hscreen);

		}*/

	}

	if((strcmp(strTag,"SS")==0))
	{
		token = strtok(line," ");
		token = strtok(NULL," ");
		sscanf(token,"%f",&SignalStrength);
		//printf("Signal %f\n",SignalStrength);
	}

	if((strcmp(strTag,"MER")==0))
	{

		token = strtok(line," ");
		token = strtok(NULL," ");
		sscanf(token,"%f",&MER);
		//printf("MER %f\n",MER);
	}

	if((strcmp(strTag,"FREQ")==0))
	{

		token = strtok(line," ");
		token = strtok(NULL," ");
		sscanf(token,"%f",&FREQ);
		//printf("FREQ %f\n",FREQ);
	}

	if((strcmp(strTag,"LOCK")==0))
	{

		token = strtok(line," ");
		token = strtok(NULL," ");
		sscanf(token,"%d",&Lock);
	}

	free(line);
	line=NULL;
    }
  printf("End Lean - Clean\n");
  system("sudo killall rtl_sdr >/dev/null 2>/dev/null");
  system("sudo killall fbi >/dev/null 2>/dev/null");  // kill any previous images
  system("sudo fbi -T 1 -noverbose -a /home/pi/rpidatv/scripts/images/BATC_Black.png");  // Add logo image

  MsgBox4("Switching back to Main Menu", "", "Please wait for the receiver", "to clean up files");
  usleep(5000000); // Time to FFT end reading samples
  pthread_join(thfft, NULL);
	//pclose(fp);
	pthread_join(thbutton, NULL);
	printf("End Lean\n");
 
  system("sudo killall hello_video.bin >/dev/null 2>/dev/null");
  system("sudo killall fbi >/dev/null 2>/dev/null");
  system("sudo killall leandvb >/dev/null 2>/dev/null");
  system("sudo killall ts2es >/dev/null 2>/dev/null");
  finish();
  strcpy(ScreenState, "RXwithImage");            //  Signal to display touch menu without further touch
}

void ReceiveStart()
{
  strcpy(ScreenState, "RXwithImage");
  system("sudo killall hello_video.bin >/dev/null 2>/dev/null");
  ProcessLeandvb();
}

void ReceiveStop()
{
  system("sudo killall leandvb >/dev/null 2>/dev/null");
  system("sudo killall hello_video.bin >/dev/null 2>/dev/null");
  printf("Receive Stop\n");
}

void wait_touch()
// Wait for Screen touch, ignore position, but then move on
// Used to let user acknowledge displayed text
{
  int rawX, rawY, rawPressure;
  printf("wait_touch called\n");

  // Check if screen touched, if not, wait 0.1s and check again
  while(getTouchSample(&rawX, &rawY, &rawPressure)==0)
  {
    usleep(100000);
  }
  // Screen has been touched
  printf("wait_touch exit\n");
}

void MsgBox(const char *message)
{
  //init(&wscreen, &hscreen);  // Restart the gui
  BackgroundRGB(0,0,0,255);  // Black background
  Fill(255, 255, 255, 1);    // White text

  TextMid(wscreen/2, hscreen/2, message, SansTypeface, 25);

  VGfloat tw = TextWidth("Touch Screen to Continue", SansTypeface, 25);
  Text(wscreen / 2.0 - (tw / 2.0), 20, "Touch Screen to Continue", SansTypeface, 25);
  End();
  printf("MsgBox called and waiting for touch\n");
}

void MsgBox2(const char *message1, const char *message2)
{
  //init(&wscreen, &hscreen);  // Restart the gui
  BackgroundRGB(0,0,0,255);  // Black background
  Fill(255, 255, 255, 1);    // White text

  VGfloat th = TextHeight(SansTypeface, 25);


  TextMid(wscreen/2, hscreen/2+th, message1, SansTypeface, 25);
  TextMid(wscreen/2, hscreen/2-th, message2, SansTypeface, 25);

  VGfloat tw = TextWidth("Touch Screen to Continue", SansTypeface, 25);
  Text(wscreen / 2.0 - (tw / 2.0), 20, "Touch Screen to Continue", SerifTypeface, 25);
  End();
  printf("MsgBox2 called and waiting for touch\n");
}

void MsgBox4(const char *message1, const char *message2, const char *message3, const char *message4)
{
  //init(&wscreen, &hscreen);  // Restart the gui
  BackgroundRGB(0,0,0,255);  // Black background
  Fill(255, 255, 255, 1);    // White text

  VGfloat th = TextHeight(SansTypeface, 25);

  TextMid(wscreen/2, hscreen/2 + 2.1 * th, message1, SansTypeface, 25);
  TextMid(wscreen/2, hscreen/2 + 0.7 * th, message2, SansTypeface, 25);
  TextMid(wscreen/2, hscreen/2 - 0.7 * th, message3, SansTypeface, 25);
  TextMid(wscreen/2, hscreen/2 - 2.1 * th, message4, SansTypeface, 25);

  End();
  printf("MsgBox4 called and waiting for touch\n");
}


void InfoScreen()
{
  char result[256];

  // Look up and format all the parameters to be displayed
  char swversion[256] = "Software Version: ";
  GetSWVers(result);
  strcat(swversion, result);

  char ipaddress[256] = "IP: ";
  strcpy(result, "Not connected");
  GetIPAddr(result);
  strcat(ipaddress, result);

  char CPUTemp[256];
  GetCPUTemp(result);
  sprintf(CPUTemp, "CPU temp=%.1f\'C      GPU ", atoi(result)/1000.0);
  GetGPUTemp(result);
  strcat(CPUTemp, result);

  char PowerText[256] = "Temperature has been or is too high";
  GetThrottled(result);
  result[strlen(result) - 1]  = '\0';
  if(strcmp(result,"throttled=0x0")==0)
  {
    strcpy(PowerText,"Temperatures and Supply voltage OK");
  }
  if(strcmp(result,"throttled=0x50000")==0)
  {
    strcpy(PowerText,"Low supply voltage event since start-up");
  }
  if(strcmp(result,"throttled=0x50005")==0)
  {
    strcpy(PowerText,"Low supply voltage now");
  }
  //strcpy(PowerText,result);
  //strcat(PowerText,"End");

  char TXParams1[256] = "TX ";
  GetConfigParam(PATH_CONFIG,"freqoutput",result);
  strcat(TXParams1, result);
  strcat(TXParams1, " MHz  SR ");
  GetConfigParam(PATH_CONFIG,"symbolrate",result);
  strcat(TXParams1, result);
  strcat(TXParams1, "  FEC ");
  GetConfigParam(PATH_CONFIG,"fec",result);
  strcat(TXParams1, result);
  strcat(TXParams1, "/");
  sprintf(result, "%d", atoi(result)+1);
  strcat(TXParams1, result);

  char TXParams2[256];
  char vcoding[256];
  char vsource[256];
  ReadModeInput(vcoding, vsource);
  strcpy(TXParams2, vcoding);
  strcat(TXParams2, " coding from ");
  strcat(TXParams2, vsource);
  
  char TXParams3[256];
  char ModeOutput[256];
  ReadModeOutput(ModeOutput);
  strcpy(TXParams3, "Output to ");
  strcat(TXParams3, ModeOutput);

  char SerNo[256];
  char CardSerial[256] = "SD Card Serial: ";
  GetSerNo(SerNo);
  strcat(CardSerial, SerNo);

  char DeviceTitle[256] = "Audio Devices:";

  char Device1[256]=" ";
  char Device2[256]=" ";
  GetDevices(Device1, Device2);

  // Initialise and calculate the text display
  init(&wscreen, &hscreen);  // Restart the gui
  BackgroundRGB(0,0,0,255);  // Black background
  Fill(255, 255, 255, 1);    // White text
  Fontinfo font = SerifTypeface;
  int pointsize = 20;
  VGfloat txtht = TextHeight(font, pointsize);
  VGfloat txtdp = TextDepth(font, pointsize);
  VGfloat linepitch = 1.1 * (txtht + txtdp);
  VGfloat linenumber = 1.0;
  VGfloat tw;

  // Display Text
  tw = TextWidth("BATC Portsdown Information Screen", font, pointsize);
  Text(wscreen / 2.0 - (tw / 2.0), hscreen - linenumber * linepitch, "BATC Portsdown Information Screen", font, pointsize);
  linenumber = linenumber + 2.0;

  Text(wscreen/12.0, hscreen - linenumber * linepitch, swversion, font, pointsize);
  linenumber = linenumber + 1.0;

  Text(wscreen/12.0, hscreen - linenumber * linepitch, ipaddress, font, pointsize);
  linenumber = linenumber + 1.0;

  Text(wscreen/12.0, hscreen - linenumber * linepitch, CPUTemp, font, pointsize);
  linenumber = linenumber + 1.0;

  Text(wscreen/12.0, hscreen - linenumber * linepitch, PowerText, font, pointsize);
  linenumber = linenumber + 1.0;

  Text(wscreen/12.0, hscreen - linenumber * linepitch, TXParams1, font, pointsize);
  linenumber = linenumber + 1.0;

  Text(wscreen/12.0, hscreen - linenumber * linepitch, TXParams2, font, pointsize);
  linenumber = linenumber + 1.0;

  Text(wscreen/12.0, hscreen - linenumber * linepitch, TXParams3, font, pointsize);
  linenumber = linenumber + 1.0;

  Text(wscreen/12.0, hscreen - linenumber * linepitch, CardSerial, font, pointsize);
  linenumber = linenumber + 1.0;

  Text(wscreen/12.0, hscreen - linenumber * linepitch, DeviceTitle, font, pointsize);
  linenumber = linenumber + 1.0;

  Text(wscreen/12.0, hscreen - linenumber * linepitch, Device1, font, pointsize);
  linenumber = linenumber + 1.0;

  Text(wscreen/12.0, hscreen - linenumber * linepitch, Device2, font, pointsize);
  linenumber = linenumber + 1.0;

    tw = TextWidth("Touch Screen to Continue",  font, pointsize);
  Text(wscreen / 2.0 - (tw / 2.0), 20, "Touch Screen to Continue",  font, pointsize);

  // Push to screen
  End();

  printf("Info Screen called and waiting for touch\n");
  wait_touch();
}

void rtlradio1()
{
  if(CheckRTL()==0)
  {
    char rtlcall[256];
    char card[256];
    GetPiAudioCard(card);
    strcpy(rtlcall, "(rtl_fm -M wbfm -f 92.9M | aplay -D plughw:");
    strcat(rtlcall, card);
    strcat(rtlcall, ",0 -f S16_LE -r32) &");
    system(rtlcall);

    MsgBox("Radio 4 92.9 FM");
    wait_touch();

    system("sudo killall rtl_fm >/dev/null 2>/dev/null");
    system("sudo killall aplay >/dev/null 2>/dev/null");
    usleep(1000);
    system("sudo killall -9 rtl_fm >/dev/null 2>/dev/null");
    system("sudo killall -9 aplay >/dev/null 2>/dev/null");
  }
  else
  {
    MsgBox("No RTL-SDR Connected");
    wait_touch();
  }
}

void rtlradio2()
{
  if(CheckRTL()==0)
  {
    char rtlcall[256];
    char card[256];
    GetPiAudioCard(card);
    strcpy(rtlcall, "(rtl_fm -M wbfm -f 106.0M | aplay -D plughw:");
    strcat(rtlcall, card);
    strcat(rtlcall, ",0 -f S16_LE -r32) &");
    system(rtlcall);

    MsgBox("SAM FM 106.0");
    wait_touch();

    system("sudo killall rtl_fm >/dev/null 2>/dev/null");
    system("sudo killall aplay >/dev/null 2>/dev/null");
    usleep(1000);
    system("sudo killall -9 rtl_fm >/dev/null 2>/dev/null");
    system("sudo killall -9 aplay >/dev/null 2>/dev/null");
  }
  else
  {
    MsgBox("No RTL-SDR Connected");
    wait_touch();
  }
}

void rtlradio3()
{
  if(CheckRTL()==0)
  {
    char rtlcall[256];
    char card[256];
    GetPiAudioCard(card);
    strcpy(rtlcall, "(rtl_fm -M fm -f 144.75M -s 20k -g 50 -l 0 -E pad | aplay -D plughw:");
    strcat(rtlcall, card);
    strcat(rtlcall, ",0 -f S16_LE -r20 -t raw) &");
    system(rtlcall);

    MsgBox("ATV Calling Channel 144.75 MHz FM");
    wait_touch();

    system("sudo killall rtl_fm >/dev/null 2>/dev/null");
    system("sudo killall aplay >/dev/null 2>/dev/null");
    usleep(1000);
    system("sudo killall -9 rtl_fm >/dev/null 2>/dev/null");
    system("sudo killall -9 aplay >/dev/null 2>/dev/null");
  }
  else
  {
    MsgBox("No RTL-SDR Connected");
    wait_touch();
  }
}
void rtlradio4()
{
  if(CheckRTL()==0)
  {
    char rtlcall[256];
    char card[256];
    GetPiAudioCard(card);
    strcpy(rtlcall, "(rtl_fm -M fm -f 145.7875M -s 20k -g 50 -l 0 -E pad | aplay -D plughw:");
    strcat(rtlcall, card);
    strcat(rtlcall, ",0 -f S16_LE -r20 -t raw) &");
    system(rtlcall);

    MsgBox("GB3BF Bedford 145.7875");
    wait_touch();

    system("sudo killall rtl_fm >/dev/null 2>/dev/null");
    system("sudo killall aplay >/dev/null 2>/dev/null");
    usleep(1000);
    system("sudo killall -9 rtl_fm >/dev/null 2>/dev/null");
    system("sudo killall -9 aplay >/dev/null 2>/dev/null");
  }
  else
  {
    MsgBox("No RTL-SDR Connected");
    wait_touch();
  }
}

void rtlradio5()
{
  if(CheckRTL()==0)
  {
    char rtlcall[256];
    char card[256];
    GetPiAudioCard(card);
    strcpy(rtlcall, "(rtl_fm -M fm -f 145.8M -s 20k -g 50 -l 0 -E pad | aplay -D plughw:");
    strcat(rtlcall, card);
    strcat(rtlcall, ",0 -f S16_LE -r20 -t raw) &");
    system(rtlcall);

    MsgBox("ISS Downlink 145.8 MHz FM");
    wait_touch();

    system("sudo killall rtl_fm >/dev/null 2>/dev/null");
    system("sudo killall aplay >/dev/null 2>/dev/null");
    usleep(1000);
    system("sudo killall -9 rtl_fm >/dev/null 2>/dev/null");
    system("sudo killall -9 aplay >/dev/null 2>/dev/null");
  }
  else
  {
    MsgBox("No RTL-SDR Connected");
    wait_touch();
  }
}

void rtl_tcp()
{
  if(CheckRTL()==0)
  {
    char rtl_tcp_start[256];
    char current_IP[256];
    char message1[256];
    char message2[256];
    char message3[256];
    char message4[256];
    GetIPAddr(current_IP);
    strcpy(rtl_tcp_start,"(rtl_tcp -a ");
    strcat(rtl_tcp_start, current_IP);
    strcat(rtl_tcp_start, ") &");
    system(rtl_tcp_start);

    strcpy(message1, "RTL-TCP server running on");
    strcpy(message2, current_IP);
    strcat(message2, ":1234");
    strcpy(message3, "Touch screen again to");
    strcpy(message4, "stop the RTL-TCP Server");
    MsgBox4(message1, message2, message3, message4);
    wait_touch();

    system("sudo killall rtl_tcp >/dev/null 2>/dev/null");
    usleep(500);
    system("sudo killall -9 rtl_tcp >/dev/null 2>/dev/null");
  }
  else
  {
    MsgBox("No RTL-SDR Connected");
    wait_touch();
  }
}

void do_snap()
{
  char USBVidDevice[255];

  GetUSBVidDev(USBVidDevice);
  if (strlen(USBVidDevice) != 12)  // /dev/video* with a new line
  {
    MsgBox("No EasyCap Found");
    wait_touch();
    UpdateWindow();
    BackgroundRGB(0,0,0,255);
  }
  else
  {
    finish();
    printf("do_snap\n");
    system("/home/pi/rpidatv/scripts/snap.sh >/dev/null 2>/dev/null");
    wait_touch();
    system("sudo killall fbi >/dev/null 2>/dev/null");  // kill any previous images
    system("sudo fbi -T 1 -noverbose -a /home/pi/rpidatv/scripts/images/BATC_Black.png  >/dev/null 2>/dev/null");  // Add logo image
    init(&wscreen, &hscreen);
    Start(wscreen,hscreen);
    BackgroundRGB(0,0,0,255);
    UpdateWindow();
    system("sudo killall fbi >/dev/null 2>/dev/null");  // kill fbi now
  }
}

void do_videoview()
{
  printf("videoview called\n");
  char Param[255];
  char Value[255];
  char USBVidDevice[255];
  char ffmpegCMD[255];

  GetUSBVidDev(USBVidDevice);
  if (strlen(USBVidDevice) != 12)  // /dev/video* with a new line
  {
    MsgBox("No EasyCap Found");
    wait_touch();
    UpdateWindow();
    BackgroundRGB(0,0,0,255);
  }
  else
  {
    // Make the display ready
    finish();

    // Create a thread to listen for display touches
    pthread_create (&thview,NULL, &WaitButtonEvent,NULL);

    strcpy(Param,"display");
    GetConfigParam(PATH_CONFIG,Param,Value);
    if ((strcmp(Value,"Waveshare")==0) || (strcmp(Value,"Waveshare4")==0))
    // Write directly to the touchscreen framebuffer for Waveshare displays
    {
      USBVidDevice[strcspn(USBVidDevice, "\n")] = 0;  //remove the newline
      strcpy(ffmpegCMD, "/home/pi/rpidatv/bin/ffmpeg -hide_banner -loglevel panic -f v4l2 -i ");
      strcat(ffmpegCMD, USBVidDevice);
      strcat(ffmpegCMD, " -vf \"yadif=0:1:0,scale=480:320\" -f rawvideo -pix_fmt rgb565 -vframes 3 /home/pi/tmp/frame.raw");
      system("sudo killall fbcp");
      // Refresh image until display touched
      while ( FinishedButton == 0 )
      {
        system("sudo rm /home/pi/tmp/* >/dev/null 2>/dev/null");
        system(ffmpegCMD);
        system("split -b 307200 -d -a 1 /home/pi/tmp/frame.raw /home/pi/tmp/frame");
        system("cat /home/pi/tmp/frame2>/dev/fb1");
      }
      // Screen has been touched so stop and tidy up
      system("fbcp &");
      system("sudo rm /home/pi/tmp/* >/dev/null 2>/dev/null");
    }
    else  // not a waveshare display so write to the main framebuffer
    {
      while ( FinishedButton == 0 )
      {
        system("/home/pi/rpidatv/scripts/view.sh");
        usleep(100000);
      }
    }
    // Screen has been touched
    printf("videoview exit\n");

    // Tidy up and display touch menu
    FinishedButton = 0;
    system("sudo killall fbi >/dev/null 2>/dev/null");  // kill any previous images
    system("sudo fbi -T 1 -noverbose -a /home/pi/rpidatv/scripts/images/BATC_Black.png  >/dev/null 2>/dev/null");  // Add logo image
    init(&wscreen, &hscreen);
    Start(wscreen,hscreen);
    BackgroundRGB(0,0,0,255);
    UpdateWindow();
    system("sudo killall fbi >/dev/null 2>/dev/null");  // kill fbi now
  }
}

void do_snapcheck()
{
  FILE *fp;
  char SnapIndex[256];
  int SnapNumber;
  int Snap;
  char fbicmd[256];

  // Fetch the Next Snap serial number
  fp = popen("cat /home/pi/snaps/snap_index.txt", "r");
  if (fp == NULL) 
  {
    printf("Failed to run command\n" );
    exit(1);
  }
  /* Read the output a line at a time - output it. */
  while (fgets(SnapIndex, 20, fp) != NULL)
  {
    printf("%s", SnapIndex);
  }
  /* close */
  pclose(fp);

  // Make the display ready
  finish();

  SnapNumber=atoi(SnapIndex);

  // Show the last 5 snaps
  for( Snap = SnapNumber - 1; Snap > SnapNumber - 6 && Snap >= 0; Snap = Snap - 1 )
  {
    sprintf(SnapIndex, "%d", Snap);
    strcpy(fbicmd, "sudo fbi -T 1 -noverbose -a /home/pi/snaps/snap");
    strcat(fbicmd, SnapIndex);
    strcat(fbicmd, ".jpg >/dev/null 2>/dev/null");
    system(fbicmd);
    wait_touch();
  }

  // Tidy up and display touch menu
  system("sudo killall fbi >/dev/null 2>/dev/null");  // kill any previous images
  system("sudo fbi -T 1 -noverbose -a /home/pi/rpidatv/scripts/images/BATC_Black.png  >/dev/null 2>/dev/null");  // Add logo image
  init(&wscreen, &hscreen);
  Start(wscreen,hscreen);
  BackgroundRGB(0,0,0,255);
  UpdateWindow();
}

static void cleanexit(int exit_code)
{
  TransmitStop();
  ReceiveStop();
  finish();
  printf("Clean Exit Code %d\n", exit_code);
  char Commnd[255];
  sprintf(Commnd,"stty echo");
  system(Commnd);
  sprintf(Commnd,"reset");
  system(Commnd);
  exit(exit_code);
}

void waituntil(int w,int h)
{
  // Wait for a screen touch and act on its position

  int rawX, rawY, rawPressure, i;

  // printf("Entering WaitUntil\n");
  // Start the main loop for the Touchscreen
  for (;;)
  {
    if (strcmp(ScreenState, "RXwithImage") != 0) // Don't wait for touch if returning from recieve
    {
      // Wait here until screen touched
      if (getTouchSample(&rawX, &rawY, &rawPressure)==0) continue;
    }

    // Screen has been touched or returning from recieve
    printf("x=%d y=%d\n", rawX, rawY);

    // React differently depending on context: char ScreenState[255]

      // Menu (normal)                              NormalMenu  (implemented)
      // Menu (Specials)                            SpecialMenu (not implemented yet)
      // Transmitting
        // with image displayed                     TXwithImage (implemented)
        // with menu displayed but not active       TXwithMenu  (implemented)
      // Receiving                                  RXwithImage (implemented)
      // Video Output                               VideoOut    (not implemented yet)
      // Snap View                                  SnapView    (not implemented yet)
      // VideoView                                  VideoView   (not implemented yet)
      // Snap                                       Snap        (not implemented yet)
      // SigGen?                                    SigGen      (not implemented yet)

     // Sort TXwithImage first:
    if (strcmp(ScreenState, "TXwithImage") == 0)
    {
      TransmitStop();
      ReceiveStop();
      system("sudo fbi -T 1 -noverbose -a /home/pi/rpidatv/scripts/images/BATC_Black.png  >/dev/null 2>/dev/null");  // Add logo image
      system("(sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &");
      init(&wscreen, &hscreen);
      Start(wscreen, hscreen);
      BackgroundRGB(255,255,255,255);
      SelectPTT(20,0);
      strcpy(ScreenState, "NormalMenu");
      UpdateWindow();
      continue;  // All reset, and Menu displayed so go back and wait for next touch
     }

    // Now Sort TXwithMenu:
    if (strcmp(ScreenState, "TXwithMenu") == 0)
    {
      TransmitStop();
      SelectPTT(i,0);
      strcpy(ScreenState, "NormalMenu");

      UpdateWindow();
      continue;
    }

    // Now deal with return from receiving
    if (strcmp(ScreenState, "RXwithImage") == 0)
    {
      ReceiveStop();
      system("sudo fbi -T 1 -noverbose -a /home/pi/rpidatv/scripts/images/BATC_Black.png  >/dev/null 2>/dev/null");  // Add logo image
      system("(sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &");
      init(&wscreen, &hscreen);
      Start(wscreen, hscreen);
      BackgroundRGB(255,255,255,255);
      SelectPTT(21,0);
      strcpy(ScreenState, "NormalMenu");
      UpdateWindow();
      continue;
    }

    // Not transmitting or receiving, so sort NormalMenu
    if (strcmp(ScreenState, "NormalMenu") == 0)
    {
      // For Menu (normal), check which button has been pressed (Returns 0 - 23)

      i = IsMenuButtonPushed(rawX, rawY);
      if (i == -1)
      {
        continue;  //Pressed, but not on a button so wait for the next touch
      }

      // Now do the reponses for each Menu in turn
      if (CurrentMenu == 1)  // Main Menu
      {
        printf("Button Event %d, Entering Menu 1 Case Statement\n",i);
        switch (i)
        {
        case 0:
          SelectFreq(i);
          UpdateWindow();
          break;
        case 1:
          SelectFreq(i);
          UpdateWindow();
          break;
        case 2:
          SelectFreq(i);
          UpdateWindow();
          break;
        case 3:
          SelectFreq(i);
          UpdateWindow();
          break;
        case 4:
          SelectFreq(i);
          UpdateWindow();
          break;
        case 5:
          SelectSR(i);
          UpdateWindow();
          break;
        case 6:
          SelectSR(i);
          UpdateWindow();
          break;
        case 7:
          SelectSR(i);
          UpdateWindow();
          break;
        case 8:
          SelectSR(i);
          UpdateWindow();
          break;
        case 9:
          SelectSR(i);
          UpdateWindow();
          break;
        case 10:
          SelectFec(i);
          UpdateWindow();
          break;
        case 11:
          SelectFec(i);
          UpdateWindow();
          break;
        case 12:
          SelectFec(i);
          UpdateWindow();
          break;
        case 13:
          SelectFec(i);
          UpdateWindow();
          break;
        case 14:
          SelectFec(i);
          UpdateWindow();
          break;
        case 15:
          SelectSource(i,1);
          UpdateWindow();
          break;
        case 16:
          SelectSource(i,1);
          UpdateWindow();
          break;
        case 17:
          SelectSource(i,1);
          UpdateWindow();
          break;
        case 18:
          SelectSource(i,1);
          UpdateWindow();
          break;
        case 19:
          SelectSource(i,1);
          UpdateWindow();
          break;
        case 20:
          //usleep(500000);
          SelectPTT(i,1);
          UpdateWindow();
          TransmitStart();
          break;
        case 21:
          if(CheckRTL()==0)
          {
            BackgroundRGB(0,0,0,255);
            Start(wscreen,hscreen);
            ReceiveStart();
            break;
          }
          else
          {
            MsgBox("No RTL-SDR Connected");
            wait_touch();
            BackgroundRGB(255,255,255,255);
            UpdateWindow();
          }
          break;
        case 22:
          ;
          break;
        case 23:
          printf("MENU 2 \n");
          CurrentMenu=2;
          BackgroundRGB(0,0,0,255);
          Start_Highlights_Menu2();
          UpdateWindow();
          break;
        default:
          printf("Menu 1 Error\n");
        }
        continue;  // Completed Menu 1 action, go and wait for touch
      }

      if (CurrentMenu == 2)  // Menu 2
      {
        printf("Button Event %d, Entering Menu 2 Case Statement\n",i);
        switch (i)
        {
        case 0:                               // Shutdown
          finish();
          system("sudo shutdown now");
          break;
        case 1:                               // Reboot
          finish();
          system("sudo reboot now");
          break;
        case 2:                               // CamHD MPEG-2 Video Source
          SelectSource2(i,1);
          UpdateWindow();
          break;
        case 3:                               // Caption on/off
          SelectCaption(i,1);
          UpdateWindow();
          break;
        case 4:                               // Blank
          break;
        case 5:                               // Audio Mic
          SelectAudio(i,1);
          UpdateWindow();
          break;
        case 6:                               // Audio Auto
          SelectAudio(i,1);
          UpdateWindow();
          break;
        case 7:                               // Audio EasyCap
          SelectAudio(i,1);
          UpdateWindow();
          break;
        case 8:                               // PAL Input
          SelectSTD(i,1);
          UpdateWindow();
          break;
        case 9:                               // NTSC Input
          SelectSTD(i,1);
          UpdateWindow();
          break;
        case 10:                              // IQ Output
          SelectOP(i,1);
          UpdateWindow();
          break;
        case 11:                              // Ugly Output
          SelectOP(i,1);
          UpdateWindow();
          break;
        case 12:                              // DATV Express Output
          SelectOP(i,1);
          UpdateWindow();
          break;
        case 13:                              // BATC Streamer Output
          SelectOP(i,1);
          UpdateWindow();
          break;
        case 14:                              // Comp Video Output
          SelectOP(i,1);
          UpdateWindow();
          break;
        case 15:                              // Contest Card Source
          SelectSource2(i,1);
          UpdateWindow();
          break;
        case 16:                              // IPTS Source
          SelectSource2(i,1);
          UpdateWindow();
          break;
        case 17:                              // MPEG-2 Video Source
          SelectSource2(i,1);
          UpdateWindow();
          break;
        case 18:                              // MPEG-2 Card Source
          SelectSource2(i,1);
          UpdateWindow();
          break;
        case 19:                              // DTX-1 Output
          SelectOP(i,1);
           UpdateWindow();
         break;
        case 20:                              // Not shown
          ;
          break;
        case 21:                              // Not shown
          ;
          break;
        case 22:                              // Menu 1
          printf("MENU 1 \n");
          CurrentMenu=1;
          BackgroundRGB(255,255,255,255);
          Start_Highlights_Menu1();
          UpdateWindow();
          break;
        case 23:                              // Menu 3
          printf("MENU 3 \n");
          CurrentMenu=3;
          BackgroundRGB(0,0,0,255);
          Start_Highlights_Menu3();
          UpdateWindow();
          break;
        default:
          printf("Menu 2 Error\n");
        }
        continue;   // Completed Menu 2 action, go and wait for touch
      }

      if (CurrentMenu == 3)  // Menu 3
      {
        printf("Button Event %d, Entering Menu 2 Case Statement\n",i);
        switch (i)
        {
        case 0:                               // Exit to Linux (debug only)
          // cleanexit(128);
          break;
        case 1:                               // Select FreqShow
          if(CheckRTL()==0)
          {
            cleanexit(131);
          }
          else
          {
            MsgBox("No RTL-SDR Connected");
            wait_touch();
          }
          BackgroundRGB(0,0,0,255);
          UpdateWindow();
          break;
        case 2:                               // Display Info Page
          InfoScreen();
          BackgroundRGB(0,0,0,255);
          UpdateWindow();
          break;
        case 3:                               // Start RTL-TCP server
          rtl_tcp();
          BackgroundRGB(0,0,0,255);
          UpdateWindow();
          break;
        case 4:                               // Start Sig Gen and Exit
          cleanexit(130);
          break;
          /*
          finish();
          system("(sleep .5 && /home/pi/rpidatv/bin/siggen) &");
          char Commnd[255];
          sprintf(Commnd,"stty echo");
          system(Commnd);
          sprintf(Commnd,"reset");
          system(Commnd);
          exit(0);
          */
        case 5:                              // RTL Radio 1
          rtlradio1();
          BackgroundRGB(0,0,0,255);
          UpdateWindow();
          break;
        case 6:                              // RTL Radio 2
          rtlradio2();
          BackgroundRGB(0,0,0,255);
          UpdateWindow();
          break;
        case 7:                              // RTL Radio 3
          rtlradio3();
          BackgroundRGB(0,0,0,255);
          UpdateWindow();
          break;
        case 8:                              // RTL Radio 4
          rtlradio4();
          BackgroundRGB(0,0,0,255);
          UpdateWindow();
          break;
        case 9:                              // RTL Radio 5
          rtlradio5();
          BackgroundRGB(0,0,0,255);
          UpdateWindow();
          break;
        case 10:                               // Blank
          break;
        case 11:                               // Blank
          break;
        case 12:                               // Blank
          break;
        case 13:                               // Blank
          break;
        case 14:                               // Blank
          break;
        case 15:                              // Take Snap from EasyCap Input
          do_snap();
          UpdateWindow();
          break;
        case 16:                              // View EasyCap Input
          do_videoview();
          UpdateWindow();
          break;
        case 17:                              // Check Snaps
          do_snapcheck();
          UpdateWindow();
          break;
        case 18:                              // Calibrate Touch
          touchcal();
          BackgroundRGB(0,0,0,255);
          UpdateWindow();
          break;
        case 19:                              // Blank
         break;
        case 20:                              // Not shown
          break;
        case 21:                              // Not shown
          break;
        case 22:                              // Menu 1
          printf("MENU 1 \n");
          CurrentMenu=1;
          BackgroundRGB(255,255,255,255);
          Start_Highlights_Menu1();
          UpdateWindow();
          break;
        case 23:                              // Not Shown
          break;
        default:
          printf("Menu 3 Error\n");
        }
        continue;   // Completed Menu 3 action, go and wait for touch
      }
    }   
  }
}

void Start_Highlights_Menu1()
// Retrieves stored value for each group of buttons
// and then sets the correct highlight
{
  char Param[255];
  char Value[255];
  printf("Entering Start_Highlights_Menu1\n");

  // Frequency

  strcpy(Param,"freqoutput");
  GetConfigParam(PATH_CONFIG,Param,Value);
  strcpy(freqtxt,Value);
  printf("Value=%s %s\n",Value,"Freq");
  if(strcmp(Value,TabFreq[0])==0)
  {
    SelectInGroup(0,4,0,1);
  }
  if(strcmp(Value,TabFreq[1])==0)
  {
    SelectInGroup(0,4,1,1);
  }
  if(strcmp(Value,TabFreq[2])==0)
  {
    SelectInGroup(0,4,2,1);
  }
  if(strcmp(Value,TabFreq[3])==0)
  {
    SelectInGroup(0,4,3,1);
  }
  if(strcmp(Value,TabFreq[4])==0)
  {
    SelectInGroup(0,4,4,1);
  }

  // Symbol Rate

  strcpy(Param,"symbolrate");
  GetConfigParam(PATH_CONFIG,Param,Value);
  SR=atoi(Value);
  printf("Value=%s %s\n",Value,"SR");
  if ( SR == TabSR[0] )
  {
    SelectInGroup(5,9,5,1);
  }
  else if ( SR == TabSR[1] )
  {
    SelectInGroup(5,9,6,1);
  }
  else if ( SR == TabSR[2] )
  {
    SelectInGroup(5,9,7,1);
  }
  else if ( SR == TabSR[3] )
  {
    SelectInGroup(5,9,8,1);
  }
  else if ( SR == TabSR[4] )
  {
    SelectInGroup(5,9,9,1);
  }

  // FEC

  strcpy(Param,"fec");
  strcpy(Value,"");
  GetConfigParam(PATH_CONFIG,Param,Value);
  printf("Value=%s %s\n",Value,"Fec");
  fec=atoi(Value);
  switch(fec)
  {
    case 1:SelectInGroup(10,14,10,1);break;
    case 2:SelectInGroup(10,14,11,1);break;
    case 3:SelectInGroup(10,14,12,1);break;
    case 5:SelectInGroup(10,14,13,1);break;
    case 7:SelectInGroup(10,14,14,1);break;
  }

  // Input Mode

  strcpy(Param,"modeinput");
  GetConfigParam(PATH_CONFIG,Param,Value);
  strcpy(ModeInput,Value);
  printf("Value=%s %s\n",Value,"Input Mode");

  if(strcmp(Value,"CAMMPEG-2")==0)
  {
    SelectInGroup(15,19,15,1);
  }
  if(strcmp(Value,"CAMH264")==0)
  {
    SelectInGroup(15,19,16,1);
  }
  if(strcmp(Value,"PATERNAUDIO")==0)
  {
    SelectInGroup(15,19,17,1);
  }
  if(strcmp(Value,"ANALOGCAM")==0)
  {
    SelectInGroup(15,19,18,1);
  }
  if(strcmp(Value,"CARRIER")==0)
  {
    SelectInGroup(15,19,19,1);
  }
}

void Define_Menu1()
{
  int button = 0;
  color_t Green;
  color_t Blue;
  color_t Red;
  strcpy(MenuTitle[1], "BATC Portsdown Transmitter Main Menu"); 

  Green.r=0; Green.g=128; Green.b=0;
  Blue.r=0; Blue.g=0; Blue.b=128;
  Red.r=255; Red.g=0; Red.b=0;

  // Frequency - Bottom Row, Menu 1

  button = CreateButton(1, 0);
  AddButtonStatus(button,FreqLabel[0],&Blue);
  AddButtonStatus(button,FreqLabel[0],&Green);

  button = CreateButton(1, 1);
  AddButtonStatus(button,FreqLabel[1],&Blue);
  AddButtonStatus(button,FreqLabel[1],&Green);

  button = CreateButton(1, 2);
  AddButtonStatus(button,FreqLabel[2],&Blue);
  AddButtonStatus(button,FreqLabel[2],&Green);

  button = CreateButton(1, 3);
  AddButtonStatus(button,FreqLabel[3],&Blue);
  AddButtonStatus(button,FreqLabel[3],&Green);

  button = CreateButton(1, 4);
  AddButtonStatus(button,FreqLabel[4],&Blue);
  AddButtonStatus(button,FreqLabel[4],&Green);

  // Symbol Rate - 2nd Row, Menu 1

  button = CreateButton(1, 5);
  AddButtonStatus(button,SRLabel[0],&Blue);
  AddButtonStatus(button,SRLabel[0],&Green);

  button = CreateButton(1, 6);
  AddButtonStatus(button,SRLabel[1],&Blue);
  AddButtonStatus(button,SRLabel[1],&Green);

  button = CreateButton(1, 7);
  AddButtonStatus(button,SRLabel[2],&Blue);
  AddButtonStatus(button,SRLabel[2],&Green);

  button = CreateButton(1, 8);
  AddButtonStatus(button,SRLabel[3],&Blue);
  AddButtonStatus(button,SRLabel[3],&Green);

  button = CreateButton(1, 9);
  AddButtonStatus(button,SRLabel[4],&Blue);
  AddButtonStatus(button,SRLabel[4],&Green);

  // FEC - 3rd line up Menu 1

  button = CreateButton(1, 10);
  AddButtonStatus(button,"FEC 1/2",&Blue);
  AddButtonStatus(button,"FEC 1/2",&Green);

  button = CreateButton(1, 11);
  AddButtonStatus(button,"FEC 2/3",&Blue);
  AddButtonStatus(button,"FEC 2/3",&Green);

  button = CreateButton(1, 12);
  AddButtonStatus(button,"FEC 3/4",&Blue);
  AddButtonStatus(button,"FEC 3/4",&Green);

  button = CreateButton(1, 13);
  AddButtonStatus(button,"FEC 5/6",&Blue);
  AddButtonStatus(button,"FEC 5/6",&Green);

  button = CreateButton(1, 14);
  AddButtonStatus(button,"FEC 7/8",&Blue);
  AddButtonStatus(button,"FEC 7/8",&Green);

  //SOURCE - 4th line up Menu 1

  button = CreateButton(1, 15);
  AddButtonStatus(button,"CAM MPEG2",&Blue);
  AddButtonStatus(button,"CAM MPEG2",&Green);

  button = CreateButton(1, 16);
  AddButtonStatus(button,"CAM H264",&Blue);
  AddButtonStatus(button,"CAM H264",&Green);

  button = CreateButton(1, 17);
  AddButtonStatus(button,"Pattern",&Blue);
  AddButtonStatus(button,"Pattern",&Green);

  button = CreateButton(1, 18);
  AddButtonStatus(button,"VID H264",&Blue);
  AddButtonStatus(button,"VID H264",&Green);

  button = CreateButton(1, 19);
  AddButtonStatus(button,"Carrier",&Blue);
  AddButtonStatus(button,"Carrier",&Green);

  //TRANSMIT RECEIVE BLANK MENU2 - Top of Menu 1

  button = CreateButton(1, 20);
  AddButtonStatus(button,"TX   ",&Blue);
  AddButtonStatus(button,"TX ON",&Red);

  button = CreateButton(1, 21);
  AddButtonStatus(button,"RX   ",&Blue);
  AddButtonStatus(button,"RX ON",&Green);

  // Button 22 not used

  button = CreateButton(1, 23);
  AddButtonStatus(button," M2  ",&Blue);
  AddButtonStatus(button," M2  ",&Green);
}

void Define_Menu2()
{
  int button;
  color_t Green;
  color_t Blue;
  color_t Black;

  Green.r=0; Green.g=128; Green.b=0;
  Blue.r=0; Blue.g=0; Blue.b=128;
  Black.r=0; Black.g=0; Black.b=0;

  strcpy(MenuTitle[2], "BATC Portsdown Transmitter Menu 2"); 

  // Bottom Row, Menu 2

  button = CreateButton(2, 0);
  AddButtonStatus(button, "Shutdown", &Blue);
  AddButtonStatus(button, "Shutdown", &Green);

  button = CreateButton(2, 1);
  AddButtonStatus(button, "Reboot ", &Blue);
  AddButtonStatus(button, "Reboot ", &Green);

  button = CreateButton(2, 2);
  AddButtonStatus(button, "CAM HD ", &Blue);
  AddButtonStatus(button, "CAM HD ", &Green);

  button = CreateButton(2, 3);
  AddButtonStatus(button, "Caption", &Blue);
  AddButtonStatus(button, "Caption", &Green);

  //button = CreateButton(2, 4);
  //AddButtonStatus(button, " ", &Blue);
  //AddButtonStatus(button, " Menu 3", &Green);

  // 2nd Row, Menu 2

  button = CreateButton(2, 5);
  AddButtonStatus(button, "Audio Mic", &Blue);
  AddButtonStatus(button, "Audio Mic", &Green);

  button = CreateButton(2, 6);
  AddButtonStatus(button, "Audio Auto", &Blue);
  AddButtonStatus(button, "Audio Auto", &Green);

  button = CreateButton(2, 7);
  AddButtonStatus(button, "Audio EC ", &Blue);
  AddButtonStatus(button, "Audio EC ", &Green);

  button = CreateButton(2, 8);
  AddButtonStatus(button, " PAL in", &Blue);
  AddButtonStatus(button, " PAL in", &Green);

  button = CreateButton(2, 9);
  AddButtonStatus(button, "NTSC in", &Blue);
  AddButtonStatus(button, "NTSC in", &Green);

  // 3rd line up Menu 2

  button = CreateButton(2, 10);
  AddButtonStatus(button, "  IQ  ", &Blue);
  AddButtonStatus(button, "  IQ  ", &Green);

  button = CreateButton(2, 11);
  AddButtonStatus(button, " Ugly  ", &Blue);
  AddButtonStatus(button, " Ugly  ", &Green);

  button = CreateButton(2, 12);
  AddButtonStatus(button, "Express", &Blue);
  AddButtonStatus(button, "Express", &Green);

  button = CreateButton(2, 13);
  AddButtonStatus(button, " BATC ", &Blue);
  AddButtonStatus(button, " BATC ", &Green);

  button = CreateButton(2, 14);
  AddButtonStatus(button, "Vid Out", &Blue);
  AddButtonStatus(button, "Vid Out", &Green);

  //SOURCE - 4th line up Menu 2

  button = CreateButton(2, 15);
  AddButtonStatus(button, "CONTEST", &Blue);
  AddButtonStatus(button, "CONTEST", &Green);

  button = CreateButton(2, 16);
  AddButtonStatus(button, " IPTS IN", &Blue);
  AddButtonStatus(button, " IPTS IN", &Green);

  button = CreateButton(2, 17);
  AddButtonStatus(button, "VID MPEG2", &Blue);
  AddButtonStatus(button, "VID MPEG2", &Green);

  button = CreateButton(2, 18);
  AddButtonStatus(button, "Card MPEG2", &Blue);
  AddButtonStatus(button, "Card MPEG2", &Green);

  button = CreateButton(2, 19);
  AddButtonStatus(button, " DTX-1 ", &Blue);
  AddButtonStatus(button, " DTX-1 ", &Green);

  // Top of Menu 2

  button = CreateButton(2, 20);
  AddButtonStatus(button, " ", &Black);

  button = CreateButton(2, 21);
  AddButtonStatus(button, " ", &Black);

  button = CreateButton(2, 22);
  AddButtonStatus(button," M1  ",&Blue);
  AddButtonStatus(button," M1  ",&Green);

  button = CreateButton(2, 23);
  AddButtonStatus(button," M3  ",&Blue);
  AddButtonStatus(button," M3  ",&Green);
  
  // IndexButtonInArray = 47;
}

void Start_Highlights_Menu2()
// Retrieves stored value for each group of buttons
// and then sets the correct highlight
{
  char Param[255];
  char Value[255];
  int STD=1;

  // Audio Input

  strcpy(Param,"audio");
  strcpy(Value,"");
  GetConfigParam(PATH_CONFIG,Param,Value);
  printf("Value=%s %s\n",Value,"Audio");
  if(strcmp(Value,"mic")==0)
  {
    SelectInGroup(25 + 5, 25 + 7, 25 + 5, 1);
  }
  if(strcmp(Value, "auto") == 0)
  {
    SelectInGroup(25 + 5, 25 + 7, 25 + 6, 1);
  }
  if(strcmp(Value,"video") == 0)
  {
    SelectInGroup(25 + 5, 25 + 7, 25 + 7, 1);
  }

  // PAL or NTSC

  strcpy(Param,"analogcamstandard");
  GetConfigParam(PATH_CONFIG,Param,Value);
  STD=atoi(Value);
  printf("Value=%s %s\n",Value,"Video Standard");
  if ( STD == 6 ) //PAL
  {
    SelectInGroup(25 + 8, 25 + 9, 25 + 8, 1);
  }
  else if ( STD == 0 ) //NTSC
  {
    SelectInGroup(25 + 8, 25 + 9, 25 + 9, 1);
  }

  // Output Modes

  strcpy(Param,"modeoutput");
  strcpy(Value,"");
  GetConfigParam(PATH_CONFIG,Param,Value);
  printf("Value=%s %s\n",Value," Output");
  if(strcmp(Value,"IQ")==0)
  {
    SelectInGroup(25 + 10, 25 + 14, 25 + 10, 1);
  }
  if(strcmp(Value,"QPSKRF")==0)
  {
    SelectInGroup(25 + 10, 25 + 14, 25 + 11, 1);
  }
  if(strcmp(Value,"DATVEXPRESS")==0)
  {
    SelectInGroup(25 + 10, 25 + 14, 25 + 12, 1);
  }
  if(strcmp(Value,"BATC")==0)
  {
    SelectInGroup(25 + 10, 25 + 14, 25 + 13, 1);
  }
  if(strcmp(Value,"COMPVID")==0)
  {
    SelectInGroup(25 + 10, 25 + 14, 25 + 14, 1);
  }
  if(strcmp(Value,"DTX1")==0)
  {
    SelectInGroup(25 + 19, 25 + 19, 25 + 19, 1);
  }

  // Extra Input Modes

  strcpy(Param,"modeinput");
  GetConfigParam(PATH_CONFIG,Param,Value);
  strcpy(ModeInput,Value);
  printf("Value=%s %s\n",Value,"Input Mode");

  if(strcmp(Value,"CONTEST")==0)
  {
    SelectInGroup(25 + 15, 25 + 18, 25 + 15, 1);
  }
  if(strcmp(Value,"IPTSIN")==0)
  {
    SelectInGroup(25 + 15, 25 + 18, 25 + 16, 1);
  }
  if(strcmp(Value,"ANALOGMPEG-2")==0)
  {
    SelectInGroup(25 + 15, 25 + 18, 25 + 17, 1);
  }
  if(strcmp(Value,"CARDMPEG-2")==0)
  {
    SelectInGroup(25 + 15, 25 + 18, 25 + 18, 1);
  }
  if(strcmp(Value,"CAMHDMPEG-2")==0)
  {
    SelectInGroup(25 + 2, 25 + 2, 25 + 2, 1);
  }

  // Caption
  strcpy(Param,"caption");
  GetConfigParam(PATH_CONFIG,Param,Value);
  strcpy(Caption,Value);
  printf("Value=%s %s\n",Value,"Caption");
  if(strcmp(Value,"on")==0)
  {
    SelectInGroup(25 + 3, 25 + 3, 25 + 3, 1);
  }
  else
  {
    SelectInGroup(25 + 3, 25 + 3, 25 + 3, 0);
  }
}

void Define_Menu3()
{
  int button;
  color_t Green;
  color_t Blue;

  Green.r=0; Green.g=128; Green.b=0;
  Blue.r=0; Blue.g=0; Blue.b=128;

  strcpy(MenuTitle[3], "BATC Portsdown Transmitter Menu 3"); 

  // Bottom Row, Menu 3

  // button = CreateButton(3, 0);
  // AddButtonStatus(button, "  Clean ^  Exit  ", &Blue);

  button = CreateButton(3, 1);
  AddButtonStatus(button, "FreqShow^Spectrum", &Blue);

  button = CreateButton(3, 2);
  AddButtonStatus(button, " Info  ", &Blue);
  AddButtonStatus(button, " Info  ", &Green);

  button = CreateButton(3, 3);
  AddButtonStatus(button, "RTL-TCP", &Blue);
  AddButtonStatus(button, "RTL-TCP", &Green);

  button = CreateButton(3, 4);
  AddButtonStatus(button, "Sig Gen", &Blue);
  AddButtonStatus(button, "Sig Gen", &Green);

  // 2nd Row, Menu 3

  button = CreateButton(3, 5);
  AddButtonStatus(button, " 92.9 FM", &Blue);
  AddButtonStatus(button, " 92.9 FM", &Green);

  button = CreateButton(3, 6);
  AddButtonStatus(button, "106.0 FM", &Blue);
  AddButtonStatus(button, "106.0 FM", &Green);

  button = CreateButton(3, 7);
  AddButtonStatus(button, "144.75 FM", &Blue);
  AddButtonStatus(button, "144.75 FM", &Green);

  button = CreateButton(3, 8);
  AddButtonStatus(button, " GB3BF ", &Blue);
  AddButtonStatus(button, " GB3BF ", &Green);

  button = CreateButton(3, 9);
  AddButtonStatus(button, "145.8 FM ", &Blue);
  AddButtonStatus(button, "145.8 FM ", &Green);

  // 3rd line up Menu 3:  Not displayed

  // button = CreateButton(3, 10);
  // AddButtonStatus(button, "  IQ  ", &Blue);
  // AddButtonStatus(button, "  IQ  ", &Green);

  // button = CreateButton(3, 11);
  // AddButtonStatus(button, " Ugly  ", &Blue);
  // AddButtonStatus(button, " Ugly  ", &Green);

  // button = CreateButton(3, 12);
  // AddButtonStatus(button, "Express", &Blue);
  // AddButtonStatus(button, "Express", &Green);

  // button = CreateButton(3, 13);
  // AddButtonStatus(button, " BATC ", &Blue);
  // AddButtonStatus(button, " BATC ", &Green);

  // button = CreateButton(3, 14);
  // AddButtonStatus(button, "Vid Out", &Blue);
  // AddButtonStatus(button, "Vid Out", &Green);

  //SOURCE - 4th line up Menu 3: Snap, View and Check, Cal

  button = CreateButton(3, 15);
  AddButtonStatus(button, " Snap  ", &Blue);
  AddButtonStatus(button, " Snap  ", &Green);

  button = CreateButton(3, 16);
  AddButtonStatus(button, " View  ", &Blue);
  AddButtonStatus(button, " View  ", &Green);

  button = CreateButton(3, 17);
  AddButtonStatus(button, " Check ", &Blue);
  AddButtonStatus(button, " Check ", &Green);

  button = CreateButton(3, 18);
  AddButtonStatus(button, "Calibrate^  Touch  ", &Blue);
  //AddButtonStatus(button, "Card MPEG2", &Green);

  //button = CreateButton(3, 19);
  //AddButtonStatus(button, " DTX-1 ", &Blue);
  //AddButtonStatus(button, " DTX-1 ", &Green);

  // Top of Menu 3

  //button = CreateButton(3, 20);
  //AddButtonStatus(button, " ", &Black);

  //button = CreateButton(3, 21);
  //AddButtonStatus(button, " ", &Black);

  button = CreateButton(3, 22);
  AddButtonStatus(button," M1  ",&Blue);
  //AddButtonStatus(button," M1  ",&Green);

  //button = CreateButton(3, 23);
  //AddButtonStatus(button," M3  ",&Blue);
  //AddButtonStatus(button," M3  ",&Green);
  
//  IndexButtonInArray = 68;
}

void Start_Highlights_Menu3()
// Retrieves stored value for each group of buttons
// and then sets the correct highlight
{
  ;
  //char Param[255];
  //char Value[255];
  //int STD=1;
}

static void
terminate(int dummy)
{
  TransmitStop();
  ReceiveStop();
  finish();
  printf("Terminate\n");
  char Commnd[255];
  sprintf(Commnd,"stty echo");
  system(Commnd);
  sprintf(Commnd,"reset");
  system(Commnd);
  exit(1);
}

// main initializes the system and shows the picture. 

int main(int argc, char **argv)
{
  int NoDeviceEvent=0;
  saveterm();
  init(&wscreen, &hscreen);
  rawterm();
  int screenXmax, screenXmin;
  int screenYmax, screenYmin;
  int ReceiveDirect=0;
  int i;
  char Param[255];
  char Value[255];
  char USBVidDevice[255];
  char SetStandard[255];
 
  // Catch sigaction and call terminate
  for (i = 0; i < 16; i++)
  {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = terminate;
    sigaction(i, &sa, NULL);
  }

  // Set up wiringPi module
  if (wiringPiSetup() < 0)
  {
    return 0;
  }

  // Determine if using waveshare or waveshare B screen
  // Either by first argument or from rpidatvconfig.txt
  if(argc>1)
    Inversed=atoi(argv[1]);
  strcpy(Param,"display");
  GetConfigParam(PATH_CONFIG,Param,Value);
  if(strcmp(Value,"Waveshare")==0)
    Inversed=1;
  if(strcmp(Value,"WaveshareB")==0)
    Inversed=1;
  if(strcmp(Value,"Waveshare4")==0)
    Inversed=1;

  // Set the Band (and filter) Switching
  system ("sudo /home/pi/rpidatv/scripts/ctlfilter.sh");
  // and wait for it to finish using rpidatvconfig.txt
  usleep(100000);

  // Set the Analog Capture (input) Standard
  GetUSBVidDev(USBVidDevice);
  if (strlen(USBVidDevice) == 12)  // /dev/video* with a new line
  {
    strcpy(Param,"analogcamstandard");
    GetConfigParam(PATH_CONFIG,Param,Value);
    USBVidDevice[strcspn(USBVidDevice, "\n")] = 0;  //remove the newline
    strcpy(SetStandard, "v4l2-ctl -d ");
    strcat(SetStandard, USBVidDevice);
    strcat(SetStandard, " --set-standard=");
    strcat(SetStandard, Value);
    printf(SetStandard);
    system(SetStandard);
  }

  // Determine if ReceiveDirect 2nd argument 
	if(argc>2)
		ReceiveDirect=atoi(argv[2]);

	if(ReceiveDirect==1)
	{
		getTouchScreenDetails(&screenXmin,&screenXmax,&screenYmin,&screenYmax);
		 ProcessLeandvb(); // For FrMenu and no 
	}

// Check for presence of touchscreen
	for(NoDeviceEvent=0;NoDeviceEvent<5;NoDeviceEvent++)
	{
		if (openTouchScreen(NoDeviceEvent) == 1)
		{
			if(getTouchScreenDetails(&screenXmin,&screenXmax,&screenYmin,&screenYmax)==1) break;
		}
	}
	if(NoDeviceEvent==5) 
	{
		perror("No Touchscreen found");
		exit(1);
	}

  // Replace BATC Logo with IP address with BATC Logo alone
  system("sudo fbi -T 1 -noverbose -a \"/home/pi/rpidatv/scripts/images/BATC_Black.png\" >/dev/null 2>/dev/null");
  system("(sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &");

  // Calculate screen parameters
  scaleXvalue = ((float)screenXmax-screenXmin) / wscreen;
  printf ("X Scale Factor = %f\n", scaleXvalue);
  scaleYvalue = ((float)screenYmax-screenYmin) / hscreen;
  printf ("Y Scale Factor = %f\n", scaleYvalue);

  // Define button grid
  // -25 keeps right hand side symmetrical with left hand side
  wbuttonsize=(wscreen-25)/5;
  hbuttonsize=hscreen/6;

  // Read in the touchscreen Calibration
  ReadTouchCal();
  
  printf("Read in the presets from the Config file \n");
  // Read in the presets from the Config file
  ReadPresets();

  // Initialise all the button Status Indexes to 0
  InitialiseButtons();

  // Define the buttons for Menu 1
  printf("Entering Define Menu 1 \n");
  Define_Menu1();

  // Define the buttons for Menu 2
  Define_Menu2();

  // Define the buttons for Menu 3
  Define_Menu3();

  // Start the button Menu
  Start(wscreen,hscreen);

  // Determine button highlights
  Start_Highlights_Menu1();
  printf("Entering Update Window\n");  
  UpdateWindow();
  printf("Update Window\n");

  // Go and wait for the screen to be touched
  waituntil(wscreen,hscreen);

  // Not sure that the program flow ever gets here

  restoreterm();
  finish();
  return 0;
}
