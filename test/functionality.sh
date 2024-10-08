run_with_sudo() {
    echo "$PASSWORD" | sudo -S bash -c "$*"
}

if [ -z "$1" ] || [ -z "$2" ] || [ -z "$3" ]; then 
    echo "Usage: $0 <sudo_password> <ref_monitor_pwd> <ref_monitor_bad_pwd>"
    exit 1
else

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

echo "creating directories"
mkdir -p test-dir
target_file="test-dir/test-file"
echo "not modify this file" > "$target_file"
before_state=$(cat /sys/module/the_reference_monitor/parameters/current_state)
before_state_arg=$(get_string_state "$before_state")
echo "compiling source test code"
gcc functionality_test.c lib/ref_syscall.c -o functionality.out
echo "state before running test $before_state_arg"
run_with_sudo ./functionality.out "$2" "$3" "$target_file" "test-dir/no-file" "$before_state"
echo "restoring previous state=$before_state_arg"
run_with_sudo echo "cngst -opt $before_state_arg -pwd $2" > /proc/ref_syscall/access_point
rm -rf test-dir
echo "done..."
fi
