
//Assume use such Ioctl number
#define ATH6KL_IOCTL_PRIV_S10 SIOCDEVPRIVATE+10

// Command ID Mapping
enum ioctl_priv_cmds {
    //ID: 0,1 should be reserved not to use
    IOCTL_PRIV_READ           = 0,
    IOCTL_PRIV_WRITE          = 1,

/*From debugfs*/
    IOCTL_PRIV_GET_NF           =   2,
    IOCTL_PRIV_DEBUG_MASK_W     =   3,
    IOCTL_PRIV_DEBUG_MASK_R     =   4,
    IOCTL_PRIV_DEBUG_QUIRKS_R   =   5,
    IOCTL_PRIV_DRIVER_VERSION_R =   6,
    IOCTL_PRIV_REG_WRITE        =   7,
    IOCTL_PRIV_REG_READ         =   8, 
    IOCTL_PRIV_TGT_STATS        =   9, // use "big_buf"
    IOCTL_PRIV_CHAN_LIST        =   10,// use "big_buf"
    IOCTL_PRIV_TX_RATEMASK_W    =   11,
    IOCTL_PRIV_TX_RATEMASK_R    =   12,
    IOCTL_PRIV_HT_CAP_PARAMS_W  =   13,
    IOCTL_PRIV_HT_CAP_PARAMS_R  =   14,// use "big_buf"
    IOCTL_PRIV_RX_AGGR_PARAMS_W =   15,
    IOCTL_PRIV_RX_AGGR_PARAMS_R =   16,// use "big_buf"
    IOCTL_PRIV_TX_AMSDU_W       =   17,
    IOCTL_PRIV_TX_AMSDU_R       =   18,
    IOCTL_PRIV_TX_AMSDU_PARAMS_W=   19,
    IOCTL_PRIV_TX_AMSDU_PARAMS_R=   20, // use "big_buf"
    IOCTL_PRIV_GET_TX_RATE      =   21, // use "big_buf"
    IOCTL_PRIV_GET_AP_STATS     =   22, // use "big_buf"
    IOCTL_PRIV_AMPDU_W          =   23,
    IOCTL_PRIV_TXRX_ERR_STATS   =   24, // use "big_buf"
    IOCTL_PRIV_TXPOWER_W        =   25, 
    IOCTL_PRIV_TXPOWER_R        =   26,
    IOCTL_PRIV_GET_TSF_R        =   27,
    IOCTL_PRIV_SYN_TSF          =   28, //Trigger command only need to fill command ID
    IOCTL_PRIV_RETRY_LIMIT_W    =   29,
    IOCTL_PRIV_PS_SET           =   30,

/*For SWOW command ID from 33 (0x21)*/
    IOCTL_PRIV_SWOW_START       = 33,//Write of swow_start
    IOCTL_PRIV_SWOW_WAKER_W     = 34,//Write of swow_waker
    IOCTL_PRIV_SWOW_WAKER_R     = 35,//Read of swow_waker
    IOCTL_PRIV_SWOW_EXCEPTION_W = 36,//Write of swow_exception
    IOCTL_PRIV_SWOW_EXCEPTION_R = 37,//Read of swow_exception
    IOCTL_PRIV_SWOW_PATTEN_W    = 38,//Write of swow_pattern
    IOCTL_PRIV_SWOW_PATTEN_R    = 39,//Read of swow_pattern
    IOCTL_PRIV_SWOW_FILTER_W    = 40,//Write of swow_filter
    IOCTL_PRIV_SWOW_FILTER_R    = 41,//Read of swow_filter
    IOCTL_PRIV_SWOW_PULSE_TEST  = 42,//Write of swow_pulse_test
//------------------------------------

//New command added from ID 60
    IOCTL_PRIV_RX_BUNFLE_PARAMS_W   = 60,

};
//Ioctl data structure

/*SWOW waker used format*/
struct waker{
u8 key_mac[6];
u8 key_len;
u8 key[16]; //Hex format
u32 app_mask;
};

#define MAX_BUF_SIZE    2048

struct ioctl_priv_params {
    u16 cmd_id; //Mapping to "enum ioctl_priv_cmds"
    u16 cmd_len;//Read or Write length
    union {
        struct {
            char cmd_read[16];
        } read_example;     //IOCTL_PRIV_READ
        
