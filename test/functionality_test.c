#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

#include "lib/include/ref_syscall.h"
#include "lib/include/mock.h"

int all_state [] = {ON, OFF, REC_ON, REC_OFF}; 
int bad_state [] = {0, 0x40};
int current_state;
//before test



void test_change_state(char *good_password, char *bad_password){
    int i = 0;

    seteuid(1000);
    // good argment but bad user
    int ret;
    assert((ret = change_state(good_password, 0x1)) == -1);
    perror("good password bad euid expected EPERM");
    assert(errno == EPERM);
    seteuid(0);
    // bad password good state
    assert(change_state(bad_password, 0x1) == -1);
    perror("bad password bad good state expected EACCESS");
    assert(errno == EACCES);
    // bad password bad state
    assert(change_state(bad_password, bad_state[0]) == -1);
    perror("bad password bad bad_state expected EINVAL");
    assert(errno == EINVAL);
    // bad password bad state
    assert(change_state(bad_password, bad_state[1]) == -1);
    perror("bad password bad_state expected EINVAL");
    assert(errno == EINVAL);
    // good password bad state
    assert(change_state(good_password, bad_state[0]) == -1);
    perror("good password bad_state expected EINVAL");
    assert(errno == EINVAL);
    // good password bad state
    perror("good password bad_state expected EINVAL");
    assert(change_state(good_password, bad_state[1]) == -1);
    assert(errno == EINVAL);

    i = 0;
    //passing current state 
    while (1)
    {
        if (current_state == all_state[i]){
            break;
        }
        i++;
    }

    assert(change_state(good_password, all_state[i]) == -1);
    perror("good password same state expected ECANCELED");
    assert(errno == ECANCELED);
    FILE *sys;
    char buff[16];
    int dummy;
    int read_bytes;
    //checking the state transition
    for (i = 0; i < 4; i++){
        if (current_state != all_state[i]){
            sys = fopen("/sys/module/the_reference_monitor/parameters/current_state", "r");
            if (sys == NULL){
                perror("cannot ope sys");
                exit(EXIT_FAILURE);
            }
            assert(change_state(good_password, all_state[i]) == 0);
            read_bytes = fread(buff, sizeof(char), sizeof(buff), sys);
            if (read_bytes < 0){
                perror("some problem occured");
                exit(EXIT_FAILURE);
            }
            buff[read_bytes - 1] = 0;
            dummy = atoi(buff);
            assert(dummy == all_state[i]);
            printf("{Current state=%s; Expected state=%d}\n", buff, all_state[i]);
            fflush(stdout);
            current_state = all_state[i];
        }
        
    }

    
}

void test_change_path(char *good_password, char *bad_password, node_list_t file_list) {
    int ret;

    // Test with non-root user
    seteuid(1000);
    ret = change_path(good_password, "/valid/path", ADD_PATH);
    assert(ret == -1);
    perror("Expected EPERM for non-root user");
    assert(errno == EPERM);
    seteuid(0);

    // Test with bad password
    ret = change_path(bad_password, "/valid/path", ADD_PATH);
    assert(ret == -1);
    perror("Expected EACCESS for bad password");
    assert(errno == EACCESS);

    // Test with forbidden path
    ret = change_path(good_password, "/proc/forbidden", ADD_PATH);
    assert(ret == -1);
    perror("Expected EINAVL for forbidden path");
    assert(errno == EINAVL);

    // Test with null path
    ret = change_path(good_password, NULL, ADD_PATH);
    assert(ret == -1);
    perror("Expected EINAVL for null path");
    assert(errno == EINAVL);

    // Test valid add path
    ret = change_path(good_password, "/valid/path", ADD_PATH);
    assert(ret == 0);
    printf("Path added successfully\n");

    // Test valid remove path
    ret = change_path(good_password, "/valid/path", REMOVE_PATH);
    assert(ret == 0);
    printf("Path removed successfully\n");

    // Test state change with path operation
    ret = change_path(good_password, "/valid/path", ADD_PATH | REC_ON);
    if (ret == 0) {
        printf("Path added and state changed to REC_ON successfully\n");
    } else {
        perror("Failed to add path and change state");
    }
}


int main(int argc, char **argv){

    
    password_t password;
    node_list_t file_list;
    size_t len;
    int before_state;
    if (argc < 5){
        printf("%s\n", password_error_message);
        exit(EXIT_FAILURE);
    }
    
    mock_password(argc, argv, &password);
    current_state = mock_get_state();
    before_state = current_state;
    
    test_change_state(password.good_password, password.bad_password);
    
    if (change_state(password.good_password, before_state) < 0 && errno != ECANCELED){
        perror("unable to restore previous state");
        exit(EXIT_FAILURE);
    }

    current_state = before_state;

    mock_gen_test_file_env(argc, argv, &file_list);
    test_change_path(password.good_password, password.bad_password, file_list);

}