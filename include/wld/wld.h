/****************************************************************************
**
** SPDX-License-Identifier: BSD-2-Clause-Patent
**
** SPDX-FileCopyrightText: Copyright (c) 2022 SoftAtHome
**
** Redistribution and use in source and binary forms, with or
** without modification, are permitted provided that the following
** conditions are met:
**
** 1. Redistributions of source code must retain the above copyright
** notice, this list of conditions and the following disclaimer.
**
** 2. Redistributions in binary form must reproduce the above
** copyright notice, this list of conditions and the following
** disclaimer in the documentation and/or other materials provided
** with the distribution.
**
** Subject to the terms and conditions of this license, each
** copyright holder and contributor hereby grants to those receiving
** rights under this license a perpetual, worldwide, non-exclusive,
** no-charge, royalty-free, irrevocable (except for failure to
** satisfy the conditions of this license) patent license to make,
** have made, use, offer to sell, sell, import, and otherwise
** transfer this software, where such license applies only to those
** patent claims, already acquired or hereafter acquired, licensable
** by such copyright holder or contributor that are necessarily
** infringed by:
**
** (a) their Contribution(s) (the licensed copyrights of copyright
** holders and non-copyrightable additions of contributors, in
** source or binary form) alone; or
**
** (b) combination of their Contribution(s) with the work of
** authorship to which such Contribution(s) was added by such
** copyright holder or contributor, if, at the time the Contribution
** is added, such addition causes such combination to be necessarily
** infringed. The patent license shall not apply to any other
** combinations which include the Contribution.
**
** Except as expressly stated above, no rights or licenses from any
** copyright holder or contributor is granted under this license,
** whether expressly, by implication, estoppel or otherwise.
**
** DISCLAIMER
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
** CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
** INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
** MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
** DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
** CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
** USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
** AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
** LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
** ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
** POSSIBILITY OF SUCH DAMAGE.
**
****************************************************************************/
#ifndef __WLD_H__
#define __WLD_H__

#include <stdbool.h>
#include <stdint.h>
#include <swl/swl_common.h>
#include <swla/swla_commonLib.h>
#include <swl/swl_uuid.h>
#include <swla/swla_radioStandards.h>
#include <swla/swla_time.h>
#include <swla/swla_time_spec.h>
#include <swla/swla_oui.h>
#include "swla/swla_delayExec.h"
#include <swla/swla_function.h>
#include <swl/swl_usp_cmdStatus.h>
#include <swl/swl_wps.h>
#include <swl/swl_security.h>

#include <amxc/amxc.h>
#include <amxp/amxp.h>
#include <amxd/amxd_parameter.h>
#include <amxd/amxd_action.h>
#include <amxd/amxd_object_parameter.h>
#include <amxd/amxd_object_event.h>
#include <amxd/amxd_function.h>
#include <amxd/amxd_object_hierarchy.h>
#include <amxd/amxd_object.h>
#include <amxd/amxd_dm.h>
#include <amxd/amxd_transaction.h>
#include <amxb/amxb_types.h>
#include <amxb/amxb_operators.h>
#include <amxb/amxb_error.h>
#include <amxo/amxo.h>
#include <amxo/amxo_types.h>

#include "wld_types.h"
#include "wld_channel_types.h"
#include "wld_bgdfs.h"
#include "wld_rad_delayMgr.h"
#include "wld_fsm.h"
#include "wld_sensing.h"
#include "wld_mld.h"
#include "Utils/wld_autoCommitRadData.h"
#include "Utils/wld_dmnMgt.h"
#include "swla/swla_radioStandards.h"
#include "swla/swla_chanspec.h"
#include "swl/swl_uuid.h"
#include "swl/swl_staCap.h"
#include "swla/swla_table.h"
#include "swl/swl_assert.h"
#include "swl/swl_returnCode.h"
#include "swla/swla_tupleType.h"
#include "swl/types/swl_arrayType.h"
#include "swl/swl_mlo.h"
#include "swla/swla_namedTupleType.h"
#include "swla/swla_circTable.h"
#include "swla/swla_object.h"
#include "swla/swla_dm.h"
#include "wld_dm_trans.h"
#include "wld_nl80211_core.h"
#include "wld_secDmn.h"
#include "wld_secDmnGrp.h"
#include "wld_hostapd_cfgManager.h"
#include "wld_wpaSupp_cfgManager.h"
#include "swl/swl_80211.h"
#include "swl/swl_ieee802.h"
#include "wld_extMod.h"

#ifndef IFNAMSIZ
#define IFNAMSIZ 16
#endif

#ifndef AP_NAME_SIZE
#define AP_NAME_SIZE 32
#endif

#ifndef ETHER_ADDR_LEN
#define ETHER_ADDR_LEN 6
#endif

#ifndef ETHER_ADDR_STR_LEN
#define ETHER_ADDR_STR_LEN 18
#endif

#ifndef SSID_NAME_LEN
#define SSID_NAME_LEN 33
#endif

#ifndef PSK_KEY_SIZE_LEN
//PSK key size is 64 chars max, plus NULL char.
#define PSK_KEY_SIZE_LEN 65
#endif

#ifndef SAE_KEY_SIZE_LEN
//SAE key size is 127 chars max, plus NULL char.
#define SAE_KEY_SIZE_LEN 128
#endif

#ifndef NB_ACCESS_POINTS
#define NB_ACCESS_POINTS 10
#endif

#define MAX_NR_ANTENNA 8

#ifndef DFLT_NB_SAVED_INTERF_EVTS
#define DFLT_NB_SAVED_INTERF_EVTS 10
#endif

#ifndef NAS_IDENTIFIER_MAX_LEN
#define NAS_IDENTIFIER_MAX_LEN 17
#endif

#ifndef KHKEY_MAX_LEN
#define KHKEY_MAX_LEN 33
#endif

#ifndef QOS_MAP_SET_MAX_LEN
#define QOS_MAP_SET_MAX_LEN 128
#endif

#ifndef HE_SPR_SRG_BSS_COLORS_MAX_LEN
#define HE_SPR_SRG_BSS_COLORS_MAX_LEN 128
#endif

#ifndef HE_SPR_SRG_PARTIAL_BSSID_MAX_LEN
#define HE_SPR_SRG_PARTIAL_BSSID_MAX_LEN 128
#endif

#ifndef WLD_TMP_DEBUG_DIR
#define WLD_TMP_DEBUG_DIR "/tmp/wifiDbg"
#endif

#ifndef WLD_PERSIST_DEBUG_DIR
#define WLD_PERSIST_DEBUG_DIR "/ext/wifiDbg"
#endif


struct WLD_RADIO;

enum {
    NOTIFY_WLD_START = 100,
    NOTIFY_PAIRING_READY = 110,
    NOTIFY_PAIRING_DONE = 111,
    NOTIFY_PAIRING_ERROR = 112,
    NOTIFY_BSS_SCAN_DONE = 113,
    NOTIFY_RSSI = 114,
    NOTIFY_SSID_NOT_FOUND = 115,
    NOTIFY_AUTHENTICATION_ERROR = 116,
    NOTIFY_BACKHAUL_CREDS = 117,
    NOTIFY_AIRSTATS_RESULTS = 118,
};


//#define NOTIFY_PAIRING_READY 110
//#define NOTIFY_PAIRING_DONE 111
#define NOTIFY_SCAN_DONE 112

/**
 * Some basic structures that will hold the driver interface
 * handler (ioctl socket) and state. Each VAP section has a
 * backup of it's state and current FSM state when we're
 * updating the driver state.
 */

/*
 * Statically defined limit number of APs, EPs, SSIDs
 * Only to be used for initial hw devs/ifaces detection
 */
#define MAXNROF_RADIO          (3)
#define MAXNROF_ACCESSPOINT   (16)
#define MAXNROF_ENDPOINT      (32)
#define MAXNROF_SSID  (MAXNROF_ACCESSPOINT + MAXNROF_ENDPOINT)

#define MAXNROF_MFENTRY       (32)
#define MAXNROF_PFENTRY       (32)
#define NR_OF_STICKY_UNAUTHORIZED_STATIONS 1
#define MAXNROF_STAENTRY (wld_getMaxNrSta() + NR_OF_STICKY_UNAUTHORIZED_STATIONS)

/*
 * APIs to get learned global limit number of: stations, accessPoints, endpoints, SSIDs
 */
int32_t wld_getMaxNrSta();
uint32_t wld_getMaxNrAPs();
uint32_t wld_getMaxNrEPs();
uint32_t wld_getMaxNrSSIDs();

#define MAX_FSM_STACK_STATE   (16)
#define FSM_BW                 (2)

/* Used for DEBUGGING!!! FIX ME - this must be removed at end of project */
#define ENDP_POINTER    0x454E4450      /* "ENDP" */
#define VAP_POINTER     0x20564150      /* " VAP" */
#define RAD_POINTER     0x20524144      /* " RAD" */
#define SSID_POINTER    0x53534944      /* "SSID" */
#define ENDP_POINTER    0x454E4450      /* "ENDP" */


#define GET       (0)   /* Get data directly from cached data (default) */
#define SET       (1)   /* Set data */
#define NO_COMMIT (2)   /* Don't commit it (only on SET) */
#define DIRECT    (4)   /* Get or Set data directly from driver (Don't use the FSM) */
#define MVRADIO   (8)   /* Multi Vendor Radio */

/* Always the same... if you don't have it, it fails. When you set it, it complains! */
#ifndef FALSE
    #define FALSE   0
#endif
#ifndef TRUE
    #define TRUE    1
#endif


//Generic OK return
#define WLD_OK SWL_RC_OK


//Return ok and work is done
#define WLD_OK_DONE SWL_RC_DONE

//Return ok and work is continueing
#define WLD_OK_CONTINUE SWL_RC_CONTINUE

//Generic wld error state
#define WLD_ERROR SWL_RC_ERROR
//Invalid parameter
#define WLD_ERROR_INVALID_PARAM SWL_RC_INVALID_PARAM
//Invalid state
#define WLD_ERROR_INVALID_STATE SWL_RC_INVALID_STATE
//Requested function not implemented
#define WLD_ERROR_NOT_IMPLEMENTED SWL_RC_NOT_IMPLEMENTED

extern amxb_bus_ctx_t* wld_plugin_amx;
extern amxd_dm_t* wld_plugin_dm;
extern amxo_parser_t* wld_plugin_parser;
extern amxd_object_t* wifi;

/* Must be used in driver modules but they are mapping on this strings.*/
extern const char* Rad_SupStatus[];

extern const char** Rad_SupFreqBands;

extern const char** Rad_SupBW;

extern const char* wld_rad_autoBwSelectMode_str[];
typedef enum {
    BW_SELECT_MODE_MAXAVAILABLE,
    BW_SELECT_MODE_MAXCLEARED,
    BW_SELECT_MODE_DEFAULT,
    BW_SELECT_MODE_MAX
} wld_rad_bwSelectMode_e;

extern const char* Rad_SupCExt[];

extern const char* Rad_SupGI[];

extern const char* cstr_AP_status[];
extern const char* cstr_EndPoint_status[];
typedef enum {
    APSTI_DISABLED,
    APSTI_ENABLED,
    APSTI_ERROR_MISCONFIGURED,
    APSTI_ERROR,
    APSTI_UNKNOWN
} wld_intfStatus_e;

extern const char* cstr_EndPoint_connectionStatus[];
typedef enum {
    EPCS_DISABLED,
    EPCS_IDLE,
    EPCS_DISCOVERING,
    EPCS_CONNECTING,
    EPCS_WPS_PAIRING,
    EPCS_WPS_PAIRINGDONE,
    EPCS_WPS_TIMEOUT,
    EPCS_CONNECTED,
    EPCS_DISCONNECTED,
    EPCS_ERROR,
    EPCS_ERROR_MISCONFIGURED,
} wld_epConnectionStatus_e;

extern const char* cstr_EndPoint_lastError[];
typedef enum {
    EPE_NONE,
    EPE_SSID_NOT_FOUND,
    EPE_INVALID_PASSPHRASE,
    EPE_SECURITYMETHOD_UNSUPPORTED,
    EPE_WPS_TIMEOUT,
    EPE_WPS_CANCELED,
    EPE_ERROR_MISCONFIGURED,
    EPE_ASSOCIATION_TIMEOUT,
} wld_epError_e;

extern const char* cstr_EndPointProfile_status[];
typedef enum {
    EPPS_ACTIVE,
    EPPS_AVAILABLE,
    EPPS_ERROR,
    EPPS_DISABLED,
} wld_epProfileStatus_e;

extern const char* cstr_AP_MFMode[];
typedef enum {
    APMFM_OFF,
    APMFM_WHITELIST,
    APMFM_BLACKLIST,
    APMFM_COUNT
} wld_mfMode_e;

typedef enum {
    AP_TD_WPA3_P,
    AP_TD_SAE_PK,
    AP_TD_WPA3_E,
    AP_TD_OWE,
    AP_TD_MAX,
} wld_ap_td_e;

#define M_AP_TD_WPA3_P (1 << AP_TD_WPA3_P)
#define M_AP_TD_SAE_PK (1 << AP_TD_SAE_PK)
#define M_AP_TD_WPA3_E (1 << AP_TD_WPA3_E)
#define M_AP_TD_OWE (1 << AP_TD_OWE)
typedef uint32_t wld_ap_td_m;
extern const char* g_str_wld_ap_td[];

#define DEVICE_TYPE_MAX_NAME_LENGTH 5
extern const char* cstr_DEVICE_TYPES[];
enum {
    DEVICE_TYPE_VIDEO,
    DEVICE_TYPE_DATA,
    DEVICE_TYPE_GUEST,
    DEVICE_TYPE_MAX,
};

/*
 * @brief enum define all WPS configuration methods as per tr-181-2-15-1-usp
 * They correspond directly to the "Config Methods" attribute of
 * Wi-Fi_Protected_Setup_Specification_v2.0.8 (Table 33 - Configuration Methods)
 */
typedef enum {
    WPS_CFG_MTHD_USBFD,       // USBA (Flash Drive): Deprecated
    WPS_CFG_MTHD_ETH,         // Ethernet: Deprecated
    WPS_CFG_MTHD_LABEL,       // Label: 8 digit static PIN, typically available on device.
    WPS_CFG_MTHD_DISPLAY,     // Display: A dynamic 4 or 8 digit PIN is available from a display.
    WPS_CFG_MTHD_EXTNFCTOKEN, // External NFC Token: An NFC Tag is used to transfer the configuration or device password.
    WPS_CFG_MTHD_INTNFCTOKEN, // Integrated NFC Token: The NFC Tag is integrated in the device.
    WPS_CFG_MTHD_NFCINTF,     // NFC Interface: The device contains an NFC interface.
    WPS_CFG_MTHD_PBC,         // Pushbutton: The device contains either a physical or virtual Pushbutton.
    WPS_CFG_MTHD_PIN,         // same as legacy "Keypad": Device is capable of entering a PIN
    /* Below enums are only defined in WPS 2.x */
    WPS_CFG_MTHD_PBC_P,       // Physical Pushbutton: A physical Pushbutton is available on the device.
    WPS_CFG_MTHD_DISPLAY_P,   // Physical Display PIN: The dynamic 4 or 8 digit PIN is shown on a display/screen that is part of the device.
    WPS_CFG_MTHD_PBC_V,       // Virtual Pushbutton: Pushbutton functionality is available through a software user interface.
    WPS_CFG_MTHD_DISPLAY_V,   // Virtual Display PIN: The dynamic 4 or 8 digit PIN is displayed through a remote user interface.
    WPS_CFG_MTHD_MAX
} wld_wps_cfgMethod_e;

#define M_WPS_CFG_MTHD_USBFD (SWL_BIT_SHIFT(WPS_CFG_MTHD_USBFD))
#define M_WPS_CFG_MTHD_ETH (SWL_BIT_SHIFT(WPS_CFG_MTHD_ETH))
#define M_WPS_CFG_MTHD_LABEL (SWL_BIT_SHIFT(WPS_CFG_MTHD_LABEL))
#define M_WPS_CFG_MTHD_DISPLAY (SWL_BIT_SHIFT(WPS_CFG_MTHD_DISPLAY))
#define M_WPS_CFG_MTHD_EXTNFCTOKEN (SWL_BIT_SHIFT(WPS_CFG_MTHD_EXTNFCTOKEN))
#define M_WPS_CFG_MTHD_INTNFCTOKEN (SWL_BIT_SHIFT(WPS_CFG_MTHD_INTNFCTOKEN))
#define M_WPS_CFG_MTHD_NFCINTF (SWL_BIT_SHIFT(WPS_CFG_MTHD_NFCINTF))
#define M_WPS_CFG_MTHD_PBC (SWL_BIT_SHIFT(WPS_CFG_MTHD_PBC))
#define M_WPS_CFG_MTHD_PIN (SWL_BIT_SHIFT(WPS_CFG_MTHD_PIN))
#define M_WPS_CFG_MTHD_PBC_P (SWL_BIT_SHIFT(WPS_CFG_MTHD_PBC_P))
#define M_WPS_CFG_MTHD_DISPLAY_P (SWL_BIT_SHIFT(WPS_CFG_MTHD_DISPLAY_P))
#define M_WPS_CFG_MTHD_PBC_V (SWL_BIT_SHIFT(WPS_CFG_MTHD_PBC_V))
#define M_WPS_CFG_MTHD_DISPLAY_V (SWL_BIT_SHIFT(WPS_CFG_MTHD_DISPLAY_V))

#define M_WPS_CFG_MTHD_PBC_ALL (M_WPS_CFG_MTHD_PBC | M_WPS_CFG_MTHD_PBC_P | M_WPS_CFG_MTHD_PBC_V)
#define M_WPS_CFG_MTHD_DISPLAY_ALL (M_WPS_CFG_MTHD_DISPLAY | M_WPS_CFG_MTHD_DISPLAY_P | M_WPS_CFG_MTHD_DISPLAY_V)
// All WPS 1.x config method flags
#define M_WPS_CFG_MTHD_WPS10_ALL (M_WPS_CFG_MTHD_PBC_P - 1)

typedef swl_mask_m wld_wps_cfgMethod_m;

extern const char* cstr_WPS_CM_Supported[];
extern const char* cstr_AP_WPS_VERSUPPORTED[];
extern const char* cstr_AP_WPS_Status[];

enum {
    APWPSVERSI_AUTO,
    APWPSVERSI_10,
    APWPSVERSI_20,
    APWPSVERSI_UNKNOWN
};

typedef enum {
    APWPS_DISABLED,
    APWPS_ERROR,
    APWPS_CONFIGURED,
    APWPS_UNCONFIGURED,
    APWPS_SETUPLOCKED,
    APWPS_STATUS_MAX
} wld_wps_status_e;

#define WLD_MAX_OUI_NUM     6

extern const char* cstr_AP_EncryptionMode[];
typedef enum {
    APEMI_DEFAULT,
    APEMI_AES,
    APEMI_TKIP,
    APEMI_TKIPAES,
    APEMI_MAX
} wld_enc_modes_e;

