#pragma once
#include <linux/atomic.h>
#include "reference_rcu_restrict_list.h"

#include <linux/atomic.h>

extern struct list_head restrict_path_list;
#ifdef NO_LOCK
extern atomic_long_t atomic_current_state;
#endif

/**
 * system call implementation to change the state of reference monitor {ON: 0x01 | OFF: 0x02 | REC_ON: 0x04 | REC_OFF: 0x08}
 * @param password: the password to pass to the reference monitor to activate functionality
 * @param state: the new state
 * @return int: if is correct return 0 else return error < 0
 * @error: 
 * EPERM: not root user attempting to change state
 * EACCESS: password passed
 * EINVAL: state is not {ON: 0x01 | OFF: 0x02 | REC_ON: 0x04 | REC_OFF: 0x08}
 * ECANCELED: passing the previoius state to the reference monitor
 * 
 */
int do_change_state(char*password, int state);


/**
 * system call implementation to add or remove path from reference restrict path list
 * @param password: the password to pass to the reference monitor to activate functionality
 * @param path: the path that have to be added or removed depending on parameter
 * @param op: operation can be ADD_PATH 0x10 REMOVE_PATH 0x20 i addition can be passed 
 * ADD_PATH | STATES or REMOVE_PATH | STATES this will call in addition do_change_state
 * STATES: {ON: 0x01 | OFF: 0x02 | REC_ON: 0x04 | REC_OFF: 0x08}
 * @return int: if is correct return 0 else return error < 0
 * @error: 
 * EPERM: not root user attempting to change state
 * EACCESS:  bad password passed
 * EINAVL: not passing ADD_PATH | REMOVE_PATH passing nul path or forbitten_path containng 
 * {/proc, /sys ,/run, /usr, /var, /bin} on the top of the path example not allowed /run/some_folder/some_file
 * /proc/some_folder
 * ECANCELED: if actual state is not in REC_ON | REC_OFF and not passing ADD_PATH | {REC_ON, REC_OFF}
 *  or REMOVE_PATH | {REC_ON, REC_OFF}
 * if passing  ADD_PATH | STATES or REMOVE_PATH | STATES system can return change state failure
 */

int do_change_path(char *password,char *path, int op); 

/**
 * system call implementation to change reference monitor password
 * @param: old_password the previous password of reference monitor
 * @param: new_password of the reference monitor
 * @return int: if is correct return 0 else return error < 0
 * @error: 
 * EPERM: not root user attempting to change state
 * EINVAL: password len <= 8 or old_password == new_passord or bad_old password or null password
 * ENOMEM: if hash_plaintext internal function fail
 */
int do_change_password(char *old_password, char *new_password);
