#!/bin/sh

if [ "$1" = "updatesystem" ]
then
	echo "Updating system"
	sudo apt-get update || 
         { echo "ERROR : Unable to apt-get update" && exit 1 ;}
	sudo apt-get upgrade -y || 
         { echo "ERROR : Unable to apt-get upgrade" && exit 1 ;}
	sudo rpi-update || 
         { echo "ERROR : Unable to updta the firmware" && exit 1 ;}
	echo "Please reboot if the message above asks for it"
else
	echo "Installing required dependencies"
	sudo apt-get install -y --force-yes dkms cpp-4.7 gcc-4.7 git joystick || 
         { echo "ERROR : Unable to install required dependencies" && exit 1 ;}
	echo "Downloading and installing current kernel headers"
	sudo apt-get install -y --force-yes raspberrypi-kernel-headers || 
         { echo "ERROR : Unable to install kernel headers" && exit 1 ;}
	echo "Downloading am_joyin 0.1.0"
	wget https://github.com/amos42/am_joyin/releases/download/v0.1.0/am_joyin-0.1.deb || 
         { echo "ERROR : Unable to find am_joyin package" && exit 1 ;}
	echo "Installing am_joyin 0.1.0"
	sudo dpkg -i am_joyin-0.1.0.deb || 
         { echo "ERROR : Unable to install am_joyin" && exit 1 ;}
	echo "Installation OK"
	echo "Load the module with 'sudo modprobe am_joyin device1=1' for 1 joystick"
	echo "or with 'sudo modprobe am_joyin device1=1 device1=1,-2' for 2 joysticks"
	echo "See https://github.com/amos42/am_joyin for more details"
fi