#!/bin/bash

# Version 201711270

############ Set Environment Variables ###############

PATHSCRIPT=/home/pi/rpidatv/scripts
PATHRPI=/home/pi/rpidatv/bin
CONFIGFILE=$PATHSCRIPT"/rpidatvconfig.txt"
PATHCONFIGS="/home/pi/rpidatv/scripts/configs"  ## Path to config files
GPIO_PTT=29  ## WiringPi value, not BCM

############ Function to Write to Config File ###############

set_config_var() {
lua - "$1" "$2" "$3" <<EOF > "$3.bak"
local key=assert(arg[1])
local value=assert(arg[2])
local fn=assert(arg[3])
local file=assert(io.open(fn))
local made_change=false
for line in file:lines() do
if line:match("^#?%s*"..key.."=.*$") then
line=key.."="..value
made_change=true
end
print(line)
end
if not made_change then
print(key.."="..value)
end
EOF
mv "$3.bak" "$3"
}

############ Function to Read from Config File ###############

get_config_var() {
lua - "$1" "$2" <<EOF
local key=assert(arg[1])
local fn=assert(arg[2])
local file=assert(io.open(fn))
for line in file:lines() do
local val = line:match("^#?%s*"..key.."=(.*)$")
if (val ~= nil) then
print(val)
break
end
end
EOF
}

############ Function to Select Files ###############

Filebrowser() {
if [ -z $1 ]; then
imgpath=$(ls -lhp / | awk -F ' ' ' { print $9 " " $5 } ')
else
imgpath=$(ls -lhp "/$1" | awk -F ' ' ' { print $9 " " $5 } ')
fi
if [ -z $1 ]; then
pathselect=$(whiptail --menu "$FileBrowserTitle""$filename" 20 50 10 --cancel-button Cancel --ok-button Select $imgpath 3>&1 1>&2 2>&3)
else
pathselect=$(whiptail --menu "$FileBrowserTitle""$filename" 20 50 10 --cancel-button Cancel --ok-button Select ../ BACK $imgpath 3>&1 1>&2 2>&3)
fi
RET=$?
if [ $RET -eq 1 ]; then
## This is the section where you control what happens when the user hits Cancel
Cancel
elif [ $RET -eq 0 ]; then
	if [[ -d "/$1$pathselect" ]]; then
		Filebrowser "/$1$pathselect"
	elif [[ -f "/$1$pathselect" ]]; then
		## Do your thing here, this is just a stub of the code I had to do what I wanted the script to do.
		fileout=`file "$1$pathselect"`
		filename=`readlink -m $1$pathselect`
	else
		echo pathselect $1$pathselect
		whiptail --title "$FileMenuTitle" --msgbox "$FileMenuContext" 8 44
		unset base
		unset imgpath
		Filebrowser
	fi
fi
}

############ Function to Select Paths ###############

Pathbrowser() {
if [ -z $1 ]; then
imgpath=$(ls -lhp / | awk -F ' ' ' { print $9 " " $5 } ')
else
imgpath=$(ls -lhp "/$1" | awk -F ' ' ' { print $9 " " $5 } ')
fi
if [ -z $1 ]; then
pathselect=$(whiptail --menu "$FileBrowserTitle""$filename" 20 50 10 --cancel-button Cancel --ok-button Select $imgpath 3>&1 1>&2 2>&3)
else
pathselect=$(whiptail --menu "$FileBrowserTitle""$filename" 20 50 10 --cancel-button Cancel --ok-button Select ../ BACK $imgpath 3>&1 1>&2 2>&3)
fi
RET=$?
if [ $RET -eq 1 ]; then
## This is the section where you control what happens when the user hits Cancel
Cancel	
elif [ $RET -eq 0 ]; then
	if [[ -d "/$1$pathselect" ]]; then
		Pathbrowser "/$1$pathselect"
	elif [[ -f "/$1$pathselect" ]]; then
		## Do your thing here, this is just a stub of the code I had to do what I wanted the script to do.
		fileout=`file "$1$pathselect"`
		filenametemp=`readlink -m $1$pathselect`
		filename=`dirname $filenametemp` 

	else
		echo pathselect $1$pathselect
		whiptail --title "$FileMenuTitle" --msgbox "$FileMenuContext" 8 44
		unset base
		unset imgpath
		Pathbrowser
	fi

fi
}

############### Function to rewrite in-use Contest Numbers Image #########################

do_refresh_numbers()
{
  # Delete old numbers
  rm /home/pi/rpidatv/scripts/images/contest0.png >/dev/null 2>/dev/null
  rm /home/pi/rpidatv/scripts/images/contest1.png >/dev/null 2>/dev/null
  rm /home/pi/rpidatv/scripts/images/contest2.png >/dev/null 2>/dev/null
  rm /home/pi/rpidatv/scripts/images/contest3.png >/dev/null 2>/dev/null

  FREQ_OUTPUT=$(get_config_var freqoutput $CONFIGFILE)
  INT_FREQ_OUTPUT=${FREQ_OUTPUT%.*}
  LOCATOR=$(get_config_var locator $CONFIGFILE)

  NUMBERS0=$(get_config_var numbers0 $CONFIGFILE)
  convert -size 480x360 xc:white \
    -gravity North -pointsize 75 -annotate 0 "$CALL" \
    -gravity Center -pointsize 150 -annotate 0 "$NUMBERS0" \
    -gravity South -pointsize 50 -annotate 0 "$LOCATOR""    4 Metres" \
    /home/pi/rpidatv/scripts/images/contest0.png

  NUMBERS1=$(get_config_var numbers1 $CONFIGFILE)
  convert -size 480x360 xc:white \
    -gravity North -pointsize 75 -annotate 0 "$CALL" \
    -gravity Center -pointsize 150 -annotate 0 "$NUMBERS1" \
    -gravity South -pointsize 50 -annotate 0 "$LOCATOR""    2 Metres" \
    /home/pi/rpidatv/scripts/images/contest1.png

  NUMBERS2=$(get_config_var numbers2 $CONFIGFILE)
  convert -size 480x360 xc:white \
    -gravity North -pointsize 75 -annotate 0 "$CALL" \
    -gravity Center -pointsize 150 -annotate 0 "$NUMBERS2" \
    -gravity South -pointsize 50 -annotate 0 "$LOCATOR""    70 cm" \
    /home/pi/rpidatv/scripts/images/contest2.png

  NUMBERS3=$(get_config_var numbers3 $CONFIGFILE)
  convert -size 480x360 xc:white \
    -gravity North -pointsize 75 -annotate 0 "$CALL" \
    -gravity Center -pointsize 150 -annotate 0 "$NUMBERS3" \
    -gravity South -pointsize 50 -annotate 0 "$LOCATOR""    23 cm" \
    /home/pi/rpidatv/scripts/images/contest3.png

  if (( $INT_FREQ_OUTPUT \< 100 )); then
    cp -f /home/pi/rpidatv/scripts/images/contest0.png /home/pi/rpidatv/scripts/images/contest.png
  elif (( $INT_FREQ_OUTPUT \< 250 )); then
    cp -f /home/pi/rpidatv/scripts/images/contest1.png /home/pi/rpidatv/scripts/images/contest.png
  elif (( $INT_FREQ_OUTPUT \< 950 )); then
    cp -f /home/pi/rpidatv/scripts/images/contest2.png /home/pi/rpidatv/scripts/images/contest.png
  else
    cp -f /home/pi/rpidatv/scripts/images/contest3.png /home/pi/rpidatv/scripts/images/contest.png
  fi
}

############### Function to show in-use Contest Numbers Image #########################

