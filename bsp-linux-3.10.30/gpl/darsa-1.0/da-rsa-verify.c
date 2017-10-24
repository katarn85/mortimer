/* da-rsa-verify.c  -- RSA Verifying Routines.
 *
 * Copyright (c) 2013, Google Inc.
 * Copyright (c) 2014, Samsung Electronics.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 *
 * Modified by Elmurod Talipov and Seong-Yong Kang.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <stdint.h>
#include "byteorder.h"
#include "da-rsa-verify.h"

// RSA2048 - SHA256 Padding
const uint8_t padding_sha256_rsa2048[RSA2048_BYTES - SHA256_SUM_LEN] = {
	0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x30, 0x31, 0x30,
	0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x05,
	0x00, 0x04, 0x20
};

// RSA2048 - SHA1 Padding
const uint8_t padding_sha1_rsa2048[RSA2048_BYTES - SHA1_SUM_LEN] = {
	0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff,	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,	0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff,	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,	0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,	0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff,	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,	0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff,	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,	0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00,
	0x30, 0x21, 0x30, 0x09, 0x06, 0x05, 0x2b, 0x0e, 0x03, 0x02, 0x1a, 0x05, 0x00,
	0x04, 0x14
};

const uint8_t *g_padding;
int g_pad_len;

#ifndef RSA_FULL_INCLUDE
int main(int argc, char **argv)
{
	char is_sha1 = 0;

	if (argc<4)
	{
		printf("Usage : %s <image file> <image signature file> <public key parameter file>\n", argv[0]);
		return -1;
	}

	// we want to use sha1 hashing instead of sha256
	if ((argc > 4) && (strncmp(argv[3], "sha1", 4) == 0))
	{
		is_sha1 = 0x01;
		// Determine padding to use depending on the signature type.
		g_padding = padding_sha1_rsa2048;
		g_pad_len = RSA2048_BYTES - SHA1_SUM_LEN;

	}
	else
	{
		is_sha1 = 0x00;
		// Determine padding to use depending on the signature type.
		g_padding = padding_sha256_rsa2048;
		g_pad_len = RSA2048_BYTES - SHA256_SUM_LEN;
	}

	return rsa_verify_custom(argv[1], argv[2], argv[3], is_sha1);

}
#endif

int rsa_verify_custom(char *target_file, char *signature_file, char *pkey_param_file, char is_sha1)
{
	int ret, i;
	FILE *sfp, *pkfp;
	unsigned char *signature = NULL;
	unsigned int  signature_length = 0;
	struct rsa_public_key key;
	key.modulus = NULL;
	key.rr = NULL;

	uint8_t *hash = NULL;
	uint32_t pkey_parameters[PKEY_PARAMETER_SIZE] = {0, };

	// read signature
	sfp = fopen((const char*)signature_file, "rb");
	if(sfp == NULL)
	{
		LOGD("Failed to open signature file %s\n", signature_file);
		ret = -1;
		goto finish;
	}

	//check size of input file
	fseek(sfp, 0, SEEK_END);
	signature_length = ftell(sfp);
	rewind(sfp);

	if (signature_length > RSA2048_BYTES)
	{
		LOGD("Signature file too big: %d \n", signature_length);
		ret = -1;
		fclose(sfp);
		goto finish;
	}

	//memory allocation for signature data
	signature = (unsigned char*)malloc(signature_length + 1);
	memset(signature, 0, signature_length + 1);

	//read signature from file
	ret = fread(signature, 1, signature_length, sfp);
	if(ret != signature_length)
	{
		LOGD("Signature file read error \n");
		ret = -1;
		fclose(sfp);
		goto finish;
	}
	fclose(sfp);
	// end of reading signature

	pkfp = 	fopen((const char*)pkey_param_file, "rb");
	if(pkfp == NULL)
	{
		LOGD("Failed to open public key parameter file: %s \n", pkey_param_file);
		ret = -1;
		goto finish;
	}

	i = 0;
	while (i < PKEY_PARAMETER_SIZE)
	{
		ret = fscanf(pkfp, "%x", &(pkey_parameters[i]));
		if (ret < 0) break;
		i++;
	}
	fclose(pkfp);

	if (PKEY_PARAMETER_SIZE != i)
	{
		LOGD("Could not read all public key parameters: %d \n", i);
		ret = -1;
		goto finish;
	}

	key.len = be32_to_cpu(pkey_parameters[0]);

	// Sanity check for stack size
	if (key.len > RSA_MAX_KEY_BITS || key.len < RSA_MIN_KEY_BITS) {
		LOGD("RSA key bits %u outside allowed range %d..%d \n",  key.len, RSA_MIN_KEY_BITS, RSA_MAX_KEY_BITS);
		ret = -1;
		goto finish;
	}

	LOGD("Key length: %u \n", key.len);
	key.len /= sizeof(uint32_t) * 8;

	key.n0inv = be32_to_cpu(pkey_parameters[1]);
	LOGD("n0 inverse: 0x%08x \n", key.n0inv);

	key.modulus = (uint32_t *)malloc(key.len*sizeof(uint32_t));
	if (!key.modulus)
	{
		ret = -1;
		goto finish;
	}

	key.rr = (uint32_t *)malloc(key.len*sizeof(uint32_t));
	if (!key.rr)
	{
		ret = -1;
		goto finish;
	}

	rsa_convert_big_endian(key.modulus, pkey_parameters + 2 , key.len);
	rsa_convert_big_endian(key.rr, pkey_parameters + 2 + 64, key.len);

	LOGD("modulus[] : { ");
	for (i = 0; i < key.len ; i++)
	{
		LOGD("0x%08x \n", key.modulus[i]);
	}
	LOGD("} \n");

	LOGD("r-squared[] : { ");
	for (i = 0; i < key.len ; i++)
	{
		LOGD("0x%08x \n", key.rr [i]);
	}
	LOGD("} \n");

	LOGD("Start verifying ... \n");

	// calculate hash
	ret = rsa_calculate_hash(target_file, &hash, is_sha1);
	if (ret != 0)
	{
		ret = -1;
		goto finish;
	}

	ret = rsa_verify_with_key(&key, signature, signature_length, hash);

finish:

	if (ret == 0)
	{
		printf("Verification success\n\r");
	}
	else
	{
		printf("Verification failed\n\r");
	}

	free(hash);
	free(signature);
	free(key.modulus);
	free(key.rr);

	return ret;
}

static int rsa_verify_with_key(const struct rsa_public_key *key, const uint8_t *sig,
		const uint32_t sig_len, const uint8_t *hash)
{
	int ret;

	if (!key || !sig || !hash)
		return -1;

	if (sig_len != (key->len * sizeof(uint32_t))) {
		LOGD("Signature is of incorrect length %d", sig_len);
		return -1;
	}

	// Sanity check for stack size
	if (sig_len > RSA_MAX_SIG_BITS / 8) {
		LOGD("Signature length %u exceeds maximum %d", sig_len, RSA_MAX_SIG_BITS / 8);
		return -1;
	}

	uint32_t buf[sig_len / sizeof(uint32_t)];

	memcpy(buf, sig, sig_len);

	ret = pow_mod(key, buf);
	if (ret)
		return ret;

	// Check pkcs1.5 padding bytes.
	if (memcmp(buf, g_padding, g_pad_len)) {
        LOGD("Padding check failed!");
        /*
        int i;
        uint8_t *bufx = (uint8_t *)buf;
        for (i=0;i<g_pad_len;i++) {
            /printf("0x%02x = 0x%02x | ", bufx[i], g_padding[i]);
            if (( i + 1) % 8 == 0) printf("\n");
        }
        */
		return -1;
	}

	// Check hash.
	if (memcmp((uint8_t *)buf + g_pad_len, hash, sig_len - g_pad_len)) {
		LOGD("Hash check failed!");
		return -1;
	}

	return 0;
}

