#!/bin/bash

# Updated by davecrump 201712180

# Modified to overwrite ~/rpidatv/scripts and
# ~/rpidatv/src, then compile
# rpidatv, rpidatvgui avc2ts and adf4351

reset

printf "\nCommencing update.\n\n"
printf "Note that if you have not updated since 201703060, this will take longer than previous updates\n\n"

# Note previous version number
cp -f -r /home/pi/rpidatv/scripts/installed_version.txt /home/pi/prev_installed_version.txt

# Make a safe copy of rpidatvconfig.txt
cp -f -r /home/pi/rpidatv/scripts/rpidatvconfig.txt /home/pi/rpidatvconfig.txt

# Make a safe copy of siggencal.txt if required (201710281)
if [ -f "/home/pi/rpidatv/src/siggen/siggencal.txt" ]; then
  cp -f -r /home/pi/rpidatv/src/siggen/siggencal.txt /home/pi/siggencal.txt
fi

# Make a safe copy of touchcal.txt if required (201711030)
if [ -f "/home/pi/rpidatv/scripts/touchcal.txt" ]; then
  cp -f -r /home/pi/rpidatv/scripts/touchcal.txt /home/pi/touchcal.txt
fi

# Check if fbi (frame buffer imager) needs to be installed
if [ ! -f "/usr/bin/fbi" ]; then
  sudo apt-get -y install fbi
fi

# Uninstall the apt-listchanges package to allow silent install of ca certificates
# http://unix.stackexchange.com/questions/124468/how-do-i-resolve-an-apparent-hanging-update-process
sudo apt-get -y remove apt-listchanges

# Prepare to update the distribution (added 20170405)
sudo dpkg --configure -a
sudo apt-get clean
sudo apt-get update

# Update the distribution (added 20170403)
sudo apt-get -y dist-upgrade

# Check that ImageMagick is installed (201704050)
sudo apt-get -y install imagemagick

# Check that libraries required for new ffmpeg are installed (20170630)
sudo apt-get -y install libvdpau-dev libva-dev

# Check that htop is installed (201710080)
sudo apt-get -y install htop

#  Delete the duplicate touchscreen driver if it is still there (201704030)
cd /boot
sudo sed -i '/dtoverlay=ads7846/d' config.txt

# ---------- Update rpidatv -----------

cd /home/pi

# Check which source to download.  Default is production
# option -d is development from davecrump
# option -s is staging from batc/staging
if [ "$1" == "-d" ]; then
  echo "Installing development load"
  wget https://github.com/davecrump/rpidatv/archive/master.zip -O master.zip
elif [ "$1" == "-s" ]; then
  echo "Installing BATC Staging load"
  wget https://github.com/BritishAmateurTelevisionClub/rpidatv/archive/batc_staging.zip -O master.zip
else
  echo "Installing BATC Production load"
  wget https://github.com/BritishAmateurTelevisionClub/rpidatv/archive/master.zip -O master.zip
fi

# Unzip and overwrite where we need to
unzip -o master.zip

