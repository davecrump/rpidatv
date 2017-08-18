//
// shapedemo: testbed for OpenVG APIs
// Anthony Starks (ajstarks@gmail.com)
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

#define KWHT  "\x1B[37m"
#define KYEL  "\x1B[33m"

#define PATH_CONFIG "/home/pi/rpidatv/scripts/rpidatvconfig.txt"
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
	int x,y,w,h;

	status_t Status[MAX_STATUS];
	int IndexStatus;
	int NoStatus;
	int LastEventTime;
} button_t;

#define MAX_BUTTON 100
int IndexButtonInArray=0;
button_t ButtonArray[MAX_BUTTON];
int IsDisplayOn=0;
#define TIME_ANTI_BOUNCE 500
int CurrentMenu=1;
int Menu1Buttons=0;
int Menu2Buttons=0;
int Menu3Buttons=0;
int Menu4Buttons=0;

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

// Values for buttons
// May be over-written by values from from rpidatvconfig.txt:

int TabSR[5]={125,333,1000,2000,4000};
char SRLabel[5][255]={"SR 125","SR 333","SR1000","SR2000","SR4000"};
int TabFec[5]={1,2,3,5,7};
char TabModeInput[8][255]={"CAMMPEG-2","CAMH264","PATERNAUDIO","ANALOGCAM","CARRIER","CONTEST","IPTSIN","ANALOGMPEG-2"};
char TabFreq[5][255]={"71","146.5","437","1249","1255"};
char FreqLabel[5][255]={" 71 MHz ","146.5 MHz","437 MHz ","1249 MHz","1255 MHz"};
char TabModeAudio[3][255]={"mic","auto","video"};
char TabModeSTD[2][255]={"6","0"};
char TabModeOP[5][255]={"IQ","QPSKRF","DATVEXPRESS","BATC","COMPVID"};
int Inversed=0;//Display is inversed (Waveshare=1)

pthread_t thfft,thbutton,thview;

// Function Prototypes

void Start_Highlights_Menu1();
void Start_Highlights_Menu2();
void Start_Highlights_Menu3();

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
  fp = popen("ifconfig | grep -Eo \'inet (addr:)?([0-9]*\\.){3}[0-9]*\' | grep -Eo \'([0-9]*\\.){3}[0-9]*\' | grep -v \'127.0.0.1\'", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(IPAddress, 16, fp) != NULL) {
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
 * @brief Looks up the Audio Devices
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
  //printf("Read Freqs\n");
  //printf("Value=%d %s\n",len,"SR");
  }

}

//    strcpy(Param,"vfinder");
//    GetConfigParam(PATH_CONFIG,Param,Value);
//    if(strcmp(Value,"on")==0)
//    {
//      IsDisplayOn=0;
//      finish();
//      system("v4l2-ctl --overlay=1 >/dev/null 2>/dev/null");
//    }
//  strcpy(BackupConfigName,PathConfigFile);
//  strcat(BackupConfigName,".bak");

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

int IsButtonPushed(int NbButton,int x,int y)
{
  int  scaledX, scaledY;

  // scaledx range approx 0 - 700
  // scaledy range approx 0 - 480

  // Adjust registration of touchscreen for Waveshare
  int shiftX, shiftY;
  double factorX, factorY;

  shiftX=30; // move touch sensitive position left (-) or right (+).  Screen is 700 wide
  shiftY=-5; // move touch sensitive positions up (-) or down (+).  Screen is 480 high

  factorX=-0.4;  // expand (+) or contract (-) horizontal button space from RHS. Screen is 5.6875 wide
  factorY=-0.3;  // expand or contract vertical button space.  Screen is 8.53125 high

  // Switch axes for normal and waveshare displays
  if(Inversed==0) //TonTec
  {
    scaledX = x/scaleXvalue;
    scaledY = hscreen-y/scaleYvalue;
  }
  else //Waveshare (inversed)
  {
    scaledX = shiftX+wscreen-y/(scaleXvalue+factorX);
    scaledY = shiftY+hscreen-x/(scaleYvalue+factorY);
  }

  // printf("x=%d y=%d scaledx %d scaledy %d sxv %f syv %f\n",x,y,scaledX,scaledY,scaleXvalue,scaleYvalue);

  int margin=10;  // was 20

  if((scaledX<=(ButtonArray[NbButton].x+ButtonArray[NbButton].w-margin))&&(scaledX>=ButtonArray[NbButton].x+margin) &&
    (scaledY<=(ButtonArray[NbButton].y+ButtonArray[NbButton].h-margin))&&(scaledY>=ButtonArray[NbButton].y+margin)
    /*&&(mymillis()-ButtonArray[NbButton].LastEventTime>TIME_ANTI_BOUNCE)*/)
  {
    ButtonArray[NbButton].LastEventTime=mymillis();
    return 1;
  }
  else
    return 0;
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
	NewButton->LastEventTime=mymillis();
	return IndexButtonInArray++;
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
	TextMid(Button->x+Button->w/2, Button->y+Button->h/2, Button->Status[Button->NoStatus].Text, SerifTypeface, Button->w/strlen(Button->Status[Button->NoStatus].Text)/*25*/);	
}

void SetButtonStatus(int ButtonIndex,int Status)
{
	button_t *Button=&(ButtonArray[ButtonIndex]);
	Button->NoStatus=Status;
}

