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
#include <wiringPi.h>

#define KWHT  "\x1B[37m"
#define KYEL  "\x1B[33m"

#define PATH_CONFIG "/home/pi/rpidatv/src/siggen/siggenconfig.txt"
#define PATH_CONFIG_PORTSDOWN "/home/pi/rpidatv/scripts/rpidatvconfig.txt"
#define PATH_CAL "/home/pi/rpidatv/src/siggen/siggencal.txt"

int fd=0;
int wscreen, hscreen;
float scaleXvalue, scaleYvalue; // Coeff ratio from Screen/TouchArea
int wbuttonsize;
int hbuttonsize;
int swbuttonsize;
int shbuttonsize;

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
int Inversed=0;            //Display is inversed (Waveshare=1)
int FinishedButton=1;     // Used to signal button has been pushed

//GLOBAL PARAMETERS

int64_t DisplayFreq = 437000000;  // Input freq and display freq are the same
int DisplayLevel = 987;           // calculated for display from (LO) level, atten and freq
char osctxt[255]="portsdown";     // current source
int level;                        // current LO level.  Raw data
int atten;                        // current atten level.  Raw data (0.25 dB steps



char ref_freq_4351[255] = "25000000";        // read on startup from rpidatvconfig.txt

uint64_t SourceUpperFreq = 13600000000;       // set every time an oscillator is selected
int SourceLowerFreq = 54000000;              // set every time an oscillator is selected

char ref_freq_5355[255] = "25000000";        // read from siggenconfig.txt on startup

int OutputStatus = 0;                        // 0 = off, 1 = on
int PresetStoreTrigger = 0;                  // 0 = normal, 1 = next press should be preset
int ModOn = 0;                               // 0 =  modulation off, 1 = modulation on
int AttenIn = 0;                             // 0 = No attenuator, 1 = attenuator in circuit
char AttenType[256] = "PE43703";             // or PE4302

// Titles for presets in file
// [0] is the start-up condition. [1] - [4 ] are the presets
char FreqTag[5][255]={"freq","p1freq","p2freq","p3freq","p4freq"};
char OscTag[5][255]={"osc","p1osc","p2osc","p3osc","p4osc"};
char LevelTag[5][255]={"level","p1level","p2level","p3level","p4level"};
char AttenTag[5][255]={"atten","p1atten","p2atten","p3atten","p4atten"};

// Values for presets [0] not used. [1] - [4] are presets
// May be over-written by values from from siggenconfig.txt:

uint64_t TabFreq[5]={144750000,146500000,437000000,1249000000,1255000000};
char TabOscOption[5][255]={"portsdown","portsdown","portsdown","portsdown","portsdown"};
char TabLevel[5][255]={"0","0","0","0","0"};
char TabAtten[5][255]={"out","out","out","out","out"};

// Button functions (in button order):
char TabOsc[10][255]={"audio","pirf","adf4351","portsdown","express","a", "a","a", "a","adf5355"};

uint64_t CalFreq[50];
int CalLevel[50];
int CalPoints;

int8_t BandLSBGPIO = 31;
int8_t BandMSBGPIO = 24;
int8_t FiltLSBGPIO = 27;
int8_t FiltNSBGPIO = 25;
int8_t FiltMSBGPIO = 28;
int8_t I_GPIO = 26;
int8_t Q_GPIO = 23;

pthread_t thfft,thbutton,thview;

// Function Prototypes

void Start_Highlights_Menu1();


/***************************************************************************//**
 * @brief Looks up the value of Param in PathConfigFile and sets value
 *        Used to look up the configuration from *****config.txt
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
 * @brief Reads the Presets from siggenconfig.txt and formats them for
 *        Display and switching
 *
 * @param None.  Works on global variables
 *
 * @return void

uint64_t TabFreq[4]={146500000,437000000,1249000000,1255000000};
char TabOscOption[4][255]={"portsdown","portsdown","portsdown","portsdown"};
char TabLevel[4][255]={"0","0","0","0"};
char TabAtten[4][255]={"out","out","out","out"};

*******************************************************************************/

void ReadPresets()
{
  // Called at application start
  int n;
  char Param[255];
  char Value[255];

  // Read Freqs, Oscs, Levels and Attens

  n = 0; // Read Start-up  value

  strcpy(Param, FreqTag[n]);
  GetConfigParam(PATH_CONFIG,Param,Value);
  DisplayFreq = strtoull(Value, 0, 0);

  // Check that a valid number has been read.  If not, reload factory settings

  if (strlen(Value) <=2)  // invalid start-up frequency
  {                       // So copy in factory defaults
    system("sudo cp -f -r /home/pi/rpidatv/src/siggen/factoryconfig.txt /home/pi/rpidatv/src/siggen/siggenconfig.txt"); 

    GetConfigParam(PATH_CONFIG,Param,Value); // read in the frequency again
    DisplayFreq = strtoull(Value, 0, 0);
  }

  strcpy(Param, OscTag[n]);
  GetConfigParam(PATH_CONFIG,Param,Value);
  strcpy(osctxt, Value);

  strcpy(Param, LevelTag[n]);
  GetConfigParam(PATH_CONFIG,Param,Value);
  level=atoi(Value);

  strcpy(Param, AttenTag[n]);
  GetConfigParam(PATH_CONFIG,Param,Value);
  atten=atoi(Value);

  for( n = 1; n < 5; n = n + 1)  // Read Presets
  {
    strcpy(Param, FreqTag[n]);
    GetConfigParam(PATH_CONFIG,Param,Value);
    TabFreq[n] = strtoull(Value, 0, 0);

    strcpy(Param, OscTag[n]);
    GetConfigParam(PATH_CONFIG,Param,Value);
    strcpy(TabOscOption[n], Value);

    strcpy(Param, LevelTag[n]);
    GetConfigParam(PATH_CONFIG,Param,Value);
    strcpy(TabLevel[n], Value);

    strcpy(Param, AttenTag[n]);
    GetConfigParam(PATH_CONFIG,Param,Value);
    strcpy(TabAtten[n], Value);
  }

  // Read ADF5535 Ref Frequency
  strcpy(Param, "adf5355ref");
  GetConfigParam(PATH_CONFIG,Param,Value);
  strcpy (ref_freq_5355, Value);

  // Read Attenuator Type
  strcpy(Param, "attentype");
  GetConfigParam(PATH_CAL,Param,Value);
  strcpy (AttenType, Value);
}

