/* da-rsa-sign.c  -- RSA Signing Routines
 *
 * Copyright (c) 2014, Samsung Electronics
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
 * Written by Elmurod Talipov and Seong-Yong Kang.
 * 
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h> 
#include <stdint.h>

#include <openssl/sha.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/evp.h>

#include "da-rsa-sign.h"
#include "da-rsa-verify.h"
#include "byteorder.h"

void usage(char *program)
{
	printf("Usage: %s [options] \n", program);
	printf("--------------------------- OPTIONS ---------------------------------------------------\n");
	printf(" [--sign|verify|param] : sign or verify image, get parameters from public key \n");
	printf(" [--target]            : target file to sign or verify \n");
	printf(" [--signature]         : signature file to store or read signature for target file \n");
	printf(" [--private]           : private key file \n");
	printf(" [--public]            : public key file \n");
	printf(" [--help]              : show this help \n");
	printf("--------------------------- EXAMPLE ---------------------------------------------------\n");
	printf(" %s --sign --target uImage --signature uImage.sgn --private key/private.pem \n", program);
	printf(" %s --verify --target uImage --signature uImage.sgn --public key/public.pem \n", program);
	printf(" %s --param --public key/public.pem \n", program);
	printf(" %s -s -t uImage -i uImage.sgn -r key/private.pem \n", program);
	printf(" %s -v -t uImage -i uImage.sgn -u key/public.pem \n", program);
	printf(" %s -p -u key/public.pem \n", program);
}

int main(int argc, char **argv) 
{
	LOGD("++ ");
	char pkp_file[255];
	int ret = DA_RSA_SUCCESS;
	int option, option_index = 0;
	da_rsa_info dsi;

	// initialize
	dsi.type = -1;
	dsi.prv_key_file = NULL;
	dsi.pub_key_file = NULL;
	dsi.target_file = NULL;
	dsi.signature_file = NULL;
	dsi.password = NULL;
	dsi.is_append = 0;
	dsi.is_sha1 = 0;
	
	// options
	static struct option long_options[] = {
		{ "sign", no_argument, NULL, 's' },
		{ "verify", no_argument, NULL, 'v' },
		{ "param", no_argument, NULL, 'p' },
		{ "private", required_argument, NULL, 'r' },
		{ "public", required_argument, NULL, 'u' },
		{ "target", required_argument, NULL, 't' },
		{ "signature", required_argument, NULL, 'i' },
		{ "password", required_argument, NULL, 'w' },
		{ "shaone", no_argument, NULL, 'c' },
		{ "append", no_argument, NULL, 'a' },
		{ "help", no_argument, NULL, 'h' },
		{ NULL, no_argument, NULL, 0 }
	};
	
	// get options
	while ( (option = getopt_long (argc, argv, "svpr:u:t:i:h?", long_options, &option_index)) != -1 )
		switch (option)
    {
    	case 's':
    		dsi.type = DA_RSA_TYPE_SIGN;
    		LOGD("type : sign");
    		break;
    	case 'v':
    		dsi.type = DA_RSA_TYPE_VERIFY;
    		LOGD("type : verify");
    		break;
    	case 'p':
    		dsi.type = DA_RSA_TYPE_PARAM;
    		LOGD("type : get param");
    		break;
    	case 'r':
    		dsi.prv_key_file = optarg;
    		LOGD("private key file : %s", dsi.prv_key_file);
    		break;
    	case 'u':
    		dsi.pub_key_file = optarg;
    		LOGD("public key file : %s", dsi.pub_key_file);
    		break;
    	case 't':
    		dsi.target_file = optarg;
    		LOGD("target file : %s", dsi.target_file);
    		break;
    	case 'i':
    		dsi.signature_file = optarg;
    		LOGD("signature file : %s", dsi.signature_file);
    		break;
    	case 'w':
    		dsi.password = optarg;
    		LOGD("password : %s", dsi.password);
    		break;
    	case 'a':
    		dsi.is_append = 1;
    		LOGD("append : %d", dsi.is_append);
    		break;
    	case 'c':
    		dsi.is_sha1 = 1;
    		LOGD("sha1 : true (%d)", dsi.is_sha1);
    		break;
    	case '?':
    	case 'h':
    		usage(argv[0]);
			ret = DA_RSA_FAIL;
    		goto finish;
			break;
    	default:
    		usage(argv[0]);
    		ret = DA_RSA_FAIL;
    		goto finish;
	}
	
	if (dsi.type == DA_RSA_TYPE_SIGN )
	{
		if ( (dsi.prv_key_file == NULL) || (dsi.target_file == NULL) ||
			 (dsi.signature_file == NULL) )
		{
			usage(argv[0]);
    		ret = DA_RSA_FAIL;
    		goto finish;	
		}
		
		if (dsi.password == NULL)
		{
			char password_input[50];
			printf("Input private key password: ");
			scanf("%49s", password_input);
			dsi.password = password_input;
		}
		
		ret = rsa_sign(&dsi);
	}
	else if (dsi.type == DA_RSA_TYPE_VERIFY)
	{
		if ( (dsi.pub_key_file == NULL) || (dsi.target_file == NULL) ||
			 (dsi.signature_file == NULL) )
		{
			usage(argv[0]);
    		ret = DA_RSA_FAIL;
    		goto finish;
		}

		dsi.pkp_param_file = "pkp.tmp";
		ret = rsa_verify(&dsi);
	}
	else if (dsi.type == DA_RSA_TYPE_PARAM)
	{
		if (dsi.pub_key_file == NULL)
		{
			usage(argv[0]);
    		ret = DA_RSA_FAIL;
    		goto finish;	
		}
		
		// prepare public key parameter file
		snprintf(pkp_file, 255, "param_%s", dsi.pub_key_file);
		dsi.pkp_param_file = pkp_file;
		ret = rsa_param(&dsi, 1);
	}
	else
	{
		usage(argv[0]);
		ret = DA_RSA_FAIL;
		goto finish;
	}

finish:
	LOGD("-- ");

	return ret;
}

/**
* rsa_int() - initializes rsa algorithms
* @return     0 = ok, -1 on error
*/
int rsa_init(void)
{
	OpenSSL_add_all_algorithms();
	OpenSSL_add_all_digests();
	OpenSSL_add_all_ciphers();

	return DA_RSA_SUCCESS;
}

