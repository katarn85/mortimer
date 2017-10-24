#ifndef _DA_VERIFY_HEADER_
#define _DA_VERIFY_HEADER_

#include <stdint.h>
#include "sha256.h"
#include "sha1.h"

#ifndef LOGD
	#ifdef DEBUG
		#define LOGD(fmt, args...)	printf (" [%s:%04d] " fmt , __FUNCTION__, __LINE__, ##args)
	#else
		#define LOGD(fmt, args...)
	#endif
#endif

#define UINT64_MULT32(v, multby)  (((uint64_t)(v)) * ((uint32_t)(multby)))

// This is the minimum/maximum key size we support, in bits
#define RSA_MIN_KEY_BITS	2048
#define RSA_MAX_KEY_BITS	2048

// This is the maximum signature length that we support, in bits
#define RSA_MAX_SIG_BITS	2048
#define RSA2048_BYTES		(RSA_MAX_SIG_BITS / 8)

#define PKEY_PARAMETER_SIZE  (1 + 1 + 64 + 64)  // bits, n0_inv, modulus, r_squared

#define FILE_READ_BUFFER 	 4096 // 4K

/**
 * struct rsa_public_key - holder for a public key
 *
 * An RSA public key consists of a modulus (typically called N), the inverse
 * and R^2, where R is 2^(# key bits).
 */
struct rsa_public_key {
	uint len;			// Length of modulus[] in number of uint32_t
	uint32_t n0inv;		// -1 / modulus[0] mod 2^32
	uint32_t *modulus;	// modulus as little endian array
	uint32_t *rr;		// R^2 as little endian array
};

int rsa_verify_custom(char *target_file, char *signature_file, char *pkey_param_file, char is_sha1);
static int 	rsa_verify_with_key(const struct rsa_public_key *key, const uint8_t *sig, const uint32_t sig_len, const uint8_t *hash);
static int 	rsa_calculate_hash(char *target_file, uint8_t **hashp, char is_sha1);
static void subtract_modulus(const struct rsa_public_key *key, uint32_t num[]);
static int 	greater_equal_modulus(const struct rsa_public_key *key, uint32_t num[]);
static void montgomery_mul_add_step(const struct rsa_public_key *key, uint32_t result[], const uint32_t a, const uint32_t b[]);
static int 	pow_mod(const struct rsa_public_key *key, uint32_t *inout);
static void rsa_convert_big_endian(uint32_t *dst, const uint32_t *src, int len);

#endif /* _DA_RSA_HEADER_ */