do_show_numbers()
{
  sudo fbi -T 1 -noverbose -a /home/pi/rpidatv/scripts/images/contest.png >/dev/null 2>/dev/null
  (sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &  ## kill fbi once it has done its work
}

################################### Menus ####################################

do_input_setup()
{
  MODE_INPUT=$(get_config_var modeinput $CONFIGFILE)
  Radio1=OFF
  Radio2=OFF
  Radio3=OFF
  Radio4=OFF
  Radio5=OFF
  Radio6=OFF
  Radio7=OFF
  Radio8=OFF
  Radio9=OFF
  Radio10=OFF
  Radio11=OFF
  Radio12=OFF
  Radio13=OFF
  Radio14=OFF
  case "$MODE_INPUT" in
  CAMH264)
    Radio1=ON
  ;;
  CAMMPEG-2)
    Radio2=ON
  ;;
  FILETS)
    Radio3=ON
  ;;
  PATERNAUDIO)
    Radio4=ON
  ;;
  CARRIER)
    Radio5=ON
  ;;
  TESTMODE)
    Radio6=ON
  ;;
  IPTSIN)
    Radio7=ON
  ;;
  ANALOGCAM)
    Radio8=ON
  ;;
  VNC)
    Radio9=ON
  ;;
  DESKTOP)
    Radio10=ON
  ;;
  CONTEST)
    Radio11=ON
  ;;
  ANALOGMPEG-2)
    Radio12=ON
  ;;
  CARDMPEG-2)
    Radio13=ON
  ;;
  CAMHDMPEG-2)
    Radio14=ON
  ;;
  *)
    Radio1=ON
  ;;
  esac

  chinput=$(whiptail --title "$StrInputSetupTitle" --radiolist \
    "$StrInputSetupDescription" 20 78 14 \
    "CAMH264" "$StrInputSetupCAMH264" $Radio1 \
    "CAMMPEG-2" "$StrInputSetupCAMMPEG_2" $Radio2 \
    "FILETS" "$StrInputSetupFILETS" $Radio3\
    "PATERNAUDIO" "$StrInputSetupPATERNAUDIO" $Radio4 \
    "CARRIER" "$StrInputSetupCARRIER" $Radio5 \
    "TESTMODE" "$StrInputSetupTESTMODE" $Radio6 \
    "IPTSIN" "$StrInputSetupIPTSIN" $Radio7 \
    "ANALOGCAM" "$StrInputSetupANALOGCAM" $Radio8 \
    "VNC" "$StrInputSetupVNC" $Radio9 \
    "DESKTOP" "$StrInputSetupDESKTOP" $Radio10 \
    "CONTEST" "$StrInputSetupCONTEST" $Radio11  \
    "ANALOGMPEG-2" "MPEG-2 and sound from Comp Video Input" $Radio12 \
    "CARDMPEG-2" "MPEG-2 Static Test Card F with Audio" $Radio13 \
    "CAMHDMPEG-2" "MPEG-2 1280x720 HD Pi Cam with Audio" $Radio14 \
  3>&2 2>&1 1>&3)

  if [ $? -eq 0 ]; then
    case "$chinput" in

    CAMMPEG-2)
      # Make sure that the camera driver is loaded
      lsmod | grep -q 'bcm2835_v4l2'
      if [ $? != 0 ]; then   ## not loaded
        sudo modprobe bcm2835_v4l2
      fi
    ;;
    FILETS)
      TSVIDEOFILE=$(get_config_var tsvideofile $CONFIGFILE)
      filename=$TSVIDEOFILE
      FileBrowserTitle=TS:
      Filebrowser "$PATHTS/"
      whiptail --title "$StrInputSetupFILETSName" --msgbox "$filename" 8 44
      set_config_var tsvideofile "$filename" $CONFIGFILE
      PATHTS=`dirname $filename`
      set_config_var pathmedia "$PATHTS" $CONFIGFILE
    ;;
    PATERNAUDIO)
      PATERNFILE=$(get_config_var paternfile $CONFIGFILE)
      filename=$PATERNFILE
      FileBrowserTitle=JPEG:
      Pathbrowser "$PATHTS/"
      whiptail --title "$StrInputSetupPATERNAUDIOName" --msgbox "$filename" 8 44
      set_config_var paternfile "$filename" $CONFIGFILE
      set_config_var pathmedia "$filename" $CONFIGFILE
    ;;
    IPTSIN)
      CURRENTIP=$(ifconfig | grep -Eo 'inet (addr:)?([0-9]*\.){3}[0-9]*' | grep -Eo '([0-9]*\.){3}[0-9]*' | grep -v '127.0.0.1')
      whiptail --title "$StrInputSetupIPTSINTitle" --msgbox "$StrInputSetupIPTSINName""$CURRENTIP" 8 78
    ;;
    ANALOGCAM)
      ANALOGCAMNAME=$(get_config_var analogcamname $CONFIGFILE)
      Radio1=OFF
      Radio2=OFF
      Radio3=OFF

      case "$ANALOGCAMNAME" in
        "/dev/video0")
          Radio1=ON
        ;;
        "/dev/video1")
          Radio2=ON
        ;;
        auto)
          Radio3=ON
        ;;
        *)
          Radio1=ON
        ;;
      esac
      newcamname=$(whiptail --title "$StrInputSetupANALOGCAMName" --radiolist \
        "$StrInputSetupANALOGCAMTitle" 20 78 5 \
        "/dev/video0" "Normal with no PiCam" $Radio1 \
        "/dev/video1" "Sometimes required with PiCam" $Radio2 \
        "auto" "Automatically select device name" $Radio3 \
        3>&2 2>&1 1>&3)
      if [ $? -eq 0 ]; then
         set_config_var analogcamname "$newcamname" $CONFIGFILE
      fi
    ;;
    VNC)
      VNCADDR=$(get_config_var vncaddr $CONFIGFILE)
      VNCADDR=$(whiptail --inputbox "$StrInputSetupVNCName" 8 78 $VNCADDR --title "$StrInputSetupVNCTitle" 3>&1 1>&2 2>&3)
      if [ $? -eq 0 ]; then
        set_config_var vncaddr "$VNCADDR" $CONFIGFILE
      fi
    ;;
    CONTEST)
      do_refresh_numbers
      do_show_numbers
    ;;
    CAMHDMPEG-2)
      # Make sure that the camera driver is loaded
      lsmod | grep -q 'bcm2835_v4l2'
      if [ $? != 0 ]; then   ## not loaded
        sudo modprobe bcm2835_v4l2
      fi
    ;;
    esac
    set_config_var modeinput "$chinput" $CONFIGFILE
  fi
}

do_station_setup()
{
  CALL=$(get_config_var call $CONFIGFILE)
  CALL=$(whiptail --inputbox "$StrCallContext" 8 78 $CALL --title "$StrCallTitle" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var call "$CALL" $CONFIGFILE
  fi

  LOCATOR=$(get_config_var locator $CONFIGFILE)
  LOCATOR=$(whiptail --inputbox "$StrLocatorContext" 8 78 $LOCATOR --title "$StrLocatorTitle" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var locator "$LOCATOR" $CONFIGFILE
  fi

  do_refresh_numbers

  MODE_INPUT=$(get_config_var modeinput $CONFIGFILE)
  if [ "$MODE_INPUT" == "CONTEST" ]; then
    do_show_numbers
  fi
}

do_output_setup_mode()
{
  MODE_OUTPUT=$(get_config_var modeoutput $CONFIGFILE)
  Radio1=OFF
  Radio2=OFF
  Radio3=OFF
  Radio4=OFF
  Radio5=OFF
  Radio6=OFF
  Radio7=OFF
  Radio8=OFF
  Radio9=OFF
  Radio10=OFF
  case "$MODE_OUTPUT" in
  IQ)
    Radio1=ON
  ;;
  QPSKRF)
    Radio2=ON
  ;;
  BATC)
    Radio3=ON
  ;;
  STREAMER)
    Radio4=ON
  ;;
  DIGITHIN)
    Radio5=ON
  ;;
  DTX1)
    Radio6=ON
  ;;
  DATVEXPRESS)
    Radio7=ON
  ;;
  IP)
    Radio8=ON
  ;;
  COMPVID)
    Radio9=ON
  ;;
  *)
    Radio10=ON
  ;;
  esac

  choutput=$(whiptail --title "$StrOutputSetupTitle" --radiolist \
    "$StrOutputSetupContext" 20 78 10 \
    "IQ" "$StrOutputSetupIQ" $Radio1 \
    "QPSKRF" "$StrOutputSetupRF" $Radio2 \
    "BATC" "$StrOutputSetupBATC" $Radio3 \
    "STREAMER" "Stream to other Streaming Facility" $Radio4 \
    "DIGITHIN" "$StrOutputSetupDigithin" $Radio5 \
    "DTX1" "$StrOutputSetupDTX1" $Radio6 \
    "DATVEXPRESS" "$StrOutputSetupDATVExpress" $Radio7 \
    "IP" "$StrOutputSetupIP" $Radio8 \
    "COMPVID" "Output PAL Comp Video from Raspberry Pi AV Socket" $Radio9 \
    3>&2 2>&1 1>&3)

  if [ $? -eq 0 ]; then
    case "$choutput" in
    IQ)
      PIN_I=$(get_config_var gpio_i $CONFIGFILE)
      PIN_I=$(whiptail --inputbox "$StrPIN_IContext" 8 78 $PIN_I --title "$StrPIN_ITitle" 3>&1 1>&2 2>&3)
      if [ $? -eq 0 ]; then
        set_config_var gpio_i "$PIN_I" $CONFIGFILE
      fi
      PIN_Q=$(get_config_var gpio_q $CONFIGFILE)
      PIN_Q=$(whiptail --inputbox "$StrPIN_QContext" 8 78 $PIN_Q --title "$StrPIN_QTitle" 3>&1 1>&2 2>&3)
      if [ $? -eq 0 ]; then
        set_config_var gpio_q "$PIN_Q" $CONFIGFILE
      fi
    ;;
    QPSKRF)
      FREQ_OUTPUT=$(get_config_var freqoutput $CONFIGFILE)
      GAIN_OUTPUT=$(get_config_var rfpower $CONFIGFILE)
      GAIN=$(whiptail --inputbox "$StrOutputRFGainContext" 8 78 $GAIN_OUTPUT --title "$StrOutputRFGainTitle" 3>&1 1>&2 2>&3)
      if [ $? -eq 0 ]; then
        set_config_var rfpower "$GAIN" $CONFIGFILE
      fi
    ;;
    BATC)
      BATC_OUTPUT=$(get_config_var batcoutput $CONFIGFILE)
      ADRESS=$(whiptail --inputbox "$StrOutputBATCContext" 8 78 $BATC_OUTPUT --title "$StrOutputBATCTitle" 3>&1 1>&2 2>&3)
      if [ $? -eq 0 ]; then
        set_config_var batcoutput "$ADRESS" $CONFIGFILE
      fi
    ;;
    STREAMER)
      STREAM_URL=$(get_config_var streamurl $CONFIGFILE)
      STREAM=$(whiptail --inputbox "Enter the stream URL: rtmp://server.tld/folder" 8 78\
        $STREAM_URL --title "Enter other Stream Details" 3>&1 1>&2 2>&3)
      if [ $? -eq 0 ]; then
        set_config_var streamurl "$STREAM" $CONFIGFILE
      fi
      STREAM_KEY=$(get_config_var streamkey $CONFIGFILE)
      STREAMK=$(whiptail --inputbox "Enter the stream key" 8 78 $STREAM_KEY --title "Enter other Stream Details" 3>&1 1>&2 2>&3)
      if [ $? -eq 0 ]; then
        set_config_var streamkey "$STREAMK" $CONFIGFILE
      fi
    ;;
    DIGITHIN)
      PIN_I=$(get_config_var gpio_i $CONFIGFILE)
      PIN_I=$(whiptail --inputbox "$StrPIN_IContext" 8 78 $PIN_I --title "$StrPIN_ITitle" 3>&1 1>&2 2>&3)
      if [ $? -eq 0 ]; then
        set_config_var gpio_i "$PIN_I" $CONFIGFILE
      fi
      PIN_Q=$(get_config_var gpio_q $CONFIGFILE)
      PIN_Q=$(whiptail --inputbox "$StrPIN_QContext" 8 78 $PIN_Q --title "$StrPIN_QTitle" 3>&1 1>&2 2>&3)
      if [ $? -eq 0 ]; then
        set_config_var gpio_q "$PIN_Q" $CONFIGFILE
      fi
      FREQ_OUTPUT=$(get_config_var freqoutput $CONFIGFILE)
      sudo ./si570 -f $FREQ_OUTPUT -m off
    ;;
    DTX1)
      :
    ;;
    DATVEXPRESS)
      echo "Starting the DATV Express Server.  Please wait."
      if pgrep -x "express_server" > /dev/null; then
        # Express already running
        sudo killall express_server  >/dev/null 2>/dev/null
      fi
      # Make sure that the Control file is not locked
      sudo rm /tmp/expctrl >/dev/null 2>/dev/null
      # Start Express from its own folder otherwise it doesn't read the config file
      cd /home/pi/express_server
      sudo nice -n -40 /home/pi/express_server/express_server  >/dev/null 2>/dev/null &
      cd /home/pi
      sleep 5
      # Set the ports
      $PATHSCRIPT"/ctlfilter.sh"
    ;;
    IP)
      UDPOUTADDR=$(get_config_var udpoutaddr $CONFIGFILE)
      UDPOUTADDR=$(whiptail --inputbox "$StrOutputSetupIPTSOUTName" 8 78 $UDPOUTADDR --title "$StrOutputSetupIPTSOUTTitle" 3>&1 1>&2 2>&3)
      if [ $? -eq 0 ]; then
        set_config_var udpoutaddr "$UDPOUTADDR" $CONFIGFILE
      fi
    ;;
    COMPVID)
      :
    ;;
    esac
    set_config_var modeoutput "$choutput" $CONFIGFILE
  fi
}