/**
* rsa_remove() - deinitializes rsa algorithms
*/
void rsa_remove(void)
{
	CRYPTO_cleanup_all_ex_data();
	ERR_free_strings();
#ifdef HAVE_ERR_REMOVE_THREAD_STATE
	ERR_remove_thread_state(NULL);
#else
	ERR_remove_state(0);
#endif
	EVP_cleanup();
}

/**
 * rsa_sign() - sign target file with private key
 *
 * @dsi		  da_signer info, contains target file, signature file, private key 
 * @return    0 if ok, -1 on error 
 */

int rsa_sign(da_rsa_info *dsi)
{
	LOGD("++ ");
	
	RSA *rsa;
	int ret;

	ret = rsa_init();
	if (ret != DA_RSA_SUCCESS) goto finish;
		 		 
	ret = rsa_get_priv_key(dsi, &rsa);
	if (ret != DA_RSA_SUCCESS) goto err_priv;
	
	ret = rsa_sign_with_key(dsi, rsa);
	if (ret != DA_RSA_SUCCESS) goto err_sign;

	ret = DA_RSA_SUCCESS;
		
err_sign:
	RSA_free(rsa);
err_priv:
	rsa_remove();
finish:
	LOGD("-- ");

	return ret;
}

/**
 * rsa_verify() - verify target with signature
 *
 * @dsi		  da_signer info, contains target file, signature file, public key
 * @return    0 if ok, -1 on error 
 */
 
int rsa_verify(da_rsa_info *dsi)
{
	LOGD("++ ");
	RSA *rsa;
	int ret;
	
	ret = rsa_init();
	if (ret != DA_RSA_SUCCESS) goto finish;
	
	ret = rsa_param(dsi, 0);
	if (ret != DA_RSA_SUCCESS) goto err_pub;

	ret = rsa_verify_custom(dsi->target_file, dsi->signature_file, dsi->pkp_param_file, dsi->is_sha1 );
	
err_pub:
	rsa_remove();
finish:
	LOGD("-- ");
	return ret;
}

