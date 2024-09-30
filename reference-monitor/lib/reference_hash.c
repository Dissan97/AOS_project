#include <asm-generic/errno-base.h>
#include <asm-generic/errno.h>
#define EXPORT_SYMTAB
#include "include/reference_hash.h"
#include "include/reference_types.h"
#include <crypto/internal/hash.h>
#include <linux/module.h>
#include <linux/random.h> // For get_random_bytes()

int generate_salt(char *salt)
{
        if (salt == NULL) {
                salt = kmalloc(SALT_LENGTH + 1, GFP_KERNEL);
                if (!salt)
                        return -ENOMEM;
        }

        get_random_bytes(salt, SALT_LENGTH);
        salt[SALT_LENGTH + 1] = '\0'; // Null terminate the salt
        return 0;
}

int salt_text(char *salt, char *plaintext, char **salted_plaintext)
{

        if (!salt || !plaintext) {
                return -EINVAL;
        }

        *salted_plaintext =
            kmalloc(strlen(plaintext) + SALT_LENGTH + 1, GFP_KERNEL);

        if (!*salted_plaintext) {
                return -ENOMEM;
        }
        // Concatenate salt with plaintext
        strncpy(*salted_plaintext, plaintext, strlen(plaintext));
        strncat(*salted_plaintext, salt, SALT_LENGTH);
        return 0;
}

int hash_plaintext(char *salt, char *ciphertext, char *plaintext)
{
        char *salted_plaintext = NULL;
        char hash_sha512[SHA512_LENGTH];
        struct crypto_shash *sha512;
        struct shash_desc *shash;
        int i;

        if (salt_text(salt, plaintext, &salted_plaintext) < 0 ||
            ciphertext == NULL) {
                return -EINVAL;
        }

        sha512 = crypto_alloc_shash("sha512", 0, 0);
        if (IS_ERR(sha512)) {
                pr_err("%s[%s]: Failed to allocate sha256 algorithm, enable "
                       "CONFIG_CRYPTO_SHA256 and try again.\n",
                       MODNAME, __func__);
                return -1;
        }

        shash =
            kmalloc(sizeof(struct shash_desc) + crypto_shash_descsize(sha512),
                    GFP_KERNEL);
        if (!shash)
                return -ENOMEM;

        shash->tfm = sha512;

        if (crypto_shash_init(shash))
                return -1;

        if (crypto_shash_update(shash, salted_plaintext,
                                strlen(salted_plaintext)))
                return -1;

        if (crypto_shash_final(shash, hash_sha512))
                return -1;

        kfree(salted_plaintext);
        kfree(shash);
        crypto_free_shash(sha512);

        memset(ciphertext, 0, SHA512_LENGTH * 2 + 1);

        for (i = 0; i < SHA512_LENGTH; i++) {
                sprintf(&ciphertext[i * 2], "%02x",
                        (unsigned char)hash_sha512[i]);
        }

        return 0;
}