do_symbolrate_setup()
{
  SYMBOLRATE=$(get_config_var symbolrate $CONFIGFILE)
  SYMBOLRATE=$(whiptail --inputbox "$StrOutputSymbolrateContext" 8 78 $SYMBOLRATE --title "$StrOutputSymbolrateTitle" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var symbolrate "$SYMBOLRATE" $CONFIGFILE
  fi
}

do_fec_setup()
{
	FEC=$(get_config_var fec $CONFIGFILE)
	case "$FEC" in
	1) 
	Radio1=ON
	Radio2=OFF
	Radio3=OFF
	Radio4=OFF
	Radio5=OFF
	;;
	2)
	Radio1=OFF
	Radio2=ON
	Radio3=OFF
	Radio4=OFF
	Radio5=OFF
	;;
	3)
	Radio1=OFF
	Radio2=OFF
	Radio3=ON
	Radio4=OFF
	Radio5=OFF
	;;
	5)
	Radio1=OFF
	Radio2=OFF
	Radio3=OFF
	Radio4=ON
	Radio5=OFF
	;;
	7)
	Radio1=OFF
	Radio2=OFF
	Radio3=OFF
	Radio4=OFF
	Radio5=ON
	;;
	*)
	Radio1=ON
	Radio2=OFF
	Radio3=OFF
	Radio4=OFF
	Radio5=OFF
	;;
	esac
	FEC=$(whiptail --title "$StrOutputFECTitle" --radiolist \
		"$StrOutputFECContext" 20 78 8 \
		"1" "1/2" $Radio1 \
		"2" "2/3" $Radio2 \
		"3" "3/4" $Radio3 \
		"5" "5/6" $Radio4 \
		"7" "7/8" $Radio5 3>&2 2>&1 1>&3)
if [ $? -eq 0 ]; then
	set_config_var fec "$FEC" $CONFIGFILE
fi
}

do_PID_setup()
{
  PIDPMT=$(get_config_var pidpmt $CONFIGFILE)
  PIDPMT=$(whiptail --inputbox "$StrPIDSetupContext" 8 78 $PIDPMT --title "$StrPIDSetupTitle" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var pidpmt "$PIDPMT" $CONFIGFILE
  fi
  PIDPCR=$(get_config_var pidstart $CONFIGFILE)
  PIDPCR=$(whiptail --inputbox "PCR PID - Not Implemented Yet. Will be set same as Video PID" 8 78 $PIDPCR --title "$StrPIDSetupTitle" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var pidstart "$PIDPCR" $CONFIGFILE
  fi
  PIDVIDEO=$(get_config_var pidvideo $CONFIGFILE)
  PIDVIDEO=$(whiptail --inputbox "Video PID" 8 78 $PIDVIDEO --title "$StrPIDSetupTitle" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var pidvideo "$PIDVIDEO" $CONFIGFILE
  fi
  PIDAUDIO=$(get_config_var pidaudio $CONFIGFILE)
  PIDAUDIO=$(whiptail --inputbox "Audio PID" 8 78 $PIDAUDIO --title "$StrPIDSetupTitle" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var pidaudio "$PIDAUDIO" $CONFIGFILE
  fi

  whiptail --title "PID Selection Check - but not all will be set by software yet!" \
    --msgbox "PMT: "$PIDPMT" PCR: "$PIDPCR" Video: "$PIDVIDEO" Audio: "$PIDAUDIO".  Press any key to continue" 8 78
}

do_freq_setup()
{
  FREQ_OUTPUT=$(get_config_var freqoutput $CONFIGFILE)
  FREQ=$(whiptail --inputbox "$StrOutputRFFreqContext" 8 78 $FREQ_OUTPUT --title "$StrOutputRFFreqTitle" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var freqoutput "$FREQ" $CONFIGFILE
    $PATHSCRIPT"/ctlfilter.sh" ## Refresh the band and port switching
  fi
  do_refresh_numbers
}

do_caption_setup()
{
  CAPTION=$(get_config_var caption $CONFIGFILE)
  Radio1=OFF
  Radio2=OFF
  case "$CAPTION" in
  on)
    Radio1=ON
  ;;
  off)
    Radio2=ON
  ;;
  *)
    Radio1=ON
  ;;
  esac

  CAPTION=$(whiptail --title "SET CAPTION ON OR OFF (MPEG-2 ONLY)" --radiolist \
    "Select one" 20 78 8 \
    "on" "Callsign Caption inserted on transmitted signal" $Radio1 \
    "off" "No Callsign Caption" $Radio2 \
    3>&2 2>&1 1>&3)

  if [ $? -eq 0 ]; then                     ## If the selection has changed
    set_config_var caption "$CAPTION" $CONFIGFILE
  fi
}

do_output_standard()
{
  OPSTD=$(get_config_var outputstandard $CONFIGFILE)
  Radio1=OFF
  Radio2=OFF
  case "$OPSTD" in
  576)
    Radio1=ON
  ;;
  480)
    Radio2=ON
  ;;
  *)
    Radio1=ON
  ;;
  esac

  OPSTD=$(whiptail --title "SET OUTPUT STANDARD" --radiolist \
    "Select one" 20 78 8 \
    "576" "DATV 576x720 25 fps, Comp Video PAL" $Radio1 \
    "480" "DATV 480x720 30 fps, Comp Video NTSC" $Radio2 \
    3>&2 2>&1 1>&3)

  if [ $? -eq 0 ]; then                     ## If the selection has changed
    set_config_var outputstandard "$OPSTD" $CONFIGFILE
  fi
}


do_output_setup() {
menuchoice=$(whiptail --title "$StrOutputTitle" --menu "$StrOutputContext" 16 78 7 \
  "1 SymbolRate" "$StrOutputSR"  \
  "2 FEC" "$StrOutputFEC" \
  "3 Output mode" "$StrOutputMode" \
  "4 PID" "$StrPIDSetup" \
  "5 Frequency" "$StrOutputRFFreqContext" \
  "6 Caption" "Callsign Caption in MPEG-2 on/off" \
  "7 Standard" "Output 576PAL or 480NTSC" \
	3>&2 2>&1 1>&3)
	case "$menuchoice" in
            1\ *) do_symbolrate_setup ;;
            2\ *) do_fec_setup   ;;
	    3\ *) do_output_setup_mode ;;
	    4\ *) do_PID_setup ;;
	    5\ *) do_freq_setup ;;
	    6\ *) do_caption_setup ;;
	    7\ *) do_output_standard ;;
        esac
}


do_transmit() 
{
  # Call a.sh in an additional process to start the transmitter
  $PATHSCRIPT"/a.sh" >/dev/null 2>/dev/null &

  # Start the Viewfinder display if user sets it on
  if [ "$V_FINDER" == "on" ]; then
    do_display_on
  else
    do_display_off
  fi

  # Wait here transmitting until user presses a key
  whiptail --title "$StrStatusTitle" --msgbox "$INFO" 8 78

  # Stop the transmit processes and clean up
  do_stop_transmit
  do_display_off
}

do_stop_transmit()
{
  # Stop DATV Express transmitting if required
  if [ "$MODE_OUTPUT" == "DATVEXPRESS" ]; then
    echo "set car off" >> /tmp/expctrl
    echo "set ptt rx" >> /tmp/expctrl
    sudo killall netcat >/dev/null 2>/dev/null
  fi

  # Turn the Local Oscillator off
  sudo $PATHRPI"/adf4351" off

  # Kill the key processes as nicely as possible
  sudo killall rpidatv >/dev/null 2>/dev/null
  sudo killall ffmpeg >/dev/null 2>/dev/null
  sudo killall tcanim >/dev/null 2>/dev/null
  sudo killall avc2ts >/dev/null 2>/dev/null
  sudo killall netcat >/dev/null 2>/dev/null

  # Then pause and make sure that avc2ts has really been stopped (needed at high SRs)
  sleep 0.1
  sudo killall -9 avc2ts >/dev/null 2>/dev/null

  # And make sure rpidatv has been stopped (required for brief transmit selections)
  sudo killall -9 rpidatv >/dev/null 2>/dev/null

  # Stop the audio for CompVid mode
  sudo killall arecord >/dev/null 2>/dev/null

  # Make sure that the PTT is releases (required for carrier and test modes)
  gpio mode $GPIO_PTT out
  gpio write $GPIO_PTT 0
}

do_display_on()
{
  v4l2-ctl --overlay=1 >/dev/null 2>/dev/null
}

do_display_off()
{
  v4l2-ctl --overlay=0 >/dev/null 2>/dev/null
}

do_receive_status()
{
  whiptail --title "RECEIVE" --msgbox "$INFO" 8 78
  sudo killall rpidatvgui >/dev/null 2>/dev/null
  sudo killall leandvb >/dev/null 2>/dev/null
  sudo killall hello_video.bin >/dev/null 2>/dev/null
  sudo fbi -T 1 -noverbose -a /home/pi/rpidatv/scripts/images/BATC_Black.png
}