/**
 * rsa_param - verify target file with signature
 *
 * @dsi		  da_rsa_info, contains public_key_file
 * @return    0 if ok, -1 on error 
 */
int rsa_param(da_rsa_info *dsi, int print_out_params)
{
	LOGD("++");
	RSA *rsa;
	BIGNUM *modulus, *r_squared;
	uint32_t n0_inv;
	uint32_t *mod_arr, *rr_arr;
	int ret, bits, i, nwords;

	ret = rsa_get_pub_key_from_pem(dsi->pub_key_file, &rsa);
	if ( ret != DA_RSA_SUCCESS) goto finish;
	
	ret = rsa_get_params_from_key(rsa, &n0_inv, &modulus, &r_squared);
	if ( ret != DA_RSA_SUCCESS) goto err_param;
	
	bits = BN_num_bits(modulus);
	nwords = bits / 32;

	ret = rsa_bignum_to_array(modulus, bits, &mod_arr);
	if ( ret != DA_RSA_SUCCESS) goto err_convert;

	ret =  rsa_bignum_to_array(r_squared, bits, &rr_arr);
	if ( ret != DA_RSA_SUCCESS) goto err_convert;

	bits = cpu_to_be32(bits);
	n0_inv = cpu_to_be32(n0_inv);

// write to file
	FILE *fp = fopen(dsi->pkp_param_file, "wb");
	if(fp == NULL)
	{
		LOGE("Failed to open public key parameter file to save:  %s", dsi->pkp_param_file);
		ret = DA_RSA_FAIL;
		goto err_convert;
	}

	fprintf(fp, "0x%08x \n", bits );
	fprintf(fp, "0x%08x \n",  n0_inv);

	// modulus
	for (i = 0; i< nwords ; i++)
	{
		mod_arr[i] = cpu_to_be32(mod_arr[i]);
		fprintf(fp, "0x%08x ", mod_arr[i]);
		if( (i + 1) % 6 == 0) fprintf(fp, " \n");
	}
	fprintf(fp, " \n");

	// r-squared
	for (i = 0; i< nwords ; i++)
	{
		rr_arr[i] = cpu_to_be32(rr_arr[i]);
		fprintf(fp, "0x%08x ", rr_arr[i]);
		if( (i + 1) % 6 == 0) fprintf(fp, " \n");
	}
	fprintf(fp, " \n");

// end of writing

	if (print_out_params) {
		printf("Public key parameters written to '%s' file \n", dsi->pkp_param_file);

		printf("num_bits = 0x%08x \n", bits);
		printf("n0_inverse = 0x%08x \n", n0_inv);
		printf("modulus[]  =  { ");
		for (i = 0; i< nwords ; i++) {
			printf("0x%08x, ", mod_arr[i]);
			if( (i + 1) % 6 == 0)  printf("\n");
		}
		printf(" } \n");

		printf("r_squared[] = { ");
		for (i = 0; i< nwords ; i++) {
			printf("0x%08x, ", rr_arr[i]);
			if( (i + 1) % 6 == 0 ) printf("\n");
		}
		printf(" } \n");

		printf("N = 0x%s\n", BN_bn2hex(rsa->n));
		printf("e = 0x%s\n", BN_bn2hex(rsa->e));

	}

	ret = DA_RSA_SUCCESS;
	
	fclose(fp);

err_convert:

	free(mod_arr); // allocated in bignum_to_array
	free(rr_arr);  // allocated in bignum_to_array

	BN_free(modulus);
	BN_free(r_squared);

err_param:	
	RSA_free(rsa);

finish:
	LOGD("--");

	return ret;
}

static int rsa_get_password_cb(char* buf, int size, int rwflag, void *userdata)
{
	da_rsa_info *dsi = (da_rsa_info *)userdata;
	strncpy(buf, dsi->password, strlen(dsi->password));

	return strlen((char*)dsi->password);
}

/**
 * rsa_get_priv_key() - read a private key from a pem file
 *
 * @dsi		 da_rsa_info, contains a path to private key file
 * @rsap	 Returns RSA object, or NULL on failure
 * @return   0 if ok, -1 on error (in which case *rsap will be set to NULL)
 */
