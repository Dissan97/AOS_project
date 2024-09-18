#include "../lib/include/ref_syscall.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sem.h>
#include <unistd.h>
#include <pthread.h>
#include <stdatomic.h>
#include <errno.h>
#define TESTERS 5

long total_attempts = 1000;
atomic_int success = ATOMIC_VAR_INIT(0);
atomic_int fail = ATOMIC_VAR_INIT(0);
char **guess_passwords;
char *pwd0 = "test_password_0";
char *pwd1 = "test_password_1";
char *pwd2 = "test_password_2";
int states[] = {OFF, REC_OFF};

char *last_updated_password;
pthread_mutex_t mutex;

void *changer(void *dummy) {
    int index;
    int pwd_index;
    for (int i = 0; i < total_attempts; i++) {
        pthread_mutex_lock(&mutex);
        pwd_index = rand()%3;
        if (!change_password(last_updated_password, guess_passwords[pwd_index]) && (last_updated_password != guess_passwords[pwd_index])){
            //updating the last_updated password
            last_updated_password = guess_passwords[pwd_index];
            atomic_fetch_add(&success, 1);
        }else {
            atomic_fetch_add(&fail, 1);
        }
        pthread_mutex_unlock(&mutex);
    }
    pthread_exit(NULL);
}


void *guesser(void *dummy) {
    int intention; // 0.5 pass good password
    int ret;
    const char *fake_password = "this_is_fake_password";
    for (int i = 0; i < total_attempts; i++) {
        intention = rand();        
        if ((intention % 2) == 0){
            pthread_mutex_lock(&mutex);
            ret = change_state(last_updated_password, rand()%2);
            if (!ret || errno == -EPERM){
                atomic_fetch_add(&success, 1);
            }else {
                printf("error, %d -EPERM=%d", errno, -EPERM);
                perror("change_state");
                atomic_fetch_add(&fail, 1);
            }
            pthread_mutex_unlock(&mutex);
        }else {
            ret = change_state(fake_password, rand()%2);
            if (ret){
                atomic_fetch_add(&success, 1);
            }else {
                atomic_fetch_add(&fail, 1);
            }
        }
    }
    pthread_exit(NULL);
}

int main(int argc, char **argv) {

    pthread_t tids[TESTERS];
    srand(1234);  
    char *actual_password;
    if (argc < 2){
        printf("please enter current good password\n");
        return -1;
    }
    actual_password = argv[1];
    guess_passwords = malloc(sizeof(char *) * 3);
    last_updated_password = actual_password;
    if (!guess_passwords){
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    guess_passwords[0] = pwd0;
    guess_passwords[1] = pwd1;
    guess_passwords[2] = pwd2;
    
    if (pthread_mutex_init(&mutex, NULL)){
        perror("mutex init");
        exit(EXIT_FAILURE);
    }

    if (pthread_create(&tids[0], NULL, changer, NULL) != 0) {
        perror("cannot create thread");
        exit(EXIT_FAILURE);
    }

    for (long k = 1; k < TESTERS; k++) {
        long *arg = malloc(sizeof(*arg));
        if (arg == NULL) {
            perror("malloc failed");
            exit(EXIT_FAILURE);
        }
        *arg = k; 
        if (pthread_create(&tids[k], NULL, guesser, arg) != 0) {
            perror("cannot create thread");
            exit(EXIT_FAILURE);
        }
    }

    
    for (long k = 0; k < TESTERS; k++) {
        pthread_join(tids[k], NULL);
    }

    printf("total attempts: %ld, total success: %d, total fail %d\n", total_attempts * TESTERS, success, fail);
    printf("success rate: %f%% - fail rate: %f%%\n", ((float)success / (TESTERS * total_attempts)) * 100, ((float)fail / (TESTERS * total_attempts)) * 100);

    if (!change_password(last_updated_password, actual_password)){
        perror("change password issue\n");
        printf("Actual good password %s\n", last_updated_password);
    }

    exit(EXIT_SUCCESS);
}