do_receive()
{
  if pgrep -x "rtl_tcp" > /dev/null; then
    # rtl_tcp is running, so kill it, pause and really kill it
    killall rtl_tcp >/dev/null 2>/dev/null
    sleep 0.5
    sudo killall -9 rtl_tcp >/dev/null 2>/dev/null
  fi

  if ! pgrep -x "fbcp" > /dev/null; then
    # fbcp is not running, so start it
    fbcp &
  fi

  MODE_OUTPUT=$(get_config_var modeoutput $CONFIGFILE)
  case "$MODE_OUTPUT" in
  BATC)
    ORGINAL_MODE_INPUT=$(get_config_var modeinput $CONFIGFILE)
    sleep 0.1
    set_config_var modeinput "DESKTOP" $CONFIGFILE
    sleep 0.1
    /home/pi/rpidatv/bin/rpidatvgui 0 1  >/dev/null 2>/dev/null & 
    $PATHSCRIPT"/a.sh" >/dev/null 2>/dev/null &
    do_receive_status
    set_config_var modeinput "$ORGINAL_MODE_INPUT" $CONFIGFILE
  ;;
  *)
    /home/pi/rpidatv/bin/rpidatvgui 0 1  >/dev/null 2>/dev/null & 
    do_receive_status
  ;;
  esac
}

do_start_rtl_tcp()
{
  # Look up current wired IP.  If no wired, use wireless
  CURRENTIP=$(ifconfig | grep -Eo 'inet (addr:)?([0-9]*\.){3}[0-9]*' | grep -Eo '([0-9]*\.){3}[0-9]*' | grep -v '127.0.0.1' | head -1)
  rtl_tcp -a $CURRENTIP &>/dev/null &
}

do_stop_rtl_tcp()
{
  PROCESS=$(ps | grep rtl_tcp | grep -Eo '([0-9]+){4} ')
  kill -9 "$PROCESS"  >/dev/null
}

do_receive_menu()
{
  menuchoice=$(whiptail --title "Select Receive Option" --menu "RTL Menu" 20 78 13 \
    "1 Receive DATV" "Use the RTL to Receive with same settings as transmit"  \
    "2 Start RTL-TCP" "Start the RTL-TCP Server for use with SDR Sharp"  \
    "3 Stop RTL-TCP" "Stop the RTL-TCP Server" \
    3>&2 2>&1 1>&3)
  case "$menuchoice" in
    1\ *) do_receive ;;
    2\ *) do_start_rtl_tcp ;;
    3\ *) do_stop_rtl_tcp  ;;
  esac
}

do_autostart_setup()
{
  MODE_STARTUP=$(get_config_var startup $CONFIGFILE)
  Radio1=OFF
  Radio2=OFF
  Radio3=OFF
  Radio4=OFF
  Radio5=OFF
  Radio6=OFF
  Radio7=OFF
  Radio8=OFF
  Radio9=OFF
  Radio10=OFF

  case "$MODE_STARTUP" in
    Prompt)
      Radio1=ON
    ;;
    Console)
      Radio2=ON
    ;;
    TX_boot)
      Radio3=ON
    ;;
    Display_boot)
      Radio4=ON
    ;;
    TestRig_boot)
      Radio5=ON
    ;;
    Button_boot)
      Radio6=ON
    ;;
    Keyed_Stream_boot)
      Radio7=ON
    ;;
    Cont_Stream_boot)
      Radio8=ON
    ;;
    Keyed_TX_boot)
      Radio9=ON
    ;;
    SigGen_boot)
      Radio10=ON
    ;;
    *)
      Radio1=ON
    ;;
  esac

  chstartup=$(whiptail --title "$StrAutostartSetupTitle" --radiolist \
   "$StrAutostartSetupContext" 20 78 11 \
   "Prompt" "$AutostartSetupPrompt" $Radio1 \
   "Console" "$AutostartSetupConsole" $Radio2 \
   "TX_boot" "$AutostartSetupTX_boot" $Radio3 \
   "Display_boot" "$AutostartSetupDisplay_boot" $Radio4 \
   "TestRig_boot" "Boot-up to Test Rig for F-M Boards" $Radio5 \
   "Button_boot" "$AutostartSetupButton_boot" $Radio6 \
   "Keyed_Stream_boot" "Boot up to Keyed Repeater Streamer" $Radio7 \
   "Cont_Stream_boot" "Boot up to Always-on Repeater Streamer" $Radio8 \
   "Keyed_TX_boot" "Boot up to GPIO Keyed Transmitter" $Radio9 \
   "SigGen_boot" "Boot up with the Sig Gen Output On" $Radio10 \
   3>&2 2>&1 1>&3)

  if [ $? -eq 0 ]; then
     set_config_var startup "$chstartup" $CONFIGFILE
  fi
}

do_display_setup()
{
  MODE_DISPLAY=$(get_config_var display $CONFIGFILE)
  Radio1=OFF
  Radio2=OFF
  Radio3=OFF
  Radio4=OFF
  Radio5=OFF
  Radio6=OFF
  Radio7=OFF
  case "$MODE_DISPLAY" in
  Tontec35)
    Radio1=ON
  ;;
  HDMITouch)
    Radio2=ON
  ;;
  Waveshare)
    Radio3=ON
  ;;
  WaveshareB)
    Radio4=ON
  ;;
  Waveshare4)
    Radio5=ON
  ;;
  Console)
    Radio6=ON
  ;;
  Element14_7)
    Radio7=ON
  ;;
  *)
    Radio1=ON
  ;;		
  esac

  chdisplay=$(whiptail --title "$StrDisplaySetupTitle" --radiolist \
    "$StrDisplaySetupContext" 20 78 9 \
    "Tontec35" "$DisplaySetupTontec" $Radio1 \
    "HDMITouch" "$DisplaySetupHDMI" $Radio2 \
    "Waveshare" "$DisplaySetupRpiLCD" $Radio3 \
    "WaveshareB" "$DisplaySetupRpiBLCD" $Radio4 \
    "Waveshare4" "$DisplaySetupRpi4LCD" $Radio5 \
    "Console" "$DisplaySetupConsole" $Radio6 \
    "Element14_7" "Element 14 RPi 7 inch Display" $Radio7 \
 	 3>&2 2>&1 1>&3)

  if [ $? -eq 0 ]; then                     ## If the selection has changed

    ## This section modifies and replaces the end of /boot/config.txt
    ## to allow (only) the correct LCD drivers to be loaded at next boot

    ## Set constants for the amendment of /boot/config.txt
    PATHCONFIGS="/home/pi/rpidatv/scripts/configs"  ## Path to config files
    lead='^## Begin LCD Driver'               ## Marker for start of inserted text
    tail='^## End LCD Driver'                 ## Marker for end of inserted text
    CHANGEFILE="/boot/config.txt"             ## File requiring added text
    APPENDFILE=$PATHCONFIGS"/lcd_markers.txt" ## File containing both markers
    TRANSFILE=$PATHCONFIGS"/transfer.txt"     ## File used for transfer

    grep -q "$lead" "$CHANGEFILE"     ## Is the first marker already present?
    if [ $? -ne 0 ]; then
      sudo bash -c 'cat '$APPENDFILE' >> '$CHANGEFILE' '  ## If not append the markers
    fi

    case "$chdisplay" in              ## Select the correct driver text
      Tontec35)  INSERTFILE=$PATHCONFIGS"/tontec35.txt" ;;
      HDMITouch) INSERTFILE=$PATHCONFIGS"/hdmitouch.txt" ;;
      Waveshare) INSERTFILE=$PATHCONFIGS"/waveshare.txt" ;;
      WaveshareB) INSERTFILE=$PATHCONFIGS"/waveshareb.txt" ;;
      Waveshare4) INSERTFILE=$PATHCONFIGS"/waveshare.txt" ;;
      Console)   INSERTFILE=$PATHCONFIGS"/console.txt" ;;
      Element14_7)  INSERTFILE=$PATHCONFIGS"/tontec35.txt" ;;
    esac

    ## Replace whatever is between the markers with the driver text
    sed -e "/$lead/,/$tail/{ /$lead/{p; r $INSERTFILE
	        }; /$tail/p; d }" $CHANGEFILE >> $TRANSFILE

    sudo cp "$TRANSFILE" "$CHANGEFILE"          ## Copy from the transfer file
    rm $TRANSFILE                               ## Delete the transfer file

    ## Set the correct touchscreen map for FreqShow
    sudo rm /etc/pointercal                     ## Delete the old file
    case "$chdisplay" in                        ## Insert the new file
      Tontec35)  sudo cp /home/pi/rpidatv/scripts/configs/freqshow/waveshare_pointercal /etc/pointercal ;;
      HDMITouch) sudo cp /home/pi/rpidatv/scripts/configs/freqshow/waveshare_pointercal /etc/pointercal ;;
      Waveshare) sudo cp /home/pi/rpidatv/scripts/configs/freqshow/waveshare_pointercal /etc/pointercal ;;
      WaveshareB) sudo cp /home/pi/rpidatv/scripts/configs/freqshow/waveshare_pointercal /etc/pointercal ;;
      Waveshare4) sudo cp /home/pi/rpidatv/scripts/configs/freqshow/waveshare4_pointercal /etc/pointercal ;;
      Console)   sudo cp /home/pi/rpidatv/scripts/configs/freqshow/waveshare_pointercal /etc/pointercal ;;
      Element14_7)  sudo cp /home/pi/rpidatv/scripts/configs/freqshow/waveshare_pointercal /etc/pointercal ;;
    esac

    set_config_var display "$chdisplay" $CONFIGFILE
  fi
}

do_IP_setup()
{
  CURRENTIP=$(ifconfig | grep -Eo 'inet (addr:)?([0-9]*\.){3}[0-9]*' | grep -Eo '([0-9]*\.){3}[0-9]*' | grep -v '127.0.0.1')
  whiptail --title "IP" --msgbox "$CURRENTIP" 8 78
}

do_WiFi_setup()
{
  $PATHSCRIPT"/wifisetup.sh"
}

do_WiFi_Off()
{
  sudo ifconfig wlan0 down                           ## Disable it now
  cp $PATHCONFIGS"/text.wifi_off" /home/pi/.wifi_off ## Disable at start-up
}

do_Enable_DigiThin()
{
whiptail --title "Not implemented yet" --msgbox "Not Implemented yet.  Please press enter to continue" 8 78
}