void ShowFreq(uint64_t DisplayFreq)
{
  // Displays the current frequency with leading zeros blanked
  Fontinfo font = SansTypeface;
  int pointsize = 50;
  float vpos = 0.48;
  uint64_t RemFreq;
  uint64_t df01, df02, df03, df04, df05, df06, df07, df08, df09, df10, df11;
  char df01text[16], df02text[16], df03text[16], df04text[16] ,df05text[16];
  char df06text[16], df07text[16], df08text[16], df09text[16] ,df10text[16];
  char df11text[16];

  if (CurrentMenu == 1)
  {
    vpos = 0.57;
  }
  else
  {
    vpos = 0.48;
  }

  df01 = DisplayFreq/10000000000;
  snprintf(df01text, 10, "%lld", df01);
  RemFreq = DisplayFreq - df01 * 10000000000;

  df02 = RemFreq/1000000000;
  snprintf(df02text, 10, "%lld", df02);
  RemFreq = RemFreq - df02 * 1000000000;

  df03 = RemFreq/100000000;
  snprintf(df03text, 10, "%lld", df03);
  RemFreq = RemFreq - (df03 * 100000000);

  df04 = RemFreq/10000000;
  snprintf(df04text, 10, "%lld", df04);
  RemFreq = RemFreq - df04 * 10000000;

  df05 = RemFreq/1000000;
  snprintf(df05text, 10, "%lld", df05);
  RemFreq = RemFreq - df05 * 1000000;

  df06 = RemFreq/100000;
  snprintf(df06text, 10, "%lld", df06);
  RemFreq = RemFreq - df06 * 100000;

  df07 = RemFreq/10000;
  snprintf(df07text, 10, "%lld", df07);
  RemFreq = RemFreq - df07 * 10000;

  df08 = RemFreq/1000;
  snprintf(df08text, 10, "%lld", df08);
  RemFreq = RemFreq - df08 * 1000;

  df09 = RemFreq/100;
  snprintf(df09text, 10, "%lld", df09);
  RemFreq = RemFreq - df09 * 100;

  df10 = RemFreq/10;
  snprintf(df10text, 10, "%lld", df10);
  RemFreq = RemFreq - df10 * 10;

  df11 = RemFreq;
  snprintf(df11text, 10, "%lld", df11);

  // Display Text
  if (DisplayFreq >= 10000000000)
  {
    Text(0.06*wscreen, vpos*hscreen, df01text, font, pointsize);
  }
  if (DisplayFreq >= 1000000000)
  {
    Text(0.14*wscreen, vpos*hscreen, df02text, font, pointsize);
    Text(0.20*wscreen, vpos*hscreen, ",", font, pointsize);
  }
  if (DisplayFreq >= 100000000)
  {
    Text(0.24*wscreen, vpos*hscreen, df03text, font, pointsize);
  }
  if (DisplayFreq >= 10000000)
  {
    Text(0.32*wscreen, vpos*hscreen, df04text, font, pointsize);
  }
  if (DisplayFreq >= 1000000)
  {
    Text(0.40*wscreen, vpos*hscreen, df05text, font, pointsize);
    Text(0.458*wscreen, vpos*hscreen, ".", font, pointsize);
  }
  if (DisplayFreq >= 100000)
  {
    Text(0.495*wscreen, vpos*hscreen, df06text, font, pointsize);
  }
  if (DisplayFreq >= 10000)
  {
    Text(0.575*wscreen, vpos*hscreen, df07text, font, pointsize);
  }
  if (DisplayFreq >= 1000)
  {
    Text(0.655*wscreen, vpos*hscreen, df08text, font, pointsize);
    Text(0.715*wscreen, vpos*hscreen, ",", font, pointsize);
  }
  if (DisplayFreq >= 100)
  {
    Text(0.75*wscreen, vpos*hscreen, df09text, font, pointsize);
  }
  Text(0.83*wscreen, vpos*hscreen, df10text, font, pointsize);
  Text(0.91*wscreen, vpos*hscreen, df11text, font, pointsize);
}

void ShowLevel(int DisplayLevel)
{
  // DisplayLevel is a signed integer in the range +999 to - 999 tenths of dBm
  Fontinfo font = SansTypeface;
  int pointsize = 50;
  float vpos;
  //int DisplayLevel;
  int dl02, dl03, dl04, RemLevel;
  char dl01text[16], dl02text[16], dl03text[16], dl04text[16];

  if (DisplayLevel == 0)
  {
    strcpy(dl01text, " ");
    strcpy(dl02text, "0");
    strcpy(dl03text, "0");
    strcpy(dl04text, "0");
  }
  else
  {  
    strcpy(dl01text, "+");
    RemLevel = DisplayLevel;
    if (DisplayLevel <= 1)
    {
      strcpy(dl01text, "-");
      RemLevel = -1 * DisplayLevel;
    }
    dl02 = RemLevel/100;
    snprintf(dl02text, 10, "%d", dl02);
    RemLevel = RemLevel - dl02 * 100;

    dl03 = RemLevel/10;
    snprintf(dl03text, 10, "%d", dl03);
    RemLevel = RemLevel - dl03 * 10;

    dl04 = RemLevel;
    snprintf(dl04text, 10, "%d", dl04);
  }
  if (CurrentMenu == 1)
  {
    vpos = 0.43;
    pointsize = 35;
    Text(0.03*wscreen, vpos*hscreen, dl01text, font, pointsize);
    if ((DisplayLevel <= -100) || (DisplayLevel >= 100))
    {
      Text(0.08*wscreen, vpos*hscreen, dl02text, font, pointsize);
    }
    Text(0.13*wscreen, vpos*hscreen, dl03text, font, pointsize);
    Text(0.18*wscreen, vpos*hscreen, ".", font, pointsize);
    Text(0.20*wscreen, vpos*hscreen, dl04text, font, pointsize);
    Text(0.25*wscreen, vpos*hscreen, "dBm", font, pointsize);
  }
  else
  {
    vpos = 0.14;
    pointsize = 50;
    Text(0.02*wscreen, vpos*hscreen, dl01text, font, pointsize);
    if ((DisplayLevel <= -100) || (DisplayLevel >= 100))
    {
      Text(0.11*wscreen, vpos*hscreen, dl02text, font, pointsize);
    }
    Text(0.19*wscreen, vpos*hscreen, dl03text, font, pointsize);
    Text(0.25*wscreen, vpos*hscreen, ".", font, pointsize);
    Text(0.29*wscreen, vpos*hscreen, dl04text, font, pointsize);
    Text(0.37*wscreen, vpos*hscreen, "dBm", font, pointsize);
  }
}

void ShowTitle()
{
  // Initialise and calculate the text display
  BackgroundRGB(0,0,0,255);  // Black background
  Fill(255, 255, 255, 1);    // White text
  Fontinfo font = SansTypeface;
  int pointsize = 20;
  VGfloat txtht = TextHeight(font, pointsize);
  VGfloat txtdp = TextDepth(font, pointsize);
  VGfloat linepitch = 1.1 * (txtht + txtdp);
  VGfloat linenumber = 1.0;
  VGfloat tw;

  // Display Text
  tw = TextWidth("BATC Portsdown Information Screen", font, pointsize);
  Text(wscreen / 2.0 - (tw / 2.0), hscreen - linenumber * linepitch, "BATC Portsdown Signal Generator", font, pointsize);
}