static int rsa_get_priv_key(da_rsa_info *dsi, RSA **rsap)
{
	RSA *rsa;
	FILE *f;

	*rsap = NULL;
	f = fopen((const char *)dsi->prv_key_file, "r");
	if (!f) {
		LOGE("Couldn't open RSA private key: '%s': %s", dsi->prv_key_file, strerror(errno));
		return DA_RSA_FAIL;
	}

	rsa = PEM_read_RSAPrivateKey(f, 0, (pem_password_cb*)rsa_get_password_cb, dsi);
	if (!rsa) {
		LOGE("Failure reading private key");
		fclose(f);
		return DA_RSA_FAIL;;
	}
	fclose(f);
	*rsap = rsa;

	return DA_RSA_SUCCESS;
}

/**
 * rsa_get_pub_key_from_pem() - read a public key from a pem file
 *
 * @keypath Path to certificate file (have a .crt extension)
 * @rsap	 Returns RSA object, or NULL on failure
 * @return   0 if ok, -ve on error (in which case *rsap will be set to NULL)
 */
static int rsa_get_pub_key_from_pem(const char *keypath, RSA **rsap)
{
	LOGD("++");
	
	int ret = DA_RSA_SUCCESS;
	FILE *fp = NULL;
	EVP_PKEY *pkey = NULL;
	RSA *rsa;
	*rsap = NULL;

	//open public key
	fp = fopen(keypath, "rb");
	if (!fp)
	{
		LOGE("Couldn't open RSA public key: '%s': %s\n", keypath, strerror(errno));
		ret = DA_RSA_FAIL;
		goto finish;
	}

	// read public key
	pkey = PEM_read_PUBKEY(fp, &pkey, NULL, NULL);
	if (!pkey)
	{
		LOGE("Couldn't read key from %s", keypath);
		ret = DA_RSA_FAIL;
		goto err_key;
	}

	/* Convert to a RSA_style key. */
	rsa = EVP_PKEY_get1_RSA(pkey);
	if (!rsa) {
		LOGE("Couldn't convert to a RSA style key");
		ret = DA_RSA_FAIL;
		goto err_rsa;
	}
	
	LOGD("N (modulus) : 0x%s\n", BN_bn2hex(rsa->n));

	*rsap = rsa;
	ret = DA_RSA_SUCCESS;
	
err_rsa:
	EVP_PKEY_free(pkey);	
err_key:
	fclose(fp);
finish:
	
	LOGD("--");
	return ret;
}

/**
 * rsa_get_pub_key_from_cert() - read a public key from a .crt file
 *
 * @certpath Path to certificate file (have a .crt extension)
 * @rsap	 Returns RSA object, or NULL on failure
 * @return   0 if ok, -ve on error (in which case *rsap will be set to NULL)
 */
static int rsa_get_pub_key_from_cert(const char *certpath, RSA **rsap)
{
	LOGD("++");
	
	EVP_PKEY *key;
	X509 *cert;
	RSA *rsa;
	FILE *f;
	int ret = DA_RSA_SUCCESS;

	*rsap = NULL;
	f = fopen(certpath, "r");
	if (!f) {
		LOGE("Couldn't open RSA certificate: '%s': %s\n", certpath, strerror(errno));
		ret = DA_RSA_FAIL;
		goto finish;
	}

	/* Read the certificate */
	cert = NULL;
	if (!PEM_read_X509(f, &cert, NULL, NULL)) {
		LOGE("couldn't read certificate");
		ret = DA_RSA_FAIL;
		goto err_cert;
	}

	/* Get the public key from the certificate. */
	key = X509_get_pubkey(cert);
	if (!key) {
		LOGE("Couldn't read public key\n");
		ret = DA_RSA_FAIL;
		goto err_pubkey;
	}

	/* Convert to a RSA_style key. */
	rsa = EVP_PKEY_get1_RSA(key);
	if (!rsa) {
		LOGE("Couldn't convert to a RSA style key");
		ret = DA_RSA_FAIL;
		goto err_rsa;
	}

	*rsap = rsa;

err_rsa:
	EVP_PKEY_free(key);
err_pubkey:
	X509_free(cert);
err_cert:
	fclose(f);
finish:
	LOGD("--");
	
	return ret;
}

