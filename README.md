# DPUShell

## Setting up your development Environment

- OS : ubuntu-16.04.3-server-amd64.iso
- Download ISO URL : https://www.ubuntu.com/download/server
- Credentials
    - UN: root
    - PW: Test1234!
    - UN: ninja
    - PW: Test1234!
- Project directory is mounted in /srv under DPUShell

##OS Installation/Configuration Instructions

- During Installation Partition with LVM
- Install ssh server (either during installation or via apt-get)
    - apt-get install openssh-server 
- Setup root password (Test1234!)
- Setup SSH for root (/etc/ssh/sshd_config) -> PermitRootLogin yes
- As root install VirtualBox guest Additions ** needed for mounting shared directories 
    - apt-get update
    - apt-get install virtualbox-guest-dkms
    - apt-get install -y dkms build-essential linux-headers-generic linux-headers-$(uname -r)
    - follow this tutorial: http://www.techrepublic.com/article/how-to-install-virtualbox-guest-additions-on-a-gui-less-ubuntu-server-host/
        - To mount the CD start the virtual machine and go to devices within the host machine menu
    - mkdir /srv/DPUShell ** this will create a directory to mount the project to 

** reboot the server

##VirtualBox Setup (Version 5.1.26 r117224 (Qt5.6.2))

Setup Networking (varies per machine)
 
- Networking 
    - Enable Network adapter
    - Set Network adapter to "Bridged adapter"
    - Set Permiscuous Mode setting to Allow All
    - Set to cable connected
- Shared Folders
    - Add a shared folder (Folder path is the path of your project)
    - Folder Name is "DPUShell"
    - Select "Auto-mount" & "Make Permanent"
    - add the following entry to /etc/fstab "/media/sf_DPUShell /srv/DPUShell none defaults,bind 0 0"
        - ** That entry will automatically mount on startup, to test for correctness run "mount -a" 

# Test your OS/VMWare installation/configuration
- Reboot the VM 
- You should be ready to Develop on this project from your local machine
- Tests to make sure you setup your environment correctly
    - cd /srv/DPUShell
        - ls (do you see the contents for the project)
    - run ifconfig to get the IP address
    - try to ssh into the running vm from your local machine "ssh root@{{your vm ip address}}"
    - try to ssh into the running vm from your local machine "ssh ninja@{{your vm ip address}}"
   
   
# Setup & your test shell for the 'ninja' user account

** you need cmake 3.8.2

- cd ~
- wget https://cmake.org/files/v3.8/cmake-3.8.2.tar.gz
- tar -xvf cmake-3.8.2.tar.gz
- cd cmake-3.8.2.tar.gz
- ./bootstrap
- make
- make install

--- 

# setting up the ninja user
- run /srv/DPUShell/build/build_install.sh, to cmake, build and setup the existing ninja users shell

** for testing purposes, setup ssh key access for the ninja user saves you a lot of time by not having to enter in your password

# Development and Testing

- IDE: CLion (2017.2.2)
- You can either develop directly on the linux machine or develop locally via the shared directory in Virtualbox
- After coding to test your changes, run the /srv/DPUShell/build/build_install.sh script
    - This will build and deploy your shell as the default shell for the test user
- ssh into the vm as ninja@x.x.x.x and your shell will execute
- repeat the process


====================
# Assumptions Made

- Signals: https://www.tutorialspoint.com/unix/unix-signals-traps.htm
    - Only monitoring the following signals
    - SIGHUP	[1]	Hang up detected on controlling terminal or death of controlling process
    - SIGINT	[2]	Issued if the user sends an interrupt signal (Ctrl + C)
    - SIGQUIT	[3]	Issued if the user sends a quit signal (Ctrl + D)
    - SIGTERM	[15]	Software termination signal (sent by kill by default)


 




      