do_EasyCap()
{
    ## Check and set the input
    ACINPUT=$(get_config_var analogcaminput $CONFIGFILE)
    ACINPUT=$(whiptail --inputbox "Enter 0 for Composite, 1 for S-Video, - for not set" 8 78 $ACINPUT --title "SET EASYCAP INPUT NUMBER" 3>&1 1>&2 2>&3)
    if [ $? -eq 0 ]; then
        if [ "$ACINPUT" == "-" ]; then
            set_config_var analogcaminput "$ACINPUT" $CONFIGFILE
        else
            if [[ $ACINPUT =~ ^[0-9]+$ ]]; then
                set_config_var analogcaminput "$ACINPUT" $CONFIGFILE
            else
                whiptail --title "Error" --msgbox "Please enter only numbers or a -.  Please press enter to continue and reselect" 8 78
            fi
        fi
    fi

    ## Check and set the standard
    ACSTANDARD=$(get_config_var analogcamstandard $CONFIGFILE)
    ACSTANDARD=$(whiptail --inputbox "Enter 0 for NTSC, 6 for PAL, - for not set" 8 78 $ACSTANDARD --title "SET EASYCAP VIDEO STANDARD" 3>&1 1>&2 2>&3)
    if [ $? -eq 0 ]; then
        if [ "$ACSTANDARD" == "-" ]; then
            set_config_var analogcamstandard "$ACSTANDARD" $CONFIGFILE
        else
            if [[ $ACSTANDARD =~ ^[0-9]+$ ]]; then
                set_config_var analogcamstandard "$ACSTANDARD" $CONFIGFILE
            else
                whiptail --title "Error" --msgbox "Please enter only numbers or a -.  Please press enter to continue and reselect" 8 78
            fi
        fi

    fi
}


do_audio_switch()
{
  AUDIO=$(get_config_var audio $CONFIGFILE)
  Radio1=OFF
  Radio2=OFF
  Radio3=OFF
  Radio4=OFF
  Radio5=OFF
  case "$AUDIO" in
  auto)
    Radio1=ON
  ;;
  mic)
    Radio2=ON
  ;;
  video)
    Radio3=ON
  ;;
  bleeps)
    Radio4=ON
  ;;
  no_audio)
    Radio5=ON
  ;;
  *)
    Radio1=ON
  ;;
  esac

  AUDIO=$(whiptail --title "SELECT AUDIO SOURCE" --radiolist \
    "Select one" 20 78 8 \
    "auto" "Auto-select from Mic or EasyCap Dongle" $Radio1 \
    "mic" "Use the USB Audio Dongle Mic Input" $Radio2 \
    "video" "Use the EasyCap Video Dongle Audio Input" $Radio3 \
    "bleeps" "Generate test bleeps" $Radio4 \
    "no_audio" "Do not include audio" $Radio5 \
    3>&2 2>&1 1>&3)

  if [ $? -eq 0 ]; then                     ## If the selection has changed
    set_config_var audio "$AUDIO" $CONFIGFILE
  fi
}

do_attenuator()
{
  ATTEN=$(get_config_var attenuator $CONFIGFILE)
  Radio1=OFF
  Radio2=OFF
  Radio3=OFF
  Radio4=OFF
  case "$ATTEN" in
  NONE)
    Radio1=ON
  ;;
  PE4312)
    Radio2=ON
  ;;
  PE43713)
    Radio3=ON
  ;;
  HMC1119)
    Radio4=ON
  ;;
  *)
    Radio1=ON
  ;;
  esac

  ATTEN=$(whiptail --title "SELECT OUTPUT ATTENUATOR TYPE" --radiolist \
    "Select one" 20 78 4 \
    "NONE" "No Output Attenuator in Circuit" $Radio1 \
    "PE4312" "PE4302 or PE4312 Attenuator in Use" $Radio2 \
    "PE43713" "PE43703 or PE43713 Attenuator in Use" $Radio3 \
    "HMC1119" "HMC1119 Attenuator in Use" $Radio4 \
    3>&2 2>&1 1>&3)

  if [ $? -eq 0 ]; then                     ## If the selection has changed
    set_config_var attenuator "$ATTEN" $CONFIGFILE
  fi
}

do_Update()
{
reset
$PATHSCRIPT"/check_for_update.sh"
}