void AdjustFreq(int button)
{
  button=button-Menu1Buttons-8;
  switch (button)
  {
    case 0:
      if (DisplayFreq >= 10000000000)
      {
        DisplayFreq=DisplayFreq-10000000000;
      }
      break;
    case 1:
      if (DisplayFreq >= 1000000000)
      {
        DisplayFreq=DisplayFreq-1000000000;
      }
      break;
    case 2:
      if (DisplayFreq >= 100000000)
      {
        DisplayFreq=DisplayFreq-100000000;
      }
      break;
    case 3:
      if (DisplayFreq >= 10000000)
      {
        DisplayFreq=DisplayFreq-10000000;
      }
      break;
    case 4:
      if (DisplayFreq >= 1000000)
      {
        DisplayFreq=DisplayFreq-1000000;
      }
      break;
    case 5:
      if (DisplayFreq >= 100000)
      {
        DisplayFreq=DisplayFreq-100000;
      }
      break;
    case 6:
      if (DisplayFreq >= 10000)
      {
        DisplayFreq=DisplayFreq-10000;
      }
      break;
    case 7:
      if (DisplayFreq >= 1000)
      {
        DisplayFreq=DisplayFreq-1000;
      }
      break;
    case 8:
      if (DisplayFreq >= 100)
      {
        DisplayFreq=DisplayFreq-100;
      }
      break;
    case 9:
      if (DisplayFreq >= 10)
      {
        DisplayFreq=DisplayFreq-10;
      }
      break;
    case 10:
      if (DisplayFreq >= 1)
      {
        DisplayFreq=DisplayFreq-1;
      }
      break;
    case 11:
      if (DisplayFreq <= 9999999999)
      {
        DisplayFreq=DisplayFreq+10000000000;
      }
      break;
    case 12:
      if (DisplayFreq <= 18999999999)
      {
        DisplayFreq=DisplayFreq+1000000000;
      }
      break;
    case 13:
      if (DisplayFreq <= 19899999999)
      {
        DisplayFreq=DisplayFreq+100000000;
      }
      break;
    case 14:
      if (DisplayFreq <= 19989999999)
      {
        DisplayFreq=DisplayFreq+10000000;
      }
      break;
    case 15:
      if (DisplayFreq <= 19998999999)
      {
        DisplayFreq=DisplayFreq+1000000;
      }
      break;
    case 16:
      if (DisplayFreq <= 19999899999)
      {
        DisplayFreq=DisplayFreq+100000;
      }
      break;
    case 17:
      if (DisplayFreq <= 19999989999)
      {
        DisplayFreq=DisplayFreq+10000;
      }
      break;
    case 18:
      if (DisplayFreq <= 19999998999)
      {
        DisplayFreq=DisplayFreq+1000;
      }
      break;
    case 19:
      if (DisplayFreq <= 19999999899)
      {
        DisplayFreq=DisplayFreq+100;
      }
      break;
    case 20:
      if (DisplayFreq <= 19999999989)
      {
        DisplayFreq=DisplayFreq+10;
      }
      break;
    case 21:
      if (DisplayFreq <= 19999999998)
      {
        DisplayFreq=DisplayFreq+1;
      }
      break;
    default:
      
break;
  }
  if (DisplayFreq > SourceUpperFreq)
  {
    DisplayFreq = SourceUpperFreq;
  }
  if (DisplayFreq < SourceLowerFreq)
  {
    DisplayFreq = SourceLowerFreq;
  }
}

void CalcOPLevel()
{
  int PointBelow = 0;
  int PointAbove = 0;
  int n = 0;
  float proportion;

  // Calculate output level from Osc based on Cal and frequency

  while ((PointAbove == 0) && (n <= 100))
  {
    n = n+1;
    if (DisplayFreq <= CalFreq[n])
    {
      PointAbove = n;
      PointBelow = n - 1;
    }
  }
  printf("PointAbove = %d \n", PointAbove);

  if (DisplayFreq == CalFreq[n])
  {
    DisplayLevel = CalLevel[PointAbove];
  }
  else
  {
    proportion = (float)(DisplayFreq - CalFreq[PointBelow])/(CalFreq[PointAbove]- CalFreq[PointBelow]);
  printf("proportion = %f \n", proportion);
    DisplayLevel = CalLevel[PointBelow] + (CalLevel[PointAbove] - CalLevel[PointBelow]) * proportion;
  }

// Now correct for set oscillator level

  if (strcmp(osctxt, "audio")==0)
  {
    DisplayLevel = 0;
  }

  if (strcmp(osctxt, "pirf")==0)
  {
    DisplayLevel = -70;
  }

  if (strcmp(osctxt, "adf4351")==0)
  {
    DisplayLevel = DisplayLevel + 30 * level;
  }

  if (strcmp(osctxt, "portsdown")==0)
  {
    ;
    // no correction required
  }

  if (strcmp(osctxt, "express")==0)
  {
    DisplayLevel = -10*level;
  }

  if (strcmp(osctxt, "adf5355")==0)
  {
    DisplayLevel=0;
  }

  if (AttenIn == 1)
  {
    if (strcmp(AttenType, "PE4302")==0)
    {
      DisplayLevel=DisplayLevel-5*atten;
    }
    if (strcmp(AttenType, "PE43703")==0)
    {
      DisplayLevel=DisplayLevel-5*atten/2;
    }
  }
  printf("DisplayLevel = %d \n", DisplayLevel);
}

void SetAtten()
{
  // This function will send the attenuator command to the attenuator
  ;
}

void AdjustLevel(int Button)
{
  // Deal with audio levels differently
  if (strcmp(osctxt, "audio")==0)
  {
    ;  // Audio behaviour tbd
  }
  else
  {
    if (AttenIn == 0) // No attenuator
    {
      if (strcmp(osctxt, "pirf")==0)
      {
        ;  // pirf behaviour tbd
      }
      if (strcmp(osctxt, "adf4351")==0)
      {
        if (Button == (Menu1Buttons +1))  // decrement level
        {
          level = level - 1;
          if (level < 0 )
          {
            level = 0;
          }
        }
        if (Button == (Menu1Buttons +6))  // increment level
        {
          level = level + 1;
          if (level > 3 )
          {
            level = 3;
          }
        }
      }
      // No variable output for Portsdown
      if (strcmp(osctxt, "express")==0)
      {
        ;  // express behaviour tbd
      }
      if (strcmp(osctxt, "adf5355")==0)
      {
        ;  // adf5355 behaviour tbd
      }
    }
    else
    {
      ;  // code for attenuator here
    }
  }
}

