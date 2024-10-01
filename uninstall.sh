# Check if the password is provided as an argument
if [ -z "$1" ]; then
    echo "Usage: $0 <sudo_password>"
    exit 1
fi

# Assign the first argument as the password
PASSWORD="$1"

# Set global variable for the current working directory
global_pwd=$(pwd)

run_with_sudo() {
    echo "$PASSWORD" | sudo -S "$@"
}

# REFERENCE MONITOR

cd "$global_pwd/reference-monitor" || exit 1
echo "Entering reference-monitor folder"
echo "cleaning the module objects"
make clean
echo "removing module the_reference_monitor"
run_with_sudo rmmod the_reference_monitor
cd "$global_pwd"

# singlefile-FS
cd "$global_pwd/singlefile-FS" || exit 1
echo "Entering singlefile-FS folder"

rm -rf "$global_pwd/singlefile-FS/image"
run_with_sudo umount -l "$global_pwd/singlefile-FS/mount"
rm -rf "$global_pwd/singlefile-FS/mount"
echo "cleaning the module objects"
make clean
run_with_sudo rmmod singlefilefs
echo "Finished all tasks in singlefile-FS"

cd "$global_pwd/System_call_discover" || exit 1
echo "Entering System_call_discover folder"
echo "cleaning the module objects"
make clean
echo "removing module the_usctm"
run_with_sudo rmmod the_usctm
echo "Finished clean and remove module"
cd "$global_pwd"
echo "unistall completed"