do_presets()
{
  PFREQ1=$(get_config_var pfreq1 $CONFIGFILE)
  PFREQ1=$(whiptail --inputbox "Enter Preset Frequency 1 in MHz" 8 78 $PFREQ1 --title "SET TOUCHSCREEN PRESETS" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var pfreq1 "$PFREQ1" $CONFIGFILE
  fi

  PFREQ2=$(get_config_var pfreq2 $CONFIGFILE)
  PFREQ2=$(whiptail --inputbox "Enter Preset Frequency 2 in MHz" 8 78 $PFREQ2 --title "SET TOUCHSCREEN PRESETS" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var pfreq2 "$PFREQ2" $CONFIGFILE
  fi

  PFREQ3=$(get_config_var pfreq3 $CONFIGFILE)
  PFREQ3=$(whiptail --inputbox "Enter Preset Frequency 3 in MHz" 8 78 $PFREQ3 --title "SET TOUCHSCREEN PRESETS" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var pfreq3 "$PFREQ3" $CONFIGFILE
  fi

  PFREQ4=$(get_config_var pfreq4 $CONFIGFILE)
  PFREQ4=$(whiptail --inputbox "Enter Preset Frequency 4 in MHz" 8 78 $PFREQ4 --title "SET TOUCHSCREEN PRESETS" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var pfreq4 "$PFREQ4" $CONFIGFILE
  fi

  PFREQ5=$(get_config_var pfreq5 $CONFIGFILE)
  PFREQ5=$(whiptail --inputbox "Enter Preset Frequency 5 in MHz" 8 78 $PFREQ5 --title "SET TOUCHSCREEN PRESETS" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var pfreq5 "$PFREQ5" $CONFIGFILE
  fi

}

do_preset_SRs()
{

  PSR1=$(get_config_var psr1 $CONFIGFILE)
  PSR1=$(whiptail --inputbox "Enter Preset Symbol Rate 1 in KS/s" 8 78 $PSR1 --title "SET TOUCHSCREEN PRESETS" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var psr1 "$PSR1" $CONFIGFILE
  fi

  PSR2=$(get_config_var psr2 $CONFIGFILE)
  PSR2=$(whiptail --inputbox "Enter Preset Symbol Rate 2 in KS/s" 8 78 $PSR2 --title "SET TOUCHSCREEN PRESETS" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var psr2 "$PSR2" $CONFIGFILE
  fi

  PSR3=$(get_config_var psr3 $CONFIGFILE)
  PSR3=$(whiptail --inputbox "Enter Preset Symbol Rate 3 in KS/s" 8 78 $PSR3 --title "SET TOUCHSCREEN PRESETS" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var psr3 "$PSR3" $CONFIGFILE
  fi

  PSR4=$(get_config_var psr4 $CONFIGFILE)
  PSR4=$(whiptail --inputbox "Enter Preset Symbol Rate 4 in KS/s" 8 78 $PSR4 --title "SET TOUCHSCREEN PRESETS" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var psr4 "$PSR4" $CONFIGFILE
  fi

  PSR5=$(get_config_var psr5 $CONFIGFILE)
  PSR5=$(whiptail --inputbox "Enter Preset Symbol Rate 5 in KS/s" 8 78 $PSR5 --title "SET TOUCHSCREEN PRESETS" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var psr5 "$PSR5" $CONFIGFILE
  fi
}

do_4351_ref()
{
  ADFREF=$(get_config_var adfref $CONFIGFILE)
  ADFREF=$(whiptail --inputbox "Enter oscillator frequency in Hz" 8 78 $ADFREF --title "SET ADF4351 REFERENCE OSCILLATOR" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var adfref "$ADFREF" $CONFIGFILE
  fi
}

do_4351_levels()
{
  ADFLEVEL0=$(get_config_var adflevel0 $CONFIGFILE)
  ADFLEVEL0=$(whiptail --inputbox "Enter 0 to 3 = plus 0, 3, 6 or 9 dB" 8 78 $ADFLEVEL0 --title "SET ADF4351 LEVEL FOR THE 71 MHz BAND" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var adflevel0 "$ADFLEVEL0" $CONFIGFILE
  fi

  ADFLEVEL1=$(get_config_var adflevel1 $CONFIGFILE)
  ADFLEVEL1=$(whiptail --inputbox "Enter 0 to 3 = plus 0, 3, 6 or 9 dB" 8 78 $ADFLEVEL1 --title "SET ADF4351 LEVEL FOR THE 146 MHz BAND" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var adflevel1 "$ADFLEVEL1" $CONFIGFILE
  fi

  ADFLEVEL2=$(get_config_var adflevel2 $CONFIGFILE)
  ADFLEVEL2=$(whiptail --inputbox "Enter 0 to 3 = plus 0, 3, 6 or 9 dB" 8 78 $ADFLEVEL2 --title "SET ADF4351 LEVEL FOR THE 437MHz BAND" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var adflevel2 "$ADFLEVEL2" $CONFIGFILE
  fi

  ADFLEVEL3=$(get_config_var adflevel3 $CONFIGFILE)
  ADFLEVEL3=$(whiptail --inputbox "Enter 0 to 3 = plus 0, 3, 6 or 9 dB" 8 78 $ADFLEVEL3 --title "SET ADF4351 LEVEL FOR THE 1255 MHz BAND" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var adflevel3 "$ADFLEVEL3" $CONFIGFILE
  fi
}

do_set_express()
{
  EXPLEVEL0=$(get_config_var explevel0 $CONFIGFILE)
  EXPLEVEL0=$(whiptail --inputbox "Enter 0 to 47" 8 78 $EXPLEVEL0 --title "SET DATV EXPRESS OUTPUT LEVEL FOR THE 71 MHz BAND" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var explevel0 "$EXPLEVEL0" $CONFIGFILE
  fi

  Check1=OFF
  Check2=OFF
  Check3=OFF
  Check4=OFF
  EXPPORTS0=$(get_config_var expports0 $CONFIGFILE)
  TESTPORT=$EXPPORTS0
  if [ "$TESTPORT" -gt 7 ]; then
    Check4=ON
    TESTPORT=$[$TESTPORT-8]
  fi
  if [ "$TESTPORT" -gt 3 ]; then
    Check3=ON
    TESTPORT=$[$TESTPORT-4]
  fi
  if [ "$TESTPORT" -gt 1 ]; then
    Check2=ON
    TESTPORT=$[$TESTPORT-2]
  fi
  if [ "$TESTPORT" -gt 0 ]; then
    Check1=ON
  fi
  TESTPORT=$(whiptail --title "SET DATV EXPRESS PORTS FOR THE 71 MHz BAND" --checklist \
    "Select or deselect the active ports using the spacebar" 20 78 4 \
    "Port A" "J6 Pin 5" $Check1 \
    "Port B" "J6 Pin 6" $Check2 \
    "Port C" "J6 Pin 7" $Check3 \
    "Port D" "J6 Pin 10" $Check4 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    EXPPORTS0=0
    if (echo $TESTPORT | grep -q A); then
      EXPPORTS0=1
    fi
    if (echo $TESTPORT | grep -q B); then
      EXPPORTS0=$[$EXPPORTS0+2]
    fi
    if (echo $TESTPORT | grep -q C); then
      EXPPORTS0=$[$EXPPORTS0+4]
    fi
    if (echo $TESTPORT | grep -q D); then
      EXPPORTS0=$[$EXPPORTS0+8]
    fi
    set_config_var expports0 "$EXPPORTS0" $CONFIGFILE
  fi

  EXPLEVEL1=$(get_config_var explevel1 $CONFIGFILE)
  EXPLEVEL1=$(whiptail --inputbox "Enter 0 to 47" 8 78 $EXPLEVEL1 --title "SET DATV EXPRESS OUTPUT LEVEL FOR THE 146 MHz BAND" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var explevel1 "$EXPLEVEL1" $CONFIGFILE
  fi

  Check1=OFF
  Check2=OFF
  Check3=OFF
  Check4=OFF
  EXPPORTS1=$(get_config_var expports1 $CONFIGFILE)
  TESTPORT=$EXPPORTS1
  if [ "$TESTPORT" -gt 7 ]; then
    Check4=ON
    TESTPORT=$[$TESTPORT-8]
  fi
  if [ "$TESTPORT" -gt 3 ]; then
    Check3=ON
    TESTPORT=$[$TESTPORT-4]
  fi
  if [ "$TESTPORT" -gt 1 ]; then
    Check2=ON
    TESTPORT=$[$TESTPORT-2]
  fi
  if [ "$TESTPORT" -gt 0 ]; then
    Check1=ON
  fi
  TESTPORT=$(whiptail --title "SET DATV EXPRESS PORTS FOR THE 146 MHz BAND" --checklist \
    "Select or deselect the active ports using the spacebar" 20 78 4 \
    "Port A" "J6 Pin 5" $Check1 \
    "Port B" "J6 Pin 6" $Check2 \
    "Port C" "J6 Pin 7" $Check3 \
    "Port D" "J6 Pin 10" $Check4 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    EXPPORTS1=0
    if (echo $TESTPORT | grep -q A); then
      EXPPORTS1=1
    fi
    if (echo $TESTPORT | grep -q B); then
      EXPPORTS1=$[$EXPPORTS1+2]
    fi
    if (echo $TESTPORT | grep -q C); then
      EXPPORTS1=$[$EXPPORTS1+4]
    fi
    if (echo $TESTPORT | grep -q D); then
      EXPPORTS1=$[$EXPPORTS1+8]
    fi
    set_config_var expports1 "$EXPPORTS1" $CONFIGFILE
  fi

  EXPLEVEL2=$(get_config_var explevel2 $CONFIGFILE)
  EXPLEVEL2=$(whiptail --inputbox "Enter 0 to 47" 8 78 $EXPLEVEL2 --title "SET DATV EXPRESS OUTPUT LEVEL FOR THE 437 MHz BAND" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var explevel2 "$EXPLEVEL2" $CONFIGFILE
  fi

  Check1=OFF
  Check2=OFF
  Check3=OFF
  Check4=OFF
  EXPPORTS2=$(get_config_var expports2 $CONFIGFILE)
  TESTPORT=$EXPPORTS2
  if [ "$TESTPORT" -gt 7 ]; then
    Check4=ON
    TESTPORT=$[$TESTPORT-8]
  fi
  if [ "$TESTPORT" -gt 3 ]; then
    Check3=ON
    TESTPORT=$[$TESTPORT-4]
  fi
  if [ "$TESTPORT" -gt 1 ]; then
    Check2=ON
    TESTPORT=$[$TESTPORT-2]
  fi
  if [ "$TESTPORT" -gt 0 ]; then
    Check1=ON
  fi
  TESTPORT=$(whiptail --title "SET DATV EXPRESS PORTS FOR THE 437 MHz BAND" --checklist \
    "Select or deselect the active ports using the spacebar" 20 78 4 \
    "Port A" "J6 Pin 5" $Check1 \
    "Port B" "J6 Pin 6" $Check2 \
    "Port C" "J6 Pin 7" $Check3 \
    "Port D" "J6 Pin 10" $Check4 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    EXPPORTS2=0
    if (echo $TESTPORT | grep -q A); then
      EXPPORTS2=1
    fi
    if (echo $TESTPORT | grep -q B); then
      EXPPORTS2=$[$EXPPORTS2+2]
    fi
    if (echo $TESTPORT | grep -q C); then
      EXPPORTS2=$[$EXPPORTS2+4]
    fi
    if (echo $TESTPORT | grep -q D); then
      EXPPORTS2=$[$EXPPORTS2+8]
    fi
    set_config_var expports2 "$EXPPORTS2" $CONFIGFILE
  fi

  EXPLEVEL3=$(get_config_var explevel3 $CONFIGFILE)
  EXPLEVEL3=$(whiptail --inputbox "Enter 0 to 47" 8 78 $EXPLEVEL3 --title "SET DATV EXPRESS OUTPUT LEVEL FOR THE 1255 MHz BAND" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var explevel3 "$EXPLEVEL3" $CONFIGFILE
  fi

  Check1=OFF
  Check2=OFF
  Check3=OFF
  Check4=OFF
  EXPPORTS3=$(get_config_var expports3 $CONFIGFILE)
  TESTPORT=$EXPPORTS3
  if [ "$TESTPORT" -gt 7 ]; then
    Check4=ON
    TESTPORT=$[$TESTPORT-8]
  fi
  if [ "$TESTPORT" -gt 3 ]; then
    Check3=ON
    TESTPORT=$[$TESTPORT-4]
  fi
  if [ "$TESTPORT" -gt 1 ]; then
    Check2=ON
    TESTPORT=$[$TESTPORT-2]
  fi
  if [ "$TESTPORT" -gt 0 ]; then
    Check1=ON
  fi
  TESTPORT=$(whiptail --title "SET DATV EXPRESS PORTS FOR THE 1255 MHz BAND" --checklist \
    "Select or deselect the active ports using the spacebar" 20 78 4 \
    "Port A" "J6 Pin 5" $Check1 \
    "Port B" "J6 Pin 6" $Check2 \
    "Port C" "J6 Pin 7" $Check3 \
    "Port D" "J6 Pin 10" $Check4 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    EXPPORTS3=0
    if (echo $TESTPORT | grep -q A); then
      EXPPORTS3=1
    fi
    if (echo $TESTPORT | grep -q B); then
      EXPPORTS3=$[$EXPPORTS3+2]
    fi
    if (echo $TESTPORT | grep -q C); then
      EXPPORTS3=$[$EXPPORTS3+4]
    fi
    if (echo $TESTPORT | grep -q D); then
      EXPPORTS3=$[$EXPPORTS3+8]
    fi
    set_config_var expports3 "$EXPPORTS3" $CONFIGFILE
  fi

  EXPLEVEL4=$(get_config_var explevel4 $CONFIGFILE)
  EXPLEVEL4=$(whiptail --inputbox "Enter 0 to 47" 8 78 $EXPLEVEL4 --title "SET DATV EXPRESS OUTPUT LEVEL FOR THE 2400 MHz BAND" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var explevel4 "$EXPLEVEL4" $CONFIGFILE
  fi

  Check1=OFF
  Check2=OFF
  Check3=OFF
  Check4=OFF
  EXPPORTS4=$(get_config_var expports4 $CONFIGFILE)
  TESTPORT=$EXPPORTS4
  if [ "$TESTPORT" -gt 7 ]; then
    Check4=ON
    TESTPORT=$[$TESTPORT-8]
  fi
  if [ "$TESTPORT" -gt 3 ]; then
    Check3=ON
    TESTPORT=$[$TESTPORT-4]
  fi
  if [ "$TESTPORT" -gt 1 ]; then
    Check2=ON
    TESTPORT=$[$TESTPORT-2]
  fi
  if [ "$TESTPORT" -gt 0 ]; then
    Check1=ON
  fi
  TESTPORT=$(whiptail --title "SET DATV EXPRESS PORTS FOR THE 2400 MHz BAND" --checklist \
    "Select or deselect the active ports using the spacebar" 20 78 4 \
    "Port A" "J6 Pin 5" $Check1 \
    "Port B" "J6 Pin 6" $Check2 \
    "Port C" "J6 Pin 7" $Check3 \
    "Port D" "J6 Pin 10" $Check4 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    EXPPORTS4=0
    if (echo $TESTPORT | grep -q A); then
      EXPPORTS4=1
    fi
    if (echo $TESTPORT | grep -q B); then
      EXPPORTS4=$[$EXPPORTS4+2]
    fi
    if (echo $TESTPORT | grep -q C); then
      EXPPORTS4=$[$EXPPORTS4+4]
    fi
    if (echo $TESTPORT | grep -q D); then
      EXPPORTS4=$[$EXPPORTS4+8]
    fi
    set_config_var expports4 "$EXPPORTS4" $CONFIGFILE
  fi
}

do_numbers()
{
  NUMBERS0=$(get_config_var numbers0 $CONFIGFILE)
  NUMBERS0=$(whiptail --inputbox "Enter 4 digits" 8 78 $NUMBERS0 --title "SET CONTEST NUMBERS FOR THE 71 MHz BAND" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var numbers0 "$NUMBERS0" $CONFIGFILE
  fi

  NUMBERS1=$(get_config_var numbers1 $CONFIGFILE)
  NUMBERS1=$(whiptail --inputbox "Enter 4 digits" 8 78 $NUMBERS1 --title "SET CONTEST NUMBERS FOR THE 146 MHz BAND" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var numbers1 "$NUMBERS1" $CONFIGFILE
  fi

  NUMBERS2=$(get_config_var numbers2 $CONFIGFILE)
  NUMBERS2=$(whiptail --inputbox "Enter 4 digits" 8 78 $NUMBERS2 --title "SET CONTEST NUMBERS FOR THE 437 MHz BAND" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var numbers2 "$NUMBERS2" $CONFIGFILE
  fi

  NUMBERS3=$(get_config_var numbers3 $CONFIGFILE)
  NUMBERS3=$(whiptail --inputbox "Enter 4 digits" 8 78 $NUMBERS3 --title "SET CONTEST NUMBERS FOR THE 1255 MHz BAND" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    set_config_var numbers3 "$NUMBERS3" $CONFIGFILE
  fi

  do_refresh_numbers
  do_show_numbers
}

do_vfinder()
{
  V_FINDER=$(get_config_var vfinder $CONFIGFILE)
  case "$V_FINDER" in
  on)
    Radio1=ON
    Radio2=OFF
  ;;
  off)
    Radio1=OFF
    Radio2=ON
  esac

  V_FINDER=$(whiptail --title "SET VIEWFINDER ON OR OFF" --radiolist \
    "Select one" 20 78 8 \
    "on" "Transmitted image displayed on Touchscreen" $Radio1 \
    "off" "Buttons displayed on touchscreen during transmit" $Radio2 \
    3>&2 2>&1 1>&3)

  if [ $? -eq 0 ]; then                     ## If the selection has changed
    set_config_var vfinder "$V_FINDER" $CONFIGFILE
  fi
}

do_SD_info()
{
  $PATHSCRIPT"/sd_card_info.sh"
}

do_factory()
{
  FACTORY=""
  FACTORY=$(whiptail --inputbox "Enter y or n" 8 78 $FACTORY --title "RESET TO INITIAL SETTINGS?" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    if [[ "$FACTORY" == "y" || "$FACTORY" == "Y" ]]; then
      mv $PATHSCRIPT"/rpidatvconfig.txt" $PATHSCRIPT"/rpidatvconfig.txt.bak"
      cp $PATHSCRIPT"/configs/rpidatvconfig.txt.factory" $PATHSCRIPT"/rpidatvconfig.txt"
      whiptail --title "Message" --msgbox "Factory Configuration Restored.  Please press enter to continue" 8 78
    else
      whiptail --title "Message" --msgbox "Current Configuration Retained.  Please press enter to continue" 8 78
    fi
  fi
}

do_touch_factory()
{
  TOUCH_FACTORY=""
  TOUCH_FACTORY=$(whiptail --inputbox "Enter y or n" 8 78 $TOUCH_FACTORY --title "RESET TOUCHSCREEN CALIBRATION?" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    if [[ "$TOUCH_FACTORY" == "y" || "$TOUCH_FACTORY" == "Y" ]]; then
      mv $PATHSCRIPT"/touchcal.txt" $PATHSCRIPT"/touchcal.txt.bak"
      cp $PATHSCRIPT"/configs/touchcal.txt.factory" $PATHSCRIPT"/touchcal.txt"
      whiptail --title "Message" --msgbox "Touchscreen calibration reset to zero.  Please press enter to continue" 8 78
    else
      whiptail --title "Message" --msgbox "Current Configuration Retained.  Please press enter to continue" 8 78
    fi
  fi
}

do_back_up()
{
  BACKUP=""
  BACKUP=$(whiptail --inputbox "Enter y or n" 8 78 $BACKUP --title "SAVE TO USB? EXISTING FILE WILL BE OVER-WRITTEN" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    if [[ "$BACKUP" == "y" || "$BACKUP" == "Y" ]]; then
      ls -l /dev/disk/by-uuid|grep -q sda  # returns 0 if USB drive connected
      if [ $? -eq 0 ]; then
        sudo mv -f /media/usb0/rpidatvconfig.txt /media/usb0/rpidatvconfig.txt.bak >/dev/null 2>/dev/null
        sudo cp $PATHSCRIPT"/rpidatvconfig.txt" /media/usb0/rpidatvconfig.txt >/dev/null 2>/dev/null
        whiptail --title "Message" --msgbox "Configuration file copied to USB.  Please press enter to continue" 8 78
      else
        whiptail --title "Message" --msgbox "No USB Drive found.  Please press enter to continue" 8 78
      fi
    else
      whiptail --title "Message" --msgbox "Configuration file not copied.  Please press enter to continue" 8 78
    fi
  fi
}

do_load_settings()
{
  BACKUP=""
  BACKUP=$(whiptail --inputbox "Enter y or n" 8 78 $BACKUP --title "LOAD CONFIG FROM USB? EXISTING FILE WILL BE OVER-WRITTEN" 3>&1 1>&2 2>&3)
  if [ $? -eq 0 ]; then
    if [[ "$BACKUP" == "y" || "$BACKUP" == "Y" ]]; then
      ls -l /dev/disk/by-uuid|grep -q sda  # returns 0 if USB drive connected
      if [ $? -eq 0 ]; then
        if [ -f /media/usb0/rpidatvconfig.txt ]; then
          mv -f $PATHSCRIPT"/rpidatvconfig.txt" $PATHSCRIPT"/rpidatvconfig.txt.bak" >/dev/null 2>/dev/null
          cp /media/usb0/rpidatvconfig.txt $PATHSCRIPT"/rpidatvconfig.txt" >/dev/null 2>/dev/null
          whiptail --title "Message" --msgbox "Configuration file copied from USB.  Please press enter to continue" 8 78
        else
          whiptail --title "Message" --msgbox "File rpidatvconfig.txt not found.  Please press enter to continue" 8 78
        fi
      else
        whiptail --title "Message" --msgbox "No USB Drive found.  Please press enter to continue" 8 78
      fi
    else
      whiptail --title "Message" --msgbox "Configuration file not copied.  Please press enter to continue" 8 78
    fi
  fi
}

do_beta()
{
  BETA=$(get_config_var beta $CONFIGFILE)
  case "$BETA" in
  no)
    Radio1=ON
    Radio2=OFF
  ;;
  yes)
    Radio1=OFF
    Radio2=ON
  esac

  BETA=$(whiptail --title "USE BETA (EXPERIMENTAL) ENCODING?" --radiolist \
    "Select one" 20 78 8 \
    "no" "Stable software with core features" $Radio1 \
    "yes" "Experimental software with new features" $Radio2 \
    3>&2 2>&1 1>&3)

  if [ $? -eq 0 ]; then                     ## If the selection has changed
    set_config_var beta "$BETA" $CONFIGFILE
    # and switch the files
    case "$BETA" in
      no)
        SAVECHANGES="n"
        SAVECHANGES=$(whiptail --inputbox "Save Changes to beta a.sh? y/n" 8 78 $SAVECHANGES --title "Opportunity to Save Recent Changes to a.sh" 3>&1 1>&2 2>&3)
        if [ "$SAVECHANGES" == "y" ]; then
          cp /home/pi/rpidatv/scripts/a.sh /home/pi/rpidatv/scripts/a.sh.beta
        fi
        cp /home/pi/rpidatv/scripts/a.sh.rel /home/pi/rpidatv/scripts/a.sh
        cp /home/pi/rpidatv/bin/ffmpeg.old /home/pi/rpidatv/bin/ffmpeg
      ;;
      yes)
        cp /home/pi/rpidatv/scripts/a.sh.beta /home/pi/rpidatv/scripts/a.sh
        cp /home/pi/rpidatv/bin/ffmpeg.new /home/pi/rpidatv/bin/ffmpeg
    esac
  fi
}

do_system_setup()
{
menuchoice=$(whiptail --title "$StrSystemTitle" --menu "$StrSystemContext" 16 78 10 \
    "1 Autostart" "$StrAutostartMenu"  \
    "2 Display" "$StrDisplayMenu" \
    "3 Show IP" "$StrIPMenu" \
    "4 WiFi Set-up" "SSID and password"  \
    "5 WiFi Off" "Turn the WiFi Off" \
    "6 Enable DigiThin" "Not Implemented Yet" \
    "7 Set-up EasyCap" "Set input socket and PAL/NTSC"  \
    "8 Audio Input" "Select USB Dongle or EasyCap"  \
    "9 Attenuator" "Select Output Attenuator Type"  \
    "10 Update" "Check for Updated rpidatv Software"  \
    3>&2 2>&1 1>&3)
    case "$menuchoice" in
        1\ *) do_autostart_setup ;;
        2\ *) do_display_setup   ;;
	3\ *) do_IP_setup ;;
        4\ *) do_WiFi_setup ;;
        5\ *) do_WiFi_Off   ;;
        6\ *) do_Enable_DigiThin ;;
        7\ *) do_EasyCap ;;
        8\ *) do_audio_switch;;
        9\ *) do_attenuator;;
        10\ *) do_Update ;;
     esac
}

do_system_setup_2()
{
  menuchoice=$(whiptail --title "$StrSystemTitle" --menu "$StrSystemContext" 20 78 13 \
    "1 Set Presets" "For Touchscreen Frequencies"  \
    "2 Set Presets" "For Touchscreen Symbol Rates"  \
    "3 ADF4351 Ref Freq" "Set ADF4351 Reference Freq and Cal" \
    "4 ADF4351 Levels" "Set ADF4351 Levels for Each Band" \
    "5 DATV Express" "Configure DATV Express Settings for each band" \
    "6 Contest Numbers" "Set Contest Numbers for each band" \
    "7 Viewfinder" "Disable or Enable Viewfinder on Touchscreen" \
    "8 SD Card Info" "Show SD Card Information"  \
    "9 Factory Settings" "Restore Initial Configuration" \
    "10 Reset Touch Cal" "Reset Touchscreen Calibration to zero" \
    "11 Back-up Settings" "Save Settings to a USB drive" \
    "12 Load Settings" "Load settings from a USB Drive" \
    3>&2 2>&1 1>&3)
  case "$menuchoice" in
    1\ *) do_presets ;;
    2\ *) do_preset_SRs ;;
    3\ *) do_4351_ref  ;;
    4\ *) do_4351_levels ;;
    5\ *) do_set_express ;;
    6\ *) do_numbers ;;
    7\ *) do_vfinder ;;
    8\ *) do_SD_info ;;
    9\ *) do_factory;;
    10\ *) do_touch_factory;;
    11\ *) do_back_up;;
    12\ *) do_load_settings;;
  esac
}

#    Insert these lines back into do_system_setup_2() if required (201710080)
#    "12 Beta Software" "Choose whether to use experimental software" \
#    12\ *) do_beta ;;

do_language_setup()
{
  menuchoice=$(whiptail --title "$StrLanguageTitle" --menu "$StrOutputContext" 16 78 6 \
    "1 French Menus" "Menus Francais"  \
    "2 English Menus" "Change Menus to English" \
    "3 German Menus" "Menüs auf Deutsch wechseln" \
    "4 French Keyboard" "$StrKeyboardChange" \
    "5 UK Keyboard" "$StrKeyboardChange" \
    "6 US Keyboard" "$StrKeyboardChange" \
      3>&2 2>&1 1>&3)
    case "$menuchoice" in
      1\ *) set_config_var menulanguage "fr" $CONFIGFILE ;;
      2\ *) set_config_var menulanguage "en" $CONFIGFILE ;;
      3\ *) set_config_var menulanguage "de" $CONFIGFILE ;;
      4\ *) sudo cp $PATHCONFIGS"/keyfr" /etc/default/keyboard ;;
      5\ *) sudo cp $PATHCONFIGS"/keygb" /etc/default/keyboard ;;
      6\ *) sudo cp $PATHCONFIGS"/keyus" /etc/default/keyboard ;;
    esac

  # Check Language

  MENU_LANG=$(get_config_var menulanguage $CONFIGFILE)

  # Set Language

  case "$MENU_LANG" in
  en)
    source $PATHSCRIPT"/langgb.sh"
  ;;
  fr)
    source $PATHSCRIPT"/langfr.sh"
  ;;
  de)
    source $PATHSCRIPT"/langde.sh"
  ;;
  *)
    source $PATHSCRIPT"/langgb.sh"
  ;;
  esac
}