void SetBandGPIOs()
{
  // Set Band GPIos high (for 1255) and filter and IQ low 
  // for max output and repeatability

  pinMode(BandLSBGPIO, OUTPUT);
  pinMode(BandMSBGPIO, OUTPUT);
  pinMode(FiltLSBGPIO, OUTPUT);
  pinMode(FiltNSBGPIO, OUTPUT);
  pinMode(FiltMSBGPIO, OUTPUT);
  pinMode(I_GPIO, OUTPUT);
  pinMode(Q_GPIO, OUTPUT);

  digitalWrite(BandLSBGPIO, HIGH);
  digitalWrite(BandMSBGPIO, HIGH);
  digitalWrite(FiltLSBGPIO, LOW);
  digitalWrite(FiltNSBGPIO, LOW);
  digitalWrite(FiltMSBGPIO, LOW);
  digitalWrite(I_GPIO, LOW);
  digitalWrite(Q_GPIO, LOW);
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
//    scaledY = shiftY+x/(scaleYvalue+factorY); Vertical flip for 4 inch screen
  }

  // printf("x=%d y=%d scaledx %d scaledy %d sxv %f syv %f\n",x,y,scaledX,scaledY,scaleXvalue,scaleYvalue);

  int margin=1;  // was 20

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
  Fill(255, 255, 255, 1);    // White text
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
  {
    return 0;
  }
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
  if (test_bit(i, bit[0])) 
  {
    printf("  Event type %d (%s)\n", i, events[i] ? events[i] : "?");
    if (!i) continue;
    ioctl(fd, EVIOCGBIT(i, KEY_MAX), bit[i]);
    for (j = 0; j < KEY_MAX; j++)
    {
      if (test_bit(j, bit[i]))
      {
        printf("    Event code %d (%s)\n", j, names[i] ? (names[i][j] ? names[i][j] : "?") : "?");
        if(j==330) IsAtouchDevice=1;
        if (i == EV_ABS)
        {
          ioctl(fd, EVIOCGABS(j), abs);
          for (k = 0; k < 5; k++)
          if ((k < 3) || abs[k])
          {
            printf("     %s %6d\n", absval[k], abs[k]);
            if (j == 0)
            {
              if ((strcmp(absval[k],"Min  ")==0)) *screenXmin =  abs[k];
              if ((strcmp(absval[k],"Max  ")==0)) *screenXmax =  abs[k];
            }
            if (j == 1)
            {
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

void ImposeBounds()  // Constrain DisplayFreq to physical limits
{
  if (strcmp(osctxt, "audio")==0)
  {
    SourceUpperFreq = 8000;
    SourceLowerFreq = 100;
  }
  if (strcmp(osctxt, "pirf")==0)
  {
    SourceUpperFreq = 50000000;
    SourceLowerFreq = 10000000;
  }

  if (strcmp(osctxt, "adf4351")==0)
  {
    SourceUpperFreq = 4294967295LL;
    SourceLowerFreq = 35000000;
  }

  if (strcmp(osctxt, "portsdown")==0)
  {
    SourceUpperFreq = 1500000000;
    SourceLowerFreq = 50000000;
  }

  if (strcmp(osctxt, "express")==0)
  {
    SourceUpperFreq = 2450000000LL;
    SourceLowerFreq = 70000000;
  }

  if (strcmp(osctxt, "adf5355")==0)
  {
    SourceUpperFreq = 13600000000LL;
    SourceLowerFreq = 54000000;
  }

  if (DisplayFreq > SourceUpperFreq)
  {
    DisplayFreq = SourceUpperFreq;
  }
  if (DisplayFreq < SourceLowerFreq)
  {
    DisplayFreq = SourceLowerFreq;
  }

}

void InitOsc()   
// Check the freq is in bounds and start/stop DATV express if req
// Read in amplitude Cal table and hide unused buttons
// Call CalcOPLevel
{
  char Param[256];
  char Value[256];
  char KillExpressSvr[255];
  int n;
  char PointNumber[255];
  ImposeBounds();
  if (strcmp(osctxt, "express")==0)
  {
    printf("Starting DATV Express\n");
    system("/home/pi/rpidatv/src/siggen/startexpresssvr.sh");
  }
  else
  {
    strcpy(KillExpressSvr, "echo \"set kill\" >> /tmp/expctrl");
    system(KillExpressSvr);
  }
  
  // Read in amplitude Cal table

  strcpy(Param, osctxt);
  strcat(Param, "points");
  GetConfigParam(PATH_CAL,Param,Value);
  CalPoints = atoi(Value);
  printf("CalPoints= %d \n", CalPoints);
  for ( n = 1; n <= CalPoints; n = n + 1 )
  {
    snprintf(PointNumber, 4, "%d", n);

    strcpy(Param, osctxt);
    strcat(Param, "freq");
    strcat(Param, PointNumber);
    GetConfigParam(PATH_CAL,Param,Value);
    CalFreq[n] = strtoull(Value, 0, 0);
    printf("CalFreq= %lld \n", CalFreq[n]);

    strcpy(Param, osctxt);
    strcat(Param, "lev");
    strcat(Param, PointNumber);
    GetConfigParam(PATH_CAL,Param,Value);
    CalLevel[n] = atoi(Value);
    printf("CalLevel= %d \n", CalLevel[n]);
  }

  // Hide unused buttons
  // First make them all visible
  for (n = 0; n < 3; n = n + 1)
    {
      SetButtonStatus(Menu1Buttons+n,0);         // Show all level decrement
      SetButtonStatus(Menu1Buttons+5+n, 0);     // Show all level increment
    }
  for (n = 8; n < 30; n = n + 1)
    {
      SetButtonStatus(Menu1Buttons+n,0);         // Show all freq inc/decrement
    }

  // Hide the unused frequency increment/decrement buttons
  if (strcmp(osctxt, "audio")==0)
  {
    for (n = 8; n < 14; n = n + 1)
    {
      SetButtonStatus(Menu1Buttons+n,1);         //hide frequency decrement above 99,999 Hz
      SetButtonStatus(Menu1Buttons+n+11, 1);     //hide frequency increment above 99,999 Hz
    }
  }
  if (strcmp(osctxt, "pirf")==0)
  {
    for (n = 8; n < 10; n = n + 1)
    {
      SetButtonStatus(Menu1Buttons+n,1);         //hide frequency decrement above 999 MHz
      SetButtonStatus(Menu1Buttons+n+11, 1);     //hide frequency increment above 999 MHz
    }
  }
  if (strcmp(osctxt, "adf4351")==0)
  {
    SetButtonStatus(Menu1Buttons+8,1);         //hide frequency decrement above 9.99 GHz
    SetButtonStatus(Menu1Buttons+19,1);        //hide frequency increment above 9.99 GHz
  }
  if (strcmp(osctxt, "portsdown")==0)
  {
    SetButtonStatus(Menu1Buttons+8,1);         //hide frequency decrement above 9.99 GHz
    SetButtonStatus(Menu1Buttons+19,1);        //hide frequency increment above 9.99 GHz
  }
  if (strcmp(osctxt, "express")==0)
  {
    SetButtonStatus(Menu1Buttons+8,1);         //hide frequency decrement above 9.99 GHz
    SetButtonStatus(Menu1Buttons+19,1);        //hide frequency increment above 9.99 GHz
  }

  // Hide the unused level buttons
  if (AttenIn == 0)
  {
    if (strcmp(osctxt, "audio")==0)
    {
      ;  // Audio behaviour tbd
    }
    if (strcmp(osctxt, "pirf")==0)
    {
      SetButtonStatus(Menu1Buttons+0,1);         // Hide decrement 10ths
      SetButtonStatus(Menu1Buttons+1,1);         // Hide decrement 1s
      SetButtonStatus(Menu1Buttons+2,1);         // Hide decrement 10s
      SetButtonStatus(Menu1Buttons+5,1);         // Hide increment 10ths
      SetButtonStatus(Menu1Buttons+6,1);         // Hide increment 1s
      SetButtonStatus(Menu1Buttons+7,1);         // Hide increment 10s
    }
    if (strcmp(osctxt, "adf4351")==0)
    {
      SetButtonStatus(Menu1Buttons+0,1);         // Hide decrement 10ths
      SetButtonStatus(Menu1Buttons+2,1);         // Hide decrement 10s
      SetButtonStatus(Menu1Buttons+5,1);         // Hide increment 10ths
      SetButtonStatus(Menu1Buttons+7,1);         // Hide increment 10s
    }
    if (strcmp(osctxt, "portsdown")==0)
    {
      SetButtonStatus(Menu1Buttons+0,1);         // Hide decrement 10ths
      SetButtonStatus(Menu1Buttons+1,1);         // Hide decrement 1s
      SetButtonStatus(Menu1Buttons+2,1);         // Hide decrement 10s
      SetButtonStatus(Menu1Buttons+5,1);         // Hide increment 10ths
      SetButtonStatus(Menu1Buttons+6,1);         // Hide increment 1s
      SetButtonStatus(Menu1Buttons+7,1);         // Hide increment 10s
    }
    if (strcmp(osctxt, "express")==0)
    {
      SetButtonStatus(Menu1Buttons+2,1);         // Hide decrement 10ths
      SetButtonStatus(Menu1Buttons+7,1);         // Hide increment 10ths
    }
    if (strcmp(osctxt, "adf5355")==0)
    {
      SetButtonStatus(Menu1Buttons+0,1);         // Hide decrement 10ths
      SetButtonStatus(Menu1Buttons+2,1);         // Hide decrement 10s
      SetButtonStatus(Menu1Buttons+5,1);         // Hide increment 10ths
      SetButtonStatus(Menu1Buttons+7,1);         // Hide increment 10s
    }
  }
  else //attenuator in probably all visible
  {
    ; // behaviour tbd
  }
  CalcOPLevel();  
}

void SelectOsc(int NoButton)  // Select Oscillator Source
{
  SelectInGroup(0,4,NoButton,1);
  SelectInGroup(9,9,NoButton,1);
  strcpy(osctxt,TabOsc[NoButton-0]);
  printf("************** Set osc = %s\n", osctxt);

  InitOsc();

  ShowTitle();
  ShowLevel(DisplayLevel);
  ShowFreq(DisplayFreq);

  UpdateWindow();


}

void SavePreset(int PresetNo)
{
  char Param[256];
  char Value[256];

  TabFreq[PresetNo] = DisplayFreq;
  snprintf(Value, 12, "%lld", DisplayFreq);
  strcpy(Param, FreqTag[PresetNo]);
  SetConfigParam(PATH_CONFIG, Param, Value);

  strcpy(TabOscOption[PresetNo], osctxt);
  strcpy(Param, OscTag[PresetNo]);
  SetConfigParam(PATH_CONFIG, Param, osctxt);

  snprintf(Value, 4, "%d", level);
  strcpy(TabLevel[PresetNo], Value);
  strcpy(Param, LevelTag[PresetNo]);
  SetConfigParam(PATH_CONFIG ,Param, Value);

  snprintf(Value, 5, "%d", atten);
  strcpy(TabAtten[PresetNo], Value);
  strcpy(Param, AttenTag[PresetNo]);
  SetConfigParam(PATH_CONFIG ,Param, Value);
}

void RecallPreset(int PresetNo)
{
  DisplayFreq = TabFreq[PresetNo];
  strcpy(osctxt, TabOscOption[PresetNo]);
  level=atoi(TabLevel[PresetNo]);
  atten=atoi(TabAtten[PresetNo]);

  InitOsc();
}

void SelectAtten(int NoButton,int Status)  // Attenuator on or off
{
/*  char Param[]="caption";
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
*/
	SelectInGroup(10,10,NoButton,Status);

}

void SelectMod(int NoButton,int Status)  // Modulation on or off
{
  SelectInGroup(11,11,NoButton,Status);
  SelectInGroup(Menu1Buttons+5,Menu1Buttons+5,NoButton,Status);

}

void OscStart()
{
  //  Look up which oscillator we are using

  // Then use an if statement for each alternative

  printf("Oscillator Start\n");
  char StartPortsdown[256] = "sudo /home/pi/rpidatv/bin/adf4351 ";
  char transfer[256];
  //char osc[256]="portsdown";
  float freqmhz;
  int adf4351_lev = level; // 0 to 3

  if (strcmp(osctxt, "audio")==0)
  {
    printf("\nStarting Audio output\n");
  }

  if (strcmp(osctxt, "pirf")==0)
  {
    printf("\nStarting RPi RF output\n");
  }

  if (strcmp(osctxt, "adf4351")==0)
  {
    freqmhz=DisplayFreq/1000000;
    snprintf(transfer, 13, "%.6f", freqmhz);
    strcat(StartPortsdown, transfer);
    strcat(StartPortsdown, " ");
    strcat(StartPortsdown, ref_freq_4351);
    strcat(StartPortsdown, " ");
    snprintf(transfer, 2, "%d", adf4351_lev);
    strcat(StartPortsdown, transfer);
    printf(StartPortsdown);
    printf("\nStarting ADF4351 Output\n");
    system(StartPortsdown);
  }

  if (strcmp(osctxt, "portsdown")==0)
  {
    // Add code to select the band to 23cms here
    freqmhz=DisplayFreq/1000000;
    snprintf(transfer, 13, "%.6f", freqmhz);
    strcat(StartPortsdown, transfer);
    strcat(StartPortsdown, " ");
    strcat(StartPortsdown, ref_freq_4351);
    strcat(StartPortsdown, " 3"); // Level 3 for Portsdown F-M
    printf(StartPortsdown);
    printf("\nStarting portsdown output\n");
    system(StartPortsdown);
  }

  if (strcmp(osctxt, "express")==0)
  {
    printf("\nStarting Express RF output\n");
  }

  if (strcmp(osctxt, "adf5355")==0)
  {
    printf("\nStarting ADF5355 output\n");
  }


  SetButtonStatus(13,1);
  SetButtonStatus(47,1);
  OutputStatus = 1;
}

void OscStop()
{
  char expressrx[256];
  printf("Oscillator Stop\n");

  if (strcmp(osctxt, "audio")==0)
  {
    printf("\nStopping audio output\n");
  }

  if (strcmp(osctxt, "adf4351")==0)
  {
    system("sudo /home/pi/rpidatv/bin/adf4351 off");    
    printf("\nStopping adf4351 output\n");
  }

  if (strcmp(osctxt, "portsdown")==0)
  {
    system("sudo /home/pi/rpidatv/bin/adf4351 off");    
    printf("\nStopping Portsdown output\n");
  }

  if (strcmp(osctxt, "express")==0)
  {
    strcpy( expressrx, "echo \"set ptt rx\" >> /tmp/expctrl" );
    system(expressrx);
    strcpy( expressrx, "echo \"set car off\" >> /tmp/expctrl" );
    system(expressrx);
    system("sudo killall netcat >/dev/null 2>/dev/null");
    printf("\nStopping Express output\n");
  }

  if (strcmp(osctxt, "adf5355")==0)
  {
    printf("\nStopping ADF5355 output\n");
  }

  // Kill the key processes as nicely as possible
  system("sudo killall rpidatv >/dev/null 2>/dev/null");
  system("sudo killall ffmpeg >/dev/null 2>/dev/null");
  system("sudo killall tcanim >/dev/null 2>/dev/null");
  system("sudo killall avc2ts >/dev/null 2>/dev/null");
  system("sudo killall netcat >/dev/null 2>/dev/null");

  // Then pause and make sure that avc2ts has really been stopped (needed at high SRs)
  usleep(1000);
  system("sudo killall -9 avc2ts >/dev/null 2>/dev/null");

  // And make sure rpidatv has been stopped (required for brief transmit selections)
  system("sudo killall -9 rpidatv >/dev/null 2>/dev/null");
  SetButtonStatus(13,0);
  SetButtonStatus(47,0);
  OutputStatus = 0;
}

void coordpoint(VGfloat x, VGfloat y, VGfloat size, VGfloat pcolor[4])
{
  setfill(pcolor);
  Circle(x, y, size);
  setfill(pcolor);
}

void *WaitButtonEvent(void * arg)
{
  int rawX, rawY, rawPressure;
  while(getTouchSample(&rawX, &rawY, &rawPressure)==0);
  FinishedButton=1;
  return NULL;
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

// wait for a screen touch and act on its position
void waituntil(int w,int h)
{
  int rawX, rawY, rawPressure, i, ExitSignal ;
  ExitSignal=0; 

  // Start the main loop for the Touchscreen
  // Loop forever until exit button touched
  while (ExitSignal==0)
  {
    // Wait here until screen touched
    if (getTouchSample(&rawX, &rawY, &rawPressure)==0) continue;

    // Screen has been touched
    printf("x=%d y=%d\n",rawX,rawY);

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

          // Clear Preset store trigger if not a preset
          if ((i <= 4) || (i >= 9))
          {
            PresetStoreTrigger = 0;
            SetButtonStatus(12,0);
          }

          if((i>=0)&&(i<=4)) // Signal Source
          {
            OscStop();                // Stop the current output
            if ((i == 2) || (i == 3))
            {
              SelectOsc(i);
            }
            else
            {
              MsgBox("Output Mode not implemented yet");
              wait_touch();
            }
          }
          if((i>=5)&&(i<=8)) // Presets 1-4
          {
            if (PresetStoreTrigger == 0)
            {
              RecallPreset(i-4);  // Recall preset
              Start_Highlights_Menu1();
            }
            else
            {
              SavePreset(i-4);  // Set preset
              PresetStoreTrigger = 0;
              SetButtonStatus(12,0);
            }
          }
          if(i==9) // Signal Source
          {
            OscStop();               // Stop the current output
            MsgBox("ADF5355 not implemented yet");
            wait_touch();
            /*
            SelectOsc(i);
            */
          }
          if(i==10) // Attenuator in/out
          {
            MsgBox("Attenuator not implemented yet");
            wait_touch();
            /* if (AttenIn == 0)
            {
              AttenIn=1;
              SetAtten();
              SetButtonStatus(10,1);
            }
            else
            {
              AttenIn=0;
              SetButtonStatus(10,0);
            }
            CalcOPLevel();
            */
          }
          if(i==11) // Modulation on/off
          {
            MsgBox("Modulation not implemented yet");
            wait_touch();
            /* if (ModOn == 0)
            {
              ModOn=1;
              // Do something to turn the mod on
              SetButtonStatus(11,1);
              SetButtonStatus(Menu1Buttons+4,1);
            }
            else
            {
              ModOn=0;
              SetButtonStatus(11,0);
              SetButtonStatus(Menu1Buttons+4,0);
              // Do something to turn the mod off
            }
            */
          }
          if(i==12) // Set Preset
          {
            SetButtonStatus(12,1);
            PresetStoreTrigger = 1;
          }
          if(i==13) // Output on
          {
            if (OutputStatus == 1)  // Oscillator already running
            {
              OscStop();
            }
            else
            {
              OscStart();
            }
          }
          if(i==14) // Output off
          {
            OscStop();
          }
          if(i==15) // Exit to Portsdown
          {
            printf("Exiting Sig Gen \n");
            ExitSignal=1;
            BackgroundRGB(255,255,255,255);
            UpdateWindow();
          }
          if(i==16) // Freq menu
          {
            printf("Switch to FREQ menu \n");
            CurrentMenu=2;
            BackgroundRGB(0,0,0,255);
            ShowTitle();
            ShowFreq(DisplayFreq);
            ShowLevel(DisplayLevel);
          }
          ShowTitle();
          ShowLevel(DisplayLevel);
          ShowFreq(DisplayFreq);
          UpdateWindow();
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

          if(i>=((Menu1Buttons+0))&&(i<=(Menu1Buttons+2))) // Decrement Level
          {
            AdjustLevel(i);
            CalcOPLevel();
            if (OutputStatus == 1)
            {
              OscStart();
            }
          }
          if(i==(Menu1Buttons+3)) // Save
          {
            SetButtonStatus(Menu1Buttons+3,1);
            UpdateWindow();
            SavePreset(0);
            usleep(250000);
            SetButtonStatus(Menu1Buttons+3,0);
          }
          if(i==(Menu1Buttons+4)) // Modulation on-off
          {
           MsgBox("Modulation not implemented yet");
            wait_touch();
            /* if (ModOn == 0)
            {
              ModOn=1;
              // Do something to turn the mod on
              SetButtonStatus(11,1);
              SetButtonStatus(Menu1Buttons+4,1);
            }
            else
            {
              ModOn=0;
              SetButtonStatus(11,0);
              SetButtonStatus(Menu1Buttons+4,0);
              // Do something to turn the mod off
            }
            */
          }
          if(i>=((Menu1Buttons+5))&&(i<=(Menu1Buttons+7))) // Increment Level
          {
            AdjustLevel(i);
            CalcOPLevel();
            if (OutputStatus == 1)
            {
              OscStart();
            }
          }
          if(i>=((Menu1Buttons+8))&&(i<=(Menu1Buttons+29))) // Adjust Frequency
          {
            AdjustFreq(i);
            CalcOPLevel();
            if (OutputStatus == 1)
            {
              OscStart();
            }
            BackgroundRGB(0,0,0,255);
            ShowTitle();
            ShowFreq(DisplayFreq);
            ShowLevel(DisplayLevel);
          }
          if(i==(Menu1Buttons+30)) // Oscillator on
          {
            if (OutputStatus == 1)  // Oscillator already running
            {
              OscStop();
            }
            else
            {
              OscStart();
            }
          }
          if(i==(Menu1Buttons+31))
          {
            OscStop();
          }
          if(i==(Menu1Buttons+32)) // Exit to Portsdown
          {
            printf("Exiting Sig Gen \n");
            ExitSignal=1;
            BackgroundRGB(255,255,255,255);
            UpdateWindow();
          }
          if(i==(Menu1Buttons+33)) // Freq menu
          {
            printf("Switch to CTRL menu \n");
            CurrentMenu=1;
            BackgroundRGB(0,0,0,255);
            Start_Highlights_Menu1();
            ShowTitle();
            ShowFreq(DisplayFreq);
            ShowLevel(DisplayLevel);
          }
          ShowTitle();
          ShowLevel(DisplayLevel);
          ShowFreq(DisplayFreq);
          UpdateWindow();
        }
      }
      break;
    default:
      break;
    }
  }
}

void Start_Highlights_Menu1()
// Retrieves memory value for each group of buttons
// and then sets the correct highlight
// Called on program start and after preset recall
{
  char Value[255];
  strcpy(Value, osctxt);
  if(strcmp(Value,TabOsc[0])==0)
  {
    SelectInGroup(0,4,0,1);
  }
  if(strcmp(Value,TabOsc[1])==0)
  {
    SelectInGroup(0,4,1,1);
  }
  if(strcmp(Value,TabOsc[2])==0)
  {
    SelectInGroup(0,4,2,1);
  }
  if(strcmp(Value,TabOsc[3])==0)
  {
    SelectInGroup(0,4,3,1);
  }
  if(strcmp(Value,TabOsc[9])==0)
  {
    SelectInGroup(0,4,9,1);
  }
}

void Define_Menu1()
{
  Menu1Buttons=17;
  // Source: Audio, Pi RF, ADF4351, Portsdown, Express

	int button=AddButton(0*wbuttonsize+20,0+hbuttonsize*0+20,wbuttonsize*0.9,hbuttonsize*0.9);
	color_t Col;
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button," Audio ",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button," Audio ",&Col);

	button=AddButton(1*wbuttonsize+20,hbuttonsize*0+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button," Pi RF",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button," Pi RF",&Col);

	button=AddButton(2*wbuttonsize+20,hbuttonsize*0+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,"ADF4351",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button,"ADF4351",&Col);

	button=AddButton(3*wbuttonsize+20,hbuttonsize*0+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,"Portsdown",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button,"Portsdown",&Col);

	button=AddButton(4*wbuttonsize+20,hbuttonsize*0+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,"Express",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button,"Express",&Col);

// Presets

	button=AddButton(0*wbuttonsize+20,0+hbuttonsize*1+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,"  P1  ",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button,"  P1  ",&Col);

	button=AddButton(1*wbuttonsize+20,hbuttonsize*1+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,"  P2  ",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button,"  P2  ",&Col);

	button=AddButton(2*wbuttonsize+20,hbuttonsize*1+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,"  P3  ",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button,"  P3  ",&Col);

	button=AddButton(3*wbuttonsize+20,hbuttonsize*1+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,"  P4  ",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button,"  P4  ",&Col);

  // Extra Source button

	button=AddButton(4*wbuttonsize+20,hbuttonsize*1+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,"ADF5355",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button,"ADF5355",&Col);

// Space for Level then Atten (on/off) Mod (on/off) and Set

	button=AddButton(2*wbuttonsize+20,hbuttonsize*2+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button," Atten ",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button," Atten ",&Col);

	button=AddButton(3*wbuttonsize+20,hbuttonsize*2+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,"  Mod  ",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button,"  Mod  ",&Col);

	button=AddButton(4*wbuttonsize+20,hbuttonsize*2+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,"  Save P",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button,"  Save P",&Col);

//  ON, OFF, EXIT, FREQ

	button=AddButton(0*wbuttonsize+20,hbuttonsize*4+20,wbuttonsize*1.2,hbuttonsize*1.2);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,"  ON  ",&Col);
	Col.r=255;Col.g=0;Col.b=0;
	AddButtonStatus(button,"  ON  ",&Col);

        button=AddButton(1.35*wbuttonsize+20,hbuttonsize*4+20,wbuttonsize*1.2,hbuttonsize*1.2);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,"  OFF ",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button,"  OFF ",&Col);

        button=AddButton(2.7*wbuttonsize+20,hbuttonsize*4+20,wbuttonsize*1.2,hbuttonsize*1.2);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button," EXIT ",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button," EXIT ",&Col);

        button=AddButton(4*wbuttonsize+20,hbuttonsize*4+20,wbuttonsize*0.9,hbuttonsize*1.2);
        Col.r=0;Col.g=0;Col.b=128;
        AddButtonStatus(button,"Freq",&Col);
        Col.r=0;Col.g=128;Col.b=0;
        AddButtonStatus(button,"Freq",&Col);
}