/**
 * rsa_convert_big_endian() - converts big endian to little endian
 *
 * @dst:	destination: little-endian word array,
 * @src:	source : big-endinan word array
 * @len:	size of source array.
 */

static void rsa_convert_big_endian(uint32_t *dst, const uint32_t *src, int len)
{
	int i;

	for (i = 0; i < len; i++)
		dst[i] = be32_to_cpu(src[len - 1 - i]);
}

/**
 * rsa_convert_big_endian() - converts big endian to little endian
 *
 * @target_file:	target file to calculate hash.
 * @hashp:			pointer to hash result, must be freed by caller.
 * @is_sha1:		if sha1 algorithm used.
 */
static int rsa_calculate_hash(char *target_file, uint8_t **hashp, char is_sha1) {

	FILE *ifp;
	uint8_t *hash = NULL;

	int file_size = 0, i = 0, ret = 0;
	unsigned char fbuff[FILE_READ_BUFFER] = {0, };

	//open input_file (image file) to sign
	ifp = fopen((const char*)target_file, "rb");
	if(ifp == NULL)
	{
		LOGD("Failed to open target file %s \n", target_file);
		*hashp = NULL;
		return -1;
	}

	//check file size
	fseek(ifp, 0, SEEK_END);
	file_size = ftell(ifp);
	rewind(ifp);

	if (is_sha1 == 1)
	{
		sha1_context ctx;
		sha1_starts(&ctx);
		hash = (uint8_t *) malloc(SHA1_SUM_LEN);
		memset(hash, 0, SHA1_SUM_LEN);

		for(i = 0; i < file_size; )
		{
			ret = fread(fbuff, 1, FILE_READ_BUFFER, ifp);
			if (ret > 0)
			{
				sha1_update(&ctx,fbuff, ret);
			}
			else
			{
				LOGD("Failed to read target file to compute sha1\n");
				fclose(ifp);
				*hashp = NULL;
				return -1;
			}
			i += ret;
		}

		sha1_finish(&ctx, hash);

	}
	else
	{
		sha256_context ctx;
		sha256_starts(&ctx);

		hash = (uint8_t *) malloc(SHA256_SUM_LEN);
		memset(hash, 0, SHA1_SUM_LEN);

		for(i = 0; i < file_size; )
		{
			ret = fread(fbuff, 1, FILE_READ_BUFFER, ifp);
			if (ret > 0)
			{
				sha256_update(&ctx, fbuff, ret);
			}
			else
			{
				LOGD("Failed to read target file to compute sha256\n");;
				fclose(ifp);
				*hashp = NULL;
				return -1;
			}
			i += ret;
		}

		sha256_finish(&ctx, hash);

	}

	*hashp = hash;

	fclose(ifp);

	return 0;
}