typedef swl_ieee802_mbo_assocDisallowReason_e wld_mbo_denyReason_e;

extern const char* cstr_MultiAPType[];
typedef enum {
    MULTIAP_FRONTHAUL_BSS,
    MULTIAP_BACKHAUL_BSS,
    MULTIAP_BACKHAUL_STA,
    MULTIAP_MAX
} wld_multiap_type_e;
typedef uint32_t wld_multiap_type_m;
#define M_MULTIAP_FRONTHAUL_BSS (1 << MULTIAP_FRONTHAUL_BSS)
#define M_MULTIAP_BACKHAUL_BSS  (1 << MULTIAP_BACKHAUL_BSS)
#define M_MULTIAP_BACKHAUL_STA  (1 << MULTIAP_BACKHAUL_STA)
#define M_MULTIAP_ALL           ((1 << MULTIAP_MAX) - 1)

typedef enum {
    MULTIAP_NOT_SUPPORTED = 0, // Multi-AP functionality not supported
    MULTIAP_PROFILE_1 = 1,     // Supports EasyMesh R1 functionality
    MULTIAP_PROFILE_2,         // Supports EasyMesh R2 functionality
    MULTIAP_PROFILE_3,         // Supports EasyMesh R3 and greater functionalities
    MULTIAP_PROFILE_MAX
} wld_multiap_profile_e;

extern const char* wld_apRole_str[];
typedef enum {
    AP_ROLE_OFF,
    AP_ROLE_MAIN,
    AP_ROLE_RELAY,
    AP_ROLE_REMOTE,
    AP_ROLE_MAX
} wld_apRole_e;

typedef struct {
    int countryCode;
    char* fullCountryName;
    char* shortCountryName;
    swl_opClassCountry_e countryZone;
} T_COUNTRYCODE;

extern const T_COUNTRYCODE Rad_CountryCode[]; // wld_util.c

/**
 * List of strings matching the the current radio state.
 */
extern const char* cstr_chanmgt_rad_state[];
/**
 * Enumeration encoding the current radio state in detail.
 */
typedef enum {
    // Radio in unknown state
    CM_RAD_UNKNOWN,
    // Radio in down state
    CM_RAD_DOWN,
    // Radio in up state
    CM_RAD_UP,
    // Radio performing a foreground channel availability check, and as such is dormant
    CM_RAD_FG_CAC,
    // Radio performing a background channel availability check, expecting to move there when done
    CM_RAD_BG_CAC,
    // Radio performing a background channel availability check by external provider, expecting to move there when done
    CM_RAD_BG_CAC_EXT,
    // Radio performing a background channel availability check, not switching when done
    CM_RAD_BG_CAC_NS,
    // Radio performing a background channel availability check by external provider, not switching when done
    CM_RAD_BG_CAC_EXT_NS,
    // Radio currently undergoing configuration
    CM_RAD_CONFIGURING,
    // Radio in deep power down state
    CM_RAD_DEEP_POWER_DOWN,
    // Radio is starting and AP broadcast must be delayed
    CM_RAD_DELAY_AP_UP,
    // Radio in error state
    CM_RAD_ERROR,
    // End of enum, use for initializing array length
    CM_RAD_MAX
} chanmgt_rad_state;

/* PFN_WRAD_DFSRADARTRIGGER - "One-shot"-Radar simulation with optional sub-band modes */
typedef enum {
    DFST_RP_NONE,       /* Do nothing (default) */
    DFST_RP_ON_CH,      /* DFS Radar Pulse when ON-channel */
    DFST_RP_OFF_CH,     /* DFS Radar Pulse when OFF-Channel */
    DFST_RP_AUTO,       /* Driver default Radar Pulse (When on DFS "ON", when on !DFS "OFF") */
    DFST_RP_CLR_STATUS, /* Force to Clear Radar status */
    DFST_RP_CLR_OSC,    /* Force to Clear Out of Service channels */
    DFST_RP_MAX         /* End of enum, use for initializing array length */
} dfstrigger_rad_state;

/* This structure will be attached on the OBJECT void pointer of the datamodel */
typedef struct {
    FSM_STATE FSM_State;                      // Our main state
    FSM_STATE FSM_ReqState;                   // State to which vendor wants FSM to move, in certain places
    int FSM_Delay;                            // Delay the FSM with some cycles
    int FSM_ComPend;                          // Commit Pending?
    int FSM_Error;                            // Error Message
    int FSM_Warning;                          // Warning Message
    int FSM_Retry;                            // Retry the config?
    int FSM_SyncAll;                          // Force all states (Need Radio marking)
    int FSM_Loop;                             // Keep looping on the same FSM state (danger).
    int FSM_SrcVAP;                           // VAP State Run Count. Track if state has been done
    unsigned long FSM_BitActionArray[FSM_BW]; // 'bit' states?
    amxp_timer_t* timer;
    int TODC;                                 // TimeOut Delayed Commit
    amxp_timer_t* obj_DelayCommit;
    unsigned int timeout_msec;

    /* Isolate ongoing Active Commit (AC) actions from inbetween config/commits */
    unsigned long FSM_AC_BitActionArray[FSM_BW];

    /* The state of the FSM_BitActionArray at last dependency copy to AC" */
    unsigned long FSM_AC_CSC[FSM_BW];
    swl_timeMono_t FSM_ComPend_Start;         //time when commit pend started
    uint32_t retryCount;                      //how many times fase has been retried.
} T_FSM;

typedef struct {
    uint32_t nrStarts;                       // The number of time the FSM moved from IDLE to non IDLE
    uint32_t nrRunStarts;                    // The number of times the FSM moved to "run" mode
    uint32_t nrFinish;                       // The number of times the FSM moved to "run" mode
} wld_fsmStats_t;

typedef struct wld_apNeighbour {
    amxc_llist_it_t it;
    char bssid[ETHER_ADDR_LEN];
    char ssid[SSID_NAME_LEN];
    int information;
    swl_operatingClass_t operatingClass;
    int channel;
    int phyType;
    char nasIdentifier[NAS_IDENTIFIER_MAX_LEN];
    char r0khkey[KHKEY_MAX_LEN];
    char r1khkey[KHKEY_MAX_LEN];
    bool colocatedAp;
    amxd_object_t* obj;
} wld_apNeighbour_t;

/* Discovery methods to announce an AP
 *   - AP_DM_DISABLED: Disabled
 *   - AP_DM_RNR: Reduced Neighboring Report
 *   - AP_DM_UPR: Unsolicited Probe Response
 *   - AP_DM_FD: FILS Discovery
 *   - AP_DM_MAX: end of enum
 */
typedef enum {
    AP_DM_DEFAULT,
    AP_DM_DISABLED,
    AP_DM_RNR,
    AP_DM_UPR,
    AP_DM_FD,
    AP_DM_MAX,
} wld_ap_dm_e;

#define M_AP_DM_DEFAULT (1 << AP_DM_DEFAULT)
#define M_AP_DM_DISABLED (1 << AP_DM_DISABLED)
#define M_AP_DM_RNR (1 << AP_DM_RNR)
#define M_AP_DM_UPR (1 << AP_DM_UPR)
#define M_AP_DM_FD (1 << AP_DM_FD)
typedef uint32_t wld_ap_dm_m;
extern const char* g_str_wld_ap_dm[];

/* These base structures are based on the TR181 docs */
#define WLD_TEMP_INVALID_CELSIUS -274    // Temperature of The chipset, this is the default value which is INVALID.


typedef struct {
    uint32_t maxMuClients;      /* Maximum MU clients. */
    uint32_t nbMuClients;       /* current nbr of MU MIMO clients. (i.e admitted) */
    uint32_t nbActiveMuClients; /* current nbr of Active MU MIMO clients receiving data in MU Group. */
    uint32_t txAsMuPktsCnt;     /* current count of TX packets sent as MU. */
    time_t lastRefreshTime;     /* Timestamp of the last refresh of mu mimo info */
} wld_rad_muMimoInfo_t;

typedef struct {
    uint32_t muGroupId;     /* current MU Group ID (GID: 1-62). */
    uint32_t muUserPosId;   /* current MU User Position ID (GID: 1-4). */
    uint32_t txAsMuPktsCnt; /* current count of TX packets sent as MU. */
    uint32_t txAsMuPktsPrc; /* current percentage of TX packets sent as MU. */
} wld_sta_muMimoInfo_t;

/* From IEEE 802.11 Draft P802.11ax D8.0 fig 9-589cn */
#define D11_HE_MCS_0_7  0
#define D11_HE_MCS_0_9  1
#define D11_HE_MCS_0_11 2
#define D11_HE_MCS_NONE 3

#define D11_HE_MCS_SUBFIELD_SIZE 2
#define D11_HE_MCS_SUBFIELD_MASK 0x3

/*
 * @brief arguments for BSS Transfer request
 */
typedef struct {
    swl_macChar_t sta;                    /* MAC of the station to steer */
    swl_macChar_t targetBssid;            /* Bss transition request's target BSSID */
    swl_operatingClass_t operClass;       /* Operating class of target AP */
    swl_channel_t channel;                /* Channel of target AP */
    swl_ieee802_btmReqMode_m reqModeMask; /* request mode (prefered list, abridge, dissoc imminent, ...) */
    int validity;                         /* Timer of the bss request validity */
    int disassoc;                         /* Timer before device disassociation */
    uint32_t bssidInfo;                   /* BSSID Info data */
    int transitionReason;                 /* MBO Transition reason */
} wld_transferStaArgs_t;

typedef struct {
    swl_timeMono_t updateTime;
    swl_mcs_supMCS_t supportedMCS;
    swl_mcs_supMCS_t supportedHtMCS;
    swl_mcs_supMCS_adv_t supportedVhtMCS[SWL_COM_DIR_MAX];
    swl_mcs_supMCS_adv_t supportedHeMCS[SWL_COM_DIR_MAX];
    swl_mcs_supMCS_adv_t supportedHe160MCS[SWL_COM_DIR_MAX];
    swl_mcs_supMCS_adv_t supportedHe80x80MCS[SWL_COM_DIR_MAX];

    swl_oui_list_t vendorOUI;

    swl_security_apMode_e currentSecurity;
    wld_enc_modes_e encryptMode;
    swl_bandwidth_e linkBandwidth;
    bool linkBandwidthSetByDriver;

    swl_staCapHt_m htCapabilities;      /* bitmap of HT (11n) SGI, MU and beamforming capabilities */
    swl_staCapVht_m vhtCapabilities;    /* bitmap of VHT (11ac) SGI, MU and beamforming capabilities */
    swl_staCapHe_m heCapabilities;      /* bitmap of HE (11ax) SGI, MU and beamforming capabilities */
    swl_staCapEht_m ehtCapabilities;    /* bitmap of EHT (11be) SGI, MU and beamforming capabilities*/
    swl_staCapRrm_m rrmCapabilities;    /* bitmap of RRM Enabled Capabilties */
    uint32_t rrmOnChannelMaxDuration;   /* Operating Channel Max Measurement Duration */
    uint32_t rrmOffChannelMaxDuration;  /* NonOperating Channel Max Measurement Duration */
    swl_freqBandExt_m freqCapabilities; /* Frequency Capabilities */
} wld_assocDev_capabilities_t;

#define X_WLD_STA_HISTORY(X, Y) \
    X(Y, gtSwl_type_timeSpecMono, timestamp) \
    X(Y, gtSwl_type_int32, signalStrength) \
    X(Y, gtSwl_type_int32, noise) \
    X(Y, gtSwl_type_uint32, dataDownlinkRate) \
    X(Y, gtSwl_type_uint32, dataUplinkRate) \
    X(Y, gtSwl_type_uint64, txBytes) \
    X(Y, gtSwl_type_uint64, rxBytes) \
    X(Y, gtSwl_type_uint32, txPacketCount) \
    X(Y, gtSwl_type_uint32, rxPacketCount) \
    X(Y, gtSwl_type_uint32, txError) \
    X(Y, gtSwl_type_uint64, rxError) \
    X(Y, gtSwl_type_uint32, txFrameCount) \
    X(Y, gtSwl_type_uint32, rxFrameCount) \
    X(Y, gtSwl_type_uint32, tx_Retransmissions) \
    X(Y, gtSwl_type_uint32, tx_RetransmissionsFailed) \
    X(Y, gtSwl_type_uint32, rx_Retransmissions) \
    X(Y, gtSwl_type_uint32, rx_RetransmissionsFailed) \
    X(Y, gtSwl_type_uint32, retryCount) \
    X(Y, gtSwl_type_uint32, multipleRetryCount) \
    X(Y, gtSwl_type_uint32, inactive) \
    X(Y, gtSwl_type_uint32, powerSave) \
    X(Y, gtSwl_type_bandwidth, txLinkBandwidth) \
    X(Y, gtSwl_type_uint32, txSpatialStream) \
    X(Y, gtSwl_type_mcsStandard, txRateStandard) \
    X(Y, gtSwl_type_uint32, txMcsIndex) \
    X(Y, gtSwl_type_bandwidth, rxLinkBandwidth) \
    X(Y, gtSwl_type_uint32, rxSpatialStream) \
    X(Y, gtSwl_type_mcsStandard, rxRateStandard) \
    X(Y, gtSwl_type_uint32, rxMcsIndex)

SWL_TT_H(gtWld_staHistory, wld_staHistory_t, X_WLD_STA_HISTORY, );

typedef struct {
    uint8_t nr_valid_samples;
    uint8_t index_last_sample;
    wld_staHistory_t* samples;

    int32_t signalStrength;                       /* signalStrength at rssi monitor update */
    int32_t noise;                                /* noise at rssi monitor update */
    int32_t signalNoiseRatio;                     /* signalNoiseRatio at rssi monitor update */
    swl_timeSpecReal_t measurementTimestamp;      /* rssi monitor update timestamp */
    int32_t signalStrengthAssoc;                  /* first rssi monitor signalStrength since Wi-Fi assoc event */
    int32_t noiseAssoc;                           /* first rssi monitor noise since Wi-Fi assoc event */
    int32_t signalNoiseRatioAssoc;                /* first rssi monitor signalNoiseRatio since Wi-Fi assoc event */
    swl_timeSpecReal_t measurementTimestampAssoc; /* timestamp of first rssi monitor update since Wi-Fi assoc event */
} wld_assocDev_history_t;

/**
 * This structure represents a single "physical" link on a single frequency.
 * All values in this structure only apply to this physical link, and the radio on which
 * this link resides.
 */
typedef struct {
    amxd_object_t* object;
    amxc_llist_it_t it;
    T_AccessPoint* pAP;
    swl_macBin_t mac;
    uint32_t linkId;
    uint32_t index;
    bool active;
    uint64_t bytesSent;
    uint64_t bytesReceived;
    uint32_t packetsSent;
    uint32_t packetsReceived;
    uint32_t errorsSent;
    uint32_t errorsReceived;
    int32_t signalStrength;
    int32_t noise;
    uint32_t lastDataDownlinkRate;
    uint32_t lastDataUplinkRate;
    swl_mcs_t upLinkRateSpec;              /* Up link rate info (standard, mcs index, guard interval, number of spacial streams, bandwidth) */
    swl_mcs_t downLinkRateSpec;            /* down link rate info (standard, mcs index, guard interval, number of spacial streams, bandwidth) */
} wld_affiliatedSta_t;

typedef struct {
    amxc_llist_it_t entry;
    bool active;
    char* name;
    int32_t index;
    swl_macBin_t bStaMac;
    T_AccessPoint* ap;
} wld_wds_intf_t;

typedef struct {
    char Name[32];                            /* Name tag.*/
    unsigned char MACAddress[ETHER_ADDR_LEN]; /* MAC address of station */
    amxd_object_t* object;
    char Radius_CUID[256];                    /* Chargeable-User-Identity attribute in Radius Access-Accept */
    bool AuthenticationState;                 /* Associate STA or device authenticated? */
    uint32_t LastDataDownlinkRate;
    uint32_t LastDataUplinkRate;
    int32_t SignalStrength;
    int32_t minSignalStrength;
    swl_timeMono_t minSignalStrengthTime;
    int32_t maxSignalStrength;
    swl_timeMono_t maxSignalStrengthTime;
    int32_t meanSignalStrength;
    int32_t meanSignalStrengthExpAccumulator;
    int32_t meanSignalStrengthLinearAccumulator;
    uint32_t nrMeanSignalStrength;
    double SignalStrengthByChain[MAX_NR_ANTENNA]; /* dBm */
    double noiseByChain[MAX_NR_ANTENNA];          /* dBm */
    int32_t AvgSignalStrengthByChain;             /* dBm */
    int32_t noise;                                /* dBm */
    int32_t SignalNoiseRatio;                     /* dB */
    uint32_t Retransmissions;
    uint32_t Rx_Retransmissions;
    uint32_t Tx_Retransmissions;
    uint32_t Tx_RetransmissionsFailed;
    uint32_t Rx_RetransmissionsFailed;
    uint32_t retryCount;         /* The number of packets that were successfully transmitted after one or more retransmissions */
    uint32_t multipleRetryCount; /* The number of packets that were successfully transmitted after more than one retransmission */
    bool Active;                 /* Whether station is actually connected, potentially not Authenticated */
    bool seen;                   /* data field for maclist updates */
    bool operatingStandardSetByDriver;
    swl_radStd_e operatingStandard;

    wld_assocDev_capabilities_t assocCaps;
    wld_assocDev_capabilities_t probeReqCaps;

    swl_uniiBand_m uniiBandsCapabilities;  /* bitmap of UNII bands capabilities */
    swl_timeSpecMono_t lastSampleTime;     /* mono time of last sample update */
    swl_timeSpecMono_t lastSampleSyncTime; /* mono time of last update to data model */
    swl_timeMono_t lastProbeCapUpdateTime; /* mono time when probe info was last updated */

    uint32_t Inactive;                     /* seconds */
    uint32_t RxPacketCount;
    uint32_t TxPacketCount;
    uint64_t RxBytes;
    uint64_t TxBytes;
    uint32_t RxUnicastPacketCount;
    uint32_t TxUnicastPacketCount;
    uint32_t RxMulticastPacketCount;
    uint32_t TxMulticastPacketCount;
    uint32_t TxFailures;
    uint64_t RxFailures;

    uint32_t TxFrameCount;     /* total of user frames sent successfully */
                               /* the difference is that a packet is a unit of transmission as requested by upper
                                  layer and a frame is a unit of transmission sent on the wire, possible aggregating
                                  packets if small, or splitting them if big.*/
    uint32_t RxFrameCount;
    uint32_t UplinkMCS;
    uint32_t UplinkBandwidth;  /* Uplink channel bandwidth in MHz (20/40/80/160)*/
    uint32_t UplinkShortGuard; /* 400ns guard interval */

    uint32_t DownlinkMCS;
    uint32_t DownlinkBandwidth; /* Downlink channel bandwidth in MHz (20/40/80/160)*/
    uint32_t DownlinkShortGuard;

    swl_mcs_t upLinkRateSpec;              /* Up link rate info (standard, mcs index, guard interval, number of spacial streams, bandwidth) */
    swl_mcs_t lastNonLegacyUplinkMCS;
    swl_timeMono_t lastNonLegacyUplinkTime;
    swl_mcs_t downLinkRateSpec;            /* down link rate info (standard, mcs index, guard interval, number of spacial streams, bandwidth) */
    swl_mcs_t lastNonLegacyDownlinkMCS;
    swl_timeMono_t lastNonLegacyDownlinkTime;

    uint16_t MaxRxSpatialStreamsSupported; /* number of Rx Spatial streams*/
    uint16_t MaxTxSpatialStreamsSupported; /* number of Tx Spatial streams*/
    uint32_t MaxDownlinkRateSupported;
    uint32_t MaxDownlinkRateReached;
    uint32_t MaxUplinkRateSupported;
    uint32_t MaxUplinkRateReached;
    swl_bandwidth_e MaxBandwidthSupported;

    bool powerSave;
    int deviceType;                        /* The device type: index of device type in the cstr_DEVICE_TYPES */
    int devicePriority;                    /* The priority of this device. Secondary to device type. */
    swl_timeMono_t latestStateChangeTime;  /* Timestamp of the last time the state of this device changed */
    int32_t rssiAccumulator;               /* Accumulator for RSSi data */
    int32_t rssiLevel;                     /* Latest RSSi event */
    swl_staCap_m capabilities;             /* Capabilities of the station */
    swl_staCapVendor_m vendorCapabilities; /* Vendor Capabilities of the station (MS, WFA) */
    uint32_t connectionDuration;           /* duration of connexion (seconds) */
    swl_timeMono_t associationTime;        /* Timestamp of the last time the sta finalized association, i.e. got authorized */
    swl_timeMono_t disassociationTime;     /* Timestamp of the last time the sta disassociate, in real time */
    bool hadSecFailure;                    /* Number of security failures of this station */
    wld_sta_muMimoInfo_t staMuMimoInfo;
    wld_assocDev_history_t* staHistory;
    swl_IEEE80211deauthReason_ne lastDeauthReason; /* last deauth reason for this sta */
    swl_timeMono_t lastDeauthAssocTime;            /* association timestamp of last de-authenticated connection */
    void* vendor;                                  /* Pointer for wifi chipset vendor data */
    wld_extMod_dataList_t extDataList;             /* list of extention data for non-chipset vendor modules */
    swla_dm_objActionReadCtx_t onActionReadCtx;
    uint32_t nrCreatedAffiliatedStaLinks;
    amxc_llist_t affiliatedStaList;         /* list of wld_affiliatedSta_t objects */
    wld_wds_intf_t* wdsIntf;                /* wds interface info */
    amxp_timer_t* delayDisassocNotif;
    swl_mlo_mode_e mloMode;                 /* the Mlo mode */
} T_AssociatedDevice;