        struct {
            char cmd_write[16];
        } write_example;    //IOCTL_PRIV_WRITE
        /*
        Add stucture needed for read or write
        */
        struct {
            char buf[MAX_BUF_SIZE];
        } big_buf;  //For String Read

        struct {
          u32 ch0_nf;
          u32 ch1_nf; 
          u32 ch0_nf_e;
          u32 ch1_nf_e;
        } get_nf; //IOCTL_PRIV_GET_NF

        struct {
            u32 mask;
            u32 mask_ext;
        } debug_mask_params; //IOCTL_PRIV_DEBUG_MASK_W & IOCTL_PRIV_DEBUG_MASK_R

        struct {
            u32 debug_quirks;
        } debug_quirks_params; //IOCTL_PRIV_DEBUG_QUIRKS_R

        struct {
            char version[64];
        } driver_version;   //IOCTL_PRIV_DRIVER_VERSION_R

        struct {
            u32 reg_addr;
            u32 reg_value;
        } reg_params;   //IOCTL_PRIV_REG_WRITE & IOCTL_PRIV_REG_READ

        struct {
            u64 rate_mask;
        } txrate_sereies;   //IOCTL_PRIV_TX_RATEMASK_W % IOCTL_PRIV_TX_RATEMASK_W

        struct {
            u8 band;
            u8 ht40_supported;
            u8 short_GI;
            u8 intolerance_ht40;
        } ht_cap_params; //IOCTL_PRIV_HT_CAP_PARAMS_W

        struct {
            u16 rx_aggr_timeout;
        } rx_aggr_params; //IOCTL_PRIV_RX_AGGR_PARAMS_W

        struct {
            u8 enable;
        } tx_amsdu; //IOCTL_PRIV_TX_AMSDU_W & IOCTL_PRIV_TX_AMSDU_R

        struct {
            u8 max_aggr_num;
            u16 max_pdu_len;
            u16 amsdu_timeout;
            u8 seq_pkt;
            u8 progressive;
        } tx_amsdu_params; //IOCTL_PRIV_TX_AMSDU_PARAMS_W

        struct {
            u8 enable;
        } ampdu; //IOCTL_PRIV_AMPDU_W

        struct {
            u8 ratio;
            u8 max_pwr;
            u8 current_pwr;
        } tx_power_params; //IOCTL_PRIV_TXPOWER_W & IOCTL_PRIV_TXPOWER_R

        struct {
            u64 tsf_value;
        } get_tsf; //IOCTL_PRIV_GET_TSF_R

        struct {
            u32 retry_limit;
        } da_retry_limit; //IOCTL_PRIV_RETRY_LIMIT_W

        struct {
            u8 ps_set;
        } power_save; //IOCTL_PRIV_PS_SET

        /*Structure of SWOW USED*/
        struct {
            u8 cmd;
            char* arp_ip_addr;
        } swow_start;       //IOCTL_PRIV_SWOW_START

        struct {
            u8 waker_id;
            struct waker write_waker;
        } swow_waker_w;     //IOCTL_PRIV_SWOW_WAKER_W

        struct {
            struct waker read_waker[8];
        } swow_waker_r;     //IOCTL_PRIV_SWOW_WAKER_R

        struct {
            u8 exception;
            u8 exception_app;
        } swow_exception;       //IOCTL_PRIV_SWOW_EXCEPTION_W & IOCTL_PRIV_SWOW_EXCEPTION_R

        struct {
            u8 pattern;
        } swow_pattern;       //IOCTL_PRIV_SWOW_PATTEN_W & IOCTL_PRIV_SWOW_PATTEN_R

        struct {
            u8 waker_check;
            u8 pw_check;
        } swow_filter;       //IOCTL_PRIV_SWOW_FILTER_W & IOCTL_PRIV_SWOW_FILTER_R

        struct {
            u8 type;
            u8 app;
        } swow_pulse_test;   //IOCTL_PRIV_SWOW_PULSE_TEST
    //-------------------------

        struct {
            u8 min_rx_bundle_frame;
            u8 rx_bundle_timeout;
        } rx_bundle_params; //IOCTL_PRIV_RX_BUNFLE_PARAMS_W
    } params;
} __packed;