/**
 * subtract_modulus() - subtract modulus from the given value
 *
 * @key:	Key containing modulus to subtract
 * @num:	Number to subtract modulus from, as little endian word array
 */
static void subtract_modulus(const struct rsa_public_key *key, uint32_t num[])
{
	int64_t acc = 0;
	uint i;

	for (i = 0; i < key->len; i++) {
		acc += (uint64_t)num[i] - key->modulus[i];
		num[i] = (uint32_t)acc;
		acc >>= 32;
	}
}

/**
 * greater_equal_modulus() - check if a value is >= modulus
 *
 * @key:	Key containing modulus to check
 * @num:	Number to check against modulus, as little endian word array
 * @return 0 if num < modulus, 1 if num >= modulus
 */
static int greater_equal_modulus(const struct rsa_public_key *key, uint32_t num[])
{
	uint32_t i;

	for (i = key->len - 1; i >= 0; i--) {
		if (num[i] < key->modulus[i])
			return 0;
		if (num[i] > key->modulus[i])
			return 1;
	}

	return 1;  /* equal */
}

/**
 * montgomery_mul_add_step() - Perform montgomery multiply-add step
 *
 * Operation: montgomery result[] += a * b[] / n0inv % modulus
 *
 * @key:	RSA key
 * @result:	Place to put result, as little endian word array
 * @a:		Multiplier
 * @b:		Multiplicand, as little endian word array
 */
static void montgomery_mul_add_step(const struct rsa_public_key *key,
		uint32_t result[], const uint32_t a, const uint32_t b[])
{
	uint64_t acc_a, acc_b;
	uint32_t d0;
	uint i;

	acc_a = (uint64_t)a * b[0] + result[0];
	d0 = (uint32_t)acc_a * key->n0inv;
	acc_b = (uint64_t)d0 * key->modulus[0] + (uint32_t)acc_a;
	for (i = 1; i < key->len; i++) {
		acc_a = (acc_a >> 32) + (uint64_t)a * b[i] + result[i];
		acc_b = (acc_b >> 32) + (uint64_t)d0 * key->modulus[i] +
				(uint32_t)acc_a;
		result[i - 1] = (uint32_t)acc_b;
	}

	acc_a = (acc_a >> 32) + (acc_b >> 32);

	result[i - 1] = (uint32_t)acc_a;

	if (acc_a >> 32)
		subtract_modulus(key, result);
}

/**
 * montgomery_mul() - Perform montgomery mutitply
 *
 * Operation: montgomery result[] = a[] * b[] / n0inv % modulus
 *
 * @key:	RSA key
 * @result:	Place to put result, as little endian word array
 * @a:		Multiplier, as little endian word array
 * @b:		Multiplicand, as little endian word array
 */
static void montgomery_mul(const struct rsa_public_key *key,
		uint32_t result[], uint32_t a[], const uint32_t b[])
{
	uint i;

	for (i = 0; i < key->len; ++i)
		result[i] = 0;
	for (i = 0; i < key->len; ++i)
		montgomery_mul_add_step(key, result, a[i], b);
}

/**
 * pow_mod() - in-place public exponentiation
 *
 * @key:	RSA key
 * @inout:	Big-endian word array containing value and result
 */
static int pow_mod(const struct rsa_public_key *key, uint32_t *inout)
{
	uint32_t *result, *ptr;
	uint i;

	/* Sanity check for stack size - key->len is in 32-bit words */
	if (key->len > RSA_MAX_KEY_BITS / 32) {
		LOGD("RSA key words %u exceeds maximum %d", key->len, RSA_MAX_KEY_BITS / 32);
		return -1;
	}

	uint32_t val[key->len], acc[key->len], tmp[key->len];
	result = tmp;  /* Re-use location. */

	/* Convert from big endian byte array to little endian word array. */
	for (i = 0, ptr = inout + key->len - 1; i < key->len; i++, ptr--)
		val[i] = get_unaligned_be32(ptr);

	montgomery_mul(key, acc, val, key->rr);  /* axx = a * RR / R mod M */
	for (i = 0; i < 16; i += 2) {
		montgomery_mul(key, tmp, acc, acc); /* tmp = acc^2 / R mod M */
		montgomery_mul(key, acc, tmp, tmp); /* acc = tmp^2 / R mod M */
	}
	montgomery_mul(key, result, acc, val);  /* result = XX * a / R mod M */

	/* Make sure result < mod; result is at most 1x mod too large. */
	if (greater_equal_modulus(key, result))
		subtract_modulus(key, result);

	/* Convert to bigendian byte array */
	for (i = key->len - 1, ptr = inout; (int)i >= 0; i--, ptr++)
		put_unaligned_be32(result[i], ptr);

	return 0;
}