SWL_ARRAY_TYPE_H(gtWld_signalStatArray, gtSwl_type_double, MAX_NR_ANTENNA);


#define X_T_ASSOCIATED_DEVICE_ANNOT(X, Y) \
    X(Y, gtSwl_type_macBin, MACAddress, "MACAddress") \
    X(Y, gtSwl_type_bool, AuthenticationState, "AuthenticationState") \
    X(Y, gtSwl_type_uint32, LastDataDownlinkRate, "LastDataDownlinkRate") \
    X(Y, gtSwl_type_uint32, LastDataUplinkRate, "LastDataUplinkRate") \
    X(Y, gtSwl_type_int32, SignalStrength, "SignalStrength") \
    X(Y, gtWld_signalStatArray, SignalStrengthByChain, "SignalStrengthByChain") \
    X(Y, gtWld_signalStatArray, noiseByChain, "NoiseByChain") \
    X(Y, gtSwl_type_int32, noise, "Noise") \
    X(Y, gtSwl_type_int32, SignalNoiseRatio, "SignalNoiseRatio") \
    X(Y, gtSwl_type_uint32, Retransmissions, "Retransmissions") \
    X(Y, gtSwl_type_uint32, Rx_Retransmissions, "Rx_Retransmissions") \
    X(Y, gtSwl_type_uint32, Rx_RetransmissionsFailed, "Rx_RetransmissionsFailed") \
    X(Y, gtSwl_type_uint32, Tx_Retransmissions, "Tx_Retransmissions") \
    X(Y, gtSwl_type_uint32, Tx_RetransmissionsFailed, "Tx_RetransmissionsFailed") \
    X(Y, gtSwl_type_uint32, retryCount, "RetryCount") \
    X(Y, gtSwl_type_uint32, multipleRetryCount, "MultipleRetryCount") \
    X(Y, gtSwl_type_bool, Active, "Active") \
    X(Y, gtSwl_type_radStd, operatingStandard, "OperatingStandard") \
    X(Y, gtSwl_type_uint32, Inactive, "Inactive") \
    X(Y, gtSwl_type_uint32, RxPacketCount, "RxPacketCount") \
    X(Y, gtSwl_type_uint32, TxPacketCount, "TxPacketCount") \
    X(Y, gtSwl_type_uint64, RxBytes, "RxBytes") \
    X(Y, gtSwl_type_uint64, TxBytes, "TxBytes") \
    X(Y, gtSwl_type_uint32, RxUnicastPacketCount, "RxUnicastPacketCount") \
    X(Y, gtSwl_type_uint32, TxUnicastPacketCount, "TxUnicastPacketCount") \
    X(Y, gtSwl_type_uint32, RxMulticastPacketCount, "RxMulticastPacketCount") \
    X(Y, gtSwl_type_uint32, TxMulticastPacketCount, "TxMulticastPacketCount") \
    X(Y, gtSwl_type_uint32, TxFailures, "TxErrors") \
    X(Y, gtSwl_type_uint64, RxFailures, "RxErrors") \
    X(Y, gtSwl_type_uint32, TxFrameCount, "TxFrameCount") \
    X(Y, gtSwl_type_uint32, RxFrameCount, "RxFrameCount") \
    X(Y, gtSwl_type_uint32, UplinkMCS, "UplinkMCS") \
    X(Y, gtSwl_type_uint32, UplinkBandwidth, "UplinkBandwidth") \
    X(Y, gtSwl_type_uint32, UplinkShortGuard, "UplinkShortGuard") \
    X(Y, gtSwl_type_uint32, DownlinkMCS, "DownlinkMCS") \
    X(Y, gtSwl_type_uint32, DownlinkBandwidth, "DownlinkBandwidth") \
    X(Y, gtSwl_type_uint32, DownlinkShortGuard, "DownlinkShortGuard") \
    X(Y, gtSwl_type_mcs, upLinkRateSpec, "UplinkRateSpec") \
    X(Y, gtSwl_type_mcs, downLinkRateSpec, "DownlinkRateSpec") \
    X(Y, gtSwl_type_uint16, MaxRxSpatialStreamsSupported, "MaxRxSpatialStreamsSupported") \
    X(Y, gtSwl_type_uint16, MaxTxSpatialStreamsSupported, "MaxTxSpatialStreamsSupported") \
    X(Y, gtSwl_type_uint32, MaxDownlinkRateSupported, "MaxDownlinkRateSupported") \
    X(Y, gtSwl_type_uint32, MaxDownlinkRateReached, "MaxDownlinkRateReached") \
    X(Y, gtSwl_type_uint32, MaxUplinkRateSupported, "MaxUplinkRateSupported") \
    X(Y, gtSwl_type_uint32, MaxUplinkRateReached, "MaxUplinkRateReached") \
    X(Y, gtSwl_type_bandwidthUnknown, MaxBandwidthSupported, "MaxBandwidthSupported") \
    X(Y, gtSwl_type_bool, powerSave, "PowerSave") \
    X(Y, gtSwl_type_mlo_mode, mloMode, "MLOMode")


SWL_NTT_H_ANNOTATE(gtWld_associatedDevice, T_AssociatedDevice, X_T_ASSOCIATED_DEVICE_ANNOT);

typedef struct wld_nasta {
    amxc_llist_it_t it;
    swl_macBin_t bssid;                       /* Must be set if the station's bssid is required by the driver */
    unsigned char MACAddress[ETHER_ADDR_LEN]; /* MAC address of station */
    int32_t SignalStrength;
    int32_t monRssi;
    int32_t rssiAccumulator;
    swl_timeMono_t TimeStamp;
    amxd_object_t* obj;
    swl_channel_t channel;
    swl_operatingClass_t operatingClass;
    void* vendorData;
} wld_nasta_t;

typedef enum {
    HE_CAP_DEFAULT,
    HE_CAP_DL_OFDMA,
    HE_CAP_UL_OFDMA,
    HE_CAP_DL_MUMIMO,
    HE_CAP_UL_MUMIMO,
    HE_CAP_STA_UL_OFDMA,
    HE_CAP_STA_UL_MUMIMO,
    HE_CAP_HE_ER_SU_PPDU_RX,
    HE_CAP_MAX
} wld_he_cap_e;

#define M_HE_CAP_DEFAULT (1 << HE_CAP_DEFAULT)
#define M_HE_CAP_DL_OFDMA (1 << HE_CAP_DL_OFDMA)
#define M_HE_CAP_UL_OFDMA (1 << HE_CAP_UL_OFDMA)
#define M_HE_CAP_DL_MUMIMO (1 << HE_CAP_DL_MUMIMO)
#define M_HE_CAP_UL_MUMIMO (1 << HE_CAP_UL_MUMIMO)
#define M_HE_CAP_STA_UL_OFDMA (1 << HE_CAP_STA_UL_OFDMA)
#define M_HE_CAP_STA_UL_MUMIMO (1 << HE_CAP_STA_UL_MUMIMO)
#define M_HE_CAP_ER_SU_PPDU_RX (1 << HE_CAP_HE_ER_SU_PPDU_RX)
typedef uint32_t wld_he_cap_m;
extern const char* g_str_wld_he_cap[];


typedef enum {
    RAD_BF_CAP_DEFAULT,
    RAD_BF_CAP_VHT_SU,
    RAD_BF_CAP_VHT_MU,
    RAD_BF_CAP_HE_SU,
    RAD_BF_CAP_HE_MU,
    RAD_BF_CAP_HE_CQI,
    RAD_BF_CAP_EHT_SU,
    RAD_BF_CAP_EHT_MU_80MHZ,
    RAD_BF_CAP_EHT_MU_160MHZ,
    RAD_BF_CAP_EHT_MU_320MHZ,
    RAD_BF_CAP_MAX
} wld_rad_bf_cap_e;

#define M_RAD_BF_CAP_DEFAULT (1 << RAD_BF_CAP_DEFAULT)
#define M_RAD_BF_CAP_VHT_SU (1 << RAD_BF_CAP_VHT_SU)
#define M_RAD_BF_CAP_VHT_MU (1 << RAD_BF_CAP_VHT_MU)
#define M_RAD_BF_CAP_HE_SU (1 << RAD_BF_CAP_HE_SU)
#define M_RAD_BF_CAP_HE_MU (1 << RAD_BF_CAP_HE_MU)
#define M_RAD_BF_CAP_HE_CQI (1 << RAD_BF_CAP_HE_CQI)
#define M_RAD_BF_CAP_EHT_SU (1 << RAD_BF_CAP_EHT_SU)
#define M_RAD_BF_CAP_EHT_MU_80MHZ (1 << RAD_BF_CAP_EHT_MU_80MHZ)
#define M_RAD_BF_CAP_EHT_MU_160MHZ (1 << RAD_BF_CAP_EHT_MU_160MHZ)
#define M_RAD_BF_CAP_EHT_MU_320MHZ (1 << RAD_BF_CAP_EHT_MU_320MHZ)
typedef uint32_t wld_rad_bf_cap_m;
extern const char* g_str_wld_rad_bf_cap[];

typedef enum {
    CHAN_REASON_INVALID,
    CHAN_REASON_UNKNOWN,
    CHAN_REASON_INITIAL,
    CHAN_REASON_PERSISTANCE, // Load of first saved channel
    CHAN_REASON_AUTO,        //Automatic channel's selection algorith
    CHAN_REASON_DFS,
    CHAN_REASON_MANUAL,
    CHAN_REASON_REENTRY,
    CHAN_REASON_OBSS_COEX, //coexistence activated
    CHAN_REASON_EP_MOVE,   // Endpoint moving
    CHAN_REASON_RESET,     // plugin reset channel if an invalid change occurred
    CHAN_REASON_MAX
} wld_channelChangeReason_e;
extern const char* g_wld_channelChangeReason_str[];

typedef struct {
    amxc_llist_it_t it;
    unsigned char MACAddress[ETHER_ADDR_LEN]; /* MAC address of station */
} T_ConnectionAttempt;

//Maximum string size of a capability, including null termination
#define MAX_CAP_SIZE 17
/**
 * Structure representing a radio capability.
 * Each vendor plugin will have a list of these elements, indicating which capabilities can can
 * potentially support.
 */
typedef struct {

    /**
     * The name of the capability
     */
    const char* Name;
    /**
     * The change handler of the capability. The change handler can be NULL.
     * The change handler will be called AFTER the enable field has been updated to the new value.
     * It will only be called when the enable field is changed.
     */
    void (* changeHandler)(struct WLD_RADIO* pR);
} T_Radio_Capability;

/**
 * Structure representing an radio's status of a capability.
 * Each radio will have have a list of these capabilities. The amount of elements in this list must match
 * the amount of elements in the vendor capability list.
 */
typedef struct {
    /**
     * This radio is capable of supporting this feature.
     */
    bool capable;
    /**
     * The radio is requested to have this feature enabled.
     */
    bool enable;
    /**
     * The current status of this feature.
     */
    bool status;
} T_Radio_Cap_Item;

/* Local WPS parameters */
typedef struct {
    char DefaultPin[12];        /* 8-digit format wps pin  */
    char DevName[32];           /* Device name             */
    char OUI[16];               /* Device OUI              */
    char FriendlyName[32];      /* Device friendly name    */
    char Manufacturer[32];      /* Manufacture Name        */
    char ManufacturerUrl[128];  /* Manufacture URL link    */
    char ModelDescription[64];  /* Full model description  */
    char ModelName[32];         /* Vendor name             */
    char ModelNumber[32];       /* Vendor number           */
    char ModelUrl[128];         /* Vendor URL link         */
    char OsVersion[32];         /* OS version ID (A.B.C.D) */
    char SerialNumber[32];      /* Serial number (partly)  */
    char UUID[40];              /* UUID                    */
    int wpsSupVer;              /* WPS version supported   */
    int wpsUUIDShared;          /* Same WPS UUID on plugin */
} T_CONST_WPS;

struct S_CWLD_FUNC_TABLE;

/* Store a delayed WPS_pushButton command */
typedef struct S_WPS_pushButton_Delay {
    amxp_timer_t* timer;           /* timer handler       */
    union {
        struct T_AccessPoint* vap; /* Keep handler of the VAP */
        struct S_EndPoint* pEP;
    } intf;
    char val[128];    /* Save our command        */
    int bufsize;      /* Filled buffer size      */
    int setAct;       /* Action taken            */
    uint64_t call_id; /* Function call handle    */
    amxc_var_t* args; /* Arguments provided to the function */
} T_WPS_pushButton_Delay;

/* Store wps session info: to clear wps pairing timer. */
typedef struct {
    amxp_timer_t* sessionTimer; /* wps session timer       */
    amxd_object_t* intfObj;     /* VAP/EP obj */
    bool WPS_PairingInProgress; /* bool representing whether pairing is in progress */
    swl_macBin_t peerMac;       /* remote device MAC (Bssid for WPS enrollee (EP), Sta Mac for WPS registrar (AP)) */
    T_AccessPoint* pReferenceApRelay;
    bool addRelayApCredentials;
} wld_wpsSessionInfo_t;


//Note : scan type is more of what triggered the scanning not what the result has!
typedef enum {
    SCAN_TYPE_NONE,     // Default none type
    SCAN_TYPE_INTERNAL, // Internal middleware to know AP environment => no ODL update
    SCAN_TYPE_SSID,     // Scan done to get SSID list => need to update SSID ODL
    SCAN_TYPE_SPECTRUM, // Scan done to get Spectrum info => need to update spectrum info
    SCAN_TYPE_COMBINED, // Scan done to get SSID list and spectrum info => update ODL
    SCAN_TYPE_MAX
} wld_scan_type_e;


typedef struct {
    char* scanReason;
    uint32_t totalCount;
    uint32_t successCount;
    uint32_t errorCount;
    amxc_llist_it_t it;
    amxd_object_t* object;
} wld_scanReasonStats_t;

typedef struct wld_scanResultSSID {
    uint8_t ssidLen;
    uint8_t ssid[SSID_NAME_LEN];
    swl_macBin_t bssid;
    int32_t snr;
    int32_t noise;
    int32_t rssi;
    int32_t channel;
    int32_t centreChannel;
    int32_t bandwidth;
    uint16_t beaconInterval;
    uint8_t dtimPeriod;
    swl_chanspec_ext_e extensionChannel;

    // Not always filled in. In that case it is 0.
    uint16_t basicDataTransferRates;
    uint16_t supportedDataTransferRates;
    swl_radioStandard_m operatingStandards;
    swl_radioStandard_m supportedStandards;

    swl_security_apMode_e secModeEnabled;
    swl_security_mfpMode_e mfpMode;
    swl_wps_cfgMethod_m WPS_ConfigMethodsEnabled;
    bool hasActiveWpsRegistrar;
    bool adhoc;
    int32_t linkrate;
    swl_security_encMode_e encryptionMode;

    amxc_llist_t vendorIEs;
    amxc_llist_it_t it;

    swl_80211_htCapInfo_m htCaps;
    swl_80211_vhtCapInfo_m vhtCaps;
    swl_80211_heCapInfo_m heCaps;
    swl_operatingClass_t operClass; // global operating class

    //BSS load elements
    uint16_t stationCount;
    uint8_t channelUtilization;
} wld_scanResultSSID_t;

typedef struct wld_scanResults {
    amxc_llist_t ssids;
} wld_scanResults_t;

typedef struct wld_scanArgs {
    char ssid[SSID_NAME_LEN];
    swl_macBin_t bssid;               /*particular BSSID MAC address to scan*/
    int ssidLen;
    uint8_t chanlist[WLD_MAX_POSSIBLE_CHANNELS];
    int chanCount;
    bool updateUsageStats;
    bool fastScan;
    char reason[64];
    bool enableFlush;
} wld_scanArgs_t;

#define SCAN_REASON_MAX 5

typedef struct {
    uint32_t nrScanRequested;
    uint32_t nrScanDone;
    uint32_t nrScanError;
    uint32_t nrScanBlocked;
    amxc_llist_t extendedStat;
} wld_scan_stats_t;

