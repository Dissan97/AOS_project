PASSWORD="$1"
global_pwd=$(pwd)
loops=100
threads=4
run_with_sudo() {
    echo "$PASSWORD" | sudo -S "$@"
}

get_string_state() {
  before_state=$1  # Argument passed to the function
  case "$before_state" in
    "1") echo "on";;
    "2") echo "off";;
    "4") echo "ron";;
    "8") echo "rof";;
    *) echo "error: invalid state $before_state"; return 1 ;;
  esac
}


if [ -z "$1" ] || [ -z "$2" ]; then 
    echo "Usage: $0 <sudo_password> <ref_monitor_pwd>" 
else

if [ -n "$3" ]; then
  echo  wanted "$3"
  if [[ "$3" =~ ^[0-9]+$ ]] && [ "$3" -gt 0 ]; then
    threads="$3"
  fi
fi

if [ -n "$4" ]; then
  echo  wanted "$4"
  if [[ "$4" =~ ^[0-9]+$ ]] && [ "$4" -gt 0 ]; then
    loops="$4"
  fi
fi

echo "init file to test"
mkdir -p test-dir/test-child/another-dir
echo "not modify this file" > test-dir/test-file
link test-dir/test-file test-dir/test-filehl
link test-dir/test-file test-dir/test-child/test-filehl
before_state=$(cat /sys/module/the_reference_monitor/parameters/current_state)
before_state_arg=$(get_string_state "$before_state")
curren_state=$(cat /sys/module/the_reference_monitor/parameters/current_state)
echo "configuring reference monitor current_state=$before_state_arg configuring to rec_on"
run_with_sudo echo "cngst -opt ron -pwd $2" > /proc/ref_syscall/access_point
curren_state=$(cat /sys/module/the_reference_monitor/parameters/current_state)
echo "adding path protection to reference monitor"
run_with_sudo echo "cngpth -opt ADPTH -pwd $2 -pth test-dir/test-file" > /proc/ref_syscall/access_point
run_with_sudo echo "cngpth -opt ADPTH -pwd $2 -pth test-dir/test-child" > /proc/ref_syscall/access_point
echo "gcc stress_test.c -o stress_test.out -lpthread -DLOOPS=$loops"
gcc stress_test.c -o stress_test.out -lpthread -DLOOPS=$loops
echo "test open"
./stress_test.out -target open -threads $threads -filepaths "test-dir/test-file;test-dir/test-filehl;test-dir/test-child/test-filehl" -num_paths 3
echo "test mkdir"
./stress_test.out -target mkdir -threads $threads -filepaths "test-dir/test-child" -num_paths 1
echo "test rmdir"
./stress_test.out -target rmdir -threads $threads -filepaths "test-dir/test-child/another-dir" -num_paths 1
echo "test unlink"
./stress_test.out -target unlink -threads $threads -filepaths "test-dir/test-child/test-filehl" -num_paths 1
run_with_sudo echo "cngpth -opt RMPTH -pwd $2 -pth test-dir/test-file" > /proc/ref_syscall/access_point
run_with_sudo echo "cngpth -opt RMPTH -pwd $2 -pth test-dir/test-child" > /proc/ref_syscall/access_point
run_with_sudo echo "cngst -opt $before_state_arg -pwd $2" > /proc/ref_syscall/access_point

#gcc stress-test.c -o stress-test.out -lphread
#./stress-test.out
echo "deleting test dirs"
rm -rf test-dir
fi