int GetButtonStatus(int ButtonIndex)
{
	button_t *Button=&(ButtonArray[ButtonIndex]);
	return	Button->NoStatus;

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

void UpdateWindow()
// Paint each button on the current Menu
{
  int i;
  int first;
  int last;

  // Calculate the Button numbers for the Current Menu
  switch (CurrentMenu)
  {
    case 1:
      first=0;
      last=Menu1Buttons;
      break;
    case 2:
      first=Menu1Buttons;
      last=Menu1Buttons+Menu2Buttons;
      break;
    case 3:
      first=Menu1Buttons+Menu2Buttons;
      last=Menu1Buttons+Menu2Buttons+Menu3Buttons;
      break;
    case 4:
      first=0;
      last=Menu1Buttons;
      break;
    default:
      first=0;
      last=Menu1Buttons;
      break;
  }

  for(i=first;i<last;i++)
    DrawButton(i);
  End();
}

void SelectInGroup(int StartButton,int StopButton,int NoButton,int Status)
{
	int i;
	for(i=StartButton;i<=StopButton;i++)
	{
		if(i==NoButton)
		 	SetButtonStatus(i,Status);
		else
			 SetButtonStatus(i,0);
	}
}

void SelectFreq(int NoButton)  //Frequency
{
  SelectInGroup(0,4,NoButton,1);
  strcpy(freqtxt,TabFreq[NoButton-0]);
  char Param[]="freqoutput";
  printf("************** Set Frequency = %s\n",freqtxt);
  SetConfigParam(PATH_CONFIG,Param,freqtxt);

  // Set the Band (and filter) Switching
  system ("sudo /home/pi/rpidatv/scripts/ctlfilter.sh");
  // And wait for it to finish using rpidatvconfig.txt
  usleep(100000);
}

void SelectSR(int NoButton)  // Symbol Rate
{
  SelectInGroup(5,9,NoButton,1);
  SR=TabSR[NoButton-5];
  char Param[]="symbolrate";
  char Value[255];
  sprintf(Value,"%d",SR);
  printf("************** Set SR = %s\n",Value);
  SetConfigParam(PATH_CONFIG,Param,Value);
}

void SelectFec(int NoButton)  // FEC
{
	SelectInGroup(10,14,NoButton,1);
	fec=TabFec[NoButton-10];
	char Param[]="fec";
	char Value[255];
	sprintf(Value,"%d",fec);
	printf("************** Set FEC = %s\n",Value);
	SetConfigParam(PATH_CONFIG,Param,Value);
}

void SelectSource(int NoButton,int Status)  //Input mode
{
  SelectInGroup(15,19,NoButton,Status);
  SelectInGroup(Menu1Buttons+15,Menu1Buttons+17,NoButton,Status);
  strcpy(ModeInput,TabModeInput[NoButton-15]);
  printf("************** Set Input Mode = %s\n",ModeInput);
  char Param[]="modeinput";
  SetConfigParam(PATH_CONFIG,Param,ModeInput);

  // Load the Pi Cam driver for CAMMPEG-2 mode
  if(strcmp(ModeInput,"CAMMPEG-2")==0)
  {
    system("sudo modprobe bcm2835_v4l2");
  }
  // Replace Contest Numbers with BATC Logo
  system("sudo fbi -T 1 -noverbose -a \"/home/pi/rpidatv/scripts/images/BATC_Black.png\" >/dev/null 2>/dev/null");
  system("(sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &");
}

void SelectPTT(int NoButton,int Status)  // TX/RX
{
	SelectInGroup(20,21,NoButton,Status);
}

void SelectCaption(int NoButton,int Status)  // Caption on or off
{
  char Param[]="caption";
  char Value[255];

            strcpy(Param,"caption");
            GetConfigParam(PATH_CONFIG,Param,Value);
            printf("Value=%s %s\n",Value,"Caption old");
            if(strcmp(Value,"on")==0)
            {
               Status=0;
               SetConfigParam(PATH_CONFIG,Param,"off");
            }
            else
            {
              Status=1;
               SetConfigParam(PATH_CONFIG,Param,"on");
            }
	SelectInGroup(Menu1Buttons+3,Menu1Buttons+3,NoButton,Status);

}


void SelectSTD(int NoButton,int Status)  // PAL or NTSC
{
	SelectInGroup(Menu1Buttons+8,Menu1Buttons+9,NoButton,Status);
	strcpy(ModeSTD,TabModeSTD[NoButton-Menu1Buttons-8]);
	printf("************** Set Input Standard = %s\n",ModeSTD);
	char Param[]="analogcamstandard";
	SetConfigParam(PATH_CONFIG,Param,ModeSTD);
}

void SelectAudio(int NoButton,int Status)  // Audio Input
{
	SelectInGroup(Menu1Buttons+5,Menu1Buttons+7,NoButton,Status);
	strcpy(ModeAudio,TabModeAudio[NoButton-Menu1Buttons-5]);
	printf("************** Set Audio Input = %s\n",ModeAudio);
	char Param[]="audio";
	SetConfigParam(PATH_CONFIG,Param,ModeAudio);
}

void SelectOP(int NoButton,int Status)  //Output mode
{
  SelectInGroup(Menu1Buttons+10,Menu1Buttons+14,NoButton,Status);
  strcpy(ModeOP,TabModeOP[NoButton-Menu1Buttons-10]);
  printf("************** Set Output Mode = %s\n",ModeOP);
  char Param[]="modeoutput";
  SetConfigParam(PATH_CONFIG,Param,ModeOP);
}

void SelectSource2(int NoButton,int Status)  //Input mode
{
  SelectInGroup(15,19,NoButton,Status);
  SelectInGroup(Menu1Buttons+15,Menu1Buttons+17,NoButton,Status);
  strcpy(ModeInput,TabModeInput[NoButton-Menu1Buttons-10]);
  printf("************** Menu 2 Set Input Mode = %s\n",ModeInput);
  char Param[]="modeinput";
  SetConfigParam(PATH_CONFIG,Param,ModeInput);
}

void TransmitStart()
{
  printf("Transmit Start\n");

  char Param[255];
  char Value[255];
  #define PATH_SCRIPT_A "sudo /home/pi/rpidatv/scripts/a.sh >/dev/null 2>/dev/null"

  // Check if camera selected
  if((strcmp(ModeInput,TabModeInput[0])==0)||(strcmp(ModeInput,TabModeInput[1])==0))
  {
    // Start the viewfinder if required
    strcpy(Param,"vfinder");
    GetConfigParam(PATH_CONFIG,Param,Value);
    if(strcmp(Value,"on")==0)
    {
      IsDisplayOn=0;
      finish();
      system("v4l2-ctl --overlay=1 >/dev/null 2>/dev/null");
    }
  }

  // Check if CONTEST selected; if so, display desktop
  strcpy(Param,"modeinput");
  GetConfigParam(PATH_CONFIG,Param,Value);
  strcpy(ModeOutput,Value);
  if(strcmp(Value,"CONTEST")==0)
  {
    IsDisplayOn=0;
    finish();
  }

  // Check if PATTERN selected; if so, turn off buttons
  strcpy(Param,"modeinput");
  GetConfigParam(PATH_CONFIG,Param,Value);
  strcpy(ModeOutput,Value);
  if(strcmp(Value,"PATERNAUDIO")==0)
  {
    IsDisplayOn=0;
    finish();
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
}

void *WaitButtonEvent(void * arg)
{
	int rawX, rawY, rawPressure;

	while(getTouchSample(&rawX, &rawY, &rawPressure)==0);

	FinishedButton=1;
}

void ProcessLeandvb()
{
   #define PATH_SCRIPT_LEAN "sudo /home/pi/rpidatv/scripts/leandvbgui.sh 2>&1"
   char *line=NULL;
   size_t len = 0;
    ssize_t read;

	// int rawX, rawY, rawPressure; //  not used
	FILE *fp;
	// VGfloat px[1000];  // Variable not used
	// VGfloat py[1000];  // Variable not used
	VGfloat shapecolor[4];
	RGBA(255, 255, 128,1, shapecolor);

	printf("Entering LeandProcess\n");
	FinishedButton=0;
// Thread FFT

	pthread_create (&thfft,NULL, &DisplayFFT,NULL);

//END ThreadFFT

// Thread FFT

	pthread_create (&thbutton,NULL, &WaitButtonEvent,NULL);

//END ThreadFFT

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

system("sudo killall fbi");  // kill any previous images
system("sudo fbi -T 1 -noverbose -a /home/pi/rpidatv/scripts/images/BATC_Black.png");  // Add logo image

usleep(5000000); // Time to FFT end reading samples
   pthread_join(thfft, NULL);
	//pclose(fp);
	pthread_join(thbutton, NULL);
	printf("End Lean\n");
}

void ReceiveStart()
{
        system("sudo killall hello_video.bin >/dev/null 2>/dev/null");
	ProcessLeandvb();
}

void ReceiveStop()
{
  system("sudo killall leandvb >/dev/null 2>/dev/null");
  system("sudo killall hello_video.bin >/dev/null 2>/dev/null");
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
  init(&wscreen, &hscreen);  // Restart the gui
  BackgroundRGB(0,0,0,255);  // Black background
  Fill(255, 255, 255, 1);    // White text

  TextMid(wscreen/2, hscreen/2, message, SerifTypeface, 25);

  VGfloat tw = TextWidth("Touch Screen to Continue", SerifTypeface, 25);
  Text(wscreen / 2.0 - (tw / 2.0), 20, "Touch Screen to Continue", SerifTypeface, 25);
  End();
  printf("MsgBox called and waiting for touch\n");
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
  sprintf(CPUTemp, "CPU temp=%.1f\'C", atoi(result)/1000.0);

  char GPUTemp[256] = "GPU ";
  GetGPUTemp(result);
  strcat(GPUTemp, result);

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

  Text(wscreen/12.0, hscreen - linenumber * linepitch, GPUTemp, font, pointsize);
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

void do_snap()
{
  IsDisplayOn=0;
  finish();
  printf("do_snap\n");
  system("/home/pi/rpidatv/scripts/snap.sh >/dev/null 2>/dev/null");
  wait_touch();
  system("sudo killall fbi >/dev/null 2>/dev/null");  // kill any previous images
  system("sudo fbi -T 1 -noverbose -a /home/pi/rpidatv/scripts/images/BATC_Black.png  >/dev/null 2>/dev/null");  // Add logo image
  init(&wscreen, &hscreen);
  Start(wscreen,hscreen);
  BackgroundRGB(0,0,0,255);
  IsDisplayOn=1;
  UpdateWindow();
}

void do_videoview()
{
  printf("videoview called\n");

  // Make the display ready
  IsDisplayOn=0;
  finish();

  // Create a thread to listen for display touches
  pthread_create (&thview,NULL, &WaitButtonEvent,NULL);

  // Refresh image until display touched
  while ( FinishedButton == 0 )
  {
    system("/home/pi/rpidatv/scripts/view.sh");
    usleep(100000);
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
  IsDisplayOn=1;
  UpdateWindow();
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
  IsDisplayOn=0;
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
  IsDisplayOn=1;
  UpdateWindow();
}

// wait for a screen touch and act on its position
void waituntil(int w,int h)
{
  int rawX, rawY, rawPressure,i;

  // Start the main loop for the Touchscreen
  // Loop forever
  for (;;)
  {
    // Wait here until screen touched
    if (getTouchSample(&rawX, &rawY, &rawPressure)==0) continue;

    // Screen has been touched
    printf("x=%d y=%d\n",rawX,rawY);

    if(IsDisplayOn==0)
    {
      if(CurrentMenu==1)
      {
        // Display not on and Menu 1 (transmitting, receiving or first start)
        // So tidy up and display the buttons
        printf("Display ON\n");
        TransmitStop();
        ReceiveStop();
        init(&wscreen, &hscreen);
        Start(wscreen,hscreen);
        BackgroundRGB(255,255,255,255);
        IsDisplayOn=1;

        SelectPTT(20,0);
        SelectPTT(21,0);
        UpdateWindow();
        continue;
      }
    }

    // Check which Menu is currently displayed
    // and then check each button in turn
    // and take appropriate action
    switch (CurrentMenu)
    {
      case 1:
      for(i=0;i<Menu1Buttons;i++)
      {
        if(IsButtonPushed(i,rawX,rawY)==1)
        // So this number (i) button has been pushed
        {
          printf("Button Event %d\n",i);

          if((i>=0)&&(i<=4)) //Frequency
          {
            SelectFreq(i);
          }
          if((i>=5)&&(i<=9)) //SR
          {
            SelectSR(i);
          }
          if((i>=10)&&(i<=14)) //FEC
          {
            SelectFec(i);
          }
          if((i>=15)&&(i<=19)) //Source
          {
            SelectSource(i,1);
          }
          if((i>=20)&&(i<=22)) //PTT
          {
            printf("Status %d\n",GetButtonStatus(i));
            if((i==20)&&(GetButtonStatus(i)==0))
            {
              usleep(500000);
              SelectPTT(i,1);
              UpdateWindow();
              TransmitStart();
              break;
            }
            if((i==20)&&(GetButtonStatus(i)==1))
            {
              TransmitStop();
              usleep(500000);
              SelectPTT(i,0);
              UpdateWindow();
              break;
            }
            if(i==21) //Receive
            {
              if(CheckRTL()==0)
              {
              printf("DISPLAY OFF \n");
              BackgroundRGB(0,0,0,255);
              ReceiveStart();
              BackgroundRGB(255,255,255,255);
              IsDisplayOn=1;

              SelectPTT(20,0);
              SelectPTT(21,0);
              UpdateWindow();
              IsDisplayOn=1;
              }
              else
              {
                MsgBox("No RTL-SDR Connected");
                wait_touch();
                BackgroundRGB(255,255,255,255);
                UpdateWindow();
              }
            }
            if(i==22)
            {
              printf("MENU 2 \n");
              CurrentMenu=2;
              BackgroundRGB(0,0,0,255);

              Start_Highlights_Menu2();
              UpdateWindow();
            }
          }

	  if(IsDisplayOn==1)
          {
            UpdateWindow();
          }
        }
      }
      break;
    case 2:
      for(i=Menu1Buttons;i<(Menu1Buttons+Menu2Buttons);i++)
      {
        if(IsButtonPushed(i,rawX,rawY)==1)
        // So this number (i) button has been pushed
        {
          printf("Button Event %d\n",i);

          if(i==(Menu1Buttons+0)) // Shutdown
          {
            IsDisplayOn=0;
            finish();
            system("sudo shutdown now");
          }
          if(i==(Menu1Buttons+1)) // Reboot
          {
            IsDisplayOn=0;
            finish();
            system("sudo reboot now");
          }
          if(i==(Menu1Buttons+2)) // Display Info
          {
            ; // Display info todo
            //Text(10, 400, "IP 192.168.xxx.xxx", SansTypeface, 30);
            //Text(VGfloat x, VGfloat y, const char *s, Fontinfo f, int pointsize)
          }
          if(i==(Menu1Buttons+3)) // Caption on/off
          {
            SelectCaption(i,1);
              UpdateWindow();
          }
          if(i==(Menu1Buttons+4)) // Menu 3
          {
              printf("MENU 3 \n");
              CurrentMenu=3;
              BackgroundRGB(0,0,0,255);

              Start_Highlights_Menu3();
              UpdateWindow();
          }
          if((i>=(Menu1Buttons+5))&&(i<=(Menu1Buttons+7))) // Audio Selection
          {
            SelectAudio(i,1);
          }
          if((i>=(Menu1Buttons+8))&&(i<=(Menu1Buttons+9))) // PAL or NTSC
          {
            SelectSTD(i,1);
          }
            if((i>=(Menu1Buttons+10))&&(i<=(Menu1Buttons+14))) // Select Output Mode
          {
            SelectOP(i,1);
          }
          if((i>=(Menu1Buttons+15))&&(i<=(Menu1Buttons+17))) // Select Source 2
          {
            SelectSource2(i,1);
          }
          if(i==(Menu1Buttons+18)) // Spare
          {
            ; // Spare todo
          }
          if(i==(Menu1Buttons+19)) // Spare
          {
            ; // Spare todo
          }
          if(i==(Menu1Buttons+20)) // Back to Menu 1
          {
            printf("MENU 1 \n");
            CurrentMenu=1;
            BackgroundRGB(255,255,255,255);
            Start_Highlights_Menu1();
            //UpdateWindow();
            //}
          }

	  if(IsDisplayOn==1)
          {
            UpdateWindow();
          }
        }
      }
      break;
    case 3:
      for(i=Menu1Buttons+Menu2Buttons;i<(Menu1Buttons+Menu2Buttons+Menu3Buttons);i++)
      {
        if(IsButtonPushed(i,rawX,rawY)==1)
        // So this number (i) button has been pushed
        {
          printf("Button Event %d\n",i);

          if(i==(Menu1Buttons+Menu2Buttons+0)) // Shutdown
          {
            IsDisplayOn=0;
            finish();
            system("sudo shutdown now");
          }
          if(i==(Menu1Buttons+Menu2Buttons+1)) // Reboot
          {
            IsDisplayOn=0;
            finish();
            system("sudo reboot now");
          }
          if(i==(Menu1Buttons+Menu2Buttons+2)) // Display Info
          {
            InfoScreen();
            BackgroundRGB(0,0,0,255);
            UpdateWindow();
          }
          if(i==(Menu1Buttons+Menu2Buttons+3)) // Caption on/off
          {
              //SelectCaption(i,1);
              UpdateWindow();
          }
          if(i==(Menu1Buttons+Menu2Buttons+4)) // Menu 3
          {
              printf("MENU 3 \n");
              CurrentMenu=3;
              BackgroundRGB(0,0,0,255);

              Start_Highlights_Menu3();
              UpdateWindow();
          }
          if((i>=(Menu1Buttons+Menu2Buttons+5))&&(i<=(Menu1Buttons+Menu2Buttons+7))) // Audio Selection
          {
            SelectAudio(i,1);
          }
          if((i>=(Menu1Buttons+Menu2Buttons+8))&&(i<=(Menu1Buttons+Menu2Buttons+9))) // PAL or NTSC
          {
            SelectSTD(i,1);
          }
            if((i>=(Menu1Buttons+Menu2Buttons+10))&&(i<=(Menu1Buttons+Menu2Buttons+14))) // Select Output Mode
          {
            SelectOP(i,1);
          }
          if(i==(Menu1Buttons+Menu2Buttons+15)) // Snap
          {
            do_snap();
          }
          if(i==(Menu1Buttons+Menu2Buttons+16)) // View
          {
            do_videoview();
          }
          if(i==(Menu1Buttons+Menu2Buttons+17)) // Check
          {
            do_snapcheck();
          }
          if(i==(Menu1Buttons+Menu2Buttons+18)) // Spare
          {
            ; // Spare todo
          }
          if(i==(Menu1Buttons+Menu2Buttons+19)) // Spare
          {
            ; // Spare todo
          }
          if(i==(Menu1Buttons+Menu2Buttons+20)) // Back to Menu 1
          {
            printf("MENU 1 \n");
            CurrentMenu=1;
            BackgroundRGB(255,255,255,255);
            Start_Highlights_Menu1();
          }
	  if(IsDisplayOn==1)
          {
            UpdateWindow();
          }
        }
      }
      break;
    case 4:
      //first=0;
      //last=Menu1Buttons;
      break;
    default:
      //first=0;
      //last=Menu1Buttons;
      break;
    }
  }
}

void Start_Highlights_Menu1()
// Retrieves stored value for each group of buttons
// and then sets the correct highlight
{
  char Param[255];
  char Value[255];

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
  Menu1Buttons=23;
  // Frequency

	int button=AddButton(0*wbuttonsize+20,0+hbuttonsize*0+20,wbuttonsize*0.9,hbuttonsize*0.9);
	color_t Col;
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,FreqLabel[0],&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button,FreqLabel[0],&Col);

	button=AddButton(1*wbuttonsize+20,hbuttonsize*0+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,FreqLabel[1],&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button,FreqLabel[1],&Col);

	button=AddButton(2*wbuttonsize+20,hbuttonsize*0+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,FreqLabel[2],&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button,FreqLabel[2],&Col);

	button=AddButton(3*wbuttonsize+20,hbuttonsize*0+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,FreqLabel[3],&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button,FreqLabel[3],&Col);

	button=AddButton(4*wbuttonsize+20,hbuttonsize*0+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,FreqLabel[4],&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button,FreqLabel[4],&Col);

// Symbol Rate

	button=AddButton(0*wbuttonsize+20,0+hbuttonsize*1+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,SRLabel[0],&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button,SRLabel[0],&Col);

	button=AddButton(1*wbuttonsize+20,hbuttonsize*1+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,SRLabel[1],&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button,SRLabel[1],&Col);

	button=AddButton(2*wbuttonsize+20,hbuttonsize*1+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,SRLabel[2],&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button,SRLabel[2],&Col);

	button=AddButton(3*wbuttonsize+20,hbuttonsize*1+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,SRLabel[3],&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button,SRLabel[3],&Col);

	button=AddButton(4*wbuttonsize+20,hbuttonsize*1+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,SRLabel[4],&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button,SRLabel[4],&Col);

// FEC

	button=AddButton(0*wbuttonsize+20,hbuttonsize*2+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,"FEC 1/2",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button,"FEC 1/2",&Col);

	button=AddButton(1*wbuttonsize+20,hbuttonsize*2+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,"FEC 2/3",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button,"FEC 2/3",&Col);

	button=AddButton(2*wbuttonsize+20,hbuttonsize*2+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,"FEC 3/4",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button,"FEC 3/4",&Col);

	button=AddButton(3*wbuttonsize+20,hbuttonsize*2+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,"FEC 5/6",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button,"FEC 5/6",&Col);

	button=AddButton(4*wbuttonsize+20,hbuttonsize*2+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,"FEC 7/8",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button,"FEC 7/8",&Col);

//SOURCE

	button=AddButton(0*wbuttonsize+20,hbuttonsize*3+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,"CAM MPEG2",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button,"CAM MPEG2",&Col);

	button=AddButton(1*wbuttonsize+20,hbuttonsize*3+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,"CAM H264",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button,"CAM H264",&Col);

	char PictureName[255];
	//strcpy(PictureName,ImageFolder);
	GetNextPicture(PictureName);

	button=AddButton(2*wbuttonsize+20,hbuttonsize*3+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,"Pattern",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button,"Pattern",&Col);

	button=AddButton(3*wbuttonsize+20,hbuttonsize*3+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,"VID H264",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button,"VID H264",&Col);

	button=AddButton(4*wbuttonsize+20,hbuttonsize*3+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,"Carrier",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button,"Carrier",&Col);

//TRANSMIT RECEIVE MENU2

	button=AddButton(0*wbuttonsize+20,hbuttonsize*4+20,wbuttonsize*1.2,hbuttonsize*1.2);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,"TX   ",&Col);
	Col.r=255;Col.g=0;Col.b=0;
	AddButtonStatus(button,"TX ON",&Col);

        button=AddButton(2.7*wbuttonsize+20,hbuttonsize*4+20,wbuttonsize*1.2,hbuttonsize*1.2);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,"RX   ",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button,"RX ON",&Col);

        button=AddButton(4*wbuttonsize+20,hbuttonsize*4+20,wbuttonsize*0.9,hbuttonsize*1.2);
        Col.r=0;Col.g=0;Col.b=128;
        AddButtonStatus(button," M2  ",&Col);
        Col.r=0;Col.g=128;Col.b=0;
        AddButtonStatus(button," M2  ",&Col);
}

void Define_Menu2()
{
  Menu2Buttons=21;
  // Bottom row: Shutdown, Reboot, Info, Menu3, Menu4

	int button=AddButton(0*wbuttonsize+20,0+hbuttonsize*0+20,wbuttonsize*0.9,hbuttonsize*0.9);
	color_t Col;
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,"Shutdown",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button,"Shutdown",&Col);

	button=AddButton(1*wbuttonsize+20,hbuttonsize*0+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,"Reboot ",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button,"Reboot ",&Col);

	button=AddButton(2*wbuttonsize+20,hbuttonsize*0+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button," ",&Col);
	//AddButtonStatus(button," Info  ",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button," Info  ",&Col);

	button=AddButton(3*wbuttonsize+20,hbuttonsize*0+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,"Caption",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button,"Caption",&Col);

	button=AddButton(4*wbuttonsize+20,hbuttonsize*0+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
//	AddButtonStatus(button," ",&Col);
	AddButtonStatus(button," Menu 3",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button," Menu 3",&Col);

// 2nd row up: Audio Mic, Audio Auto, Audio EC, PAL In, NTSC In

	button=AddButton(0*wbuttonsize+20,0+hbuttonsize*1+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,"Audio Mic",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button,"Audio Mic",&Col);

	button=AddButton(1*wbuttonsize+20,hbuttonsize*1+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,"Audio Auto",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button,"Audio Auto",&Col);

	button=AddButton(2*wbuttonsize+20,hbuttonsize*1+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,"Audio EC ",&Col);
	//AddButtonStatus(button," No VF ",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button,"Audio EC ",&Col);

	button=AddButton(3*wbuttonsize+20,hbuttonsize*1+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button," PAL in",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button," PAL in",&Col);

	button=AddButton(4*wbuttonsize+20,hbuttonsize*1+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,"NTSC in",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button,"NTSC in",&Col);

// 3rd row up: Output to: IQ, Ugly, Express, BATC, COMPVID

	button=AddButton(0*wbuttonsize+20,hbuttonsize*2+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,"  IQ  ",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button,"  IQ  ",&Col);

	button=AddButton(1*wbuttonsize+20,hbuttonsize*2+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button," Ugly  ",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button," Ugly  ",&Col);

	button=AddButton(2*wbuttonsize+20,hbuttonsize*2+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,"Express",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button,"Express",&Col);

	button=AddButton(3*wbuttonsize+20,hbuttonsize*2+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button," BATC ",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button," BATC ",&Col);

	button=AddButton(4*wbuttonsize+20,hbuttonsize*2+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,"Vid Out",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button,"Vid Out",&Col);

// Top row, 3 more sources:

	button=AddButton(0*wbuttonsize+20,hbuttonsize*3+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,"CONTEST",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button,"CONTEST",&Col);

	button=AddButton(1*wbuttonsize+20,hbuttonsize*3+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button," IPTS IN",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button," IPTS IN",&Col);

	button=AddButton(2*wbuttonsize+20,hbuttonsize*3+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,"VID MPEG2",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button,"VID MPEG2",&Col);

	button=AddButton(3*wbuttonsize+20,hbuttonsize*3+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button," ",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button," ",&Col);

	button=AddButton(4*wbuttonsize+20,hbuttonsize*3+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button," ",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button," ",&Col);

// Single button to get back to Menu 1

        button=AddButton(4*wbuttonsize+20,hbuttonsize*4+20,wbuttonsize*0.9,hbuttonsize*1.2);
        Col.r=0;Col.g=0;Col.b=128;
        AddButtonStatus(button," M1  ",&Col);
        Col.r=0;Col.g=128;Col.b=0;
        AddButtonStatus(button," M1  ",&Col);
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
    SelectInGroup(Menu1Buttons+5,Menu1Buttons+7,Menu1Buttons+5,1);
  }
  if(strcmp(Value,"auto")==0)
  {
    SelectInGroup(Menu1Buttons+5,Menu1Buttons+7,Menu1Buttons+6,1);
  }
  if(strcmp(Value,"video")==0)
  {
    SelectInGroup(Menu1Buttons+5,Menu1Buttons+7,Menu1Buttons+7,1);
  }

  // PAL or NTSC

  strcpy(Param,"analogcamstandard");
  GetConfigParam(PATH_CONFIG,Param,Value);
  STD=atoi(Value);
  printf("Value=%s %s\n",Value,"Video Standard");
  if ( STD == 6 ) //PAL
  {
    SelectInGroup(Menu1Buttons+8,Menu1Buttons+9,Menu1Buttons+8,1);
  }
  else if ( STD == 0 ) //NTSC
  {
    SelectInGroup(Menu1Buttons+8,Menu1Buttons+9,Menu1Buttons+9,1);
  }

  // Output Modes

  strcpy(Param,"modeoutput");
  strcpy(Value,"");
  GetConfigParam(PATH_CONFIG,Param,Value);
  printf("Value=%s %s\n",Value," Output");
  if(strcmp(Value,"IQ")==0)
  {
    SelectInGroup(Menu1Buttons+10,Menu1Buttons+14,Menu1Buttons+10,1);
  }
  if(strcmp(Value,"QPSKRF")==0)
  {
    SelectInGroup(Menu1Buttons+10,Menu1Buttons+14,Menu1Buttons+11,1);
  }
  if(strcmp(Value,"DATVEXPRESS")==0)
  {
    SelectInGroup(Menu1Buttons+10,Menu1Buttons+14,Menu1Buttons+12,1);
  }
  if(strcmp(Value,"BATC")==0)
  {
    SelectInGroup(Menu1Buttons+10,Menu1Buttons+14,Menu1Buttons+13,1);
  }
  if(strcmp(Value,"COMPVID")==0)
  {
    SelectInGroup(Menu1Buttons+10,Menu1Buttons+14,Menu1Buttons+14,1);
  }

  // Extra Input Modes

  strcpy(Param,"modeinput");
  GetConfigParam(PATH_CONFIG,Param,Value);
  strcpy(ModeInput,Value);
  printf("Value=%s %s\n",Value,"Input Mode");

  if(strcmp(Value,"CONTEST")==0)
  {
    SelectInGroup(Menu1Buttons+15,Menu1Buttons+17,Menu1Buttons+15,1);
  }
  if(strcmp(Value,"IPTSIN")==0)
  {
    SelectInGroup(Menu1Buttons+15,Menu1Buttons+17,Menu1Buttons+16,1);
  }
  if(strcmp(Value,"ANALOGMPEG-2")==0)
  {
    SelectInGroup(Menu1Buttons+15,Menu1Buttons+17,Menu1Buttons+17,1);
  }

  // Caption
  strcpy(Param,"caption");
  GetConfigParam(PATH_CONFIG,Param,Value);
  strcpy(Caption,Value);
  printf("Value=%s %s\n",Value,"Caption");
  if(strcmp(Value,"on")==0)
  {
    SelectInGroup(Menu1Buttons+3,Menu1Buttons+3,Menu1Buttons+3,1);
  }
  else
  {
    SelectInGroup(Menu1Buttons+3,Menu1Buttons+3,Menu1Buttons+3,0);
  }
}