typedef struct {
    int32_t homeTime;
    int32_t activeChannelTime;
    int32_t passiveChannelTime;
    int32_t scanRequestInterval;
    int32_t scanChannelCount;
    int32_t maxChannelsPerScan;
    char* fastScanReasons;
    wld_scanArgs_t scanArguments;
    bool enableScanResultsDm;
} wld_scan_config_t;

typedef struct {
    swl_function_deferredInfo_t scanFunInfo;
    int32_t minRssi;
    char scanReason[16];
    wld_scan_type_e scanType;
    wld_scan_stats_t stats;
    wld_scan_config_t cfg;
    swl_timeMono_t lastScanTime;
    wld_scanResults_t lastScanResults;
    amxc_llist_t spectrumResults;   /*!< results of the getSpectrum */
} T_ScanState;

typedef struct wld_airStats {
    uint16_t load;
    int32_t noise;
    uint16_t bss_transmit_time; // time spent sending signal
    uint16_t bss_receive_time;  // time spent receiving signal from within bss
    uint16_t other_bss_time;    // time receiving signal from other bss
    uint16_t other_time;        // time spent handling "other than wifi" signals
    uint16_t free_time;         // time spent not sending nor receiving anything
    uint16_t total_time;        // total time should match tx + rx + other_bss + noise + free
    uint32_t timestamp;
    uint8_t short_preamble_error_percentage;
    uint8_t long_preamble_error_percentage;
    amxc_var_t* vendorStats;
} wld_airStats_t;

typedef struct wld_radioWiFi7Cap {
    bool emlmrSupported;
    bool emlsrSupported;
    bool strSupported;
    bool nstrSupported;
} wld_radioWiFi7Cap_t;

typedef struct wld_radioCap {
    wld_radioWiFi7Cap_t apCap7;
    wld_radioWiFi7Cap_t staCap7;
} wld_radioCap_t;

typedef struct {
    T_Radio* pRad;
    bool start;   //set to true if start, false if stop
    bool success; // only relevant if start == false (i.e. scan stopping). Provides result of scan.
    wld_scan_type_e scanType;
    const char* scanReason;
} wld_scanEvent_t;

typedef struct {
    bool running;
    uint64_t call_id;
    amxp_timer_t* timer;
} wld_fcallState_t;

typedef struct {
    uint32_t counter;
    amxd_object_t* object;
    swl_timeMono_t lastEventTime;
} T_EventCounter;

typedef struct {
    const char** names;
    T_EventCounter* values;
    uint32_t nrCounters;
} T_EventCounterList;

typedef enum {
    WLD_FEATURE_SEAMLESS_DFS,
    WLD_FEATURE_BG_DFS,
    WLD_FEATURE_MAX
} wld_radio_features;

typedef enum {
    WLD_RAD_FS_NOT_AVAIL,
    WLD_RAD_FS_DISABLED,
    WLD_RAD_FS_ENABLED
} wld_feature_status;

typedef enum {
    WLD_RAD_EV_FSM_COMMIT,
    WLD_RAD_EV_FSM_RESET,
    WLD_RAD_EV_DOUBLE_ASSOC,
    WLD_RAD_EV_EP_FAIL,
    WLD_RAD_EV_MAX
} wld_rad_event_counters;

typedef struct {
    char* name;
    bool active;
    bool enabled;
    bool running;
    uint32_t interval;
    amxp_timer_t* timer;
    amxd_object_t* object;
    void* userData;
    void (* callback_fn)(void* userData);
} T_Monitor;

typedef struct {
    T_Monitor monitor;
    uint32_t rssiInterval;
    uint32_t averagingFactor;
    bool historyEnable;
    uint32_t historyLen;
    uint32_t historyIntervalCoeff;
    bool sendPeriodicEvent;
    bool sendEventOnAssoc;
    bool sendEventOnDisassoc;
} T_RssiEventing;

extern const char* Rad_RadarZones[];
typedef enum
{
    RADARZONE_NONE,
    RADARZONE_ETSI,
    RADARZONE_STG,
    RADARZONE_UNCLASSIFIED,
    RADARZONE_FCC,
    RADARZONE_JP,
    RADARZONES_MAX,
} wld_radarzone_e;

typedef struct {
    uint16_t channel;
    time_t timestamp;
    swl_bandwidth_e bandwidth;
    wld_radarzone_e radarZone;
    uint8_t radarIndex;
} T_DFSEvent;

typedef enum {
    TPCMODE_OFF,
    TPCMODE_STA,
    TPCMODE_AP,
    TPCMODE_APSTA,
    TPCMODE_AUTO,
    TPCMODE_MAX,
} wld_tpcMode_e;
extern const char* g_str_wld_tpcMode[];

typedef struct {
    int txBurst;                       /* enable / disable fragment burst, -1 is driver default */
    int amsdu;                         /* enable / disable amsdu, -1 is driver default */
    int ampdu;                         /* enable / disable ampdu, -1 is driver default */
    int fragThreshold;                 /* configure fragmentation threshold, -1 is driver default */
    int rtsThreshold;                  /* configure rts threshold, -1 is driver default */
    int txBeamforming;                 /* configure force tx beam forming, -1 is driver default */
    int vhtOmnEnabled;                 /* enable vht Operating mode notifications */
    int broadcastMaxBwCapability;      /* broadcast max bw capability.*/
    wld_tpcMode_e tpcMode;             /* configure transmit power control. */
    bool skipSocketIO;                 /* bool to skip socket IO */
} wld_driverCfg_t;


typedef struct {
    bool useBaseMacOffset;             /* whether to use the base mac offset to calculate mac */
    bool useLocalBitForGuest;          /* use local bit to make guest mac addresses. */
    int64_t baseMacOffset;             /* offset value by which the base mac of radio should be offsetted from WAN_ADDR */
    int64_t localGuestMacOffset;       /* offset value by which local macs should be incremented compared to base radio mac */
    uint32_t nrBssRequired;            /* number of bss required, to be used for MAC Addr management */
} wld_macCfg_t;

typedef enum {
    RIFS_MODE_DEFAULT,
    RIFS_MODE_AUTO,
    RIFS_MODE_OFF,
    RIFS_MODE_ON,
    RIFS_MODE_MAX
} wld_rifs_mode_e;



typedef enum {
    CHANNEL_INTERNAL_STATUS_CURRENT,
    CHANNEL_INTERNAL_STATUS_TARGET,
    CHANNEL_INTERNAL_STATUS_SYNC,
    CHANNEL_INTERNAL_STATUS_MAX,
} wld_rad_channelInternalStatus_e;

typedef struct {
    swl_chanspec_t chanspec;
    wld_channelChangeReason_e reason;
    char reasonExt[128];
    swl_timeMono_t changeTime;
    swl_trl_e isApplied; //True:applied, False:failed, Unknown:ongoing
} wld_rad_detailedChanState_t;

typedef enum {
    MBSSID_ADVERTISEMENT_MODE_AUTO,
    MBSSID_ADVERTISEMENT_MODE_OFF,
    MBSSID_ADVERTISEMENT_MODE_ON,
    MBSSID_ADVERTISEMENT_MODE_ENHANCED,
    MBSSID_ADVERTISEMENT_MODE_MAX
} wld_mbssidAdvertisement_mode_e;

typedef swl_mask_m wld_mbssidAdvertisement_mode_m;

#define M_WLD_MBSSID_ADS_MODE_ON (1 << MBSSID_ADVERTISEMENT_MODE_ON)
#define M_WLD_MBSSID_ADS_MODE_ENHANCED (1 << MBSSID_ADVERTISEMENT_MODE_ENHANCED)

typedef struct {
    uint8_t heBssColor;
    uint8_t heBssColorPartial;
    bool heHESIGASpatialReuseValue15Allowed;
    bool heSRGInformationValid;
    bool heNonSRGOffsetValid;
    bool hePSRDisallowed;
    uint8_t heSprNonSrgObssPdMaxOffset;
    uint8_t heSprSrgObssPdMinOffset;
    uint8_t heSprSrgObssPdMaxOffset;
    char heSprSrgBssColors[HE_SPR_SRG_BSS_COLORS_MAX_LEN];
    char heSprSrgPartialBssid[HE_SPR_SRG_PARTIAL_BSSID_MAX_LEN];
} wld_cfg11ax_t;

typedef enum {
    HE_SR_CONTROL_PSR_DISALLOWED,
    HE_SR_CONTROL_NON_SRG_OBSS_PD_SR_DISALLOWED,
    HE_SR_CONTROL_NON_SRG_OFFSET_PRESENT,
    HE_SR_CONTROL_SRG_INFORMATION_PRESENT,
    HE_SR_CONTROL_HESIGA_SPATIAL_REUSE_VALUE15_ALLOWED,
    HE_SR_CONTROL_MAX

} wld_he_sr_control_e;

typedef uint32_t wld_he_sr_control_m;

typedef enum {
    PREAMBLE_TYPE_SHORT,
    PREAMBLE_TYPE_LONG,
    PREAMBLE_TYPE_AUTO,
    PREAMBLE_TYPE_MAX
} wld_preamble_type_e;

struct WLD_RADIO {
    int debug;   /* FIX ME */
    vendor_t* vendor;
    wld_status_e status;
    wld_status_changeInfo_t changeInfo;
    int maxBitRate;
    int index;                          /* Network device number */
    uint64_t wDevId;                    /* nl80211 wireless device id */
    int ref_index;                      /* Radio index number (0=wifi0,1=wifi1,...) */
    bool isReady;                       /* Radio is ready for config and management (i.e. caps have been filled in, struct init done) */
    swl_freqBandExt_m supportedFrequencyBands;
    swl_freqBandExt_e operatingFrequencyBand;

    // Never contains `M_SWL_RADSTD_AUTO`.
    swl_radioStandard_m supportedStandards;

    // Either `M_SWL_RADSTD_AUTO`, or a subset of `supportedStandards` different from 0.
    swl_radioStandard_m operatingStandards;

    // Used to parse the string into `operatingStandards`, so you do not need
    // to look at `operatingStandardsFormat` when looking at `operatingStandards`
    // because dealing with `operatingStandardsFormat` is taken care of already.
    swl_radStd_format_e operatingStandardsFormat;

    swl_channel_t possibleChannels[WLD_MAX_POSSIBLE_CHANNELS];
    int nrPossibleChannels;
    swl_channel_t radarDetectedChannels[WLD_MAX_POSSIBLE_CHANNELS];
    uint8_t nrRadarDetectedChannels;
    swl_channel_t lastRadarChannelsAdded[SWL_BW_CHANNELS_MAX];
    uint8_t nrLastRadarChannelsAdded;

    unsigned char MACAddr[ETHER_ADDR_LEN];
    swl_macBin_t mbssBaseMACAddr;                  /* multiple BSS base mac address: */
                                                   /* may be shifted radio base mac, to handle enough bss macs. */

    /* Add STA & WDS support. */
    bool isAP;                                     /* Currenlty FIX TRUE! */
    bool isSTA;                                    /* as both isAP && isSTA is true, we've APSTA */
    bool isWDS;                                    /* Wireless Distributed System - 4MAC mode */
    bool isWET;                                    /* Wireless Ethernet bridging - 3MAC + STA MAC/IP NAT */
    bool isSTASup;                                 /* Is STA supported? Has impact the way we manage the Radio! */
    bool isWPSEnrol;                               /* Set WPS Enrollee mode when STA is active! */

    char channelsInUse[64];                        /* String presentation of used channels */
    swl_channel_t channel;                         /* When set we set autoChannelEnable FALSE */
    wld_channelChangeReason_e channelChangeReason; /* The cause of the last channel change */
    int autoChannelSupported;
    /**
     * Indicates whether automatic channel selection is managed by an external process
     * When set, autoChannelSetByUser will reflect the automatic channel selection state (enabled/disabled)
     * and the autoChannelEnable is not set to avoid calling the LL APIs implementation for automatic channel selection.
     **/
    bool externalAcsMgmt;
    /**
     * Indicates whether autochannel is enabled internally for
     * When set, we set channel on 0.
     **/
    bool autoChannelEnable;
    int autoChannelSetByUser;                               /* Keep track of UI interaction on the autoChannelEnable */
    int autoChannelRefreshPeriod;
    swl_radBw_e operatingChannelBandwidth;                  /* bandwidth as requested by user */
    swl_radBw_e runningChannelBandwidth;                    /* bandwidth currently configured */
    swl_radBw_m supportedChannelBandwidth;                  /* SupportedBandwidths */
    wld_channelChangeReason_e channelBandwidthChangeReason; /* The cause of the last channel's Bandwidth change */
    swl_bandwidth_e maxChannelBandwidth;                    /* max available bandwidth */
    wld_rad_channelInternalStatus_e channelShowing;
    wld_rad_detailedChanState_t targetChanspec;
    swl_function_deferredInfo_t callIdReqChanspec;
    amxp_timer_t* timerReqChanspec;
    wld_rad_detailedChanState_t currentChanspec;
    swl_chanspec_t userChanspec;
    uint16_t totalNrTargetChanspecChanges;                  /* Total number of target chanspec changes */
    uint16_t totalNrCurrentChanspecChanges;                 /* Total number of current chanspec changes */
    uint16_t channelChangeCounters[CHAN_REASON_MAX];        /* Counter for channel changes */
    uint32_t channelChangeListSize;                         /* Maximum number of channel changes entries */
    int32_t acsBootChannel;                                 /* IEEE Channel number to be used at boot, for the first channel set */
    amxc_llist_t channelChangeList;                         /* List of channel changes */
    wld_rad_bwSelectMode_e autoBwSelectMode;                /* channel bandwidth pushed for a fixed channel */
    bool obssCoexistenceEnabled;                            /* Enable the coexistence Bandwidth */
    bool obssCoexistenceActive;                             /* Obss coexistence activated */
    swl_operatingClass_t operatingClass;
    wld_channel_extensionPos_e extensionChannel;
    swl_guardinterval_e guardInterval;
    int MCS;                                    /* Modulation Coding Scheme index */
    int8_t transmitPowerSupported[64];          /* Steps that can be set */
    int transmitPower;                          /* Percentual transmit power [0..100] */
    int drvPowerVal;                            /* Most drivers use it in dB value */
    int actAntennaCtrl;                         /* bit-pattern of Active radio antennas */
    int rxChainCtrl;                            /* bit-pattern of configured rx radio antennas, -1 is default */
    int txChainCtrl;                            /* bit-pattern of configured tx radio antennas, -1 is default */
    uint8_t retryLimit;                         /* The maximum number of retransmissions of a short packet */
    uint8_t longRetryLimit;                     /* The maximum number of retransmissions of a long packet */
    bool twtEnable;                             /* Enable the target wake time 11ax feature */
    bool IEEE80211hSupported;                   /* Indicate if we've 802.11h support */
    bool IEEE80211kSupported;                   /* Indicate if we've 802.11k support */
    bool IEEE80211rSupported;                   /* Indicate if we've 802.11r support */
    wld_multiap_type_m m_multiAPTypesSupported; /* Indicate if we've 802.11r support */
    bool setRadio80211hEnable;                  /* Enable 802.11h */
    int regulatoryDomainIdx;                    /* Responding country code */
    char regulatoryDomain[8];                   /* Country code (BE, FR, EU, US,...)*/
    wld_cfg11ax_t cfg11ax;
    bool implicitBeamFormingSupported;
    bool implicitBeamFormingEnabled;
    bool explicitBeamFormingSupported;
    bool explicitBeamFormingEnabled;
    swl_80211_htCapInfo_m htCapabilities;                                   /* HT(High Throughput) 802.11n capabilities*/
    uint8_t htMcsSet[SWL_80211_MCS_SET_LEN];                                /* The Supported HT-MCS Set */
    swl_80211_vhtCapInfo_m vhtCapabilities;                                 /* VHT(very High Throughput) 802.11ac capabilities*/
    uint8_t vhtMcsNssSet[SWL_80211_VHT_MCS_NSS_SIZE];                       /* The Supported VHT-MCS and NSS Set */
    swl_80211_hecap_macCapInfo_t heMacCapabilities;                         /* HE(High Efficiency) 802.11ax mac capabilities */
    swl_80211_hecap_phyCapInfo_t hePhyCapabilities;                         /* HE(High Efficiency) 802.11ax phy capabilities */
    swl_80211_hecap_MCSCap_t heMcsCaps[SWL_80211_HECAP_MCS_CAP_ARRAY_SIZE]; /* The Supported HE-MCS and NSS Set */
    wld_rifs_mode_e RIFSEnabled;
    bool airtimeFairnessEnabled;                                            /* Enable airtime fairness feature */
    bool intAirtimeSchedEnabled;                                            /* Enable intelligent airtime scheduling*/
    bool rxPowerSaveEnabled;                                                /* Enable receive chain power save*/
    bool rxPowerSaveEnabledWhenRepeater;                                    /* Enable receive chain power save when repeater*/
    bool multiUserMIMOSupported;                                            /* Whether MU MIMO is supported */
    bool multiUserMIMOEnabled;                                              /* Whether MU MIMO is enabled */
    bool kickRoamStaEnabled;                                                /* Enable kicking of roaming stations in this rad */
    bool ofdmaEnable;                                                       /* Whether OFDMA is enabled */
    wld_he_cap_m heCapsSupported;                                           /* Which 11ax he caps are supported */
    wld_he_cap_m heCapsEnabled;                                             /* Which 11ax he caps are enabled */
    wld_rad_bf_cap_m bfCapsSupported[COM_DIR_MAX];                          /* Which beamforming capabilities are available */
    wld_rad_bf_cap_m bfCapsEnabled[COM_DIR_MAX];                            /* Which beamforming capabilities are enabled */
    int32_t nrAntenna[COM_DIR_MAX];                                         /* Number of antennas available. -1 means undefined */
    int32_t nrActiveAntenna[COM_DIR_MAX];                                   /* Number of antennas active. -1 means undefined */
    uint16_t rtsThreshold;                                                  /* Number of octets in an MPDU below which an RTS/CTS handshake is not performed */
    uint32_t beaconPeriod;                                                  /* Beaconing period in ms */
    uint32_t dtimPeriod;                                                    /* Delivery Traffic Information Map period, in nrBeacons*/
    wld_preamble_type_e preambleType;                                       /* Longer preamble are needed by 802.11g to coexist with legacy systems 802.11 and 802.11b */
    bool packetAggregationEnable;                                           /* Enable packet Aggregation (commonly called "frame aggregation") */
    unsigned int DFSChannelChangeEventCounter;                              /* Number of DFS channel change events */
    swl_timeReal_t DFSChannelChangeEventTimestamp;                          /* Last DFS channel change event realTime value */
    uint8_t dfsFileLogLimit;                                                /* MAX number of DFS events to be saved into dfsEvent log file */
    uint8_t dfsEventLogLimit;                                               /* MAX number of DFS events to be saved into datamodel */
    uint8_t dfsEventNbr;                                                    /* Number of DFS events currently in datamodel */
    uint8_t dfsFileEventNbr;                                                /* Number of DFS events currently in dfsEvent log file */

