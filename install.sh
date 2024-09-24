#!/bin/bash

# Check if the password is provided as an argument
if [ -z "$1" ]; then
    echo "Usage: $0 <sudo_password>"
    exit 1
fi

# Assign the first argument as the password
PASSWORD="$1"

# Set global variable for the current working directory
global_pwd=$(pwd)

# Function to run sudo commands with password
run_with_sudo() {
    echo "$PASSWORD" | sudo -S "$@"
}

source "$global_pwd/unistall.sh" "$PASSWORD"

cd "$global_pwd/System_call_discover" || exit 1
echo "Entering System_call_discover folder"
echo "make module objects"
make
echo "loading the kernel module"
run_with_sudo insmod the_usctm.ko
echo "Finished building and inserting module"


cd "$global_pwd/singlefile-FS" || exit 1
echo "Entering singlefile-FS folder"


rm -rf image
run_with_sudo umount ./mount
rm -rf mount
echo "make module objects"
make
echo "creating file for the filesystem"
make create
echo "loading the kernel module"
run_with_sudo make load
echo "mounting the filesystem volume"
run_with_sudo make mnt
echo "Finished all tasks in singlefile-FS"

cd "$global_pwd/reference-monitor" || exit 1
echo "Entering reference-monitor folder"
echo "make module objects"
make
echo "loading the kernel module"
run_with_sudo make mount
cd "$global_pwd"
echo "installation completed successfully."