static int rsa_sign_with_key(da_rsa_info *dsi, RSA *rsa)
{
	LOGD("++");
	
	EVP_PKEY *key;
	EVP_MD_CTX *context;

	int file_size = 0;
	int key_size = 0 ;
	int ret = DA_RSA_SUCCESS, i = 0;
	unsigned char *signature;
	int signature_length;
	unsigned char fbuff[DA_RSA_BUFFER_SIZE] = {0, };
	
	FILE *fp = NULL;
	
	key = EVP_PKEY_new();
	if (!key)
	{
		LOGE("EVP_PKEY object creation failed");
		ret = DA_RSA_FAIL;
		goto finish;
	}

	if (!EVP_PKEY_set1_RSA(key, rsa)) {
		LOGE("EVP key setup failed");
		ret = DA_RSA_FAIL;
		goto err_set;
	}

	key_size = EVP_PKEY_size(key);
	signature = malloc(key_size);
	signature_length = key_size;
	
	if (!signature) {
		LOGE("Out of memory for signature (%d bytes) ",	key_size);
		ret = DA_RSA_FAIL;
		goto err_alloc;
	}

	context = EVP_MD_CTX_create();
	if (!context) {
		LOGE("EVP context creation failed");
		ret = DA_RSA_FAIL;
		goto err_create;
	}
	
	EVP_MD_CTX_init(context);

	if (dsi->is_sha1){
		ret = EVP_SignInit(context, EVP_sha1());
	}
	else
	{
		ret = EVP_SignInit(context, EVP_sha256());
	}

	if (ret < 0) {
		LOGE("Signer setup failed");
		ret = DA_RSA_FAIL;
		goto err_sign;
	}
	
	//open input_file(image file) to sign
	fp = fopen((const char*)dsi->target_file, "rb");
	if(fp == NULL)
	{
		LOGE("Failed to open target file %s", dsi->target_file);
		ret = DA_RSA_FAIL;
		goto err_sign;
	}
	
	//check file size
	fseek(fp, 0, SEEK_END);
	file_size = ftell(fp);
	rewind(fp);

	for(i = 0; i < file_size; )
	{
		ret = fread(fbuff, 1, DA_RSA_BUFFER_SIZE, fp);
		if (ret > 0)
		{
			if (!EVP_SignUpdate(context, fbuff, ret)) {
				LOGE("Signing data failed");
				ret = DA_RSA_FAIL;
				goto err_fread;
			}
		}
		else
		{
			ret = DA_RSA_FAIL;
			goto err_fread;
		}

		i += ret;
	}

	if (!EVP_SignFinal(context, signature, &signature_length, key)) {
		LOGE("Could not obtain signature");
		ret = DA_RSA_FAIL;
		goto err_fread;
	}
	
	EVP_MD_CTX_cleanup(context);
	LOGD("Got signature: %d bytes, expected %d\n", signature_length, key_size);
	rsa_print_hex(signature, signature_length);

	rsa_save_signature(dsi, signature, signature_length);

	ret = DA_RSA_SUCCESS;

err_fread:
	fclose(fp);
err_sign:
	EVP_MD_CTX_destroy(context);
err_create:
	free(signature);
err_alloc:
err_set:
	EVP_PKEY_free(key);
finish:
	LOGD("--");

	return ret;
}

/**
 * rsa_save_signature - save signature to file
 *
 * @sigfile	 signature file to save output
 * @sig	 	 signature to save
 * @siglen 	 lenght of signature to save
 * @return   0 if ok, -1 on error
 */