    unsigned long drv_caps;                                                 /* Driver capablities for this Radio */

    T_Stats stats;                                                          /* Radio statistics */
    swla_dm_objActionReadCtx_t onReadStatsCtx;                              /* Radio.Stats object read handler ctx */
    T_CONST_WPS* wpsConst;                                                  /* WPS constant strings (Build defined) */
    int currentStations;                                                    /* Stat the current # of stations connected to this radio */
    uint32_t currentVideoStations;                                          /* Stat the current # of video endpoints connected to to this radio */
    int maxStations;                                                        /* config the MAX # of stations this radio can handle */
    uint32_t maxNrHwBss;                                                    /* The max nr of Bss that radio can create (determined by hardware) */
    uint32_t maxNrHwSta;                                                    /* The max nr of stations that radio can create (determined by hardware) */

    amxc_llist_t llAP;                                                      /* VAP linked list on this radio (used when commit is used)! */
    amxc_llist_t llEndPoints;                                               /* Endpoints linked list on this radio (used when commit is used)! */

    FSM_STATE fsm_radio_st;                                                 /* Radio FSM state? */
    T_FSM fsmRad;                                                           /* Global Radio FSM state */
    wld_fsmStats_t fsmStats;                                                /* Statistics on FSM usage */
    int fsmTO;

    bool stationMonitorEnabled;
    bool stationMonitorOffChannelSupported;

    /**
     * @listof T_NonAssociatedDevice
     * Used to maintain monitored devices with normal priority.
     */
    amxc_llist_t naStations;

    /**
     * @listof T_NonAssociatedDevice
     * Used to maintain monitored devices with high priority.
     * High priority devices are added by the user manually and/or
     * devices added with Rssi and/or prioritization mechanism.
     */
    amxc_llist_t monitorDev;            /* @listof T_MonitorDevice */

    T_ScanState scanState;              /* SSID Scan state */

    int enable;                         /* Enable/Disable the Radio */
    int wlRadio_SK;                     /* Storage of the Radio Socket handler */
    char Name[IFNAMSIZ];                /* The interface name */
    char instanceName[IFNAMSIZ];        /* The object name */
    struct S_CWLD_FUNC_TABLE* pFA;      /* Function Array */
    uint64_t call_id;                   /* Used for delayed function return state */
    int blockCommit;                    /* Blocks TR98-commit way, edit(), ..XXX.. , commit()! */
    swl_timeMono_t blockStart;          /* Set when block was started */
    int ignoreWPSEvents;                /* Ignore commands received from wpa_talk when this is set */
    amxc_llist_it_t it;
    amxd_object_t* pBus;                /* Keep a copy of the amxd_object_t */
    bool hasDmReady;                    /* Radio datamodel is ready for write actions */
    void* vendorData;                   /* Additional vendor specific data */
    const T_Radio_Capability* capabilities;
    T_Radio_Cap_Item* cap_status;
    char* suppDrvCaps[SWL_FREQ_BAND_MAX];  /* supported driver capabilities per supported frequency band (at freqBand index). */
    wld_rad_bgdfs_config_t bgdfs_config;
    wld_rad_bgdfs_stats_t bgdfs_stats;
    chanmgt_rad_state detailedState;
    chanmgt_rad_state detailedStatePrev;
    int nrCapabilities;
    wld_feature_status features[WLD_FEATURE_MAX];
    T_EventCounterList vendorCounters;
    T_EventCounter counterList[WLD_RAD_EV_MAX];
    T_EventCounterList genericCounters;
    int dbgEnable;                                      /* Enable Deamon debugging */
    char* dbgOutput;                                    /* Filename to store data  */
    T_RssiEventing naStaRssiMonitor;
    wld_rad_muMimoInfo_t radMuMimoInfo;                 /* MU MIMO Info. */
    wld_driverCfg_t driverCfg;                          /* Detailed driver config options */
    wld_macCfg_t macCfg;                                /* Detailed driver config options */
    wld_rad_delayMgr_t delayMgr;
    wld_autoCommitRadData_t autoCommitData;             /* struct for managing auto commiting */
    wld_nl80211_listener_t* nl80211Listener;            /* nl80211 events listener */
    wld_secDmn_t* hostapd;                              /* hostapd daemon context. */
    uint32_t wiphy;                                     /* nl80211 wireless physical device id */
    char wiphyName[IFNAMSIZ];                           /* nl80211 wireless physical device name */
    wld_nl80211_channelSurveyInfo_t* pLastSurvey;       /* last active chan survey result (cached) */
    wld_airStats_t* pLastAirStats;                      /* last air stats calculated based on diff with active chan survey result (cached for nl80211) */
    bool csiEnable;                                     /* Enable CSI */
    amxc_llist_t csiClientList;                         /* CSI client list */
    char firmwareVersion[64];                           /* Radio’s WiFi firmware version */
    char chipVendorName[64];                            /* Radio’s Hw vendor name */

    wld_extMod_dataList_t extDataList;                  /* Non chipset vendor module data list. @type wld_extMod_registration_t */

    wld_radioCap_t cap;                                 /* Datamodel capabilities; */

    swl_mcs_legacyIndex_m supportedDataTransmitRates;   /* Supported data transmit rates in Mbps */
    swl_mcs_legacyIndex_m operationalDataTransmitRates; /* Data transmit rates in Mbps at which the radio will permit operation with any associated station */
    swl_mcs_legacyIndex_m basicDataTransmitRates;       /* The set of data rates, in Mbps, that have to be supported by all stations that desire to join this BSS */
    void* zwdfsData;                                    /* ZW DFS data */

    wld_mbssidAdvertisement_mode_m suppMbssidAdsModes;  /* supported MBSSID Advertisement modes */
    wld_mbssidAdvertisement_mode_e mbssidAdsMode;       /* operating MBSSID Advertisement mode. */
    amxp_timer_t* setMaxNumStaTimer;                    /* Timer used to delay the setMaxNumStation task */
};

typedef struct {
    T_Radio* radio;
    wld_status_e oldStatus;
    chanmgt_rad_state oldDetailedState;
} wld_radio_status_change_event_t;

typedef enum {
    WLD_AUTOMACSRC_DUMMY,                        /* mac is generated from a dummy base */
    WLD_AUTOMACSRC_RADIO_BASE,                   /* mac is generated from radio base mac */
    WLD_AUTOMACSRC_MAX,
} wld_autoMacSrc_e;

struct S_SSID {
    amxc_llist_it_t it;
    int debug;                                /* Magic header to validate SSID ctx */
    wld_status_e status;                      /* == AP/ENDP.Status --> VAP up == TRUE else FALSE */
    wld_status_changeInfo_t changeInfo;       /* Status change info */
    T_Radio* RADIO_PARENT;                    /* Pointer to the T_Radio structure */
    T_AccessPoint* AP_HOOK;                   /* Pointer to the T_AccessPoint structure */
    T_EndPoint* ENDP_HOOK;                    /* Pointer to the T_EndPoint structure */
    unsigned char BSSID[ETHER_ADDR_LEN];      /* BSSID of interface. VAP => same as MAC, EP => BSSID of connected AP */
    unsigned char MACAddress[ETHER_ADDR_LEN]; /* Mac Address of the interface */
    char SSID[36];                            /* VAP SSID broadcast name */
    char Name[32];                            /* Contains the datamodel name of this SSID */
    T_Stats stats;
    bool enable;                              /* current internal value of SSID.Enable */
    bool targetEnable;                        /* The actual enable value this interface shall be. */
    amxd_object_t* pBus;                      /* Keep a copy of the amxd_object_t */
    void* vendorData;                         /* Additional vendor specific data */
    amxp_timer_t* enableSyncTimer;            /* Timer to keep the SSID and Intf enable synced */
    bool syncEnableToIntf;                    /* Whether the sync should be SSID to intf (true) or other way (false)*/
    int32_t bssIndex;                         /* interface creation order among all radio's interfaces */
    int32_t mldUnit;                          /* the index in which "mld unit" this SSID is located */
    wld_autoMacSrc_e autoMacSrc;              /* auto generated mac source: from radio (/or dummy) base mac, or statically learned from driver */
    uint32_t autoMacRefIndex;                 /* mac address offset from source base mac */
    char customNetDevName[IFNAMSIZ];          /* custom interface name set by dm conf */
    bool initDone;                            /* Whether this SSID is properly initialized, and ready for configuration */

    wld_mldLink_t* pMldLink;
};

typedef struct {
    int enable;                         /* Enable/Disable HotSpot2.0 */
    bool dgaf_disable;
    bool l2_traffic_inspect;
    bool icmpv4_echo;
    bool interworking;
    bool internet;
    bool hs2_ie;                        /* Show HotSpot Info Element in BF */
    bool p2p_enable;
    int32_t gas_delay;
    int8_t access_network_type;
    int8_t venue_type;
    int8_t venue_group;
} T_HotSpot2;

typedef struct {
    uint32_t nrToggles;
    uint32_t nrToggleDisconnects;
    uint32_t nrErrorToggles;
    uint32_t nrErrorTogglesDisconnects;
} T_ApMgtStats;

typedef enum {
    VENDOR_IE_BEACON,
    VENDOR_IE_PROBE_RESP,
    VENDOR_IE_ASSOC_RESP,
    VENDOR_IE_AUTH_RESP,
    VENDOR_IE_PROBE_REQ,
    VENDOR_IE_ASSOC_REQ,
    VENDOR_IE_AUTH_REQ,
    VENDOR_IE_MAX
} wld_vendorIeFrame_e;

#define M_VENDOR_IE_BEACON (1 << VENDOR_IE_BEACON)
#define M_VENDOR_IE_PROBE_RESP (1 << VENDOR_IE_PROBE_RESP)
#define M_VENDOR_IE_ASSOC_RESP (1 << VENDOR_IE_ASSOC_RESP)
#define M_VENDOR_IE_AUTH_RESP (1 << VENDOR_IE_AUTH_RESP)
#define M_VENDOR_IE_PROBE_REQ (1 << VENDOR_IE_PROBE_REQ)
#define M_VENDOR_IE_ASSOC_REQ (1 << VENDOR_IE_ASSOC_REQ)
#define M_VENDOR_IE_AUTH_REQ (1 << VENDOR_IE_AUTH_REQ)

#define WLD_VENDORIE_T_DATA_SIZE 256

typedef uint32_t wld_vendorIeFrame_m;

typedef struct {
    amxc_llist_it_t it;
    char oui[SWL_OUI_STR_LEN];       /* XX:XX:XX\0 string format */
    char data[WLD_VENDORIE_T_DATA_SIZE];
    wld_vendorIeFrame_m frame_type;  /* Types of frame that sent vendor IE */
    amxd_object_t* object;
} wld_vendorIe_t;


typedef enum {
    WLD_VAP_DRIVER_CFG_CHANGE_BSS_MAX_IDLE_PERIOD,
    WLD_VAP_DRIVER_CFG_CHANGE_MAX
} wld_vap_driverCfgChange_e;
typedef uint32_t wld_vap_driverCfgChange_m;
#define M_WLD_VAP_DRIVER_CFG_CHANGE_BSS_MAX_IDLE_PERIOD      (1 << WLD_VAP_DRIVER_CFG_CHANGE_BSS_MAX_IDLE_PERIOD)
#define M_WLD_VAP_DRIVER_CFG_CHANGE_ALLFLAGS                 ((1 << WLD_VAP_DRIVER_CFG_CHANGE_MAX) - 1)

typedef struct {
    int32_t bssMaxIdlePeriod;
} wld_vapConfigDriver_t;

typedef struct {
    bool interworkingEnable;
    char qosMapSet[QOS_MAP_SET_MAX_LEN];
} wld_cfg11u_t;

typedef struct {
    swl_trl_e emlmrEnable;
    swl_trl_e emlsrEnable;
    swl_trl_e strEnable;
    swl_trl_e nstrEnable;
} wld_apMldCfg_t;

struct S_ACCESSPOINT {
    int debug;                       /* FIX ME */
    wld_intfStatus_e status;
    swl_timeMono_t lastStatusChange; /* timestamp when the status changed last */
    char name[AP_NAME_SIZE];         /* Contains the object name of this interface */
    char alias[AP_NAME_SIZE];        /* Contains the netdev name of this interface */
    char bridgeName[IFNAMSIZ];       /* Contains the name of the bridge which contains this vap interface */
    int index;                       /* Network device number */
    uint64_t wDevId;                 /* nl80211 wireless device id */
    int ref_index;                   /* VAP index number, basically the index of the VAP in the radio's AP list */
    int SSIDReference;               /* Index # in the T_SSID entry table */

    T_SSID* pSSID;                   /* Contains a direct pointer to the created SSID for this VAP */
    T_Radio* pRadio;                 /* Contains a direct pointer to the parent of this VAP */

    bool SSIDAdvertisementEnabled;
    bool initDone;                   /* Whether this AP is properly initialized, and ready for configuration */
    int retryLimit;
    bool WMMCapability;
    bool UAPSDCapability;
    bool WMMEnable;
    bool UAPSDEnable;
    int MCEnable;       /* multicast enable */
    int doth;
    int APBridgeDisable;
    int ActiveAssociatedDeviceNumberOfEntries;
    int AssociatedDeviceNumberOfEntries;
    int MaxStations;
    uint16_t StaInactivityTimeout;
    bool IEEE80211kEnable;
    bool IEEE80211rEnable;
    bool IEEE80211rFTOverDSEnable;
    char NASIdentifier[NAS_IDENTIFIER_MAX_LEN]; /* NAS Identifier, unique per AP */
    char R0KHKey[KHKEY_MAX_LEN];                /* R0 key used by the AP */
    uint16_t mobilityDomain;                    /* MobilityDomain, common to all APs inside the network */
    wld_cfg11u_t cfg11u;
    swl_security_apMode_m secModesAvailable;    /* Bit pattern for available security modes */
    swl_security_apMode_m secModesSupported;    /* Bit pattern for supported security modes */
    swl_security_apMode_e secModeEnabled;       /* ModesSupported. */
    char WEPKey[36];                            /* Max 32, but we need some 0 for termination (and alignment) */
    char preSharedKey[PSK_KEY_SIZE_LEN];
    char keyPassPhrase[PSK_KEY_SIZE_LEN];
    char saePassphrase[SAE_KEY_SIZE_LEN];
    wld_enc_modes_e encryptionModeEnabled;
    swl_security_mfpMode_e mfpConfig;
    int sppAmsdu;
    int rekeyingInterval;
    int SHA256Enable;              /* (NEW) If supported add extra key protection */
    char oweTransModeIntf[32];
    wld_ap_td_m transitionDisable; /* Transition Disable indication */

    char radiusServerIPAddr[16];
    int radiusServerPort;
    char radiusSecret[64];
    int radiusDefaultSessionTimeout;
    char radiusOwnIPAddress[40];
    char radiusNASIdentifier[128];
    char radiusCalledStationId[88];
    int radiusChargeableUserId;

    int WPS_CertMode;                               /* Correct default behavior for Certification Mode */
    int WPS_Enable;                                 /* WPS enabled? */
    wld_wps_status_e WPS_Status;                    /* WPS real status  */
    bool WPS_ApSetupLocked;                         /* Whether WPS AP Pin is locked */
    int WPS_Registrar;                              /* Enable the Registrar */
    wld_wps_cfgMethod_m WPS_ConfigMethodsSupported; /* bit mask of supported wps config methods */
    wld_wps_cfgMethod_m WPS_ConfigMethodsEnabled;   /* bit mask of enabled wps config methods */
    int WPS_Configured;                             /* Not in ODL but required */
    char WPS_ClientPIN[16];                         /* Client PIN used for this VAP */
    struct S_WPS_pushButton_Delay WPS_PBC_Delay;    /* PushButton delay, commit running */
    wld_wpsSessionInfo_t wpsSessionInfo;            /* WPS session context (with safety timer, to close session beyond WPS walk time) */
    bool addRelayApCredentials;                     /* Use relay accesspoint as WPS credentials */
    bool wpsRestartOnRequest;                       /* bool representing whether restarting wps within a wps session is allowed */

    T_HotSpot2 HotSpot2;
    int CurrentAssociatedDevice;
    T_AssociatedDevice** AssociatedDevice;
    wld_mfMode_e MF_Mode;
    int MF_EntryCount;
    char* MF_AddressList;
    bool MF_AddressListBlockSync;                /* Block MF_AddressLIst update when multiple objects are added/deleted */
    unsigned char MF_Entry[MAXNROF_MFENTRY][ETHER_ADDR_LEN];
    bool MF_TempBlacklistEnable;
    int MF_TempEntryCount;
    unsigned char MF_Temp_Entry[MAXNROF_MFENTRY][ETHER_ADDR_LEN];
    int PF_TempEntryCount;
    unsigned char PF_Temp_Entry[MAXNROF_PFENTRY][ETHER_ADDR_LEN];
    amxc_llist_t neighbours;
    wld_ap_dm_m discoveryMethod;

    int defaultDeviceType;              /* Default device type: index of device type in cstr_DEVICE_TYPES*/

    int enable;
    //int wlradio_sk; Use RADIO pointer for this (if needed)
    int wlvap_sk;
    T_FSM fsm;
    struct S_CWLD_FUNC_TABLE* pFA;        /* Function Array */
    amxc_llist_it_t it;
    amxd_object_t* pBus;                  /* Keep a copy of the amxd_object_t */
    void* vendorData;                     /* Additional vendor specific data for chipset vendor only*/

    int dbgEnable;                        /* Enable Deamon debugging */
    char* dbgOutput;                      /* Filename to store data  */
    T_RssiEventing rssiEventing;          /* Struct to keep track of RssiEventing */
    T_ApMgtStats mgtStats;                /* Statistics with regards to management */
    bool wdsEnable;                       /* Enable 4 mac address */
    wld_multiap_type_m multiAPType;       /* Bitmask of all MultiAP type applied to this accesspoint */
    wld_multiap_profile_e multiAPProfile; /* MultiAP profile status */
    uint16_t multiAPVlanId;               /* Primary VLAN ID config for MultiAP */
    wld_apRole_e apRole;                  /* Current AccessPoint role */
    T_AccessPoint* pReferenceApRelay;     /* Use the credentials of this AP inside WPS M8 */
    bool mboEnable;                       /* Enable multi band operation*/
    wld_mbo_denyReason_e mboDenyReason;   /* MBO Assoc Disallow Reason to add to the MBO IE*/
    bool enableVendorIEs;                 /* Enable the broadcast of custom vendor IEs */
    amxc_llist_t vendorIEs;               /* List of vendor IE */
    wld_fcallState_t stationsStatsState;  /* Station stats state */
    wld_vapConfigDriver_t driverCfg;      /* Detailed driver config options */

    /* Table of recent station disconnections.
     * Only for stations that were authenticated. Stations that did not get authenticated
     * will not be added to this list */
    swl_unLiTable_t staDcList;
    int32_t historyCnt;