do_Exit()
{
  exit
}

do_Reboot()
{
  sudo reboot now
}

do_Shutdown()
{
  sudo shutdown now
}

do_TouchScreen()
{
  reset
  sudo killall fbcp >/dev/null 2>/dev/null
  fbcp &
  /home/pi/rpidatv/scripts/scheduler.sh
}

do_KTransmit()
{
  /home/pi/rpidatv/bin/keyedtx 1 7
}

do_KStreamer()
{
  /home/pi/rpidatv/bin/keyedstream 1 7
}

do_CStreamer()
{
  /home/pi/rpidatv/bin/keyedstream 0
}

do_TestRig()
{
  reset
  sudo killall fbcp >/dev/null 2>/dev/null
  fbcp &
  /home/pi/rpidatv/bin/testrig
}

do_EnableButtonSD()
{
  cp $PATHCONFIGS"/text.pi-sdn" /home/pi/.pi-sdn  ## Load it at logon
  /home/pi/.pi-sdn                                ## Load it now
}

do_DisableButtonSD()
{
  rm /home/pi/.pi-sdn             ## Stop it being loaded at log-on
  sudo pkill -x pi-sdn            ## kill the current process
} 

do_shutdown_menu()
{
menuchoice=$(whiptail --title "Shutdown Menu" --menu "Select Choice" 16 78 10 \
    "1 Shutdown now" "Immediate Shutdown"  \
    "2 Reboot now" "Immediate reboot" \
    "3 Exit to Linux" "Exit menu to Command Prompt" \
    "4 Restore TouchScreen" "Exit to LCD.  Use ctrl-C to return" \
    "5 Start Keyed TX" "Start the GPIO keyed Transmitter" \
    "6 Start Keyed Streamer" "Start the keyed Repeater Streamer" \
    "7 Start Constant Streamer" "Start the constant Repeater Streamer" \
    "8 Start Test Rig"  "Test rig for pre-sale testing of FM Boards" \
    "9 Button Enable" "Enable Shutdown Button" \
    "10 Button Disable" "Disable Shutdown Button" \
      3>&2 2>&1 1>&3)
    case "$menuchoice" in
        1\ *) do_Shutdown ;;
        2\ *) do_Reboot ;;
        3\ *) do_Exit ;;
        4\ *) do_TouchScreen ;;
        5\ *) do_KTransmit ;;
        6\ *) do_KStreamer ;;
        7\ *) do_CStreamer ;;
        8\ *) do_TestRig ;;
        9\ *) do_EnableButtonSD ;;
        10\ *) do_DisableButtonSD ;;
    esac
}

