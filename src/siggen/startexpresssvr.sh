#! /bin/bash

Called by the SigGen to start the Express Server running when Express selected as an output mode

    if pgrep -x "express_server" > /dev/null
    then
      # Express already running
      :
    else
      # Stopped, so make sure the control file is not locked and start it
      # From its own folder otherwise it doesnt read the config file
      sudo rm /tmp/expctrl >/dev/null 2>/dev/null
      cd /home/pi/express_server
      sudo nice -n -40 /home/pi/express_server/express_server  >/dev/null 2>/dev/null &
      cd /home/pi
      sleep 5
    fi

# Code below as reference

#    # Set output for ffmpeg (avc2ts uses netcat to pipe output from videots)
#    OUTPUT="udp://127.0.0.1:1314?pkt_size=1316&buffer_size=1316"
#    FREQUENCY_OUT=0  # Not used in this mode?
#    # Calculate output freq in Hz using floating point
#    FREQ_OUTPUTHZ=`echo - | awk '{print '$FREQ_OUTPUT' * 1000000}'`
#    echo "set freq "$FREQ_OUTPUTHZ >> /tmp/expctrl
#    echo "set fec "$FECNUM"/"$FECDEN >> /tmp/expctrl
#    echo "set srate "$SYMBOLRATE >> /tmp/expctrl
#    # Set the ports
#    $PATHSCRIPT"/ctlfilter.sh"

#    # Set the output level based on the band
#    INT_FREQ_OUTPUT=${FREQ_OUTPUT%.*}
#    if (( $INT_FREQ_OUTPUT \< 100 )); then
#      GAIN=$(get_config_var explevel0 $CONFIGFILE);
#    elif (( $INT_FREQ_OUTPUT \< 250 )); then
#      GAIN=$(get_config_var explevel1 $CONFIGFILE);
#    elif (( $INT_FREQ_OUTPUT \< 950 )); then
#      GAIN=$(get_config_var explevel2 $CONFIGFILE);
#    elif (( $INT_FREQ_OUTPUT \< 2000 )); then
#      GAIN=$(get_config_var explevel3 $CONFIGFILE);
#    elif (( $INT_FREQ_OUTPUT \< 4400 )); then
#      GAIN=$(get_config_var explevel4 $CONFIGFILE);
#    else
#      GAIN="30";
#    fi

#    # Set Gain
#    echo "set level "$GAIN >> /tmp/expctrl

#    # Make sure that carrier mode is off
#    echo "set car off" >> /tmp/expctrl