void Define_Menu2()
{
  Menu2Buttons=34;
  // Bottom row: subtract 10s, units and tenths of a dB

  int button=AddButton(1*swbuttonsize+20,0+shbuttonsize*0+20,swbuttonsize*0.9,shbuttonsize*0.9);
  color_t Col;
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," _ ",&Col);
  Col.r=0;Col.g=0;Col.b=0;
  AddButtonStatus(button," ",&Col);

  button=AddButton(2*swbuttonsize+20,shbuttonsize*0+20,swbuttonsize*0.9,shbuttonsize*0.9);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," _ ",&Col);
  Col.r=0;Col.g=0;Col.b=0;
  AddButtonStatus(button," ",&Col);

  button=AddButton(3.2*swbuttonsize+20,shbuttonsize*0+20,swbuttonsize*0.9,shbuttonsize*0.9);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," _ ",&Col);
  Col.r=0;Col.g=0;Col.b=0;
  AddButtonStatus(button," ",&Col);

  // Save and Mod Buttons

  button=AddButton(3*wbuttonsize+20,hbuttonsize*0.5+20,wbuttonsize*0.9,hbuttonsize*0.9);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," Save ",&Col);
  Col.r=0;Col.g=128;Col.b=0;
  AddButtonStatus(button," Save ",&Col);

  button=AddButton(4*wbuttonsize+20,hbuttonsize*0.5+20,wbuttonsize*0.9,hbuttonsize*0.9);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button,"  Mod ",&Col);
  Col.r=0;Col.g=128;Col.b=0;
  AddButtonStatus(button,"  Mod ",&Col);

  // Bottom row: add 10s, units and tenths of a dB

  button=AddButton(1*swbuttonsize+20,0+shbuttonsize*2+20,swbuttonsize*0.9,shbuttonsize*0.9);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," + ",&Col);
  Col.r=0;Col.g=0;Col.b=0;
  AddButtonStatus(button," ",&Col);

  button=AddButton(2*swbuttonsize+20,shbuttonsize*2+20,swbuttonsize*0.9,shbuttonsize*0.9);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," + ",&Col);
  Col.r=0;Col.g=0;Col.b=0;
  AddButtonStatus(button," ",&Col);

  button=AddButton(3.2*swbuttonsize+20,shbuttonsize*2+20,swbuttonsize*0.9,shbuttonsize*0.9);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," + ",&Col);
  Col.r=0;Col.g=0;Col.b=0;
  AddButtonStatus(button," ",&Col);

  // Decrement frequency: subtract 10s, units and tenths of a dB

  button=AddButton(0.4*swbuttonsize+20,0+shbuttonsize*3.5+20,swbuttonsize*0.9,shbuttonsize*0.9);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," _ ",&Col);
  Col.r=0;Col.g=0;Col.b=0;
  AddButtonStatus(button," ",&Col);

  button=AddButton(1.4*swbuttonsize+20,shbuttonsize*3.5+20,swbuttonsize*0.9,shbuttonsize*0.9);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," _ ",&Col);
  Col.r=0;Col.g=0;Col.b=0;
  AddButtonStatus(button," ",&Col);

  button=AddButton(2.6*swbuttonsize+20,shbuttonsize*3.5+20,swbuttonsize*0.9,shbuttonsize*0.9);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," _ ",&Col);
  Col.r=0;Col.g=0;Col.b=0;
  AddButtonStatus(button," ",&Col);

  button=AddButton(3.6*swbuttonsize+20,0+shbuttonsize*3.5+20,swbuttonsize*0.9,shbuttonsize*0.9);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," _ ",&Col);
  Col.r=0;Col.g=0;Col.b=0;
  AddButtonStatus(button," ",&Col);

  button=AddButton(4.6*swbuttonsize+20,shbuttonsize*3.5+20,swbuttonsize*0.9,shbuttonsize*0.9);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," _ ",&Col);
  Col.r=0;Col.g=0;Col.b=0;
  AddButtonStatus(button," ",&Col);

  button=AddButton(5.8*swbuttonsize+20,shbuttonsize*3.5+20,swbuttonsize*0.9,shbuttonsize*0.9);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," _ ",&Col);
  Col.r=0;Col.g=0;Col.b=0;
  AddButtonStatus(button," ",&Col);

  button=AddButton(6.8*swbuttonsize+20,0+shbuttonsize*3.5+20,swbuttonsize*0.9,shbuttonsize*0.9);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," _ ",&Col);
  Col.r=0;Col.g=0;Col.b=0;
  AddButtonStatus(button," ",&Col);

  button=AddButton(7.8*swbuttonsize+20,shbuttonsize*3.5+20,swbuttonsize*0.9,shbuttonsize*0.9);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," _ ",&Col);
  Col.r=0;Col.g=0;Col.b=0;
  AddButtonStatus(button," ",&Col);

  button=AddButton(9*swbuttonsize+20,shbuttonsize*3.5+20,swbuttonsize*0.9,shbuttonsize*0.9);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," _ ",&Col);
  Col.r=0;Col.g=0;Col.b=0;
  AddButtonStatus(button," ",&Col);

  button=AddButton(10*swbuttonsize+20,0+shbuttonsize*3.5+20,swbuttonsize*0.9,shbuttonsize*0.9);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," _ ",&Col);
  Col.r=0;Col.g=0;Col.b=0;
  AddButtonStatus(button," ",&Col);

  button=AddButton(11*swbuttonsize+20,shbuttonsize*3.5+20,swbuttonsize*0.9,shbuttonsize*0.9);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," _ ",&Col);
  Col.r=0;Col.g=0;Col.b=0;
  AddButtonStatus(button," ",&Col);


  // Increment frequency

  button=AddButton(0.4*swbuttonsize+20,0+shbuttonsize*5.5+20,swbuttonsize*0.9,shbuttonsize*0.9);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," + ",&Col);
  Col.r=0;Col.g=0;Col.b=0;
  AddButtonStatus(button," ",&Col);

  button=AddButton(1.4*swbuttonsize+20,shbuttonsize*5.5+20,swbuttonsize*0.9,shbuttonsize*0.9);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," + ",&Col);
  Col.r=0;Col.g=0;Col.b=0;
  AddButtonStatus(button," ",&Col);

  button=AddButton(2.6*swbuttonsize+20,shbuttonsize*5.5+20,swbuttonsize*0.9,shbuttonsize*0.9);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," + ",&Col);
  Col.r=0;Col.g=0;Col.b=0;
  AddButtonStatus(button," ",&Col);

  button=AddButton(3.6*swbuttonsize+20,0+shbuttonsize*5.5+20,swbuttonsize*0.9,shbuttonsize*0.9);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," + ",&Col);
  Col.r=0;Col.g=0;Col.b=0;
  AddButtonStatus(button," ",&Col);

  button=AddButton(4.6*swbuttonsize+20,shbuttonsize*5.5+20,swbuttonsize*0.9,shbuttonsize*0.9);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," + ",&Col);
  Col.r=0;Col.g=0;Col.b=0;
  AddButtonStatus(button," ",&Col);

  button=AddButton(5.8*swbuttonsize+20,shbuttonsize*5.5+20,swbuttonsize*0.9,shbuttonsize*0.9);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," + ",&Col);
  Col.r=0;Col.g=0;Col.b=0;
  AddButtonStatus(button," ",&Col);

  button=AddButton(6.8*swbuttonsize+20,shbuttonsize*5.5+20,swbuttonsize*0.9,shbuttonsize*0.9);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," + ",&Col);
  Col.r=0;Col.g=0;Col.b=0;
  AddButtonStatus(button," ",&Col);

  button=AddButton(7.8*swbuttonsize+20,shbuttonsize*5.5+20,swbuttonsize*0.9,shbuttonsize*0.9);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," + ",&Col);
  Col.r=0;Col.g=0;Col.b=0;
  AddButtonStatus(button," ",&Col);

  button=AddButton(9*swbuttonsize+20,0+shbuttonsize*5.5+20,swbuttonsize*0.9,shbuttonsize*0.9);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," + ",&Col);
  Col.r=0;Col.g=0;Col.b=0;
  AddButtonStatus(button," ",&Col);

  button=AddButton(10*swbuttonsize+20,shbuttonsize*5.5+20,swbuttonsize*0.9,shbuttonsize*0.9);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," + ",&Col);
  Col.r=0;Col.g=0;Col.b=0;
  AddButtonStatus(button," ",&Col);

  button=AddButton(11*swbuttonsize+20,shbuttonsize*5.5+20,swbuttonsize*0.9,shbuttonsize*0.9);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," + ",&Col);
  Col.r=0;Col.g=0;Col.b=0;
  AddButtonStatus(button," ",&Col);

