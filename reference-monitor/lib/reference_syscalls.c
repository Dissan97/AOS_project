
#define EXPORT_SYMTAB

#include <linux/printk.h>
#include <linux/version.h>
#include <asm-generic/errno-base.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/cred.h>
#include <linux/uidgid.h>
#include <linux/atomic.h>
#include <linux/rwlock.h>
#include "include/reference_types.h"
#include "include/reference_hooks.h"
#include "include/reference_syscalls.h"
#include "include/reference_rcu_restrict_list.h"
#include "include/reference_hash.h"

#define REFERENCE_SYSCALLS "REFERENCE SYSCALLS"
/**
 * @param state: int it allow to change_the current state of the reference monitor
 * @param the_pwd: char* if is correc than the function is committed
 * @return: int, 0 if is al good -1 othrewise
 */



//reader
int check_pwd(char *try_password)
{
        char tmp[DIGEST_SIZE];
        int ret = 0;

        ret = hash_plaintext(salt, tmp, try_password);


        if (ret) {
                pr_err("%s[%s]: hash_plaintext_error %d\n", MODNAME, __func__, ret);
                return ret;
        }

        read_lock(&(password_lock));
        ret = strncmp(tmp, pwd, (DIGEST_SIZE));
        read_unlock(&(password_lock));
        return ret;
}


int do_change_state(char * the_pwd, int the_state)
{
        
        int i;
        unsigned long ret;
        
        const struct cred *cred;
#ifdef NO_LOCK
        long old_state;

#endif
        cred = current_cred();

        if (likely(cred->euid.val != admin)){
                return -EACCES;
        }

        if (the_pwd == NULL || !(the_state & ON || the_state & OFF || the_state & REC_ON || the_state & REC_OFF)){
                return -EINVAL;
        }
        

        if ((ret = check_pwd(the_pwd))){
                pr_info("%s[%s]: check pwd failed from_user %s\n", MODNAME, __func__, the_pwd);
                return -EINVAL;
        }; 
#ifdef NO_LOCK
        old_state = atomic_long_read(&atomic_current_state);
        AUDIT
        pr_info("%s[%s]: no_lock requested previous state: %ld\n", MODNAME, __func__, old_state);

        
        if (atomic_long_cmpxchg(&atomic_current_state, old_state, the_state) == old_state) {
                if (old_state == the_state) {
                        return -ECANCELED;  
                }

                current_state = the_state;

                if ((the_state & REC_ON) || (the_state & ON)) {
                        for (i = 0; i < HOOKS_SIZE; i++) {
                                enable_kprobe(&probes[i]);
                        }
                } else {
                        for (i = 0; i < HOOKS_SIZE; i++) {
                                disable_kprobe(&probes[i]);
                        }
                        restore_black_list_entries();
                }
                
                pr_info("%s[%s]: requested new state: %d\n", MODNAME, __func__, the_state);
                return 0;
        }
        return -ECANCELED;
#else         
        write_lock(&(state_lock));
        AUDIT
        pr_info("%s[%s]: requested previous state: %ld\n", MODNAME, __func__, current_state);
        if (current_state == the_state){
                write_unlock(&(state_lock));
                return -ECANCELED;
        }

        current_state = the_state;

        if ((current_state & REC_ON) || (current_state & ON)){
                for(i=0;i<HOOKS_SIZE;i++){
                        enable_kprobe(&probes[i]);
                }
        }else {
                for(i=0;i<HOOKS_SIZE;i++){
                        disable_kprobe(&probes[i]);
                }
                restore_black_list_entries();
        }
        AUDIT
        pr_info("%s[%s]: requested new state: %d\n", MODNAME, __func__, the_state);
        write_unlock(&(state_lock));
#endif
        return 0;
}

/**
 * @param path: char* defined pathname that has to be added to restricted list or 
 *              deleted from restricted list
 * @param op: int ADD_PATH to add REMOVE_PATH to remove
 * @return : int ret = 0 all good ret < 0 reporting typeof error
 */

int do_change_path(char *the_pwd, char *the_path, int op)
{

        int ret = 0;
        long old_state;
        
        
        const struct cred *cred;
        
        cred = current_cred();

        if (likely(cred->euid.val != admin)){
                pr_warn("%s[%s]: this system call must be called from security admin\n", MODNAME, __func__);
                return -EACCES;
        }

        if ((the_path == NULL) || !((op & ADD_PATH) || (op & REMOVE_PATH)) ||
                (op & REMOVE_PATH & ADD_PATH)){
                return -EINVAL;
        }


        if (forbitten_path(the_path) < 0){
                pr_warn("%s[%s]: required forbitten path=%s\n", MODNAME, __func__, the_path);
                return -EINVAL;
        }
        if ((ret = check_pwd(the_pwd)) < 0){
                return -EINVAL;
        }; 

#ifdef NO_LOCK
    old_state = atomic_long_read(&atomic_current_state);
#else        

        read_lock(&(state_lock));
        old_state = current_state;
        read_unlock(&(state_lock));
#endif
        if (!((old_state &  REC_OFF) || (old_state & REC_ON))){
                if (!((op & REC_ON) || (op & REC_OFF))){
                        return  -ECANCELED;
                }
                
                if ((ret = do_change_state(the_pwd, op))){
                        return ret;
                }
        }
        
        if (op & ADD_PATH){
                ret = add_path(the_path);
                AUDIT {
                        if (!ret){
                                pr_info("%s[%s]: %s added successfully\n", MODNAME, __func__, the_path);
                        }
                }
        }
        if (op & REMOVE_PATH){
                ret = del_path(the_path);
                AUDIT {
                        if (!ret){
                                pr_info("%s[%s]: %s deleted successfully\n", MODNAME, __func__, the_path);
                        }
                }
        }
        
        return ret;
}

// writer
int do_change_password(char *old_password, char *new_password)
{
        char tmp[DIGEST_SIZE];
        int ret = 0;
        
        if (strlen(new_password) <=8){
                pr_err("%s: password to short\n", MODNAME);
                return -EINVAL;
        }

        if (!strcmp(old_password, new_password)){
                pr_err("%s[%s]: same old and new password\n", MODNAME, __func__);
                return -EINVAL;
        }

        ret = check_pwd(old_password);
        if (ret){
                pr_err("%s[%s]: cannot change the password %d\n", MODNAME, __func__, ret);
                return -EINVAL;
        }
        //check gone
        ret = hash_plaintext(salt, tmp, new_password);

        if (ret){
                return -EINVAL;
        }

        write_lock(&(password_lock));

        memset(pwd, 0,(DIGEST_SIZE));
        strncpy(pwd, tmp, (DIGEST_SIZE));
        write_unlock(&password_lock);

        return ret;
}