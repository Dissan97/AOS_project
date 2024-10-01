#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>
#include <sys/wait.h>
#include <errno.h>

#define BUFFER_SIZE 256
#define PATH_MAX (1 << 12)
#define ARGS  "-target <open, mkdir, rmdir, unlink> -threads <num_threads> -filepaths <path1;path2;...;pathn> -num_paths # ||filepaths||"
#ifndef LOOPS
#define LOOPS 100
#endif


char **pathnames=NULL;
int threads;
int num_paths;


void *target_open(void *dummy){
    int id = *((int *)dummy);
    int ret;
    int i;
    int fd;
    int fail;
    free(dummy);
    
    printf("target_open[%d] started\n", id);
    fflush(stdout);
    
    for (i = 0; i < LOOPS; i++){
        fd = open(pathnames[i % num_paths], O_WRONLY | O_RDWR | O_CREAT);
        if (fd < 0){
            fail ++;
        }else {
            close(fd);
        }
    }
    printf("target_open[%d] finish %d loop - %d fail\n",id, LOOPS, fail);
    fflush(stdout);
    
}
const char *try_dir = "new_dir";
void *target_mkdir(void *dummy) {
    int id = *((int *)dummy);
    int ret;
    int i;
    int fail = 0;
    char buffer[256];
    
    free(dummy);

    printf("target_mkdir[%d] started\n", id);
    fflush(stdout);

    for (i = 0; i < LOOPS; i++) {
        memset(buffer, 0 , 256);
        sprintf(buffer, "%s/%s", pathnames[i % num_paths],try_dir);
        ret = mkdir(buffer, 0755); 
        if (ret < 0) {
            fail++;
        }

    }
    printf("target_mkdir[%d] finish %d loops - %d fail\n", id, LOOPS, fail);
    fflush(stdout);
    return NULL;
}

void *target_unlink(void *dummy) {
    int id = *((int *)dummy);
    int ret;
    int i;
    int fail = 0;
    free(dummy);

    printf("target_unlink[%d] started\n", id);
    fflush(stdout);

    for (i = 0; i < LOOPS; i++) {
        ret = unlink(pathnames[i % num_paths]);  // Remove files
        if (ret < 0) {
            fail++;
        }
    }
    printf("target_unlink[%d] finish %d loops - %d fail\n", id, LOOPS, fail);
    fflush(stdout);
    return NULL;
}

void *target_rmdir(void *dummy) {
    int id = *((int *)dummy);
    int ret;
    int i;
    int fail = 0;
    free(dummy);

    printf("target_rmdir[%d] started\n", id);
    fflush(stdout);

    for (i = 0; i < LOOPS; i++) {
        ret = rmdir(pathnames[i % num_paths]);  
        if (ret < 0) {
            fail++;
        }
    }
    printf("target_rmdir[%d] finish %d loops - %d fail\n", id, LOOPS, fail);
    fflush(stdout);
    return NULL;
}