void Define_Menu3()
{
  Menu3Buttons=21;
  // Bottom row: Shutdown, Reboot, Info, Spare, Spare (Menu4?)

	int button=AddButton(0*wbuttonsize+20,0+hbuttonsize*0+20,wbuttonsize*0.9,hbuttonsize*0.9);
	color_t Col;
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,"Shutdown",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button,"Shutdown",&Col);

	button=AddButton(1*wbuttonsize+20,hbuttonsize*0+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,"Reboot ",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button,"Reboot ",&Col);

	button=AddButton(2*wbuttonsize+20,hbuttonsize*0+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button," Info  ",&Col);
	//AddButtonStatus(button," Info  ",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button," Info  ",&Col);

	button=AddButton(3*wbuttonsize+20,hbuttonsize*0+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button," ",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button," ",&Col);

	button=AddButton(4*wbuttonsize+20,hbuttonsize*0+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button," ",&Col);
	//AddButtonStatus(button," Menu 3",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button," Menu 3",&Col);

// 2nd row up: Audio Mic, Audio Auto, Audio EC, PAL In, NTSC In

	button=AddButton(0*wbuttonsize+20,0+hbuttonsize*1+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button," ",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button," ",&Col);

	button=AddButton(1*wbuttonsize+20,hbuttonsize*1+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button," ",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button," ",&Col);

	button=AddButton(2*wbuttonsize+20,hbuttonsize*1+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,"  ",&Col);
	//AddButtonStatus(button," No VF ",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button,"  ",&Col);

	button=AddButton(3*wbuttonsize+20,hbuttonsize*1+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button," ",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button," ",&Col);

	button=AddButton(4*wbuttonsize+20,hbuttonsize*1+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button," ",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button," ",&Col);

