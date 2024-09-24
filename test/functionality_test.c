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
    assert(change_state(good_password, 0x1) == -1);
    assert(errno == -EPERM);
    seteuid(0);
    // bad password good state
    assert(change_state(bad_password, 0x1) == -1);
    assert(errno == -EACCES);
    // bad password bad state
    assert(change_state(bad_password, bad_state[0]) == -1);
    assert(errno == -EACCES);
    // bad password bad state
    assert(change_state(bad_password, bad_state[1]) == -1);
    assert(errno == -EACCES);
    // good password bad state
    assert(change_state(good_password, bad_state[0]) == -1);
    assert(errno == -EINVAL);
    // good password bad state
    assert(change_state(good_password, bad_state[1]) == -1);
    assert(errno == -EINVAL);

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
    assert(errno == ECANCELED);

    //checking the state transition
    for (i = 0; i < 4; i++){
        if (current_state != all_state[i]){
            assert(change_state(good_password, all_state[i]) == 0);
        }
        current_state = all_state[i];
    }

    
}


int main(int argc, char **argv){

    
    password_t password;
    size_t len;

    if (argc < 3){
        printf("%s\n", password_error_message);
        exit(EXIT_FAILURE);
    }
    
    mock_password(argc, argv, &password);
    current_state = mock_get_state();
    test_change_state(password.good_password, password.bad_password);
    


}