static void rsa_save_signature(da_rsa_info *dsi, unsigned char *sig, int siglen)
{
	int ret = -1;
	FILE *ofp = fopen((const char*)dsi->signature_file, "wb");
	
	if(ofp == NULL)
	{
		LOGE("Failed to open signature file to save:  %s", dsi->signature_file);
		goto finish;
	}

	// we append signature to target file save to signed image
	if (dsi->is_append == 1)
	{
		FILE *ifp = fopen((const char*)dsi->target_file, "rb");
		char fbuff[DA_RSA_BUFFER_SIZE];
		if (ifp == NULL)
		{
			LOGE("Fail to append signature to target file, could not open %s ", dsi->target_file);
			fclose(ofp);
			goto finish;
		}

		do {
			ret = fread(fbuff, 1, DA_RSA_BUFFER_SIZE, ifp);
			if (ret > 0)
				fwrite(fbuff, 1, ret, ofp);

		} while (ret > 0);

		fclose(ifp);

		// write magic code
		uint8_t magic_code[] = {0xDE, 0xAD, 0xBE, 0xEF};
		fwrite(magic_code, 1, 4*sizeof(uint8_t), ofp);
	}

	//write signature to signature file (sifgile)
	ret = fwrite(sig, 1, siglen, ofp);
	if(ret != siglen)
	{
		LOGE("Signature write to %s is failed", dsi->signature_file);
		LOGE("Only %d bytes written (signature is %d bytes)", ret, siglen);
	}

	fclose(ofp);

finish:
	LOGD("--");
}

/*
 * rsa_get_params(): - Get the important parameters of an RSA public key
 */
static int rsa_get_params_from_key(RSA *key, uint32_t *n0_invp, BIGNUM **modulusp, BIGNUM **r_squaredp)
{
	LOGD("++ ");
	
	BIGNUM *big1, *big2, *big32, *big2_32;
	BIGNUM *n, *r, *r_squared, *tmp;
	BN_CTX *bn_ctx;
	int ret = 0;

	// create big number context
	bn_ctx = BN_CTX_new();
	if (!bn_ctx) {
		LOGE("Out of memory (bug number context)");
		ret = DA_RSA_FAIL;
		goto finish;
	}

	// Initialize BIGNUMs
	big1 = BN_new();
	big2 = BN_new();
	big32 = BN_new();
	r = BN_new();
	r_squared = BN_new();
	tmp = BN_new();
	big2_32 = BN_new();
	n = BN_new();

	if (!big1 || !big2 || !big32 || !r || !r_squared || !tmp || !big2_32 || !n)
	{
		LOGE("Out of memory (big number)");
		ret = DA_RSA_FAIL;
		goto finish;
	}

	if (!BN_copy(n, key->n) || !BN_set_word(big1, 1L) ||
	    !BN_set_word(big2, 2L) || !BN_set_word(big32, 32L))
	{
		LOGE("Bignum copy or set word failed");
		ret = DA_RSA_FAIL;
		goto finish;
	}

	/* big2_32 = 2^32 */
	if (!BN_exp(big2_32, big2, big32, bn_ctx)) 
	{
		LOGE("Bignum exponential failed");
		ret = DA_RSA_FAIL;
		goto finish;
	}

	/* Calculate n0_inv = -1 / n[0] mod 2^32 */
	if (!BN_mod_inverse(tmp, n, big2_32, bn_ctx) || !BN_sub(tmp, big2_32, tmp))
	{
		LOGE("Calculate n0_inv failed");
		ret = DA_RSA_FAIL;
		goto finish;
	}

	*n0_invp = BN_get_word(tmp);

	/* Calculate R = 2^(# of key bits) */
	if (!BN_set_word(tmp, BN_num_bits(n)) || !BN_exp(r, big2, tmp, bn_ctx)) 
	{
		LOGE("Calculate R failed");
		ret = DA_RSA_FAIL;
		goto finish;
	}

	/* Calculate r_squared = R^2 mod n */
	if (!BN_copy(r_squared, r) || !BN_mul(tmp, r_squared, r, bn_ctx) ||
	    !BN_mod(r_squared, tmp, n, bn_ctx))
	{
		LOGE("Calculate r_squared failed");
		ret = DA_RSA_FAIL;
		goto finish;
	}	

	*modulusp = n;
	*r_squaredp = r_squared;

	ret = DA_RSA_SUCCESS;

finish:
	BN_CTX_free(bn_ctx);
	BN_free(big1);
	BN_free(big2);
	BN_free(big32);
	BN_free(r);
	BN_free(tmp);
	BN_free(big2_32);

	LOGD("--");
	
	return ret;
}

