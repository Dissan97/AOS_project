#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "lib/include/mock.h"
#include "lib/include/mock_list.h"
#include "lib/include/ref_syscall.h"

int all_state[] = {ON, OFF, REC_ON, REC_OFF};
int bad_state[] = {
    0,  ON | OFF, REC_OFF | REC_ON, ON | OFF | REC_ON | REC_OFF, 0x40,
    -1, -1000};
int current_state;
// before test

struct sys_function_pointer {
    int (*change_password_func)(const char *old_password,
                                const char *new_password);
    int (*change_state_func)(const char *password, int state);
    int (*change_path_func)(const char *password, const char *the_path,
                            int add_or_remove);
};

int get_state_from_sys()
{
    char buff[16];
    int dummy;
    int read_bytes;
    FILE *current_state_pseudo_file;

    current_state_pseudo_file = fopen(
        "/sys/module/the_reference_monitor/parameters/current_state", "r");
    if (current_state_pseudo_file == NULL) {
        perror("cannot ope sys");
        return -1;
    }

    read_bytes =
        fread(buff, sizeof(char), sizeof(buff), current_state_pseudo_file);
    if (read_bytes < 0) {
        perror("some problem occured");
        return -1;
    }
    buff[read_bytes - 1] = 0;
    fclose(current_state_pseudo_file);
    return atoi(buff);
}

void test_change_state(char *good_password, char *bad_password,
                       struct sys_function_pointer syscall_function_pointer)
{
    int i = 0;
    int read_state;
    seteuid(1000);
    // good argment but bad user
    int ret;
    assert((ret = syscall_function_pointer.change_state_func(good_password,
                                                             0x1)) < 0);
    perror("good password bad euid expected EPERM");
    assert(errno == EPERM);
    seteuid(0);
    // bad password good state
    assert(syscall_function_pointer.change_state_func(bad_password, 0x1) < 0);
    perror("bad password bad good state expected EACCESS");
    assert(errno == EACCES);
    // bad password bad state
    for (i = 0; bad_state[i] != -1000; i++) {
        printf("testing bad_state=%d\n", bad_state[i]);
        fflush(stdout);
        assert(syscall_function_pointer.change_state_func(bad_password,
                                                          bad_state[i]) < 0);
        perror("bad password bad bad_state expected EINVAL");
        assert(errno == EINVAL);
        perror("good password bad_state expected EINVAL");
        assert(syscall_function_pointer.change_state_func(good_password,
                                                          bad_state[i]) < 0);
        assert(errno == EINVAL);
    }

    i = 0;
    // passing current state
    while (1) {
        if (current_state == all_state[i]) {
            break;
        }
        i++;
    }

    assert(syscall_function_pointer.change_state_func(good_password,
                                                      all_state[i]) < 0);
    perror("good password same state expected ECANCELED");
    assert(errno == ECANCELED);

    // checking the state transition
    for (i = 0; i < 4; i++) {
        if (current_state != all_state[i]) {
            assert(syscall_function_pointer.change_state_func(
                       good_password, all_state[i]) == 0);
            read_state = get_state_from_sys();
            assert(read_state == all_state[i]);
            printf("{Current state=%d; Expected state=%d}\n", read_state,
                   all_state[i]);
            fflush(stdout);
            current_state = all_state[i];
        }
    }
}

void test_change_path(char *good_password, char *bad_password,
                      node_list_t file_list,
                      struct sys_function_pointer syscall_function_pointer)
{
    int ret;
    int expected;
    node_list_t added_list;
    node_list_t not_added_list;
    init_list(&added_list);
    init_list(&not_added_list);
    const char *forbitten_paths[8] = {"/proc", "/sys", "/run",
                                      "/var",  "/tmp", NULL};
    seteuid(1000);

    ret = change_path(good_password, file_list.head->path, ADD_PATH);
    perror("change path invalid euid expected EPERM");
    assert(ret < 0 && errno == EPERM);

    seteuid(0);
    printf("good_password %s\n", good_password);
    fflush(stdout);
    ret = syscall_function_pointer.change_path_func(good_password, NULL,
                                                    ADD_PATH);
    perror("change path invalid path=NULL expected EINVAL");