//  ON, OFF, EXIT, CTRL

  button=AddButton(0*wbuttonsize+20,hbuttonsize*4+20,wbuttonsize*1.2,hbuttonsize*1.2);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button,"  ON  ",&Col);
  Col.r=255;Col.g=0;Col.b=0;
  AddButtonStatus(button,"  ON  ",&Col);

  button=AddButton(1.35*wbuttonsize+20,hbuttonsize*4+20,wbuttonsize*1.2,hbuttonsize*1.2);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button,"  OFF ",&Col);
  Col.r=0;Col.g=128;Col.b=0;
  AddButtonStatus(button,"  OFF ",&Col);

  button=AddButton(2.7*wbuttonsize+20,hbuttonsize*4+20,wbuttonsize*1.2,hbuttonsize*1.2);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," EXIT ",&Col);
  Col.r=0;Col.g=128;Col.b=0;
  AddButtonStatus(button," EXIT ",&Col);

  button=AddButton(4*wbuttonsize+20,hbuttonsize*4+20,wbuttonsize*0.9,hbuttonsize*1.2);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button,"Config",&Col);
  Col.r=0;Col.g=128;Col.b=0;
  AddButtonStatus(button,"Config",&Col);
}

static void
terminate(int dummy)
{
  OscStop();
  printf("Terminate\n");
  char Commnd[255];
  sprintf(Commnd,"stty echo");
  system(Commnd);
  sprintf(Commnd,"reset");
  system(Commnd);
  exit(1);
}

