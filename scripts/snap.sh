#!/bin/bash

# This script transfers a frame of the EasyCap video input to a touchscreen and
# a file.  It needs the creation of a /home/pi/tmp/ folder
# as a tmpfs by adding an entry in /etc/fstab
# tmpfs           /home/pi/tmp    tmpfs   defaults,noatime,nosuid,size=10m  0  0
# and a /home/pi/snaps folder
# It takes 3 frames and uses the 3rd as this seemed to be the best quality
# the yadif=0:1:0 sorts out the Fushicai PAL interlace error

#set -x

sudo rm /home/pi/tmp/frame*.jpg >/dev/null 2>/dev/null

if  [ ! -f "/home/pi/snaps/snap_index.txt" ]; then
    echo '0' > /home/pi/snaps/snap_index.txt
fi

SNAP_SERIAL=$(head -c 4 /home/pi/snaps/snap_index.txt)

  /home/pi/rpidatv/bin/ffmpeg -hide_banner -loglevel panic \
    -f v4l2 \
    -i /dev/video1 -vf "format=yuva420p,yadif=0:1:0" \
    -vframes 3 \
    /home/pi/tmp/frame-%03d.jpg

sudo fbi -T 1 -noverbose -a /home/pi/tmp/frame-003.jpg >/dev/null 2>/dev/null

cp /home/pi/tmp/frame-003.jpg /home/pi/snaps/snap"$SNAP_SERIAL".jpg

let SNAP_SERIAL=$SNAP_SERIAL+1

rm  /home/pi/snaps/snap_index.txt

echo $SNAP_SERIAL  >  /home/pi/snaps/snap_index.txt

sleep 0.1  # Let fbi finish

sudo killall -9 fbi >/dev/null 2>/dev/null  ## kill fbi once it has done its work

