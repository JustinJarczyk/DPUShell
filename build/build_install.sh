#!/bin/bash


# make build
cd /srv/DPUShell/build/artifacts/

echo "Checking Cmake to see if it needs to run"
if [ -e /srv/DPUShell/build/artifacts/Makefile ]
then
    echo "CMAKE has already been executed"
else
    echo "Running CMAKE"
    cmake ../../
fi

# make the project
make

# setup build in /bin/DPUShell
echo "Checking if /bin/DPUShell Exists"
if [ -e /bin/DPUShell ]
then
    #remove existing DPUShell if exists
    echo "Removing /bin/DPUShell"
    rm /bin/DPUShell
fi

# set 'ninja' user shell to /bin/DPUShell
echo "Copying DPUShell to bin"
cp /srv/DPUShell/build/artifacts/DPUShell /bin/
chmod 777 /bin/DPUShell

echo "Setting [ninja] user default shell to DPUShell"
chsh -s /bin/DPUShell ninja