if [ "$1" == "-s" ]; then
  cp -f -r rpidatv-batc_staging/bin rpidatv
  # cp -f -r rpidatv-batc_staging/doc rpidatv
  cp -f -r rpidatv-batc_staging/scripts rpidatv
  cp -f -r rpidatv-batc_staging/src rpidatv
  rm -f rpidatv/video/*.jpg
  cp -f -r rpidatv-batc_staging/video rpidatv
  cp -f -r rpidatv-batc_staging/version_history.txt rpidatv/version_history.txt
  rm master.zip
  rm -rf rpidatv-batc_staging
else
  cp -f -r rpidatv-master/bin rpidatv
  # cp -f -r rpidatv-master/doc rpidatv
  cp -f -r rpidatv-master/scripts rpidatv
  cp -f -r rpidatv-master/src rpidatv
  rm -f rpidatv/video/*.jpg
  cp -f -r rpidatv-master/video rpidatv
  cp -f -r rpidatv-master/version_history.txt rpidatv/version_history.txt
  rm master.zip
  rm -rf rpidatv-master
fi

# Compile rpidatv core
sudo killall -9 rpidatv
cd rpidatv/src
make clean
make
sudo make install

# Compile rpidatv gui
sudo killall -9 rpidatvgui
cd gui
make clean
make
sudo make install
cd ../

# Compile avc2ts
sudo killall -9 avc2ts
cd avc2ts
make clean
make
sudo make install

#install adf4351
cd /home/pi/rpidatv/src/adf4351
touch adf4351.c
make
cp adf4351 ../../bin/
cd /home/pi

## Get tstools
# cd /home/pi/rpidatv/src
# wget https://github.com/F5OEO/tstools/archive/master.zip
# unzip master.zip
# rm -rf tstools
# mv tstools-master tstools
# rm master.zip

## Compile tstools
#cd tstools
#make
#cp bin/ts2es ../../bin/

## install H264 Decoder : hello_video
## compile ilcomponet first
#cd /opt/vc/src/hello_pi/
#sudo ./rebuild.sh

# cd /home/pi/rpidatv/src/hello_video
# make
#cp hello_video.bin ../../bin/

## TouchScreen GUI
## FBCP : Duplicate Framebuffer 0 -> 1
#cd /home/pi/
#wget https://github.com/tasanakorn/rpi-fbcp/archive/master.zip
#unzip master.zip
#rm -rf rpi-fbcp
#mv rpi-fbcp-master rpi-fbcp
#rm master.zip

## Compile fbcp
#cd rpi-fbcp/
#rm -rf build
#mkdir build
#cd build/
#cmake ..
#make
#sudo install fbcp /usr/local/bin/fbcp
#cd ../../

# Disable fallback IP (201701230)

cd /etc
sudo sed -i '/profile static_eth0/d' dhcpcd.conf
sudo sed -i '/static ip_address=192.168.1.60/d' dhcpcd.conf
sudo sed -i '/static routers=192.168.1.1/d' dhcpcd.conf
sudo sed -i '/static domain_name_servers=192.168.1.1/d' dhcpcd.conf
sudo sed -i '/interface eth0/d' dhcpcd.conf
sudo sed -i '/fallback static_eth0/d' dhcpcd.conf

# Disable the Touchscreen Screensaver (201701070)
cd /boot
if ! grep -q consoleblank cmdline.txt; then
  sudo sed -i -e 's/rootwait/rootwait consoleblank=0/' cmdline.txt
fi
cd /etc/kbd
sudo sed -i 's/^BLANK_TIME.*/BLANK_TIME=0/' config
sudo sed -i 's/^POWERDOWN_TIME.*/POWERDOWN_TIME=0/' config
cd /home/pi

# Delete, download, compile and install DATV Express-server (201702021)

if [ ! -f "/bin/netcat" ]; then
  sudo apt-get -y install netcat
fi

sudo rm -f -r /lib/firmware/datvexpress
sudo rm -f /usr/bin/express_server
sudo rm -f /etc/udev/rules.d/10-datvexpress.rules
sudo rm -f -r /home/pi/express_server-master
cd /home/pi
wget https://github.com/G4GUO/express_server/archive/master.zip -O master.zip
sudo rm -f -r express_server-master
unzip -o master.zip
sudo rm -f -r express_server
mv express_server-master express_server
rm master.zip
cd /home/pi/express_server
make
sudo make install
cd /home/pi

# Update pi-sdn with less trigger-happy version (201705200)
rm -fr /home/pi/pi-sdn /home/pi/pi-sdn-build/
git clone https://github.com/philcrump/pi-sdn /home/pi/pi-sdn-build
cd /home/pi/pi-sdn-build
make
mv pi-sdn /home/pi/
cd /home/pi

# Update the call to pi-sdn if it is enabled (201702020)
if [ -f /home/pi/.pi-sdn ]; then
  rm -f /home/pi/.pi-sdn
  cp /home/pi/rpidatv/scripts/configs/text.pi-sdn /home/pi/.pi-sdn
fi

# Restore or update rpidatvconfig.txt for
# 201701020 201701270 201702100 201707220
# Note the optional addion of outputformat in 201712180
if ! grep -q caption /home/pi/rpidatvconfig.txt; then
  # File needs updating
  printf "Adding new entries to user's rpidatvconfig.txt\n"
  source /home/pi/rpidatv/scripts/copy_config.sh