    wld_nl80211_listener_t* nl80211Listener;  /* nl80211 events listener */
    wld_wpaCtrlInterface_t* wpaCtrlInterface; /* wpaCtrlInterface to hostapd interface */
    bool clientIsolationEnable;
    swl_circTable_t lastAssocReq;             /* (mac, bssid, assoc/reassoc frame, timestamp,assocType) where station mac format: xx:xx:xx:xx:xx:xx */
    uint32_t lastDevIndex;
    wld_extMod_dataList_t extDataList;        /* list of extention data for non-chipset vendor modules */
    amxc_llist_t llIntfWds;                   /* list of wds interface related to this VAP (wld_wds_intf_t) */
    wld_apMldCfg_t mldCfg;                    /* MLD config options */
};

typedef struct SWL_PACKED {
    swl_macBin_t mac;
    swl_macBin_t bssid;
    char* frame;
    swl_timeMono_t timestamp;
    uint16_t stype;
} wld_vap_assocTableStruct_t;


typedef struct {
    T_AccessPoint* vap;
    wld_intfStatus_e oldVapStatus;
    wld_status_e oldSsidStatus;
} wld_vap_status_change_event_t;

typedef enum {
    WLD_VAP_CHANGE_EVENT_CREATE,       // Initial creation of object, no transaction possible
    WLD_VAP_CHANGE_EVENT_CREATE_FINAL, // Evented add handler, transaction possible (referencing ssid/radio)
    WLD_VAP_CHANGE_EVENT_DESTROY,
    WLD_VAP_CHANGE_EVENT_DEINIT,       // VAP de-initialized (ie not referencing to ssid/radio)
    WLD_VAP_CHANGE_EVENT_MAX
} wld_vap_changeEvent_e;

typedef struct {
    wld_vap_changeEvent_e changeType;
    T_AccessPoint* vap;
    void* data;
} wld_vap_changeEvent_t;

typedef enum {
    WLD_VAP_ACTION_RSSI_SAMPLE,
    WLD_VAP_ACTION_STEER,
    WLD_VAP_ACTION_KICK,
    WLD_VAP_ACTION_MAX,
} wld_vap_actionEvent_e;

typedef struct {
    wld_vap_actionEvent_e changeType;
    T_AccessPoint* vap;
    void* data;
} wld_vap_actionEvent_t;

typedef enum {
    WLD_EP_CHANGE_EVENT_CREATE,       // Initial creation of object, no transaction possible
    WLD_EP_CHANGE_EVENT_CREATE_FINAL, // Evented add handler, transaction possible
    WLD_EP_CHANGE_EVENT_DESTROY,
    WLD_EP_CHANGE_EVENT_MAX
} wld_ep_changeEvent_e;

typedef struct {
    wld_ep_changeEvent_e changeType;
    T_EndPoint* ep;
    void* data;
} wld_ep_changeEvent_t;

typedef enum {
    WLD_AD_CHANGE_EVENT_CREATE,
    WLD_AD_CHANGE_EVENT_ASSOC,
    WLD_AD_CHANGE_EVENT_AUTH,
    WLD_AD_CHANGE_EVENT_DISASSOC,
    WLD_AD_CHANGE_EVENT_DESTROY,
    WLD_AD_CHANGE_EVENT_MAX
} wld_ad_changeEvent_e;

typedef struct {
    wld_ad_changeEvent_e changeType;
    T_AccessPoint* vap;
    T_AssociatedDevice* ad;
    void* data;
} wld_ad_changeEvent_t;


typedef enum bs_uplink_type {
    UPLINK_TYPE_UNKNOWN,
    UPLINK_TYPE_WIRED,
    UPLINK_TYPE_WIRELESS,
    UPLINK_TYPE_ROOT,
    UPLINK_TYPE_MAX
} wld_uplinkType_e;

extern const char* wld_uplinkType_str[UPLINK_TYPE_MAX];

typedef struct {
    uint32_t txbyte;
    uint32_t rxbyte;
} T_epMon_counters;

typedef struct {
    swl_timeMono_t lastAssocTime;
    swl_timeMono_t lastDisassocTime;
    uint32_t nrAssociations;
    uint32_t nrAssocAttempts;
    uint32_t nrAssocAttemptsSinceDc;
} wld_ep_assocStats_t;

typedef struct {
    uint32_t LastDataDownlinkRate;
    uint32_t LastDataUplinkRate;
    int32_t SignalStrength;     // Received signal strength in dBm, -1 if unavailable
    int32_t noise;              // Measured noise floor in dBm, -1 if unavailable
    int32_t SignalNoiseRatio;   // Signal to noise radio, dBm, 0 if unavailable
    int32_t RSSI;               // Received signal strength indicator in percent
    uint32_t Retransmissions;
    uint64_t txbyte;            /**< tx data bytes */
    uint32_t txPackets;         /**< tx data packets */
    uint32_t txRetries;         /**< tx data packet retries */
    uint64_t rxbyte;            /**< rx data bytes */
    uint32_t rxPackets;         /**< rx data packets */
    uint32_t rxRetries;         /**< rx data packet retries */


    wld_assocDev_capabilities_t assocCaps;
    swl_radStd_e operatingStandard;

    uint16_t maxRxStream;
    uint16_t maxTxStream;
} T_EndPointStats;

struct _EndPointProfile;
typedef struct _EndPointProfile T_EndPointProfile;

struct S_EndPoint {
    int debug;                                /* FIX ME */
    wld_intfStatus_e status;
    char bridgeName[IFNAMSIZ];                /* The bridge interface name */
    wld_epConnectionStatus_e connectionStatus;
    swl_timeMono_t lastConnStatusChange;
    wld_ep_assocStats_t assocStats;
    wld_epError_e error;
    char Name[16];      /* The interface name */
    char alias[32];
    int index;          /* Network device number */
    int ref_index;      /* VAP index number (used bcm nvram) */
    int SSIDReference;  /* Index # in the T_SSID entry table */

    T_EndPointProfile* currentProfile;
    amxc_llist_t llProfiles;

    T_SSID* pSSID;                                  /* Contains a direct pointer to the created SSID for this VAP */
    T_Radio* pRadio;                                /* Contains a direct pointer to the parent of this VAP */

    swl_security_apMode_m secModesSupported;        /* Bit pattern for supported security modes */

    bool WPS_Enable;                                /* WPS enabled? */
    int WPS_Configured;                             /* Not in ODL but required */
    wld_wps_cfgMethod_m WPS_ConfigMethodsSupported; /* bit mask of supported wps config methods */
    wld_wps_cfgMethod_m WPS_ConfigMethodsEnabled;   /* bit mask of enabled wps config methods */
    char WPS_ClientPIN[16];                         /* Client PIN used for this VAP */
    T_WPS_pushButton_Delay WPS_PBC_Delay;           /* To delay the push button when state machine not idle */
    wld_wpsSessionInfo_t wpsSessionInfo;            /* WPS session context (with safety timer, to close session beyond WPS walk time) */
    uint32_t reconnect_count;                       /* Amount of reconnect retries currently done */
    uint32_t reconnect_rad_trigger;                 /* Amount of reconnect retries before toggling the radio */

    int enable;
    T_FSM fsm;
    struct S_CWLD_FUNC_TABLE* pFA;        /* Function Array */
    amxc_llist_it_t it;
    amxd_object_t* pBus;                  /* Keep a copy of the amxd_object_t */
    void* vendorData;                     /* Additional vendor specific data */

    amxp_timer_t* reconnectTimer;         /* Timer to retry connecting when the connection is lost/could not be made */
    uint32_t reconnectDelay;              /* Minimum time before try to reconnect */
    uint32_t reconnectInterval;           /* Minimum time between two reconnections attempt */
    bool multiAPEnable;                   /* Set MultiAP BackhaulSTA on this EndPoint */
    bool toggleBssOnReconnect;            /* Disable hostapd on EP reconnection */

    wld_multiap_profile_e multiAPProfile; /* MultiAP profile status */
    uint16_t multiAPVlanId;               /* Primary VLAN ID config for MultiAP */

    wld_tinyRoam_t* tinyRoam;
    wld_fcallState_t statsCall;

    wld_nl80211_listener_t* nl80211Listener;  /* nl80211 events listener */
    wld_wpaCtrlInterface_t* wpaCtrlInterface; /* wpaCtrlInterface to wpa_supplicant interface */
    wld_secDmn_t* wpaSupp;                    /* wpa_supplicant daemon context. */
    uint64_t wDevId;                          /* nl80211 wireless device id */
    T_EndPointStats stats;
    swla_dm_objActionReadCtx_t onActionReadCtx;
};

typedef struct {
    T_EndPoint* ep;
    wld_intfStatus_e oldStatus;
    wld_epConnectionStatus_e oldConnectionStatus;
} wld_ep_status_change_event_t;

struct _EndPointProfile {
    int enable;
    wld_epProfileStatus_e status;
    int index;
    char alias[64];
    char SSID[SSID_NAME_LEN];
    uint8_t BSSID[ETHER_ADDR_LEN];
    char location[64];
    uint8_t priority;

    swl_security_apMode_e secModeEnabled;
    char WEPKey[36];        /* Max 32, but we need some 0 for termination (and alignment) */
    char preSharedKey[PSK_KEY_SIZE_LEN];
    char keyPassPhrase[PSK_KEY_SIZE_LEN];
    char saePassphrase[SAE_KEY_SIZE_LEN];
    swl_security_mfpMode_e mfpConfig;
    amxc_llist_it_t it;
    amxd_object_t* pBus;
    T_EndPoint* endpoint;
};



typedef struct wld_spectrumChannelInfoEntry {
    amxc_llist_it_t it;
    uint8_t channel;
    uint32_t availability;
    uint32_t ourUsage;
    int32_t noiselevel;
    uint32_t nrCoChannelAP;
    swl_bandwidth_e bandwidth;
} wld_spectrumChannelInfoEntry_t;

typedef struct {
    char* driver;
    char* vendor;
} wld_rad_chipVendorInfo_t;