    assert(ret < 0 && errno == EINVAL);
    ret = syscall_function_pointer.change_path_func(
        good_password, file_list.head->path, (ADD_PATH + 1000));
    perror("change path invalid op=ADD_PATH+1000 expected EINVAL");
    assert(ret < 0 && errno == EINVAL);

    ret = syscall_function_pointer.change_path_func(
        bad_password, file_list.head->path, (ADD_PATH & REMOVE_PATH));
    perror("change path invalid op=ADD_PATH&REMOVE_PATH expected EINVAL");
    assert(ret < 0 && errno == EINVAL);

    ret = syscall_function_pointer.change_path_func(
        bad_password, file_list.head->path, (ADD_PATH & REMOVE_PATH));
    perror("change path invalid op=password expected EINVAL");
    assert(ret < 0 && errno == EINVAL);

    // changing configuration to rec_off
    ret = syscall_function_pointer.change_state_func(good_password, REC_OFF);
    if (ret) {
        perror("cannot call sys_change_state");
        return;
    }
    current_state = get_state_from_sys();
    if (current_state < 0) {
        return;
    }

    for (int i = 0; forbitten_paths[i] != NULL; i++) {
        ret = syscall_function_pointer.change_path_func(
            good_password, forbitten_paths[i], ADD_PATH);
        printf("passing %s not allowed ", forbitten_paths[i]);
        fflush(stdout);
        perror("expected EINVAL");
        assert(ret < 0 && errno == EINVAL);
    }

    ret = syscall_function_pointer.change_path_func(
        bad_password, file_list.head->path, ADD_PATH);
    perror("change path invalid passoword expected EACCESS");
    assert(ret < 0 && errno == EACCES);

    current_state = get_state_from_sys();

    if (((current_state & REC_OFF) ||
         (current_state & REC_ON))) { // reconfiguration mode is on
        ret = syscall_function_pointer.change_state_func(good_password, OFF);
        if (ret) {
            perror("cannot change state to off");
            exit(EXIT_FAILURE);
        }
    }

    // test -ECANCELED

    ret = syscall_function_pointer.change_path_func(
        good_password, file_list.head->path, ADD_PATH);
    perror("change path invalid op=ADD_PATH and state != REC_ON && state "
           "!= REC_ON REC_OFF expected ECANCELED");
    assert(ret < 0 && errno == ECANCELED);

    ret = syscall_function_pointer.change_path_func(
        good_password, file_list.head->path, REMOVE_PATH);
    perror("change path invalid op=REMOVE_PATH and state != REC_ON && "
           "state != REC_ON REC_OFF expected ECANCELED");
    assert(ret < 0 && errno == ECANCELED);

    // test good

    current_state = get_state_from_sys();
    if (!(current_state & REC_OFF & REC_ON)) { // reconfiguration mode is off
        ret =
            syscall_function_pointer.change_state_func(good_password, REC_OFF);
        if (ret) {
            perror("cannot change state to rec_off");
            exit(EXIT_FAILURE);
        }
    }

    ret = syscall_function_pointer.change_path_func(
        good_password, file_list.head->path, ADD_PATH);
    printf("change path op=ADD_PATH and state = REC_OFF\n");
    fflush(stdout);
    assert(ret == 0);

    ret = syscall_function_pointer.change_path_func(
        good_password, file_list.head->path, ADD_PATH);
    printf("change path op=REMOVE_PATH and path already in list: ");
    fflush(stdout);
    perror("expected EEXIST");
    assert(ret < 0 && errno == EEXIST);

    ret = syscall_function_pointer.change_path_func(
        good_password, file_list.head->path, REMOVE_PATH);
    printf("change path op=REMOVE_PATH and state = REC_OFF\n");
    fflush(stdout);
    assert(ret == 0);

    // no testing removing not existing path
    ret = syscall_function_pointer.change_path_func(
        good_password, file_list.head->path, REMOVE_PATH);
    printf("change path op=REMOVE_PATH and path not in list: ");
    fflush(stdout);
    perror("expected ENOENT");
    assert(ret < 0 && errno == ENOENT);
}