// 3rd row up: Output to: IQ, Ugly, Express, BATC, COMPVID

	button=AddButton(0*wbuttonsize+20,hbuttonsize*2+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button," ",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button," ",&Col);

	button=AddButton(1*wbuttonsize+20,hbuttonsize*2+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button," ",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button," ",&Col);

	button=AddButton(2*wbuttonsize+20,hbuttonsize*2+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button," ",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button," ",&Col);

	button=AddButton(3*wbuttonsize+20,hbuttonsize*2+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button," ",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button," ",&Col);

	button=AddButton(4*wbuttonsize+20,hbuttonsize*2+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button," ",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button," ",&Col);

// Top row, Snap, View and Check

	button=AddButton(0*wbuttonsize+20,hbuttonsize*3+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button," Snap  ",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button," Snap  ",&Col);

	button=AddButton(1*wbuttonsize+20,hbuttonsize*3+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button," View  ",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button," View  ",&Col);

	button=AddButton(2*wbuttonsize+20,hbuttonsize*3+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button," Check ",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button," Check ",&Col);

	button=AddButton(3*wbuttonsize+20,hbuttonsize*3+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button," ",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button," ",&Col);

	button=AddButton(4*wbuttonsize+20,hbuttonsize*3+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button," ",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button," ",&Col);

