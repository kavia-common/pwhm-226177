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
#ifndef __WLD_UTIL_H__
#define __WLD_UTIL_H__

#include "wld.h"
#include "swl/swl_genericFrameParser.h"
#include "swl/swl_hex.h"


#ifdef __cplusplus
extern "C" {
#endif

// Fix compiler warnings
#define _UNUSED_(x) (void) x
#define _UNUSED __attribute__((unused))

// Generic utilities
/* Taken from NeMo, used in attribute flags */
#define ARG_BYNAME request_function_args_by_name
#define ARG_MANDATORY   (0x80000000)

#define TRACEZ_IO_IN "ioIn"
#define TRACEZ_IO_OUT "ioOut"
#define TRACEZ_IO_PROBE "ioProbe" //Special trace zone for probe updates.

/* For our helper function */
#define TPH_STR       (0x01)
#define TPH_INT8      (0x02)
#define TPH_INT16     (0x03)
#define TPH_INT32     (0x04)
#define TPH_UINT8     (0x05)
#define TPH_UINT16    (0x06)
#define TPH_UINT32    (0x07)
#define TPH_BOOL      (0x08)
#define TPH_DOUBLE    (0x09)
#define TPH_BSTR      (0x0A)
#define TPH_SECSTR    (0x0B)
#define TPH_WPS_CM    (0x0C)
#define TPH_SEL_SSID  (0x0D)
#define TPH_PSTR      (0x0E)

#define MAX_USER_DEFINED_MAGIC  256

#define WLD_S_BUF 32
#define WLD_M_BUF 128
#define WLD_L_BUF 1024
#define WLD_XL_BUF 8192

#define WLD_SEC_PER_H 3600
#define WLD_SEC_PER_DAY 86400

typedef struct {
    swl_80211_mgmtFrameControl_t fc;
    swl_chanspec_t chanspec;
    swl_macBin_t mac;
    swl_bit8_t* data;
    size_t dataLen;
} wld_util_managementFrame_t;

typedef struct {
    char buf[WLD_S_BUF];
} wld_sbuf_t;

typedef struct {
    char buf[WLD_M_BUF];
} wld_mbuf_t;

typedef struct {
    char buf[WLD_L_BUF];
} wld_lbuf_t;

#define isxDigit(c)  ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))

#define WLD_MIN(a, b) \
    ({ __typeof__ (a) _a = (a); \
         __typeof__ (b) _b = (b); \
         _a < _b ? _a : _b; })
#define WLD_MAX(a, b) \
    ({ __typeof__ (a) _a = (a); \
         __typeof__ (b) _b = (b); \
         _a > _b ? _a : _b; })

#define WLD_ARRAY_SIZE(X) (((X) == NULL) ? 0 : (sizeof((X)) / sizeof((X)[0])))
#define WLD_BYTE_SIZE(X) (sizeof(typeof(X)))
#define WLD_HEXSTR_SIZE(X) (WLD_BYTE_SIZE(X) * 2)
#define WLD_BIT_SIZE(X) (WLD_BYTE_SIZE(X) * 8)

bool __WLD_BUG_ON(bool bug, const char* condition, const char* function,
                  const char* file, int line);

#define WLD_BUG_ON(x) \
    __WLD_BUG_ON(!!(x), #x, __FUNCTION__, __FILE__, __LINE__)

typedef int32_t lbool_t;
#define LTRUE __LINE__
#define LFALSE -__LINE__
#define LBOOL(x) ((x) ? LTRUE : LFALSE)

void wldu_llist_freeDataInternal(amxc_llist_t* list, size_t offset);
void wldu_llist_freeDataFunInternal(amxc_llist_t* list, size_t offset, void (* destroy)(void* val));
void wldu_llist_mapInternal(amxc_llist_t* list, size_t offset, void (* map)(void* val, void* data), void* data);


#define llist_freeData(list, type, it) wldu_llist_freeDataInternal(list, offsetof(type, it))
#define llist_destroyData(list, destroyFun, type, it) wldu_llist_freeDataInternal(list, offsetof(type, it), destroyFun)
#define llist_map(list, mapFun, type, it) wldu_llist_freeDataInternal(list, offsetof(type, it), mapFun, data)

/* Helper macro's for MAC debugging */
#define MAC_PRINT_FMT   "%02x:%02x:%02x:%02x:%02x:%02x"
#define MAC_PRINT_ARG(mac) (mac)[0], (mac)[1], (mac)[2], (mac)[3], (mac)[4], (mac)[5]

/* MAC address define */
#define WLD_EMPTY_MAC_ADDRESS "00:00:00:00:00:00"
#define WLD_BROADCAST_MAC_ADDRES "FF:FF:FF:FF:FF:FF"

/* Helper macros for deltas on unsigned types */
#define DELTA64(inNew, inOld) ((inNew >= inOld) ? (inNew - inOld) : (ULLONG_MAX - ((uint64_t) inOld) + ((uint64_t) inNew)))
#define DELTA32(inNew, inOld) ((inNew >= inOld) ? (inNew - inOld) : (UINT32_MAX - ((uint32_t) inOld) + ((uint32_t) inNew)))

static const uint8_t wld_ether_bcast[ETHER_ADDR_LEN] = {255, 255, 255, 255, 255, 255};
static const uint8_t wld_ether_null[ETHER_ADDR_LEN] = {0, 0, 0, 0, 0, 0};

bool findStrInArray(const char* str, const char** strarray, int* index);
int findStrInArrayN(const char* str, const char** strarray, int size, int default_index);
bool convIntArrToString(char* str, int str_size, const int* int_list, int list_size);
bool convStrArrToStr(char* str, int str_size, const char** strList, int list_size);
bool convStrToIntArray(int* int_list, size_t list_size, const char* str, int* outSize);
bool set_OBJ_ParameterHelper(int type, amxd_object_t* obj, char* rpath, const void* pdata);
bool get_OBJ_ParameterHelper(int type, amxd_object_t* obj, const char* crpath, void* pdata);
unsigned long conv_ModeBitStr(char** enumStrList, char* str);
long conv_ModeIndexStr(const char** enumStrList, const char* str);
bool conv_maskToStrSep(uint32_t mask, const char** enumStrList, uint32_t maxVal, char* str, uint32_t strSize, char separator);
bool conv_maskToStr(uint32_t mask, const char** enumStrList, uint32_t maxVal, char* str, uint32_t strSize);
uint32_t conv_strToEnum(const char** enumStrList, const char* str, uint32_t maxVal, uint32_t defaultVal);
int conv_ModeIndexStrEx(const char** enumStrList, const char* str, int minIdx, int maxIdx);
char* itoa(int value, char* result, int base);

int wldu_convStrToUInt8Arr(uint8_t* tgtList, uint tgtListSize, char* srcStrList);
int getLowestBitLongArray(unsigned long* pval, int elem);
int areBitsSetLongArray(unsigned long* pval, int elem);
int clearAllBitsLongArray(unsigned long* pval, int elem);
int isBitSetLongArray(unsigned long* pval, int elem, int bitNr);
int setBitLongArray(unsigned long* pval, int elem, int bitNr);
int clearBitLongArray(unsigned long* pval, int elem, int bitNr);
int markAllBitsLongArray(unsigned long* pval, int elem, int bitNr);
int isBitOnlySetLongArray(unsigned long* pval, int elem, int bitNr);
int areBitsOnlySetLongArray(unsigned long* pval, int elem, int* bitNrArray, int nrBitElem);

void moveFSM_VAPBit2ACFSM(T_AccessPoint* pAP);
void moveFSM_RADBit2ACFSM(T_Radio* pR);
void moveFSM2ACFSM(T_FSM* fsm);


void longArrayCopy(unsigned long* to, unsigned long* from, int len);
void longArrayBitOr(unsigned long* to, unsigned long* from, int len);
void longArrayClean(unsigned long* array, int len);

char* convASCII_WEPKey(const char* Key, char* out, int sizeout);
char* convHex_WEPKey(const char* key, char* out, int sizeout);

swl_security_apMode_e isValidWEPKey (const char* key);
bool isValidPSKKey(const char* key);
bool isValidAESKey(const char* key, int maxLen);
bool isValidSSID(const char* ssid);
bool isSanitized (const char* pname);

int isListOfChannels_US(unsigned char* possibleChannels, int len, int band);

#define WPS_PIN_LEN 8
void wpsPinGen(char pwd[WPS_PIN_LEN + 1]);
int wpsPinValid(unsigned long PIN);
bool wldu_checkWpsPinStr(const char* pinStr);

int wldu_convStrToNum(const char* numSrcStr, void* numDstBuf, uint8_t numDstByteSize, uint8_t base, bool isSigned);
int32_t wldu_convStr2Mac(unsigned char* mac, uint32_t sizeofmac, const char* str, uint32_t sizeofstr);
bool wldu_convMac2Str(unsigned char* mac, uint32_t sizeofmac, char* str, uint32_t sizeofstr);
int mac2str(char* str, const unsigned char* mac, size_t maxstrlen);
int convQuality2Signal(int quality);
int convSignal2Quality(int dBm);

/* SSID manipulation */
char* wld_ssid_to_string(const uint8_t* ssid, uint8_t len);
int convSsid2Str(const uint8_t* ssid, uint8_t ssidLen, char* str, size_t maxStrSize);

void fsm_delay_reply(uint64_t call_id, amxd_status_t state, int* errval);
int getCountryParam(const char* instr, int CC, int* idx);
char* getFullCountryName(int idx);
char* getShortCountryName(int idx);
int getCountryCode(int idx);
swl_opClassCountry_e getCountryZone(int idx);

int debugIsVapPointer(void* p);
int debugIsEpPointer(void* p);
int debugIsRadPointer(void* p);
int debugIsSsidPointer(void* p);

int get_random(unsigned char* buf, size_t len);
int get_randomstr(unsigned char* buf, size_t len);
int get_randomhexstr(unsigned char* buf, size_t len);

char* stripOutToken(char* pD, const char* pT);
char** stripString(char** pTL, int nrTL, char* pD, const char* pT);

void convStrToHex(uint8_t* pbDest, int destSize, const char* pbSrc, int srcSize);
int get_pattern_string(const char* arg, uint8_t* pattern);

/* libc5 combatability just declare them */
unsigned int if_nametoindex(const char* __ifname);


//how to go from accumulator to actual value. add + or - 499 to ensure proper rounding
#define WLD_ACC_TO_VAL(val) (val >= 0 ? ((val + 500) / 1000) : ((val - 500) / 1000))
int32_t wld_util_performFactorStep(int32_t accum, int32_t newVal, int32_t factor);

int wld_writeCapsToString(char* buffer, uint32_t size, const char* const* strArray, uint32_t caps, uint32_t maxCaps);
void wld_bitmaskToCSValues(char* string, size_t stringsize, const int bitmask, const char* strings[]);
bool wld_util_areAllVapsDisabled(T_Radio* pRad);
bool wld_util_areAllEndpointsDisabled(T_Radio* pRad);
bool wld_util_isPowerOf(int32_t value, int32_t power);
bool wld_util_isPowerOf2(int32_t value);
int wld_util_getLinuxIntfState(int socket, char* ifname);
int wld_util_setLinuxIntfState(int socket, char* ifname, int state);

typedef struct {
    int64_t id;
    int64_t index;
} wld_tupleId_t;

typedef struct {
    wld_tupleId_t* tuples;
    uint32_t size;
} wld_tupleIdlist_t;

typedef struct {
    int64_t id;
    char* str;
} wld_tuple_t;

typedef struct {
    wld_tuple_t* tuples;
    uint32_t size;
} wld_tuplelist_t;

typedef struct {
    int64_t id;
    void* ptr;
} wld_tupleVoid_t;

typedef struct {
    wld_tupleVoid_t* tuples;
    uint32_t size;
} wld_tupleVoidlist_t;

wld_tupleVoid_t* wldu_getTupleVoidById(wld_tupleVoidlist_t* list, int64_t id);
wld_tupleId_t* wldu_getTupleIdById(wld_tupleIdlist_t* list, int64_t id);
wld_tuple_t* wldu_getTupleById(wld_tuplelist_t* list, int64_t id);
wld_tuple_t* wldu_getTupleByStr(wld_tuplelist_t* list, char* str);
const char* wldu_tuple_id2str(wld_tuplelist_t* list, int64_t id, const char* defStr);
const void* wldu_tuple_id2void(wld_tupleVoidlist_t* list, int64_t id);
int64_t wldu_tuple_str2id(wld_tuplelist_t* list, char* str, int64_t defId);
int64_t wldu_tuple_id2index(wld_tupleIdlist_t* list, int64_t id);
bool wldu_tuple_convStrToMaskByMask(wld_tuplelist_t* pList, const amxc_string_t* pNames, uint64_t* pBitMask);
bool wldu_tuple_convMaskToStrByMask(wld_tuplelist_t* pList, uint64_t bitMask, amxc_string_t* pNames);
char* wldu_getLocalFile(char* buffer, size_t bufSize, char* path, char* format, ...);

swl_bandwidth_e wld_util_getMaxBwCap(wld_assocDev_capabilities_t* caps);
bool wldu_key_matches(const char* ssid, const char* oldKeyPassPhrase, const char* keyPassPhrase);
#define WLD_TUPLE(ID, STR) {ID, STR}
#define WLD_TUPLE_GEN(ID) {ID,#ID}
#define WLD_TUPLE_DEF_STR ""

void wldu_convCreds2MD5(const char* ssid, const char* key, char* md5, size_t md5_size);

void wld_util_updateStatusChangeInfo(wld_status_changeInfo_t* info, wld_status_e status);

/**
 * @brief wld_util_updateStats convert the internal stats context to object map,
 * this object is the actual datamodel representation of the internal context stats
 *
 * @param obj the dict output
 * @param stats stats the internal stats (input)
 * @return amxd_status_t return code
 */
amxd_status_t wld_util_stats2Obj(amxd_object_t* obj, T_Stats* stats);

/**
 * @brief dump the stats object tree into variant map,
 * then the variant map can be the return of any amxd callback
 *
 * @param map the dict output
 * @param statsObj the stats object (input)
 * @return amxd_status_t return code
 */
amxd_status_t wld_util_statsObj2Var(amxc_var_t* map, amxd_object_t* statsObj);

/**
 * @brief add stats diff to accumulated stats
 *
 * @param pAccStats pointer to the accumulated stats
 * @param pDiffStats pointer to the diff stats to be added to accu
 * @return void
 */
void wld_util_accumulateStats(wld_stats_t* pAccStats, wld_stats_t* pDiffStats);

void wld_util_initCustomAlias(amxd_trans_t* trans, amxd_object_t* object);

/**
 * @brief return previous object instance
 */
amxd_object_t* wld_util_getPrevObjInst(amxd_object_t* instance);

/*
 * @brief return next object instance
 */
amxd_object_t* wld_util_getNextObjInst(amxd_object_t* instance);

/**
 * @brief get real reference path:
 * when input Reference path (from upper layer instance) is available
 * then check it against currently referenced object path (regardless referencePath has indexed/named formats).
 * If different (empty, or pointing somewhere else), then set the output reference Path with the indexed format path
 * of the pointed referenced object.
 *
 * @param outRefPath (out) target buffer for referenced object path (dot terminated (as per tr181-usp standard))
 * @param outRefPathSize (out) target buffer size
 * @param currRefPath (in) currently saved reference path from datamodel
 * @param currRefObj (in) currently referenced object pointed by the upper layer (AP/EP => SSID or SSID => Radio)
 *
 * @return SWL_RC_OK if successful (target path set), error code otherwise
 */
swl_rc_ne wld_util_getRealReferencePath(char* outRefPath, size_t outRefPathSize, const char* currRefPath, amxd_object_t* currRefObj);

/**
 * @brief get the custom path prefix for reference paths
 * expected to contain the missing path that sits above pwhm root object (i.e. WiFi.)
 * Device. is the commonly used root object of the TR-181 datamodel
 */
const char* wld_util_getReferencePathPrefix();

/**
 * @brief set the custom path prefix for reference paths
 */
bool wld_util_setReferencePathPrefix(const char* prefix);

/**
 * @brief prefix @param[in] currRefPath with ReferencePathPrefix and write the result to @param[out] outRefPath
 * @param[out] outRefPath the resulting path to an instance that always starts with ReferencePathPrefix
 * @param[in] outRefPathSize the allocated size of outRefPath. no reallocation is done;
 * @param[in] currRefPath a path to an instance that may or may not start with ReferencePathPrefix
 */
bool wld_util_addReferencePathPrefix(char* outRefPath, size_t outRefPathSize, const char* currRefPath);

swl_rc_ne wld_util_getManagementFrameParameters(T_Radio* pRad, wld_util_managementFrame_t* mgmtFrame, amxc_var_t* args);

/**
 * @brief count number of neighbor BSS scan entries detected on a provided primary channel
 *
 * @param results the scan results list
 * @param channel the host channel
 * @return uint32_t count of BSSs detected on the channel
 */
uint32_t wld_util_countScanResultEntriesPerChannel(wld_scanResults_t* results, swl_channel_t channel);

/**
 * @brief fetch, by channel, a spectrum result entry in provided list
 *
 * @param pSpectrumInfoList the spectrum info list
 * @param channel the host channel
 * @return wld_spectrumChannelInfoEntry_t* pointer to the entry node in the list, or NULL when not match
 */
wld_spectrumChannelInfoEntry_t* wld_util_getSpectrumEntryByChannel(amxc_llist_t* pSpectrumInfoList, swl_channel_t channel);

/**
 * @brief add or update a spectrum result entry in provided list, based on the channel of the provided data struct
 *
 * @param pSpectrumInfoList the spectrum info list
 * @param channel the host channel
 * @return wld_spectrumChannelInfoEntry_t* pointer to the entry node in the list, or NULL when not match
 */
wld_spectrumChannelInfoEntry_t* wld_util_addorUpdateSpectrumEntry(amxc_llist_t* llSpectrumChannelInfo, wld_spectrumChannelInfoEntry_t* pData);

/*
 * @brief return resolved absolute path of executable command,
 * by searching in semi-colon separated string list of paths
 *
 * @param searchPaths semi-colon separated string list of paths
 * @param cmd command to search for it
 * @param buf output buffer including the full command path (if found)
 * @param bufSize maximum buf size
 *
 * @return SWL_RC_OK if successful, error code otherwise:
 *         SWL_RC_NOT_FOUND command not found in any dir of PATH env variable
 *         SWL_RC_RESULT_OUT_OF_BOUNDS incomplete path caused by too short target buffer size
 */
swl_rc_ne wld_util_fetchExecutablePath(const char* searchPaths, const char* cmd, char* buf, size_t bufSize);

/*
 * @brief return resolved absolute path of executable command,
 * by searching in directories of PATH environment variable
 * (same output as shell command 'which')
 *
 * @param cmd command to search for it
 * @param buf output buffer including the full command path (if found)
 * @param bufSize maximum buf size
 *
 * @return SWL_RC_OK if successful, error code otherwise:
 *         SWL_RC_NOT_FOUND command not found in any dir of PATH env variable
 *         SWL_RC_RESULT_OUT_OF_BOUNDS incomplete path caused by too short target buffer size
 */
swl_rc_ne wld_util_getExecutablePath(const char* cmd, char* buf, size_t bufSize);
/**
 * @brief copy Info Elements details into scan results struct
 *
 * @param pResult scan result entry
 * @param pWirelessDevIE wireless frame parsed Info elements
 * @return SWL_RC_OK if successful error code otherwise
 */
swl_rc_ne wld_util_copyScanInfoFromIEs(wld_scanResultSSID_t* pResult, swl_wirelessDevice_infoElements_t* pWirelessDevIE);

/*
 * @brief build EHT Operation Information Element
 *
 * @param tgtChspec target channelspec
 * @param bitmap disabled subchannel bitmap
 * @param nTx number of transmit antenna
 * @param nRx number of receive antenna
 *
 * @return swl_80211_ehtOpIE_t which contains eht operation IE
 */
swl_80211_ehtOpIE_t wld_util_buildEhtOperationIE(swl_chanspec_t tgtChspec, swl_bit32_t bitmap, uint32_t nTx, uint32_t nRx);
#ifdef __cplusplus
}/* extern "C" */
#endif

#endif