display_splash()
{
  sudo killall -9 fbcp >/dev/null 2>/dev/null
  fbcp & >/dev/null 2>/dev/null  ## fbcp gets started here and stays running. Not called by a.sh
  sudo fbi -T 1 -noverbose -a $PATHSCRIPT"/images/BATC_Black.png" >/dev/null 2>/dev/null
  (sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &  ## kill fbi once it has done its work
}

OnStartup()
{
CALL=$(get_config_var call $CONFIGFILE)
MODE_INPUT=$(get_config_var modeinput $CONFIGFILE)
MODE_OUTPUT=$(get_config_var modeoutput $CONFIGFILE)
SYMBOLRATEK=$(get_config_var symbolrate $CONFIGFILE)
FEC=$(get_config_var fec $CONFIGFILE)
PATHTS=$(get_config_var pathmedia $CONFIGFILE)
FREQ_OUTPUT=$(get_config_var freqoutput $CONFIGFILE)
GAIN_OUTPUT=$(get_config_var rfpower $CONFIGFILE)
let FECNUM=FEC
let FECDEN=FEC+1
V_FINDER=$(get_config_var vfinder $CONFIGFILE)

INFO=$CALL":"$MODE_INPUT"-->"$MODE_OUTPUT"("$SYMBOLRATEK"KSymbol FEC "$FECNUM"/"$FECDEN") on "$FREQ_OUTPUT"Mhz"

do_transmit
}

#********************************************* MAIN MENU *********************************
#************************* Execution of Console Menu starts here *************************

# Check Language
MENU_LANG=$(get_config_var menulanguage $CONFIGFILE)

# Set Language
  case "$MENU_LANG" in
  en)
    source $PATHSCRIPT"/langgb.sh"
  ;;
  fr)
    source $PATHSCRIPT"/langfr.sh"
  ;;
  de)
    source $PATHSCRIPT"/langde.sh"
  ;;
  *)
    source $PATHSCRIPT"/langgb.sh"
  ;;
  esac


#if [ "$MENU_LANG" == "en" ]; then
#  source $PATHSCRIPT"/langgb.sh"
#else
#  source $PATHSCRIPT"/langfr.sh"
#fi

# Display Splash on Touchscreen if fitted
display_splash
status="0"

# Start DATV Express Server if required
MODE_OUTPUT=$(get_config_var modeoutput $CONFIGFILE)
SYMBOLRATEK=$(get_config_var symbolrate $CONFIGFILE)
if [ "$MODE_OUTPUT" == "DATVEXPRESS" ]; then
  if pgrep -x "express_server" > /dev/null; then
    # Express already running so do nothing
    :
  else
    # Not running and needed, so start it
    echo "Starting the DATV Express Server.  Please wait."
    # Make sure that the Control file is not locked
    sudo rm /tmp/expctrl >/dev/null 2>/dev/null
    # From its own folder otherwise it doesn't read the config file
    cd /home/pi/express_server
    sudo nice -n -40 /home/pi/express_server/express_server  >/dev/null 2>/dev/null &
    cd /home/pi
    sleep 5                # Give it time to start
    reset                  # Clear message from screen
  fi
fi

# Set Band, Filter and Port Switching
$PATHSCRIPT"/ctlfilter.sh"

# Check whether to go straight to transmit or display the menu
if [ "$1" != "menu" ]; then # if tx on boot
  OnStartup               # go straight to transmit
fi

sleep 0.2

# Loop round main menu
while [ "$status" -eq 0 ] 
  do

    # Lookup parameters for Menu Info Message
    CALL=$(get_config_var call $CONFIGFILE)
    MODE_INPUT=$(get_config_var modeinput $CONFIGFILE)
    MODE_OUTPUT=$(get_config_var modeoutput $CONFIGFILE)
    SYMBOLRATEK=$(get_config_var symbolrate $CONFIGFILE)
    FEC=$(get_config_var fec $CONFIGFILE)
    PATHTS=$(get_config_var pathmedia $CONFIGFILE)
    FREQ_OUTPUT=$(get_config_var freqoutput $CONFIGFILE)
    GAIN_OUTPUT=$(get_config_var rfpower $CONFIGFILE)
    let FECNUM=FEC
    let FECDEN=FEC+1
    INFO=$CALL":"$MODE_INPUT"-->"$MODE_OUTPUT"("$SYMBOLRATEK"KSymbol FEC "$FECNUM"/"$FECDEN") on "$FREQ_OUTPUT"Mhz"
    V_FINDER=$(get_config_var vfinder $CONFIGFILE)

    # Display main menu

    menuchoice=$(whiptail --title "$StrMainMenuTitle" --menu "$INFO" 16 82 9 \
	"0 Transmit" $FREQ_OUTPUT" Mhz, "$SYMBOLRATEK" KS, FEC "$FECNUM"/"$FECDEN"." \
        "1 Source" "$StrMainMenuSource"" ("$MODE_INPUT" selected)" \
	"2 Output" "$StrMainMenuOutput"" ("$MODE_OUTPUT" selected)" \
	"3 Station" "$StrMainMenuCall" \
	"4 Receive" "$StrMainMenuReceive" \
	"5 System" "$StrMainMenuSystem" \
        "6 System 2" "$StrMainMenuSystem2" \
	"7 Language" "$StrMainMenuLanguage" \
        "8 Shutdown" "$StrMainMenuShutdown" \
 	3>&2 2>&1 1>&3)

        case "$menuchoice" in
	    0\ *) do_transmit   ;;
            1\ *) do_input_setup   ;;
	    2\ *) do_output_setup ;;
   	    3\ *) do_station_setup ;;
	    4\ *) do_receive_menu ;;
	    5\ *) do_system_setup ;;
	    6\ *) do_system_setup_2 ;;
            7\ *) do_language_setup ;;
            8\ *) do_shutdown_menu ;;
               *)

        # Display exit message if user jumps out of menu
        whiptail --title "$StrMainMenuExitTitle" --msgbox "$StrMainMenuExitContext" 8 78

        # Set status to exit
        status=1

        # Sleep while user reads message, then exit
        sleep 1
      exit ;;
    esac
    exitstatus1=$status1
  done
exit
