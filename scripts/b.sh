#! /bin/bash

# Version 201802040

# This script shuts down any transmission that has been started by a.sh.
# It can be called by other programs that want to start and stop transmission
# without using menu.sh or rpidatvtouch.c


############ Set Environment Variables ###############

PATHSCRIPT=/home/pi/rpidatv/scripts
PATHRPI=/home/pi/rpidatv/bin
CONFIGFILE=$PATHSCRIPT"/rpidatvconfig.txt"
PCONFIGFILE="/home/pi/rpidatv/scripts/portsdown_config.txt"
PATHCONFIGS="/home/pi/rpidatv/scripts/configs"  ## Path to config files
GPIO_PTT=29  ## WiringPi value, not BCM

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

MODE_OUTPUT=$(get_config_var modeoutput $PCONFIGFILE)

  # Stop DATV Express transmitting if required
  if [ "$MODE_OUTPUT" == "DATVEXPRESS" ]; then
    echo "set car off" >> /tmp/expctrl
    echo "set ptt rx" >> /tmp/expctrl
    sudo killall netcat >/dev/null 2>/dev/null
  fi

  # Turn the Local Oscillator off
  sudo $PATHRPI"/adf4351" off

  # Kill the key TX processes as nicely as possible
  sudo killall rpidatv >/dev/null 2>/dev/null
  sudo killall ffmpeg >/dev/null 2>/dev/null
  sudo killall tcanim >/dev/null 2>/dev/null
  sudo killall avc2ts >/dev/null 2>/dev/null
  sudo killall netcat >/dev/null 2>/dev/null

  # Kill the key RX processes as nicely as possible
  sudo killall leandvb >/dev/null 2>/dev/null
  sudo killall hello_video.bin >/dev/null 2>/dev/null
  sudo killall ts2es >/dev/null 2>/dev/null
  sudo killall rtl_sdr >/dev/null 2>/dev/null
  sudo killall rtl_tcp >/dev/null 2>/dev/null
  sudo killall aplay >/dev/null 2>/dev/null

  # Then pause and make sure that avc2ts has really been stopped (needed at high SRs)
  sleep 0.1
  sudo killall -9 avc2ts >/dev/null 2>/dev/null

  # And make sure rpidatv has been stopped (required for brief transmit selections)
  sudo killall -9 rpidatv >/dev/null 2>/dev/null

  # Stop the audio for CompVid mode
  sudo killall arecord >/dev/null 2>/dev/null

  # Make sure that the PTT is released (required for carrier and test modes)
  gpio mode $GPIO_PTT out
  gpio write $GPIO_PTT 0


  # Check if driver for Logitech C270 or C525 needs to be reloaded
  dmesg | grep -E -q "046d:0825|Webcam C525"
  if [ $? == 0 ]; then
    sleep 3
    v4l2-ctl --list-devices > /dev/null 2> /dev/null
  fi

printf "Transmit processes stopped\n"