// Single button to get back to Menu 1

        button=AddButton(4*wbuttonsize+20,hbuttonsize*4+20,wbuttonsize*0.9,hbuttonsize*1.2);
        Col.r=0;Col.g=0;Col.b=128;
        AddButtonStatus(button," M1  ",&Col);
        Col.r=0;Col.g=128;Col.b=0;
        AddButtonStatus(button," M1  ",&Col);
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
  printf("Terminate\n");
  char Commnd[255];
  sprintf(Commnd,"stty echo");
  system(Commnd);
  sprintf(Commnd,"reset");
  system(Commnd);
  exit(1);
}

// main initializes the system and shows the picture. 

int main(int argc, char **argv) {
	int NoDeviceEvent=0;
	saveterm();
	init(&wscreen, &hscreen);
	rawterm();
	int screenXmax, screenXmin;
	int screenYmax, screenYmin;
	int ReceiveDirect=0;
	int i, STD;
        char Param[255];
        char Value[255];
 
// Catch sigaction and call terminate
	for (i = 0; i < 16; i++) {
		struct sigaction sa;

		memset(&sa, 0, sizeof(sa));
		sa.sa_handler = terminate;
		sigaction(i, &sa, NULL);
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

  // Set the Band (and filter) Switching
  system ("sudo /home/pi/rpidatv/scripts/ctlfilter.sh");
  // and wait for it to finish using rpidatvconfig.txt
  usleep(100000);

// Set the Analog Capture Standard

  strcpy(Param,"analogcamstandard");
  GetConfigParam(PATH_CONFIG,Param,Value);
  STD=atoi(Value);
  printf("Value=%s %s\n",Value,"Video Standard");
  if ( STD == 6 ) //PAL
  {
    system("v4l2-ctl -d /dev/video1 --set-standard=6");
  }
  else if ( STD == 0 ) //NTSC
  {
    system("v4l2-ctl -d /dev/video1 --set-standard=0");
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

// Calculate screen parameters
	scaleXvalue = ((float)screenXmax-screenXmin) / wscreen;
	//printf ("X Scale Factor = %f\n", scaleXvalue);
	scaleYvalue = ((float)screenYmax-screenYmin) / hscreen;
	//printf ("Y Scale Factor = %f\n", scaleYvalue);

  // Define button grid
  // -25 keeps right hand side symmetrical with left hand side
  wbuttonsize=(wscreen-25)/5;
  hbuttonsize=hscreen/6;
  
  // Read in the presets from the Config file
  ReadPresets();

  // Define the buttons for Menu 1
  Define_Menu1();

  // Define the buttons for Menu 2
  Define_Menu2();

  // Define the buttons for Menu 3
  Define_Menu3();

  // Start the button Menu
  Start(wscreen,hscreen);
  IsDisplayOn=1;

  // Determine button highlights
  Start_Highlights_Menu1();

  UpdateWindow();
  printf("Update Window\n");

  // Go and wait for the screen to be touched
  waituntil(wscreen,hscreen);

  // Not sure that the program flow ever gets here

  restoreterm();
  finish();
  return 0;
}
