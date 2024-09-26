#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <fcntl.h>
#include "lib/include/ref_syscall.h"
#include "lib/include/mock.h"
#include "lib/include/mock_list.h"
#include <stdatomic.h>
#define TRY_HARDERS_THREAD (1)
#define LOOPS (5)

struct black_list_info{
    node_list_t black_list;
    pthread_rwlock_t rwlock;
};
change_path_mock_t cng;
struct black_list_info black_list;
password_t password;
int all_state [] = {ON, OFF, REC_ON, REC_OFF}; 
int bad_state [] = {0, ON|OFF, REC_OFF|REC_ON, ON|OFF|REC_ON|REC_OFF ,0x40, -1, -1000};
int current_state;
int before_state;

atomic_int try;
atomic_int fail;

void *file_openers(void *dummy){
    int id = *((int *)dummy);
    int i;
    free(dummy);
    int fd;
    node_t *curr;
    curr = cng.list.head;
    int flag;

    for (i = 1; i < LOOPS; i++){
        if (curr == NULL){
            curr = cng.list.head;
        }
        switch ((i % 7))
        {
        case 1:
            flag = O_WRONLY;
            break;
        case 2:
            flag = O_APPEND;
            break;
        case 3:
            flag = O_TRUNC;
            break;
        case 4:
            flag = O_RDWR;
            break;
        case 5: 
            flag = O_WRONLY | O_TRUNC;
            break;
        case 6:
            flag = O_RDWR | O_APPEND;
            break;
        default:
            flag = O_RDONLY;
            break;
        }

	
	
        if (!curr->id_dir){
		printf("Trying open this file %s\n", curr->path);
		fflush(stdout);
            fd = open(curr->path, flag, 0666);
            if (curr->add && flag != O_RDONLY){
                atomic_fetch_add(&try, 1);
                if (fd < 0){
                    atomic_fetch_add(&fail, 1);
                }
            }
        }
    
        curr = curr->next;
    }

}

int main(int argc, char **argv){
    change_path_mock_t cngpth_mock;
    node_list_t list;
    cngpth_mock.list = list;
    char *conf_file;
    cngpth_mock.conf_file = conf_file;
    size_t len;
    pthread_t tids[(TRY_HARDERS_THREAD)];
    int k;
    node_t *curr;
    if (argc < 5){
        printf("Launch program with -pwd ReferenceMonitorPassword -conf conf_file\n");
        exit(EXIT_FAILURE);
    }

    mock_password(argc, argv, &password);
    current_state = mock_get_state();
    before_state = current_state;
    mock_gen_test_file_env(argc, argv, &cng);
    black_list.black_list.head = NULL;
    black_list.black_list.tail = NULL;
    pthread_rwlock_init(&black_list.rwlock, NULL);


    if (change_state(password.good_password, REC_ON) < 0 && errno != ECANCELED){
        perror("cannot change the state to REC_ON");
        exit(EXIT_FAILURE);
    }

    curr = cng.list.head;
    while (curr)
    {
        if (curr->add){
            if (change_path(password.good_password, curr->path, ADD_PATH)){
                printf("%s", curr->path);
                fflush(stdout);
                perror("cannot add the path");
            }
        }
	curr = curr->next;
    }
    
    atomic_init(&try, 0);
    atomic_init(&fail, 0);

	printf("main thread launching threads\n");
    for (k=0; k < TRY_HARDERS_THREAD; k++){
        int *id = malloc(sizeof(int));
        *id = k;
        
        if (pthread_create(&tids[k], NULL, file_openers, (void *)id) < 0){
            perror("unable to create thread for update_thread");
            exit(EXIT_FAILURE);
        }   
    }
    
    for (k=0; k < (TRY_HARDERS_THREAD + 1); k++){
        pthread_join(tids[k], NULL);
    }


    curr = cng.list.head;
    while (curr)
    {
        if (curr->add){
            if (change_path(password.good_password, curr->path, REMOVE_PATH)){
                printf("%s", curr->path);
                fflush(stdout);
                perror("cannot remove the path");
            }
        }
	curr = curr->next;
    }

    if (change_state(password.good_password, before_state) < 0 && errno != ECANCELED){
        perror("cannot restore the previous state");
        exit(EXIT_FAILURE);
    }
    printf("expectiong that %d == %d\n", atomic_load(&try), atomic_load(&fail));
    fflush(stdout);
    assert(atomic_load(&try) == atomic_load(&fail));

    printf("stress_test over\n");
    fflush(stdout);

}