static int rsa_bignum_to_array(BIGNUM *num, int num_bits, uint32_t **arrp)
{
	LOGD("++");

	int nwords = num_bits / 32;
	int size, ret ;
	uint32_t *buf, *ptr;
	BIGNUM *tmp, *big2, *big32, *big2_32;
	BN_CTX *ctx;

	tmp = BN_new();
	if (!tmp )
	{
		LOGE("Out of memory, tmp big number");
		ret = DA_RSA_FAIL;
		goto err_tmp;
	}

	big2 = BN_new();
	if (!big2 )
	{
		LOGE("Out of memory, big2 big number");
		ret = DA_RSA_FAIL;
		goto err_big2;
	}

	big32 = BN_new();
	if (!big32)
	{
		LOGE("Out of memory, big32 big number");
		ret = DA_RSA_FAIL;
		goto err_big32;
	}

	big2_32 = BN_new();
	if (!big2_32)
	{
		LOGE("Out of memory, big32 big number");
		ret = DA_RSA_FAIL;
		goto err_big232;
	}

	ctx = BN_CTX_new();
	if (!ctx) {
		LOGE("Out of memory, big number context");
		ret = DA_RSA_FAIL;
		goto err_ctx;
	}

	BN_set_word(big2, 2L);
	BN_set_word(big32, 32L);
	BN_exp(big2_32, big2, big32, ctx); /* B = 2^32 */

	size = nwords * sizeof(uint32_t);
	buf = malloc(size);
	if (!buf) {
		LOGE("Out of memory (%d bytes) ", size);
		ret = DA_RSA_FAIL;
		goto err_buf;
	}

	/* Write out big number as array of integers */
	for (ptr = buf + nwords - 1; ptr >= buf; ptr--) {
		BN_mod(tmp, num, big2_32, ctx); /* n = N mod B */
		*ptr = BN_get_word(tmp);
		BN_rshift(num, num, 32); /*  N = N/B */
	}

	*arrp = buf;
	ret = DA_RSA_SUCCESS;

err_buf:
	BN_CTX_free(ctx);
err_ctx:
	BN_free(big2_32);
err_big232:
	BN_free(big32);
err_big32:
	BN_free(big2);
err_big2:
	BN_free(tmp);
err_tmp:
	LOGD("--");

	return ret;
}

/*
static void rsa_print_bignum(BIGNUM *num, int num_bits)
{
	LOGD("++");
	
	int nwords = num_bits / 32;
	int size, i = 0;
	uint32_t *buf, *ptr;
	BIGNUM *tmp, *big2, *big32, *big2_32;
	BN_CTX *ctx;
	int ret;

	tmp = BN_new();
	big2 = BN_new();
	big32 = BN_new();
	big2_32 = BN_new();
	if (!tmp || !big2 || !big32 || !big2_32) 
	{
		LOGE("Out of memory (bignum)");
		goto finish;
	}

	ctx = BN_CTX_new();
	if (!ctx) {
		LOGE("Out of memory (bignum context)");
		goto err_ctx;
	}
	
	BN_set_word(big2, 2L);
	BN_set_word(big32, 32L);
	BN_exp(big2_32, big2, big32, ctx); // B = 2^32

	size = nwords * sizeof(uint32_t);
	buf = malloc(size);
	if (!buf) {
		LOGE("Out of memory (%d bytes) ", size);
		goto err_ctx;
	}

	printf("{ ");
	// Write out modulus as big endian array of integers
	for (ptr = buf + nwords - 1; ptr >= buf; ptr--) {
		BN_mod(tmp, num, big2_32, ctx); // n = N mod B
		*ptr = BN_get_word(tmp);
		BN_rshift(num, num, 32); //  N = N/B

		//printf("0x%08x,", cpu_to_be32(*ptr));
		printf("0x%08x,", *ptr);
		i++;
		if( (i % 6) == 0 ) printf("\n");
	}
	printf("} \n");

	free(buf);
	free(ctx);

err_ctx:
	BN_free(tmp);
	BN_free(big2);
	BN_free(big32);
	BN_free(big2_32);

finish:

	LOGD("--");
}
*/


#ifdef DEBUG
static void rsa_print_hex(unsigned char* string, int len)
{
	int i;
	
	for(i=1; i <= len; i++)
	{
		printf("0x%02X, ", string[i-1]);

		if( (i % 8) == 0 )
		{
			printf("\n");
		}
	}
	printf("\n\n");
	
	return;
}
#else
static void rsa_print_hex(unsigned char* string, int len)
{
	return;
}
#endif