else
  # File is correct format
  printf "Copying user's rpidatvconfig.txt for use unchanged\n"
  cp -f -r /home/pi/rpidatvconfig.txt /home/pi/rpidatv/scripts/rpidatvconfig.txt
fi
rm -f /home/pi/rpidatvconfig.txt
rm -f /home/pi/rpidatv/scripts/copy_config.sh

# Install Waveshare 3.5B DTOVERLAY if required (201704080)
if [ ! -f /boot/overlays/waveshare35b.dtbo ]; then
  sudo cp /home/pi/rpidatv/scripts/waveshare35b.dtbo /boot/overlays/
fi

# Load new .bashrc to source the startup script at boot and log-on (201704160)
cp -f /home/pi/rpidatv/scripts/configs/startup.bashrc /home/pi/.bashrc

# Always auto-logon and run .bashrc (and hence startup.sh) (201704160)
sudo ln -fs /etc/systemd/system/autologin@.service\
 /etc/systemd/system/getty.target.wants/getty@tty1.service

# Reduce the dhcp client timeout to speed off-network startup (201704160)
# If required
if ! grep -q timeout /etc/dhcpcd.conf; then
  sudo bash -c 'echo -e "\n# Shorten dhcpcd timeout from 30 to 15 secs" >> /etc/dhcpcd.conf'
  sudo bash -c 'echo -e "timeout 15\n" >> /etc/dhcpcd.conf'
fi

# Enable the Video output in PAL mode (201707120)
cd /boot
sudo sed -i 's/^#sdtv_mode=2/sdtv_mode=2/' config.txt
cd /home/pi

# Compile and install the executable for switched repeater streaming (201708150)
cd /home/pi/rpidatv/src/rptr
make
mv keyedstream /home/pi/rpidatv/bin/
cd /home/pi

# Compile and install the executable for GPIO-switched transmission (201710080)
cd /home/pi/rpidatv/src/keyedtx
make
mv keyedtx /home/pi/rpidatv/bin/
cd /home/pi

# Check if tmpfs at ~/tmp exists.  If not,
# amend /etc/fstab to create a tmpfs drive at ~/tmp for multiple images (201708150)
if [ ! -d /home/pi/tmp ]; then
  sudo sed -i '4itmpfs           /home/pi/tmp    tmpfs   defaults,noatime,nosuid,size=10m  0  0' /etc/fstab
fi

# Check if ~/snaps folder exists for captured images.  Create if required
# and set the snap index number to zero. (201708150)
if [ ! -d /home/pi/snaps ]; then
  mkdir /home/pi/snaps
  echo "0" > /home/pi/snaps/snap_index.txt
fi

# Compile the Signal Generator (201710280)
cd /home/pi/rpidatv/src/siggen
make clean
make
sudo make install
cd /home/pi

# Restore the user's original siggencal.txt if required (uncomment after 201710280)
#if [ -f "/home/pi/siggencal.txt" ]; then
#  cp -f -r /home/pi/siggencal.txt /home/pi/rpidatv/src/siggen/siggencal.txt
#fi

# Restore the user's original touchcal.txt if required (201711030)
if [ -f "/home/pi/touchcal.txt" ]; then
  cp -f -r /home/pi/touchcal.txt /home/pi/rpidatv/scripts/touchcal.txt
fi

# Update the version number
rm -rf /home/pi/rpidatv/scripts/installed_version.txt
cp /home/pi/rpidatv/scripts/latest_version.txt /home/pi/rpidatv/scripts/installed_version.txt
cp -f -r /home/pi/prev_installed_version.txt /home/pi/rpidatv/scripts/prev_installed_version.txt
rm -rf /home/pi/prev_installed_version.txt

# Offer reboot
printf "A reboot may be required before using the update.\n"
printf "Do you want to reboot now? (y/n)\n"
read -n 1
printf "\n"
if [[ "$REPLY" = "y" || "$REPLY" = "Y" ]]; then
  printf "\nRebooting\n"
  sudo reboot now
fi
exit