int main(int argc, char **argv)
{

    password_t password;
    change_path_mock_t cngpth_mock;
    node_list_t list;
    cngpth_mock.list = list;
    char *conf_file;
    cngpth_mock.conf_file = conf_file;
    size_t len;
    int before_state;
    struct sys_function_pointer syscall_function_pointer;

    if (argc < 5) {
        printf("Launch program with -pwd ReferenceMonitorPassword "
               "-conf conf_file\n");
        exit(EXIT_FAILURE);
    }

    printf("running change state tests\n");
    fflush(stdout);
    mock_password(argc, argv, &password);
    current_state = mock_get_state();
    before_state = current_state;
    fflush(stdout);
    syscall_function_pointer.change_password_func = change_password;
    syscall_function_pointer.change_state_func = change_state;
    syscall_function_pointer.change_path_func = change_path;
    printf("TESTING_CLASSIC SYSTEM CALL\n");
    fflush(stdout);

    test_change_state(password.good_password, password.bad_password,
                      syscall_function_pointer);
    printf("change state tests passed\n");
    fflush(stdout);
    printf("current_state=%d - restoring before_state=%d\n", current_state,
           before_state);
    fflush(stdout);
    if (change_state(password.good_password, before_state) < 0 &&
        errno != ECANCELED) {
        perror("unable to restore previous state");
        exit(EXIT_FAILURE);
    }
    perror("change state problem");
    printf("get current state from sys/parameters current_state=%d\n",
           get_state_from_sys());

    current_state = before_state;
    printf("running change path tests\n");
    fflush(stdout);
    init_list(&cngpth_mock.list);
    mock_gen_test_file_env(argc, argv, &cngpth_mock);
    test_change_path(password.good_password, password.bad_password,
                     cngpth_mock.list, syscall_function_pointer);
    mock_remove_recursive("resources", cngpth_mock.conf_file);
    printf("change path test passed\n");
    printf("get current state from sys/parameters current_state=%d "
           "restoring before_state=%d\n",
           get_state_from_sys(), before_state);
    fflush(stdout);
    if (change_state(password.good_password, before_state) < 0 &&
        errno != ECANCELED) {
        perror("unable to restore previous state");
        exit(EXIT_FAILURE);
    }

    syscall_function_pointer.change_password_func = path_change_password;
    syscall_function_pointer.change_state_func = path_change_state;
    syscall_function_pointer.change_path_func = path_change_path;

    printf("TESTING_PATH_BASED SYSTEM CALL\n");
    fflush(stdout);

    test_change_state(password.good_password, password.bad_password,
                      syscall_function_pointer);
    printf("change state tests passed\n");
    fflush(stdout);
    printf("current_state=%d - restoring before_state=%d\n", current_state,
           before_state);
    fflush(stdout);
    if (change_state(password.good_password, before_state) < 0 &&
        errno != ECANCELED) {
        perror("unable to restore previous state");
        exit(EXIT_FAILURE);
    }
    perror("change state problem");
    printf("get current state from sys/parameters current_state=%d\n",
           get_state_from_sys());

    current_state = before_state;
    printf("running change path tests\n");
    fflush(stdout);
    init_list(&cngpth_mock.list);
    mock_gen_test_file_env(argc, argv, &cngpth_mock);
    test_change_path(password.good_password, password.bad_password,
                     cngpth_mock.list, syscall_function_pointer);
    mock_remove_recursive("resources", cngpth_mock.conf_file);
    printf("change path test passed\n");
    current_state = get_state_from_sys();
    if (current_state < 0) {
        printf("something wrong in calling sys/parameters\n");
        fflush(stdout);
        exit(EXIT_FAILURE);
    }
    printf("get current state from sys/parameters current_state=%d "
           "restoring before_state=%d\n",
           get_state_from_sys(), before_state);
    fflush(stdout);
    if (change_state(password.good_password, before_state) < 0 &&
        errno != ECANCELED) {
        perror("unable to restore previous state");
        exit(EXIT_FAILURE);
    }

    printf("all tests passed\n");
    fflush(stdout);
}