int main(int argc, char **argv) {
    int i;
    pthread_t *tids;
    int path_index = -1;
    char *local_pathnames = NULL;
    char *token;
    void*(*target_function)(void*) = NULL;
    int ret = EXIT_SUCCESS;

    if (argc < 9) {
        printf("%s cannot be open please pass %s %s\n", argv[0], argv[0], ARGS);
        exit(EXIT_FAILURE);
    }
    printf("launching: ");
    fflush(stdout);
    for (i = 0; i < argc; i++){
        printf("%s ",argv[i]);
    }
    printf("\n");
    fflush(stdout);

    for (i = 1; i < argc; i++) {
       	if (!strcmp("-target", argv[i])){
            if (!strcmp("mkdir", argv[i + 1])){
                target_function = target_mkdir;
            }
            if (!strcmp("open", argv[i + 1])){
                target_function = target_open;
            }
            if (!strcmp("rmdir", argv[i + 1])){
                target_function = target_rmdir;
            }
            if (!strcmp("unlink", argv[i + 1])){
                target_function = target_unlink;
            }
        }
        if (!strcmp("-threads", argv[i])) {
            threads = (int)strtol(argv[i + 1], NULL, 10);
            if (threads == 0) {
                printf("your arg [%s %s] and threads set up to=%d cannot init the program\n", argv[i], argv[i + 1], threads);
                exit(EXIT_FAILURE);
            }
        }
        if (!strcmp("-filepaths", argv[i])) {
            path_index = i + 1;
        }
         if (!strcmp("-num_paths", argv[i])) {
            num_paths = (int)strtol(argv[i + 1], NULL, 10);
            if (num_paths == 0) {
                printf("your arg [%s %s] and num_paths set up to=%d cannot init the program\n", argv[i], argv[i + 1], num_paths);
                exit(EXIT_FAILURE);
            }
        }
    }

    if (num_paths != 0 && path_index > 0){
        local_pathnames = malloc(sizeof(char) * PATH_MAX * num_paths);
        if (local_pathnames == NULL){
            perror("malloc: ");
            exit(EXIT_FAILURE);
        }
        pathnames = malloc(sizeof(char *) * num_paths);
        if (pathnames == NULL){
            perror("malloc: ");
            free(local_pathnames);
            exit(EXIT_FAILURE);
        }        
        strncpy(local_pathnames, argv[path_index], PATH_MAX * num_paths);

        token = strtok(local_pathnames, ";");
        i = 0;
        while(token != NULL){
            pathnames[i] = malloc(sizeof(char) * strlen(token));
            if (pathnames[i] == NULL) {
                perror("malloc: ");
                free(local_pathnames);
                free(pathnames);
                exit(EXIT_FAILURE);
            }

            strcpy(pathnames[i], token);
            printf("pathname[%d]: %s\n", i, pathnames[i]);
            i++;
            token = strtok(NULL, ";");
        }
        
        if (i != num_paths){
            printf("you passed num_paths=%d but actually=%d setting num_paths to -> %d\n", num_paths, i, i);
            num_paths = i;
            char **temp = malloc(sizeof(char *) * num_paths);
            if (temp == NULL){
                perror("malloc: ");
                ret = EXIT_FAILURE;
                goto exit_with_failure;
            }
            
            for (int j = 0; j < num_paths; j++) {
                temp[j] = malloc(sizeof(char) * (strlen(pathnames[j]) + 1)); 
                if (temp[j] == NULL) {
                    perror("malloc: ");
               
                    for (int k = 0; k < j; k++) {
                        free(temp[k]);
                    }
                    free(temp);
                    goto exit_with_failure;
                }
                strcpy(temp[j], pathnames[j]);
                free(pathnames[j]);
            }
            free(pathnames);
            pathnames = temp;
        }

        
    }
    if (pathnames == NULL || threads == 0 || target_function==NULL || num_paths==0) {
            printf("cannot init the program pathnames=%p, threads=%d, target_function=%p, num_pahts=%d\n",
            pathnames, threads, target_function, num_paths);
            ret = EXIT_FAILURE;
            goto exit_with_failure;
        }
    printf("target_function={%p}, #threads=%d, filepahts=%s, num_paths=%d\n", target_function, threads, argv[path_index], num_paths);
    tids = malloc(sizeof(pthread_t )* threads);
    if (tids == NULL){
        perror("malloc: ");
        ret = EXIT_FAILURE;
        goto exit_with_failure;
    }
    for (i = 0; i < threads; i++){
        int *dummy = malloc(sizeof(int));
        *dummy = i;
        if (pthread_create(&tids[i], NULL, target_function, dummy)){
            perror("pthread_create: ");
            free(dummy);
            ret = EXIT_FAILURE;
            goto exit_with_failure;
        }
    }

    for (i = 0; i < threads; i++){
        pthread_join(tids[i], NULL);
    }

exit_with_failure:
    if (pathnames != NULL){
        for (int j = 0; j < num_paths; j++) {
            free(pathnames[j]);
        }
        free(pathnames);
    }
    if (local_pathnames != NULL){
        free(local_pathnames);
    }

    
    
    
    return ret;
}