#ifdef __cplusplus
extern "C" {
#endif

#ifndef APIENTRY
#define APIENTRY
#endif

typedef int (APIENTRY* PFN_WRAD_ADDVAP_EXT)(T_Radio* rad, T_AccessPoint* vap);


/*
 * @brief: create an VAP interface and attach it to the bottom layer
 * @param rad internal radio context
 * @param vapIfName the expected netdev interface name, MUST not be empty
 * @param vapIfNameSize ifname buffer size, to get a copy of the final iface name
 * @return the netdev index of the created interface (>0), error code <=0 otherwise
 */
typedef int (APIENTRY* PFN_WRAD_ADDVAPIF)(T_Radio* rad, char* vapIfName, int vapIfNameSize);
typedef int (APIENTRY* PFN_WRAD_DELVAPIF)(T_Radio* rad, char* vap);
typedef int (APIENTRY* PFN_WRAD_ADDENDPOINTIF)(T_Radio* rad, char* endpoint, int bufsize);
typedef int (APIENTRY* PFN_WRAD_DELENDPOINTIF)(T_Radio* rad, char* endpoint);

typedef swl_rc_ne (APIENTRY* PFN_WRAD_GETSPECTRUMINFO)(T_Radio* rad, bool update, amxc_llist_t* llSpectrumChannelInfo);
typedef swl_rc_ne (APIENTRY* PFN_WRAD_SCAN_RESULTS)(T_Radio* rad, wld_scanResults_t* results);
typedef swl_rc_ne (APIENTRY* PFN_WRAD_GETSCANFILTERINFO)(T_Radio* rad, bool* isFilterActive);

/* Our common function table that all VENDOR WIFI drivers must be able to handle... */

/*********** Vendor Radio driver settings *********************/
typedef int (APIENTRY* PFN_WRAD_CREATE_HOOK)(T_Radio* rad);
typedef void (APIENTRY* PFN_WRAD_DESTROY_HOOK)(T_Radio* rad);
typedef int (APIENTRY* PFN_WRAD_RADIO_STATUS)(T_Radio* rad);
typedef int (APIENTRY* PFN_WRAD_ENABLE)(T_Radio* rad, int enable, int set);
typedef int (APIENTRY* PFN_WRAD_MAXBITRATE)(T_Radio* rad);
typedef int (APIENTRY* PFN_WRAD_DFSRADARTRIGGER)(T_Radio* rad, dfstrigger_rad_state set);
typedef int (APIENTRY* PFN_WRAD_SUPFREQBANDS)(T_Radio* rad, char* buf, int bufsize);
//typedef int (APIENTRY * PFN_WRAD_OFREQB)             (T_Radio * rad, char * buf, int bufsize, int set); // --> Info taken from channel
typedef int (APIENTRY* PFN_WRAD_SUPSTD)(T_Radio* rad, swl_radioStandard_m newStandards);
//typedef int (APIENTRY * PFN_WRAD_OSTD)               (T_Radio * rad, char * buf, int bufsize);
typedef int (APIENTRY* PFN_WRAD_POSCHANS)(T_Radio* rad, uint8_t* buf, int bufsize);            // If country change this is also changing!
typedef int (APIENTRY* PFN_WRAD_CHANSINUSE)(T_Radio* rad, char* buf, int bufsize);             // Is this the result of a scan?
typedef int (APIENTRY* PFN_WRAD_CHANNEL)(T_Radio* rad, int channel, int set);
typedef int (APIENTRY* PFN_WRAD_GET_WIFI_COUNTERS)(T_Radio* rad, T_epMon_counters* transWifiCounters);
typedef int (APIENTRY* PFN_WRAD_SUPPORTS)(T_Radio* rad, char* buf, int bufsize);
typedef int (APIENTRY* PFN_WRAD_AUTOCHANNELENABLE)(T_Radio* rad, int enable, int set);
typedef int (APIENTRY* PFN_WRAD_STARTACS)(T_Radio* rad, int set);
typedef int (APIENTRY* PFN_WRAD_STARTPLATFORMACS)(T_Radio* rad, const amxc_var_t* const args);
typedef int (APIENTRY* PFN_WRAD_BGDFS_ENABLE)(T_Radio* rad, int enable);
typedef int (APIENTRY* PFN_WRAD_BGDFS_START)(T_Radio* rad, int channel);

typedef int (APIENTRY* PFN_WRAD_BGDFS_START_EXT)(T_Radio* rad, wld_startBgdfsArgs_t* args);
typedef int (APIENTRY* PFN_WRAD_BGDFS_STOP)(T_Radio* rad);

typedef int (APIENTRY* PFN_WRAD_ACHREFPERIOD)(T_Radio* rad, int arefp, int set);
//typedef int (APIENTRY * PFN_WRAD_OCHBW)              (T_Radio * rad, char * buf, int bufsize, int set);
typedef int (APIENTRY* PFN_WRAD_OCHBW)(T_Radio* rad, int val, int set);
typedef int (APIENTRY* PFN_WRAD_EXTCHAN)(T_Radio* rad, char* buf, int bufsize, int set);
typedef int (APIENTRY* PFN_WRAD_GUARDINTVAL)(T_Radio* rad, char* buf, int bufsize, int set);
typedef int (APIENTRY* PFN_WRAD_MCS)(T_Radio* rad, char* val, int bufsize, int set);
typedef int (APIENTRY* PFN_WRAD_TXPOW)(T_Radio* rad, int val, int set);
typedef int (APIENTRY* PFN_WRAD_ANTENNACTRL)(T_Radio* rad, int val, int set);
typedef int (APIENTRY* PFN_WRAD_STAMODE)(T_Radio* rad, int val, int set);
typedef int (APIENTRY* PFN_WRAD_REGDOMAIN)(T_Radio* rad, char* val, int bufsize, int set);
typedef int (APIENTRY* PFN_WRAD_SYNC)(T_Radio* rad, int set);
typedef swl_rc_ne (APIENTRY* PFN_WRAD_SECDMN_RESTART)(T_Radio* rad, int set);
typedef swl_rc_ne (APIENTRY* PFN_WRAD_SECDMN_REFRESH)(T_Radio* rad, int set);
typedef swl_rc_ne (APIENTRY* PFN_WRAD_TOGGLE)(T_Radio* rad, int set);
typedef enum {
    beamforming_implicit,
    beamforming_explicit
} beamforming_type_t;
typedef int (APIENTRY* PFN_WRAD_BEAMFORMING)(T_Radio* rad, beamforming_type_t type, int val, int set);
typedef int (APIENTRY* PFN_WRAD_RIFS)(T_Radio* rad, int val, int set);
typedef int (APIENTRY* PFN_WRAD_AIRTIMEFAIRNESS)(T_Radio* rad, int val, int set);
typedef int (APIENTRY* PFN_WRAD_RXPOWERSAVE)(T_Radio* rad, int val, int set);
typedef int (APIENTRY* PFN_WRAD_INTELLIGENTAIRTIME)(T_Radio* rad, int val, int set);
typedef int (APIENTRY* PFN_WRAD_MULTIUSERMIMO)(T_Radio* rad, int val, int set);
typedef int (APIENTRY* PFN_WRAD_UPDATE_PROB_REQ)(T_Radio* rad);

#define DEFAULT_BASE_RSSI -200.0

typedef struct {
    int16_t rssi_value[MAX_NR_ANTENNA];
    uint16_t nr_antenna;
} T_ANTENNA_RSSI;
typedef struct {
    double power_value[MAX_NR_ANTENNA];
    uint16_t nr_antenna;
} T_ANTENNA_POWER;
typedef struct {
    uint8_t token;
    uint16_t duration;
    uint16_t interval;
    swl_ieee802_rrmReqMode_m modeMask;
    swl_ieee802_rrmBeaconReqMode_e mode;
    swl_operatingClass_t operClass;
    swl_channel_t channel;
    swl_macChar_t bssid;
    char ssid[SSID_NAME_LEN];
    bool addNeighbor;
    char* optionalEltHexStr;
} wld_rrmReq_t;
typedef struct {
    uint32_t linkid;            /*!< linkid of the affiliated accesspoint */
    uint32_t txPackets;         /*!< tx data packets */
    uint32_t txUbyte;           /*!< tx data Unicast bytes */
    uint32_t txBbyte;           /*!< tx data Broadcast bytes */
    uint32_t txMbyte;           /*!< tx data Multicast bytes */
    uint32_t txEbyte;           /*!< tx data error bytes */
    uint32_t rxEbyte;           /*!< rx data error bytes */
    uint32_t rxPackets;         /*!< rx data packets */
    uint32_t rxUbyte;           /*!< rx data Unicast bytes */
    uint32_t rxBbyte;           /*!< rx data Broadcast bytes */
    uint32_t rxMbyte;           /*!< rx data Multicast bytes */
} wld_mloStats_t;
typedef int (APIENTRY* PFN_WRAD_PER_ANTENNA_RSSI)(T_Radio* rad, T_ANTENNA_RSSI*);
typedef int (APIENTRY* PFN_WRAD_LATEST_POWER)(T_Radio* rad, T_ANTENNA_POWER*);
typedef swl_rc_ne (APIENTRY* PFN_WRAD_UPDATE_CHANINFO)(T_Radio* rad);
typedef swl_rc_ne (APIENTRY* PFN_WRAD_UPDATE_MON_STATS)(T_Radio* rad);
typedef swl_rc_ne (APIENTRY* PFN_WRAD_SETUP_STAMON)(T_Radio* rad, bool enable);
typedef swl_rc_ne (APIENTRY* PFN_WRAD_ADD_STAMON)(T_Radio* rad, T_NonAssociatedDevice* pMD);
typedef swl_rc_ne (APIENTRY* PFN_WRAD_DEL_STAMON)(T_Radio* rad, T_NonAssociatedDevice* pMD);
typedef int (APIENTRY* PFN_WRAD_SETUP_INTFRMON)(T_Radio* rad, bool enable);
typedef int (APIENTRY* PFN_WRAD_CHECK_HEALTH)(T_Radio* rad);
typedef int (APIENTRY* PFN_WRAD_DELAY_AP_UP_DONE)(T_Radio* rad);
typedef swl_rc_ne (APIENTRY* PFN_WRAD_RADIO_STATS)(T_Radio* rad);

typedef swl_rc_ne (APIENTRY* PFN_WRAD_SENSING_CMD)(T_Radio* rad);
typedef swl_rc_ne (APIENTRY* PFN_WRAD_SENSING_CSI_STATS)(T_Radio* rad, wld_csiState_t* csimonState);
typedef swl_rc_ne (APIENTRY* PFN_WRAD_SENSING_ADD_CLIENT)(T_Radio* rad, wld_csiClient_t* client);
typedef swl_rc_ne (APIENTRY* PFN_WRAD_SENSING_DEL_CLIENT)(T_Radio* rad, swl_macChar_t macAddr);
typedef swl_rc_ne (APIENTRY* PFN_WRAD_SENSING_RESET_STATS)(T_Radio* rad);

/*********** Vendor VAP driver settings *********************/
typedef int (APIENTRY* PFN_WVAP_CREATE_HOOK)(T_AccessPoint* vap);
typedef void (APIENTRY* PFN_WVAP_DESTROY_HOOK)(T_AccessPoint* vap);
typedef int (APIENTRY* PFN_WVAP_VAP_STATUS)(T_AccessPoint* vap);
typedef swl_rc_ne (APIENTRY* PFN_WVAP_VAP_UPDATE_ADL)(T_AccessPoint* vap);
typedef int (APIENTRY* PFN_WVAP_VAP_ENABLE)(T_AccessPoint* vap, int enable, int set);
typedef int (APIENTRY* PFN_WVAP_VAP_ENABLE_WMM)(T_AccessPoint* vap, int enable, int set);
typedef int (APIENTRY* PFN_WVAP_VAP_ENABLE_UAPSD)(T_AccessPoint* vap, int enable, int set);
typedef int (APIENTRY* PFN_WVAP_VAP_SSID)(T_AccessPoint* vap, char* buf, int bufsize, int set);
typedef int (APIENTRY* PFN_WVAP_SYNC)(T_AccessPoint* vap, int set);
typedef int (APIENTRY* PFN_WVAP_SEC_SYNC)(T_AccessPoint* vap, int set);
typedef swl_rc_ne (APIENTRY* PFN_WVAP_WPS_SYNC)(T_AccessPoint* vap, char* val, int bufsize, int set);
typedef swl_rc_ne (APIENTRY* PFN_WVAP_WPS_ENABLE)(T_AccessPoint* vap, int enable, int set);
typedef int (APIENTRY* PFN_WVAP_WPS_LABEL_PIN)(T_AccessPoint* vap, int set);
typedef int (APIENTRY* PFN_WVAP_WPS_COMP_MODE)(T_AccessPoint* vap, int val, int set);
typedef int (APIENTRY* PFN_WVAP_MF_SYNC)(T_AccessPoint* vap, int set);
typedef int (APIENTRY* PFN_WVAP_PF_SYNC)(T_AccessPoint* vap, int set);
typedef int (APIENTRY* PFN_WVAP_KICK_STA)(T_AccessPoint* vap, char* buf, int bufsize, int set);
typedef int (APIENTRY* PFN_WVAP_KICK_STA_REASON)(T_AccessPoint* vap, char* buf, int bufsize, int reason);
typedef swl_rc_ne (APIENTRY* PFN_WVAP_DISASSOC_STA_REASON)(T_AccessPoint* vap, swl_macBin_t* sta_mac, int reason);

typedef swl_rc_ne (APIENTRY* PFN_WVAP_RRM_REQUEST)(T_AccessPoint* vap, const swl_macChar_t* sta, wld_rrmReq_t*);
typedef swl_rc_ne (APIENTRY* PFN_WVAP_CLEAN_STA)(T_AccessPoint* vap, char* buf, int bufsize);
typedef int (APIENTRY* PFN_WVAP_MULTIAP_UPDATE_TYPE)(T_AccessPoint* vap);
typedef int (APIENTRY* PFN_WVAP_MULTIAP_UPDATE_PROFILE)(T_AccessPoint* vap);
typedef int (APIENTRY* PFN_WVAP_MULTIAP_UPDATE_VLANID)(T_AccessPoint* vap);
typedef int (APIENTRY* PNF_WVAP_SET_MBO_DENY_REASON)(T_AccessPoint* vap);
typedef int (APIENTRY* PFN_WVAP_SET_AP_ROLE)(T_AccessPoint* vap);
typedef int (APIENTRY* PFN_WVAP_ADD_VENDOR_IE)(T_AccessPoint* vap, wld_vendorIe_t* vendor_ie);
typedef int (APIENTRY* PFN_WVAP_DEL_VENDOR_IE)(T_AccessPoint* vap, wld_vendorIe_t* vendor_ie);
typedef int (APIENTRY* PFN_WVAP_ENAB_VENDOR_IE)(T_AccessPoint* vap, int enable);
typedef int (APIENTRY* PFN_WVAP_SET_DISCOVERY_METHOD)(T_AccessPoint* vap);
typedef void (APIENTRY* PFN_WVAP_SET_CONFIG_DRV)(T_AccessPoint* vap, wld_vap_driverCfgChange_m param);
/*********** Vendor Misc driver settings *********************/
typedef int (APIENTRY* PFN_WRAD_HASSUPPORT)(T_Radio* rad, T_AccessPoint* vap, char* buf, int bufsize);
typedef int (APIENTRY* PFN_WVAP_VAP_BSSID)(T_Radio* rad, T_AccessPoint* vap, unsigned char* buf, int bufsize, int set);

/*********** Vendor Endpoint driver settings *********************/
typedef swl_rc_ne (APIENTRY* PFN_WENDPOINT_CREATE_HOOK)(T_EndPoint* endpoint);
typedef swl_rc_ne (APIENTRY* PFN_WENDPOINT_DESTROY_HOOK)(T_EndPoint* endpoint);
typedef swl_rc_ne (APIENTRY* PFN_WENDPOINT_ENABLE)(T_EndPoint* endpoint, bool enable);
typedef swl_rc_ne (APIENTRY* PFN_WENDPOINT_DISCONNECT)(T_EndPoint* endpoint);
typedef swl_rc_ne (APIENTRY* PFN_WENDPOINT_CONNECT_AP)(T_EndPointProfile* endpointProfile);
typedef swl_rc_ne (APIENTRY* PFN_WENDPOINT_BSSID)(T_EndPoint* ep, swl_macChar_t* bssid);
typedef swl_rc_ne (APIENTRY* PFN_WENDPOINT_STATS)(T_EndPoint* endpoint, T_EndPointStats* stats);
typedef swl_rc_ne (APIENTRY* PFN_WENDPOINT_WPS_START)(T_EndPoint* pEP, wld_wps_cfgMethod_e method, char* pin, char* ssid, swl_macChar_t* bssid);
typedef swl_rc_ne (APIENTRY* PFN_WENDPOINT_WPS_CANCEL)(T_EndPoint* pEP);
typedef swl_rc_ne (APIENTRY* PFN_WENDPOINT_UPDATE)(T_EndPoint* pEP, int set);
typedef int (APIENTRY* PFN_WEP_ENABLE_VENDOR_ROAMING)(T_EndPoint* pEP);
typedef int (APIENTRY* PFN_WEP_UPDATE_VENDOR_ROAMING)(T_EndPoint* pEP);
typedef int (APIENTRY* PFN_WENDPOINT_MULTIAP_ENABLE)(T_EndPoint* pEP);
typedef int (APIENTRY* PFN_WENDPOINT_SET_MAC_ADDR)(T_EndPoint* pEP);
typedef int (APIENTRY* PFN_WEP_STATUS)(T_EndPoint* endpoint);

/* Here we configure all the needed parts based on our given data structure */
typedef int (APIENTRY* PFN_WVAP_FSM_STATE)(T_AccessPoint* vap);
typedef int (APIENTRY* PFN_WVAP_FSM)(T_AccessPoint* vap);
typedef int (APIENTRY* PFN_WVAP_FSM_NODELAY)(T_AccessPoint* vap);

typedef FSM_STATE (APIENTRY* PFN_WRAD_FSM_STATE)(T_Radio* rad);
typedef FSM_STATE (APIENTRY* PFN_WRAD_FSM)(T_Radio* rad);
typedef int (APIENTRY* PFN_WRAD_FSM_NODELAY)(T_Radio* rad);
typedef int (APIENTRY* PFN_WRAD_FSM_DELAY_COMMIT)(T_Radio* rad);

/* Sync issues, if enable and commit are mixed. For that we introduce some extra stages
   and a commit_pending flag in the state machine. But it also force us back todo a call
   to the upper layers sync_xxx2OBJ(). */
typedef void (APIENTRY* PFN_SYNC_RADIO)(amxd_object_t* object, T_Radio* pR, int set);
typedef void (APIENTRY* PFN_SYNC_AP)(amxd_object_t* object, T_AccessPoint* pAP, int set);
typedef void (APIENTRY* PFN_SYNC_SSID)(amxd_object_t* object, T_SSID* pS, int set);
typedef void (APIENTRY* PFN_SYNC_EP)(T_EndPoint* pEP);

/*
 * @brief handler of modified vendor parameters
 *
 * @param pRad Radio context when relevant (ie vendor subobj of radio), null otherwise
 * @param pAP AccessPoint context when relevant (ie vendor subobj of accessPoint), null otherwise
 * @param object modified vendor object (of child objects)
 * @param params modified parameters in variant htable (to: and from:)
 * @return SWL_RC_OK if successful, error code otherwise
 */
typedef swl_rc_ne (APIENTRY* PFN_WIFI_SUPVEND_MODES)(T_Radio* pRad, T_AccessPoint* pAP, amxd_object_t* object, amxc_var_t* params);

/* Functions that are called from vendor plugin in wld lib! */
typedef int (APIENTRY* PFN_HSPOT_ENABLE)(T_AccessPoint* vap, int enable, int set);
typedef int (APIENTRY* PFN_HSPOT_CONFIG)(T_AccessPoint* vap, int set);

/* This function is called when a bridge configuration change happened, some
   deamons need todo some reconfiguration */
typedef int (APIENTRY* PFN_ON_BRIDGE_STATE_CHANGE)(T_Radio* rad, T_AccessPoint* vap, int set);

typedef int (APIENTRY* PFN_WVAP_UPDATE_STA_INFO)(T_AccessPoint* vap, T_AssociatedDevice* dev);

/* <<ADD Functions that do combined task or no HW related >> */

typedef struct S_CWLD_FUNC_TABLE {
    PFN_WRAD_CREATE_HOOK mfn_wrad_create_hook;               /**< Radio constructor hook */
    PFN_WRAD_DESTROY_HOOK mfn_wrad_destroy_hook;             /**< Radio destructor hook */
    PFN_WRAD_ADDVAP_EXT mfn_wrad_addVapExt;                  /**< Create a VAP interface on the RADIO, with pAp object provided*/
    PFN_WRAD_ADDVAPIF mfn_wrad_addvapif;                     /**< Create a VAP interface on the RADIO */
    PFN_WRAD_DELVAPIF mfn_wrad_delvapif;                     /**< Delete a VAP interface  */

    /**
     * generate vap interface name, based on the vendor iface naming rules
     * param ifaceShift indicates the number of proceeding interfaces on the same radio
     */
    swl_rc_ne (* mfn_wrad_generateVapIfName)(T_Radio* rad, uint32_t ifaceShift, char* ifName, size_t ifNameSize);

    /**
     * generate endpoint interface name, based on the vendor iface naming rules
     * param ifaceShift indicates the number of proceeding interfaces on the same radio
     */
    swl_rc_ne (* mfn_wrad_generateEpIfName)(T_Radio* rad, uint32_t ifaceShift, char* ifName, size_t ifNameSize);

    PFN_WRAD_RADIO_STATUS mfn_wrad_radio_status;             /**< Collect (and update) radio status data */
    PFN_WRAD_ENABLE mfn_wrad_enable;                         /**< Enable/Disable the Radio interface */
    PFN_WRAD_MAXBITRATE mfn_wrad_maxbitrate;                 /**< Set/Get the MAX bit rate */
    PFN_WRAD_DFSRADARTRIGGER mfn_wrad_dfsradartrigger;       /**< Automation Test - Trigger a driver DFS radar pulese*/
    PFN_WRAD_SUPFREQBANDS mfn_wrad_supfreqbands;             /**< Get the supported frequence bands */
    PFN_WRAD_SUPSTD mfn_wrad_supstd;                         /**< Supported Standards */
    PFN_WRAD_POSCHANS mfn_wrad_poschans;                     /**< Get HW possible channels (scan) */

    /**
     * Notify vendor plugin that a new target chanspec is set.
     * If direct is set, then the vendor plugin should immediately update the channel, and not
     * wait for commit.
     */
    swl_rc_ne (* mfn_wrad_setChanspec)(T_Radio* rad, bool direct);

    /**
     * Start a ZeroWait DFS switching (called by @mfn_wrad_setChanspec)
     */
    swl_rc_ne (* mfn_wrad_zwdfs_start)(T_Radio* rad, bool direct);

    /**
     * Stop a ZeroWait DFS already started switching.
     */
    swl_rc_ne (* mfn_wrad_zwdfs_stop)(T_Radio* rad);

    /*
     * Get current chanspec from driver
     */
    swl_rc_ne (* mfn_wrad_getChanspec)(T_Radio* rad, swl_chanspec_t* pChSpec);

    PFN_WRAD_SUPPORTS mfn_wrad_supports;                                             /**< Update all ReadOnly Radio states */
    PFN_WRAD_AUTOCHANNELENABLE mfn_wrad_autochannelenable;                           /**< Set/Get Auto Channel Enable */
    PFN_WRAD_STARTACS mfn_wrad_startacs;                                             /**< Trigger Autochannel scan to start! */
    PFN_WRAD_BGDFS_ENABLE mfn_wrad_bgdfs_enable;                                     /**< Set background dfs feature */
    PFN_WRAD_BGDFS_START mfn_wrad_bgdfs_start;                                       /**< Start bg dfs clear for a dedicated channel */
    PFN_WRAD_BGDFS_START_EXT mfn_wrad_bgdfs_start_ext;                               /**< Start bg dfs clear for a dedicated channel with extended options */
    PFN_WRAD_BGDFS_STOP mfn_wrad_bgdfs_stop;                                         /**< Stop bg dfs clear */

    PFN_WRAD_ACHREFPERIOD mfn_wrad_achrefperiod;                                     /**< Set/Get Auto Channel Refresh Period */
    PFN_WRAD_EXTCHAN mfn_wrad_extchan;                                               /**< Set/Get secondary extension channel position (+1/Auto/-1) */
    PFN_WRAD_GUARDINTVAL mfn_wrad_guardintval;                                       /**< Set/Get guard interval between OFDM symbols */
    PFN_WRAD_MCS mfn_wrad_mcs;                                                       /**< Set/Get MCS */
    PFN_WRAD_TXPOW mfn_wrad_txpow;                                                   /**< Set/Get TxPower */
    PFN_WRAD_ANTENNACTRL mfn_wrad_antennactrl;                                       /**< Set/Get Antenna parameters */
    PFN_WRAD_STAMODE mfn_wrad_stamode;                                               /**< Set/Get STA_Mode */
    PFN_WRAD_REGDOMAIN mfn_wrad_regdomain;                                           /**< Set/Get Country code */
    PFN_WRAD_BEAMFORMING mfn_wrad_beamforming;                                       /**< Set/Get beam forming settings */
    PFN_WRAD_RIFS mfn_wrad_rifs;                                                     /**< Set/Get RIFS status */
    PFN_WRAD_AIRTIMEFAIRNESS mfn_wrad_airtimefairness;                               /**< Set/Get AirTimeFairness status */
    PFN_WRAD_RXPOWERSAVE mfn_wrad_rx_powersave;                                      /**< Set/Get Power Save status */
    PFN_WRAD_INTELLIGENTAIRTIME mfn_wrad_intelligentAirtime;                         /**< Set/Get Intelligent airtime scheduler status */
    PFN_WRAD_MULTIUSERMIMO mfn_wrad_multiusermimo;                                   /**< Set/Get MultiUserMIMO status */

    swl_rc_ne (* mfn_wrad_updateConfigMap)(T_Radio* pRad, swl_mapChar_t* configMap); /**< Update the current hostapd radio parameters map, to add or delete parameters */

    /**< Get current transmit power in dBm */
    swl_rc_ne (* mfn_wrad_getCurrentTxPow_dBm)(T_Radio* pRad, int32_t* dbm);

    /**< Get maximum transmit power in dBm */
    swl_rc_ne (* mfn_wrad_getMaxTxPow_dBm)(T_Radio* pRad, uint16_t channel, int32_t* dbm);

    /**< Get Air usage statistics */
    swl_rc_ne (* mfn_wrad_airstats)(T_Radio* pRad, wld_airStats_t* pStats);

    PFN_WRAD_SENSING_CMD mfn_wrad_sensing_cmd;                /**< Start / Stop CSI monitoring */
    PFN_WRAD_SENSING_CSI_STATS mfn_wrad_sensing_csiStats;     /**< Get CSI monitoring stats */
    PFN_WRAD_SENSING_ADD_CLIENT mfn_wrad_sensing_addClient;   /**< Add a client into CSI monitoring */
    PFN_WRAD_SENSING_DEL_CLIENT mfn_wrad_sensing_delClient;   /**< Remove a client into CSI monitoring */
    PFN_WRAD_SENSING_RESET_STATS mfn_wrad_sensing_resetStats; /**< Reset CSI monitoring counters */

    PFN_WRAD_UPDATE_PROB_REQ mfn_wrad_update_prob_req;        /**< Update probe requests */
    PFN_WRAD_SYNC mfn_wrad_sync;                              /**< Sync Enable/channel/band-mode/... */
    PFN_WRAD_SECDMN_RESTART mfn_wrad_secDmn_restart;          /**< Restart radio secDmn */
    PFN_WRAD_SECDMN_REFRESH mfn_wrad_secDmn_refresh;          /**< Refresh radio secDmn */
    PFN_WRAD_TOGGLE mfn_wrad_toggle;                          /**< Toggle radio */
    PFN_WRAD_PER_ANTENNA_RSSI mfn_wrad_per_ant_rssi;          /**< Get the RSSI values of each antenna*/
    PFN_WRAD_LATEST_POWER mfn_wrad_latest_power;              /**< Get the power values of each antenna*/
    PFN_WRAD_UPDATE_CHANINFO mfn_wrad_update_chaninfo;        /**< Update the channel information and statistics*/
    PFN_WRAD_UPDATE_MON_STATS mfn_wrad_update_mon_stats;      /**< Update the non associated station counters of SSID */
    PFN_WRAD_SETUP_STAMON mfn_wrad_setup_stamon;              /**< Set the station monitor */
    PFN_WRAD_ADD_STAMON mfn_wrad_add_stamon;                  /**< Add the non associated station */
    PFN_WRAD_DEL_STAMON mfn_wrad_del_stamon;                  /**< Del the non associated station */
    PFN_WRAD_DELAY_AP_UP_DONE mfn_wrad_delayApUpDone;         /**< Warn driver delay AP up period is over*/
    PFN_WRAD_RADIO_STATS mfn_wrad_stats;                      /**< get radio statistics */
    swl_rc_ne (* mfn_wrad_firmwareVersion)(T_Radio* rad);     /**< get the radio's firmware version */

    PFN_WVAP_CREATE_HOOK mfn_wvap_create_hook;                /**< VAP constructor hook */
    PFN_WVAP_DESTROY_HOOK mfn_wvap_destroy_hook;              /**< VAP destructor hook */
    PFN_WVAP_VAP_STATUS mfn_wvap_status;                      /**< Get ReadOnly VAP states */

    /**< Update list of associated device statistics */
    swl_rc_ne (* mfn_wvap_get_station_stats)(T_AccessPoint* pAP);
    swl_rc_ne (* mfn_wvap_get_single_station_stats)(T_AssociatedDevice* pAD);

    swl_rc_ne (* mfn_wvap_updateConfigMap)(T_AccessPoint* pAP, swl_mapChar_t* configMap); /**< Update the current hostapd vap parameters map, to add or delete parameters */

    PFN_WVAP_VAP_UPDATE_ADL mfn_wvap_update_rssi_stats;                                   /**< Update rssi of associated devices */
    /** Update the stats counters of requested accesspoint BSS */
    swl_rc_ne (* mfn_wvap_update_ap_stats)(T_AccessPoint* vap);

    PFN_WVAP_VAP_ENABLE mfn_wvap_enable;                         /**< Set/Get VAP Enable/Disable state */
    PFN_WVAP_VAP_ENABLE_WMM mfn_wvap_enable_wmm;                 /**< Set/Get WMM state */
    PFN_WVAP_VAP_ENABLE_UAPSD mfn_wvap_enable_uapsd;             /**< Set/Get UAPSD state */
    PFN_WVAP_VAP_SSID mfn_wvap_ssid;                             /**< Set/Get SSID, change hide/show SSID */
    PFN_WVAP_VAP_BSSID mfn_wvap_bssid;                           /**< Set/Get BSSID */
    PFN_WVAP_SYNC mfn_wvap_sync;                                 /**< Sync Enable/Disabel */
    PFN_WVAP_SEC_SYNC mfn_wvap_sec_sync;                         /**< Sync Security mode (none/wep/wpa(2))/Radius/... */
    PFN_WVAP_WPS_SYNC mfn_wvap_wps_sync;                         /**< Sync Mode Start/Stop/Client/Self-pin/... */
    PFN_WVAP_WPS_ENABLE mfn_wvap_wps_enable;                     /**< Set/Get WPS enable state */
    PFN_WVAP_WPS_LABEL_PIN mfn_wvap_wps_label_pin;               /**< Config PIN as label */
    PFN_WVAP_WPS_COMP_MODE mfn_wvap_wps_comp_mode;               /**< Set WPS compatibility mode */
    PFN_WVAP_MF_SYNC mfn_wvap_mf_sync;                           /**< Sync MacFiltering parameters */
    PFN_WVAP_PF_SYNC mfn_wvap_pf_sync;                           /**< Sync ProbeFiltering parameters */
    PFN_WVAP_KICK_STA mfn_wvap_kick_sta;                         /**< Disconnect a connected Station from the VAP */
    PFN_WVAP_KICK_STA_REASON mfn_wvap_kick_sta_reason;           /**< Disconnect a connected Station from the VAP with reason code */
    PFN_WVAP_DISASSOC_STA_REASON mfn_wvap_disassoc_sta_reason;   /**< Disassociate a connected Station from the VAP with reason code */

    /*
     * @brief FTA handler for sending BTM request (11v)
     *
     * @param vap accesspoint hosting the station to be transferred
     * @param params BSS transfer request arguments
     * @return SWL_RC_OK on success (i.e request sent), error code otherwise
     */
    swl_rc_ne (* mfn_wvap_transfer_sta)(T_AccessPoint* vap, wld_transferStaArgs_t* params);
    swl_rc_ne (* mfn_wvap_sendManagementFrame)(T_AccessPoint* vap, swl_80211_mgmtFrameControl_t* fc, swl_macBin_t* sta, swl_bit8_t* data, size_t dataLen, swl_chanspec_t* chanspec);
    swl_rc_ne (* mfn_wvap_setEvtHandlers)(T_AccessPoint* vap);       /**< Set the event handlers from the VAP */

    PFN_WVAP_RRM_REQUEST mfn_wvap_request_rrm_report;                /**< Send a 802.11k remote measurement request */
    PFN_WVAP_CLEAN_STA mfn_wvap_clean_sta;                           /**< Cleanup a non connected station from the VAP */
    PFN_WVAP_MULTIAP_UPDATE_TYPE mfn_wvap_multiap_update_type;       /**< Set MultiAP type */
    PFN_WVAP_MULTIAP_UPDATE_PROFILE mfn_wvap_multiap_update_profile; /**< Set MultiAP profile */
    PFN_WVAP_MULTIAP_UPDATE_PROFILE mfn_wvap_multiap_update_vlanid;  /**< Set MultiAP VLAN ID */

    PNF_WVAP_SET_MBO_DENY_REASON mfn_wvap_setMboDenyReason;          /**< Set MBO Assoc Disallow Reason*/
    PFN_WVAP_SET_AP_ROLE mfn_wvap_set_ap_role;                       /**< Set AccessPoint role */
    PFN_WVAP_ADD_VENDOR_IE mfn_wvap_add_vendor_ie;                   /**< Add vendor IE */
    PFN_WVAP_DEL_VENDOR_IE mfn_wvap_del_vendor_ie;                   /**< Del vendor IE */
    PFN_WVAP_ENAB_VENDOR_IE mfn_wvap_enab_vendor_ie;                 /**< Enable vendor IEs */
    PFN_WVAP_SET_DISCOVERY_METHOD mfn_wvap_set_discovery_method;     /**< Set BSS discovery method */
    PFN_WVAP_SET_CONFIG_DRV mfn_wvap_set_config_driver;              /**< Set Config Driver */

    PFN_WRAD_HASSUPPORT mfn_misc_has_support;                        /**< bool, for driver capabilities
                                                                        "WEP","TKIP","AES","AES_CCM",
                                                                        "CKIP","FF","TURBOP","NOTUSED"
                                                                        "IBSS","PMGT","HOSTAP","AHDEMO",
                                                                        "SWRETRY","TXPMGT","SHSLOT","SHPREAMBLE",
                                                                        "MONITOR","TKIPMIC","WPA1","WPA2",
                                                                        "WPA","BURST","WME","WDS",
                                                                        "WME_TKIPMIC","BGSCAN","UAPSD","FASTCC",
                                                                        "EXPL_BF", "IMPL_BF",
                                                                        "DFS_OFFLOAD","CSA","SAE","SAE_PWE","SCAN_DWELL" */

    PFN_WVAP_FSM_STATE mfn_wvap_fsm_state;                           /**< Get the FSM state of the VAP */
    PFN_WVAP_FSM mfn_wvap_fsm;                                       /**< Do the tasks in parts (use of callback timer) */
    PFN_WVAP_FSM_NODELAY mfn_wvap_fsm_nodelay;                       /**< Do all at once... */

    PFN_WRAD_FSM_STATE mfn_wrad_fsm_state;                           /**< Get the FSM state of the RADIO */
    PFN_WRAD_FSM mfn_wrad_fsm;                                       /**< Do the tasks in parts (use of callback timer) */
    PFN_WRAD_FSM_NODELAY mfn_wrad_fsm_nodelay;                       /**< Do all at once... */

    /**
     * Request the driver to reset the current fsm commit of the given radio
     */
    int (* mfn_wrad_fsm_reset)(T_Radio* pRad);

    PFN_WRAD_FSM_DELAY_COMMIT mfn_wrad_fsm_delay_commit; /**< Timer function for delayed actions after a commit */

    PFN_WIFI_SUPVEND_MODES mfn_wifi_supvend_modes;       /**< Update our current supported vendor modes (RO-fields) */

    PFN_SYNC_RADIO mfn_sync_radio;                       /**< Resync data with our internal Radio structure */
    PFN_SYNC_AP mfn_sync_ap;                             /**< Resync data with our internal AccessPoint structure */
    PFN_SYNC_SSID mfn_sync_ssid;                         /**< Resync data with our internal SSID structure */
    PFN_SYNC_EP mfn_sync_ep;                             /**< Resync data with our internal EndPoint structure */

    PFN_WRAD_GETSPECTRUMINFO mfn_wrad_getspectruminfo;
    PFN_WRAD_GETSCANFILTERINFO mfn_wrad_getscanfilterinfo;

    /**
     * Start a scan, using the scanState.cfg.scanArguments data in T_Radio to complete it.
     */
    swl_rc_ne (* mfn_wrad_start_scan)(T_Radio* pRad);

    /**
     * Stop a currently active scan.
     */
    swl_rc_ne (* mfn_wrad_stop_scan)(T_Radio* pRad);

    /*
     * @brief request optional external scan manager whether to continue or finish the scanning sequence
     *
     * @param pRad pointer to radio context
     *
     * @return SWL_RC_OK if scan should be continued
     *         <= SWL_RC_ERROR to finish it
     */
    swl_rc_ne (* mfn_wrad_continue_external_scan)(T_Radio* pRad);

    PFN_WRAD_SCAN_RESULTS mfn_wrad_scan_results;

    /* <<ADD Functions that do combined task or no HW related >> */
    PFN_HSPOT_ENABLE mfn_hspot_enable;
    PFN_HSPOT_CONFIG mfn_hspot_config;

    PFN_ON_BRIDGE_STATE_CHANGE mfn_on_bridge_state_change;             /**< Reconfigure some vendor deamons when needed */
    PFN_WRAD_ADDENDPOINTIF mfn_wrad_addendpointif;                     /**< Create a EndPoint interface on the RADIO */
    PFN_WRAD_DELENDPOINTIF mfn_wrad_delendpointif;                     /**< Delete a EndPoint interface  */

    PFN_WENDPOINT_CREATE_HOOK mfn_wendpoint_create_hook;               /**< Endpoint create hook */
    PFN_WENDPOINT_DESTROY_HOOK mfn_wendpoint_destroy_hook;             /**< Endpoint destroy hook */
    PFN_WENDPOINT_ENABLE mfn_wendpoint_enable;                         /**< Set/Get Endpoint Enable/Disable state */
    PFN_WENDPOINT_DISCONNECT mfn_wendpoint_disconnect;                 /**< Disassociate with current profile */
    PFN_WENDPOINT_CONNECT_AP mfn_wendpoint_connect_ap;                 /**< Associate with current profile */
    PFN_WENDPOINT_BSSID mfn_wendpoint_bssid;                           /**< Get BSSID of AP currently associated to */
    PFN_WENDPOINT_STATS mfn_wendpoint_stats;
    PFN_WENDPOINT_WPS_START mfn_wendpoint_wps_start;                   /**< Start a WPS attempt */
    PFN_WENDPOINT_WPS_CANCEL mfn_wendpoint_wps_cancel;                 /**< Cancel a WPS attempt */
    PFN_WEP_ENABLE_VENDOR_ROAMING mfn_wendpoint_enable_vendor_roaming; /**< enable/disable vendor roaming */
    PFN_WEP_UPDATE_VENDOR_ROAMING mfn_wendpoint_update_vendor_roaming; /**< update vendor roam config */
    PFN_WENDPOINT_MULTIAP_ENABLE mfn_wendpoint_multiap_enable;         /**< Set MultiAP BackhaulSTA */
    PFN_WENDPOINT_SET_MAC_ADDR mfn_wendpoint_set_mac_address;          /**< Set Mac address on an endpoint interface */
    PFN_WENDPOINT_UPDATE mfn_wendpoint_update;                         /**< Update endpoint with new configuration */
    PFN_WEP_STATUS mfn_wendpoint_status;

    swl_rc_ne (* mfn_wendpoint_sendManagementFrame)(T_EndPoint* pEP, swl_80211_mgmtFrameControl_t* fc, swl_macBin_t* sta, swl_bit8_t* data, size_t dataLen, swl_chanspec_t* chanspec);

    swl_rc_ne (* mfn_wendpoint_updateConfigMaps)(T_EndPoint* pEP, wld_wpaSupp_config_t* configMap); /**< Update the current wpa_supplicant global/network parameters map, to add or delete parameters */

    PFN_WVAP_UPDATE_STA_INFO mfn_wvap_update_assoc_dev;                                             /** Update settable changed to associated devices */

    /**
     * A neighbour has been added or updated on the AP. The system should add the new neighbour
     * to the list of supported neighbours
     */
    swl_rc_ne (* mfn_wvap_updated_neighbour)(T_AccessPoint* pAP, T_ApNeighbour* newNeighbour);
    /**
     * A neighbour has been removed from the list of neighbours of this AP. The system
     * should remove the AP from its list.
     */
    swl_rc_ne (* mfn_wvap_deleted_neighbour)(T_AccessPoint* pAP, T_ApNeighbour* newNeighbour);

    /**
     * Notify set of mldUnit
     */
    swl_rc_ne (* mfn_wvap_setMldUnit)(T_AccessPoint* pAP);

    /**
     * Notify set of mldConfig
     */
    swl_rc_ne (* mfn_wvap_setMldCfg)(T_AccessPoint* pAP);

    /*
     * Notify Dmn execution settings change to vendor
     */
    swl_rc_ne (* mfn_wvdr_setDmnExecSettings)(vendor_t* pVdr, const char* dmnName, wld_dmnMgt_dmnExecSettings_t* pCfg);

    /** get stats counters of requested affiliated accesspoint BSS */
    swl_rc_ne (* mfn_wvap_getMloStats)(T_AccessPoint* vap, wld_mloStats_t* stats);
    PFN_WRAD_STARTPLATFORMACS mfn_wrad_startPltfACS; /**< Start platform ACS process! */
} T_CWLD_FUNC_TABLE;

struct vendor {
    amxc_llist_it_t it;
    char* name;
    T_CWLD_FUNC_TABLE fta;
    wld_fsmMngr_t* fsmMngr;
    wld_dmnMgt_dmnExecInfo_t* globalHostapd;
    wld_mldMgr_t* pMldMgr;
};

typedef struct S_wld_callbackinfo {
    void* mfn_userdata;
} wld_callbackinfo_t;

bool wld_sync_assoclist(T_AccessPoint* pAP);

void wld_cleanup();

vendor_t* wld_registerVendor(const char* name, T_CWLD_FUNC_TABLE* fta);
vendor_t* wld_getVendorByName(const char* name);
const char* wld_getRootObjName();
bool wld_isVendorUsed(vendor_t* vendor);
bool wld_unregisterVendor(vendor_t* vendor);
bool wld_unregisterAllVendors();

int wld_addRadio(const char* name, vendor_t* vendor, int idx);
T_Radio* wld_createRadio(const char* name, vendor_t* vendor, int idx);
void wld_deleteRadio(const char* name);
void wld_deleteRadioObj(T_Radio* pRad);
void wld_deleteAllRadios();
void wld_deleteAllVaps();
void wld_deleteAllEps();
uint32_t wld_countRadios();

T_Radio* wld_getRadioByIndex(int index);
T_Radio* wld_getRadioByName(const char* name);
T_Radio* wld_getRadioOfIfaceIndex(int index);
T_Radio* wld_getRadioOfIfaceName(const char* ifName);
T_Radio* wld_getRadioByWiPhyId(int32_t wiPhyId);
T_Radio* wld_getUinitRadioByBand(swl_freqBandExt_e band);
T_EndPoint* wld_getEndpointByAlias(const char* name);
T_AccessPoint* wld_getAccesspointByAlias(const char* name);
T_Radio* wld_getRadioByAddress(unsigned char* macAddress);
T_AccessPoint* wld_getAccesspointByAddress(unsigned char* macAddress);
swl_freqBandExt_m wld_getAvailableFreqBands(T_Radio* ignoreRad);
swl_timeSpecMono_t* wld_getInitTime();
swl_rc_ne wld_initRadioBaseMac(T_Radio* pR, int32_t idx);

void wld_destroy_associatedDevice(T_AccessPoint* pAP, int index);

char* wld_getVendorParam(const T_Radio*, const char* parameter, const char* def);
int wld_getVendorParam_int(const T_Radio*, const char* parameter, const int def);

bool wld_isInternalBSSID(const char bssid[ETHER_ADDR_STR_LEN]);
bool wld_isInternalBssidBin(const swl_macBin_t* bssid);

/* Share this handlers */
amxd_object_t* get_wld_object();
amxd_dm_t* get_wld_plugin_dm();
amxb_bus_ctx_t* get_wld_plugin_bus();
amxo_parser_t* get_wld_plugin_parser();
const swl_macBin_t* wld_getWanAddr();

extern T_CONST_WPS g_wpsConst;

void wld_functionTable_init(vendor_t* vendor, T_CWLD_FUNC_TABLE* fta);

T_Radio* wld_firstRad();
T_Radio* wld_lastRad();
T_Radio* wld_nextRad(T_Radio* pRad);
T_Radio* wld_prevRad(T_Radio* pRad);
T_Radio* wld_firstRadFromObjs();
T_Radio* wld_lastRadFromObjs();

#define wld_for_eachRad(radPtr) \
    for(radPtr = wld_firstRad(); radPtr; radPtr = wld_nextRad(radPtr))

bool wld_plugin_start();
void wld_plugin_init(amxd_dm_t* dm, amxo_parser_t* parser);

vendor_t* wld_firstVdr();
vendor_t* wld_lastVdr();
vendor_t* wld_nextVdr(const vendor_t* pVdr);

#define wld_for_eachVdr(vdrPtr) \
    for(vdrPtr = wld_firstVdr(); vdrPtr; vdrPtr = wld_nextVdr(vdrPtr))

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __CWLD_H__ */
