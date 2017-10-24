#ifndef _DA_RSA_HEADER_
#define _DA_RSA_HEADER_

#define DA_RSA_SUCCESS			0
#define DA_RSA_FAIL				-1
#define DA_RSA_TYPE_SIGN		0x10
#define DA_RSA_TYPE_VERIFY		0x20
#define DA_RSA_TYPE_PARAM		0x30

#define DA_RSA_RSA_KEY_SIZE 	2048
#define DA_RSA_BUFFER_SIZE		4096

#ifndef no_argument
	#define no_argument 			0
#endif

#ifndef required_argument
	#define required_argument		1
#endif


#ifdef DEBUG
	#define LOGD(fmt, args...)	printf ("\033[01;32m DEBUG [%s:%04d] \033[00;00m" fmt "\n", __FUNCTION__, __LINE__, ##args)
#else
	#define LOGD(fmt, args...)
#endif
#define LOGE(fmt, args...)	printf ("\033[01;31m ERROR [%s:%04d] \033[00;00m" fmt "\n", __FUNCTION__, __LINE__, ##args)

typedef struct _da_rsa_info_ {
	int  type;
	char *prv_key_file;
	char *pub_key_file;
	char *target_file;
	char *signature_file;
	char *pkp_param_file;
	char *password;
	char  is_append;
	char  is_sha1;
} da_rsa_info;

int  rsa_init(void);
void rsa_remove(void);
int  rsa_sign(da_rsa_info *dsi);
int  rsa_verify(da_rsa_info *dsi);
int  rsa_param(da_rsa_info *dsi, int print_out_params);
void usage();

static int  rsa_sign_with_key(da_rsa_info *dsi, RSA *rsa);
static int	rsa_get_params_from_key(RSA *key, uint32_t *n0_invp, BIGNUM **modulusp, BIGNUM **r_squaredp);
static int  rsa_get_password_cb(char* buf, int size, int rwflag, void *userdata);
static int  rsa_get_priv_key(da_rsa_info *dsi, RSA **rsap);
static int 	rsa_get_pub_key_from_pem(const char *keypath, RSA **rsap);
static int  rsa_get_pub_key_from_cert(const char *certpath, RSA **rsap);
static void rsa_save_signature(da_rsa_info *dsi, unsigned char *sig, int siglen);
static int	rsa_bignum_to_array(BIGNUM *num, int num_bits, uint32_t **arrp);
static void rsa_print_bignum(BIGNUM *num, int num_bits);
static void rsa_print_hex(unsigned char* string, int len);
static char *file_name_without_ext(char *filename);

#endif /* _DA_RSA_HEADER_ */
