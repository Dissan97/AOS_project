#pragma once
#include <linux/fs.h>


int generate_salt(char *salt);
int salt_text(char *salt, char *plaintext, char **salted_plaintext);
int hash_plaintext(char *salt, char *ciphertext, char *plaintext);
