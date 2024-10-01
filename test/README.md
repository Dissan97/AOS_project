# test

containing functionality and stress test implementation

## Syscall test
Makefile for system_call_functionality test

## Stress test opening files
setup test-dir and files to test by adding them in the reference monitor black list
```
 source stress.sh <root password> <reference_monitor_password> <#threas> <loop>
```
check kernel log
```
    dmesg
```