// Main initializes the system and shows the Control menu (Menu 1) 

int main(int argc, char **argv)
{
  int NoDeviceEvent=0;
  saveterm();
  init(&wscreen, &hscreen);
  rawterm();
  int screenXmax, screenXmin;
  int screenYmax, screenYmin;
  int i;
  char Param[255];
  char Value[255];

  // Catch sigaction and call terminate
  for (i = 0; i < 16; i++)
  {
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
  GetConfigParam(PATH_CONFIG_PORTSDOWN,Param,Value);
  if((strcmp(Value,"Waveshare")==0) || (strcmp(Value,"WaveshareB")==0))
  {
    Inversed = 1;
  }

  //Look up ADF4351 Ref Freq from Portsdown Config
  strcpy(Param,"adfref");
  GetConfigParam(PATH_CONFIG_PORTSDOWN,Param,Value);
  strcpy(ref_freq_4351,Value);

  // Set the Band Switching to 23cm to take LPFs out of circuit
  //gpio -g write $band_bit0 1;
  //gpio -g write $band_bit1 1;

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
  wbuttonsize=(wscreen-25)/5;  // width of normal button
  hbuttonsize=hscreen/6;       // height of normal button
  swbuttonsize=(wscreen-25)/12;  // width of small button
  shbuttonsize=hscreen/10;       // height of small button

  // Set up wiringPi module
  if (wiringPiSetup() < 0)
  {
    return 0;
  }

  // Set the Portsdown band to 1255 to bypass the LO filter
  SetBandGPIOs();
  
  // Read in the presets from the Config file
  ReadPresets();

  // Define the buttons for Menu 1
  Define_Menu1();

  // Define the buttons for Menu 2
  Define_Menu2();

  // Start the button Menu
  Start(wscreen,hscreen);
  IsDisplayOn=1;

  // Determine button highlights
  Start_Highlights_Menu1();

  // Validate the current value and initialise the oscillator
  InitOsc();

  ShowTitle();
  ShowLevel(DisplayLevel);
  ShowFreq(DisplayFreq);

  UpdateWindow();
  printf("Update Window\n");

  // Go and wait for the screen to be touched
  waituntil(wscreen,hscreen);

  // Program flow only gets here when exit button pushed
  // Start the Portsdown DATV TX and exit
  system("(/home/pi/rpidatv/bin/rpidatvgui) &");
  exit(0);
}
