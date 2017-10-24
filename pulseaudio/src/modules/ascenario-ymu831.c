/*
 * Copyright(c) 2012 Yamaha Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <alsa/asoundlib.h>
#include <errno.h>

#include <pulsecore/log.h>

#define USE_SCN_TO_GRP_FILE

#define YSOUND_CONF			"/usr/etc/sound/ymu831/ysound.conf"
#define MIXER_NAME_OUT			"default"
#define DEVFILE_HW_CARD_NUMBER		0
#define DEVFILE_HW_DEVICE_NUMBER	0

#define MAX_NAME			(64)
#define MAX_PATH_NAME			(256)
#define MAX_VALUE_LEN			(8)
#define ERROR				(-1)
#define TARGET_PARAMETER		(1)
#define TARGET_MIXER			(2)
#define NOT_FINED			(-1)
#define MC_ASOC_MAGIC			'N'
#define MC_ASOC_IOCTL_SET_CTRL		(1)
#define YMC_IOCTL_SET_CTRL \
	_IOW(MC_ASOC_MAGIC, MC_ASOC_IOCTL_SET_CTRL, ymc_ctrl_args_t)
#define FILE_OPEN_MODE			"rb"
#define USE_FOPEN			(0)

#define YMC_DSP_OUTPUT_BASE		0x00000000
#define YMC_DSP_INPUT_BASE		0x00000010
#define YMC_DSP_VOICECALL_BASE_1MIC	0x00000100
#define YMC_DSP_VOICECALL_BASE_2MIC	0x00000200
#define YMC_DSP_VOICECALL_BASE_COMMON	0x00000F00
#define YMC_DSP_OPTION_ERROR		0xFFFFFFFF

#define DSP_OUTPUT_NAME			"DSP (Output)"
#define DSP_INPUT_NAME			"DSP (Input)"
#define DSP_VOICECALL_NAME		"VoiceCall"
#define DSP_VOICECALL_1MIC_NAME		"VoiceCall (1MIC)"
#define DSP_VOICECALL_2MIC_NAME		"VoiceCall (2MIC)"

/* Added for header */
#define IN		0
#define OUT	16
#define ADDON	31

#define INPUT_CH_0			((1 << (0 + IN)))	/* Main Mic.         0x00000001 */
#define INPUT_CH_1			((1 << (1 + IN)))	/* Sub Mic.          0x00000002 */
#define INPUT_CH_2			((1 << (2 + IN)))	/* Stereo Mic. ,     0x00000004 */
#define INPUT_CH_3			((1 << (3 + IN)))	/* Ear Mic. ,        0x00000008 */
#define INPUT_CH_4			((1 << (4 + IN)))	/* BT Mic.           0x00000010 */
#define INPUT_CH_5			((1 << (5 + IN)))	/* AP                0x00000020 */
#define INPUT_CH_6			((1 << (6 + IN)))	/* CP                0x00000040 */
#define INPUT_CH_7			((1 << (7 + IN)))	/* FM Radio          0x00000080 */
#define INPUT_CH_8			((1 << (8 + IN)))	/* Reserved */
#define INPUT_CH_9			((1 << (9 + IN)))	/* Reserved */

#define OUTPUT_CH_0			((1 << (0 + OUT)))	/* Headset (Earphone)    0x00010000 */
#define OUTPUT_CH_1			((1 << (1 + OUT)))	/* Left Speaker ,        0x00020000 */
#define OUTPUT_CH_2			((1 << (2 + OUT)))	/* Right Speaker ,       0x00040000 */
#define OUTPUT_CH_3			((1 << (3 + OUT)))	/* Stereo Speaker ,      0x00080000 */
#define OUTPUT_CH_4			((1 << (4 + OUT)))	/* Receiver (Mono) ,     0x00100000 */
#define OUTPUT_CH_5			((1 << (5 + OUT)))	/* BT Headset            0x00200000 */
#define OUTPUT_CH_6			((1 << (6 + OUT)))	/* CP                    0x00400000 */
#define OUTPUT_CH_7			((1 << (7 + OUT)))	/* AP                    0x00800000 */
#define OUTPUT_CH_8			((1 << (8 + OUT)))	/* Gain 		 0x01000000 */
#define OUTPUT_CH_9			((1 << (9 + OUT)))	/* Video call gain	 0x02000000 */
#define OUTPUT_CH_10			((1 << (10 + OUT)))	/* Video call gain 	 0x04000000 */
#define OUTPUT_CH_11			((1 << (11 + OUT)))	/* HDMI 		 0x08000000 */
#define OUTPUT_CH_12			((1 << (12 + OUT)))	/* Dock 		 0x10000000 */
#define OUTPUT_CH_13			((1 << (13 + OUT)))	/* Call alert Gain 	 0x20000000 */
#define OUTPUT_CH_14			((1 << (14 + OUT)))	/* Reserved 		 0x40000000 */
#define ADDON_MODE			(1 << ADDON)		/* For add-ons		 0x80000000 */
enum {
	ADDON_AP_PLAYBACK_INCALL = 1,
	ADDON_AP_PLAYBACK_1MIC_INCALL,
	ADDON_AP_PLAYBACK_BT_INCALL,

};

#define INPUT_MAIN_MIC		(INPUT_CH_0)
#define INPUT_SUB_MIC		(INPUT_CH_1)
#define INPUT_STEREO_MIC	(INPUT_CH_2)
#define INPUT_EAR_MIC		(INPUT_CH_3)
#define INPUT_BT_MIC		(INPUT_CH_4)
#define INPUT_AP			(INPUT_CH_5)
#define INPUT_CP			(INPUT_CH_6)
#define INPUT_FMRADIO		(INPUT_CH_7)

#define OUTPUT_HEADSET		(OUTPUT_CH_0)
#define OUTPUT_LEFT_SPK		(OUTPUT_CH_1)
#define OUTPUT_RIGHT_SPK	(OUTPUT_CH_2)
#define OUTPUT_STEREO_SPK	(OUTPUT_CH_3)
#define OUTPUT_RECV			(OUTPUT_CH_4)
#define OUTPUT_BT_HEADSET	(OUTPUT_CH_5)
#define OUTPUT_CP			(OUTPUT_CH_6)
#define OUTPUT_AP			(OUTPUT_CH_7)
#define OUTPUT_HDMI			(OUTPUT_CH_11)
#define OUTPUT_DOCK			(OUTPUT_CH_12)

#define GAIN_MODE			(OUTPUT_CH_8)
#define GAIN_VIDEO_CALL		(OUTPUT_CH_9)
#define GAIN_VOICE_CALL		(OUTPUT_CH_10)


#define GAIN_CALLALERT		(OUTPUT_CH_13)
/* Added for header : End */

struct GROUP_INFO {
	char abName[MAX_NAME];
	int nStartPos;
	int nEndPos;
};

typedef struct _ymc_ctrl_args {
	void		*param;
	unsigned long	size;
	unsigned long	option;
} ymc_ctrl_args_t;

struct FILE_DATA {
	char *pbData;
	int nSize;
};

struct SCENARIO_GROUP {
	char *pbScenario;
	char *pbGroup;
#ifdef USE_SCN_TO_GRP_FILE
	void *pNext;
#endif
};

#define STR_GAIN							"_gain"
#define STR_VC_GAIN							"_voicecall_gain"
#define STR_RT_GAIN							"_ringtone_gain"
#define STR_VT_GAIN							"_videocall_gain"
#define STR_VOIP_GAIN						"_voip_gain"
/* AP PLAYBACK */
#define STR_AP_SPK							"ap_to_speaker"
#define STR_AP_HEADSET						"ap_to_headset"
#define STR_AP_RECV							"ap_to_receiver"
#define STR_AP_BT							"ap_to_bt"
#define STR_AP_HDMI							"ap_to_hdmi"
#define STR_AP_DOCK							"ap_to_dock"
#define STR_AP_SP_HS						"ap_to_sp_hs"
/* AP CAPTURE */
#define STR_MIC1_AP							"mainmic_to_ap"
#define STR_MIC2_AP							"submic_to_ap"
#define STR_STEREO_MIC_AP					"stereomic_to_ap"
#define STR_EARMIC_AP						"earmic_to_ap"
#define STR_CAMCORDER_STEREO				"capture_ap_camcorder_stereo"
#define STR_BT_AP							"bt_to_ap"
#define STR_CP_AP							"cp_to_ap"
/* CP PLAYBACK */
#define STR_CP_SPK							"cp_to_speaker"
#define STR_CP_HEADSET						"cp_to_headset"
#define STR_CP_HEADPHONE					"cp_to_headphone"
#define STR_CP_RECV_2MIC					"cp_to_receiver_2mic"
#define STR_CP_RECV							"cp_to_receiver"
#define STR_CP_BT							"cp_to_bt"
/* CP CAPTURE */
#define STR_MIC1_CP							"mainmic_to_cp"
#define STR_MIC2_CP							"submic_to_cp"
#define STR_EARMIC_CP						"earmic_to_cp"
#define STR_BT_CP							"bt_to_cp"
/* FMRADIO */
#define STR_FM_AP							"fmradio_to_ap"
#define STR_FM_SPEAKER						"fmradio_to_speaker"
#define STR_FM_HEADSET						"fmradio_to_headset"
/* VOICECALL */
#define STR_VOICECALL_SPK					"voicecall_speaker"
#define STR_VOICECALL_SPK_EXTVOL			"voicecall_speaker_extvol"
#define STR_VOICECALL_SPK_NB				"voicecall_speaker_upsampling"
#define STR_VOICECALL_SPK_NB_EXTVOL			"voicecall_speakerupsampling_extvol"
#define STR_VOICECALL_SPK_2MIC				"voicecall_speaker_2mic"
#define STR_VOICECALL_SPK_2MIC_EXTVOL		"voicecall_speaker_2mic_extvol"
#define STR_VOICECALL_SPK_2MIC_NB			"voicecall_speaker_2mic_upsampling"
#define STR_VOICECALL_SPK_2MIC_NB_EXTVOL	"voicecall_speaker_2mic_upsampling_extvol"
#define STR_VOICECALL_RCV					"voicecall_receiver"
#define STR_VOICECALL_RCV_EXTVOL			"voicecall_receiver_extvol"
#define STR_VOICECALL_RCV_NB				"voicecall_receiver_upsampling"
#define STR_VOICECALL_RCV_NB_EXTVOL			"voicecall_receiver_upsampling_extvol"
#define STR_VOICECALL_RCV_2MIC				"voicecall_receiver_2mic"
#define STR_VOICECALL_RCV_2MIC_EXTVOL		"voicecall_receiver_2mic_extvol"
#define STR_VOICECALL_RCV_2MIC_NB			"voicecall_receiver_2mic_upsampling"
#define STR_VOICECALL_RCV_2MIC_NB_EXTVOL	"voicecall_receiver_2mic_upsampling_extvol"
#define STR_VOICECALL_HEADPHONE				"voicecall_headphone"
#define STR_VOICECALL_HEADPHONE_NB			"voicecall_headphone_upsampling"
#define STR_VOICECALL_HEADSET				"voicecall_headset"
#define STR_VOICECALL_HEADSET_NB			"voicecall_headset_upsampling"
#define STR_VOICECALL_BLUETOOTH				"voicecall_bt_wb"
#define STR_VOICECALL_BLUETOOTH_NREC		"voicecall_bt_wb_nrec"
/* VIDEOCALL */
#define STR_VIDEOCALL_SPK_2MIC				"videocall_speaker_2mic"
#define STR_VIDEOCALL_SPK_2MIC_NB			"videocall_speaker_2mic_upsampling"
#define STR_VIDEOCALL_RCV_2MIC				"videocall_receiver_2mic"
#define STR_VIDEOCALL_RCV_2MIC_NB			"videocall_receiver_2mic_upsampling"
#define STR_VIDEOCALL_HEADPHONE				"videocall_headphone"
#define STR_VIDEOCALL_HEADPHONE_NB			"videocall_headphone_upsampling"
#define STR_VIDEOCALL_HEADSET				"videocall_headset"
#define STR_VIDEOCALL_HEADSET_NB			"videocall_headset_upsampling"
#define STR_VIDEOCALL_BLUETOOTH				"videocall_bt_wb"
#define STR_VIDEOCALL_BLUETOOTH_NREC		"videocall_bt_wb_nrec"
/* INCALL ADDON */
#define STR_INCALL_ADDON					"ap_playback_incall_addon"
#define STR_INCALL_1MIC_ADDON				"ap_playback_incall_1mic_addon"
#define STR_INCALL_BT_ADDON					"ap_playback_incall_bt_addon"
/* VOIP */
#define STR_CHATON_SPK						"voip_chaton_speaker"
#define STR_CHATON_RCV						"voip_chaton_receiver"
#define STR_CHATON_HEADPHONE				"voip_chaton_headphone"
#define STR_CHATON_HEADSET					"voip_chaton_headset"
#define STR_CHATON_BLUETOOTH				"voip_chaton_bt"

#ifndef USE_SCN_TO_GRP_FILE
static struct SCENARIO_GROUP gaScnToGrp[] = {
	/* AP PLAYBACK */
	{STR_AP_SPK, "AP_SP"},
	{STR_AP_HEADSET, "AP_HS"},
	{STR_AP_RECV, "AP_RS"},
	{STR_AP_BT, "AP_BT"},
	{STR_AP_HDMI, "AP_SP"},
	{STR_AP_DOCK, "AP_LO1"},
	{STR_AP_SP_HS, "AP_SP_HS"},
	/* AP CAPTURE */
	{STR_MIC1_AP, "GENERIC_AP"},
	{STR_MIC2_AP, "GENERIC_AP"},
	{STR_STEREO_MIC_AP, "GENERIC_AP"},
	{STR_EARMIC_AP, "GENERIC_AP"},
	{STR_CAMCORDER_STEREO, "GENERIC_AP"},
	{STR_BT_AP, "BT_AP"},
	{STR_CP_AP, "CP_AP"},
	/* CP PLAYBACK */
	{STR_CP_SPK, "CP_SP"},
	{STR_CP_HEADSET, "CP_HS"},
	{STR_CP_HEADPHONE, "CP_HP"},
	{STR_CP_RECV_2MIC, "CP_RC"},
	{STR_CP_RECV, "CP_RC"},
	{STR_CP_BT, "CP_BT"},
	/* CP CAPTURE */
	{STR_MIC1_CP, "MIC_CP"},
	{STR_MIC2_CP, "MIC_CP"},
	{STR_EARMIC_CP, "HS_CP"},
	{STR_BT_CP, "BT_CP"},
	/* FMRADIO */
	{STR_FM_AP, "GENERIC_AP"},
	{STR_FM_SPEAKER, "FM_SP"},
	{STR_FM_HEADSET, "FM_HS"},
	/* VOICECALL */
	{STR_VOICECALL_SPK, "VC_SP"},
	{STR_VOICECALL_SPK_EXTVOL, "VC_SP_EXT_VOL"},
	{STR_VOICECALL_SPK_NB, "VC_SP"},
	{STR_VOICECALL_SPK_NB_EXTVOL, "VC_SP_EXT_VOL"},
	{STR_VOICECALL_SPK_2MIC, "VC_SP_2MIC"},
	{STR_VOICECALL_SPK_2MIC_EXTVOL, "VC_SP_2MIC_EXT_VOL"},
	{STR_VOICECALL_SPK_2MIC_NB, "VC_SP_2MIC"},
	{STR_VOICECALL_SPK_2MIC_NB_EXTVOL, "VC_SP_2MIC_EXT_VOL"},
	{STR_VOICECALL_RCV, "VC_RCV"},
	{STR_VOICECALL_RCV_EXTVOL, "VC_RCV_EXT_VOL"},
	{STR_VOICECALL_RCV_NB, "VC_RCV"},
	{STR_VOICECALL_RCV_NB_EXTVOL, "VC_RCV_EXT_VOL"},
	{STR_VOICECALL_RCV_2MIC, "VC_RCV_2MIC"},
	{STR_VOICECALL_RCV_2MIC_EXTVOL, "VC_RCV_2MIC_EXT_VOL"},
	{STR_VOICECALL_RCV_2MIC_NB, "VC_RCV_2MIC"},
	{STR_VOICECALL_RCV_2MIC_NB_EXTVOL, "VC_RCV_2MIC_EXT_VOL"},
	{STR_VOICECALL_HEADPHONE, "VC_HP"},
	{STR_VOICECALL_HEADPHONE_NB, "VC_HP"},
	{STR_VOICECALL_HEADSET, "VC_HS"},
	{STR_VOICECALL_HEADSET_NB, "VC_HS"},
	{STR_VOICECALL_BLUETOOTH, "VC_BT_WB"},
	{STR_VOICECALL_BLUETOOTH_NREC, "VC_BT_WB"},
	/* VIDEOCALL */
	{STR_VIDEOCALL_SPK_2MIC, "VT_SP"},
	{STR_VIDEOCALL_SPK_2MIC_NB, "VT_SP"},
	{STR_VIDEOCALL_RCV_2MIC, "VT_RCV"},
	{STR_VIDEOCALL_RCV_2MIC_NB, "VT_RCV"},
	{STR_VIDEOCALL_HEADPHONE, "VT_HP"},
	{STR_VIDEOCALL_HEADPHONE_NB, "VT_HP"},
	{STR_VIDEOCALL_HEADSET, "VT_HS"},
	{STR_VIDEOCALL_HEADSET_NB, "VT_HS"},
	{STR_VIDEOCALL_BLUETOOTH, "VT_BT_WB"},
	{STR_VIDEOCALL_BLUETOOTH_NREC, "VT_BT_WB"},
	/* INCALL ADDON */
	{STR_INCALL_ADDON, "AP_INCALL"},
	{STR_INCALL_1MIC_ADDON, "AP_INCALL_1MIC"},
	{STR_INCALL_BT_ADDON, "AP_INCALL_BT"},
	/* VOIP */
	{STR_CHATON_SPK, "CHATONV_SP"},
	{STR_CHATON_RCV, "CHATONV_RCV"},
	{STR_CHATON_HEADPHONE, "CHATONV_HP"},
	{STR_CHATON_HEADSET, "CHATONV_HS"},
	{STR_CHATON_BLUETOOTH, "CHATONV_BT"},
};
#endif

static struct FILE_DATA gsFileData = {NULL, 0};
static int gnGroupInfoNum = 0;
static int gnAllocGroupInfoNum = 0;
static char gabRootDir[MAX_PATH_NAME];
static struct GROUP_INFO *gpGroupInfo;

#ifdef USE_SCN_TO_GRP_FILE
#define MAX_BUFFER_SIZE					256
#define SCENARIO_GROUP_MATCHING_TABLE	"/usr/etc/sound/ymu831/matching_table"
static int initialized = 0;
static struct SCENARIO_GROUP *gpScnToGrp = NULL;

static int InitScenarioGroup(void)
{
	FILE *fp = NULL;
	struct SCENARIO_GROUP *pScnToGrp = NULL;
	char line_buffer[MAX_BUFFER_SIZE] = {0,};

	fp = fopen(SCENARIO_GROUP_MATCHING_TABLE, FILE_OPEN_MODE);

	if (!fp)
		return errno;

	while (fgets(line_buffer, sizeof(line_buffer), fp) != NULL) {
		char *p_start = NULL, *p_end = NULL;

		if (gpScnToGrp == NULL) {
			gpScnToGrp = malloc(sizeof(struct SCENARIO_GROUP));
			pScnToGrp = gpScnToGrp;
		} else {
			pScnToGrp->pNext = malloc(sizeof(struct SCENARIO_GROUP));
			pScnToGrp = pScnToGrp->pNext;
		}
		memset(pScnToGrp, 0x00, sizeof(struct SCENARIO_GROUP));

		/* Skip comment */
		if ((line_buffer[0] == '#') || 
			((line_buffer[0] == '/') && (line_buffer[1] == '/'))) {
			continue;
		}

		/* Parse Scenario */
		p_start = p_end = line_buffer;
		while (!isspace(*p_end))
			p_end++;
		pScnToGrp->pbScenario = malloc(p_end - p_start + 1);
		strncpy(pScnToGrp->pbScenario, p_start, p_end - p_start);
		pScnToGrp->pbScenario[p_end - p_start] = '\0';

		/* Parse Group */
		p_start = p_end;
		while (isspace(*p_start))
			p_start++;
		p_end = p_start;
		while (!isspace(*p_end))
			p_end++;
		pScnToGrp->pbGroup = malloc(p_end - p_start + 1);
		strncpy(pScnToGrp->pbGroup, p_start, p_end - p_start);
		pScnToGrp->pbGroup[p_end - p_start] = '\0';
	}
	fclose(fp);

	return 0;
}
#endif

static int GetValue(char *pbStart, int nSize, char *pbBuff, int nBuffSize)
{
	int nPos;
	int nValueSize;
	int nValidValue;

	memset(pbBuff, 0x00, nBuffSize);

	nBuffSize--;
	if (nBuffSize <= 0)
		return ERROR;

	nPos = 0;
	while (nPos < nSize) {
		if (!isblank(pbStart[nPos]))
			break;
		nPos++;
	}

	if ((nPos == nSize) || (pbStart[nPos] != '"'))
		return ERROR;
	nPos++;

	nValueSize = 0;
	nValidValue = 0;
	while (nPos < nSize) {
		if ((pbStart[nPos] == '\r') || (pbStart[nPos] == '\n'))
			return ERROR;

		if (pbStart[nPos] == '"') {
			nPos++;
			nValidValue = 1;
			break;
		}

		if (nValueSize < nBuffSize) {
			pbBuff[nValueSize] = pbStart[nPos];
			nValueSize++;
		}

		nPos++;
	}

	if (nValidValue == 0)
		return ERROR;

	return nPos;
}

static int ReadFileData(FILE *pFile, struct FILE_DATA *pFileData)
{
	int nSize;
	int nReadSize;
	char *pbData;
	long pos = 0;

	pFileData->pbData = NULL;
	pFileData->nSize = 0;

	fseek(pFile, 0, SEEK_END);
	pos = ftell(pFile);
	fseek(pFile, 0, SEEK_SET);
	pos -= ftell(pFile);

	if (pos <= 0)
		return ERROR;

	nSize = (int)pos;
	pbData = malloc(nSize);
	if (pbData == NULL)
		return ERROR;

	nReadSize = (int)fread(pbData, 1, nSize, pFile);
	if (nReadSize != nSize) {
		free(pbData);
		return ERROR;
	}

	pFileData->pbData = pbData;
	pFileData->nSize = nSize;

	return nSize;
}

#ifdef USE_FOPEN
static int ReadFile(char *pbPathName, struct FILE_DATA *pFileData)
{
	int nResult = 0;
	FILE *pFile;

	pFile = fopen(pbPathName, FILE_OPEN_MODE);
	if (pFile == NULL)
		return errno;

	nResult = ReadFileData(pFile, pFileData);

	fclose(pFile);

	return nResult;
}
#else
static int ReadFile(char *pbPathName, struct FILE_DATA *pFileData)
{
	int nResult;
	int nFd;
	FILE *pFile;

	nFd = open(pbPathName, O_RDONLY);
	if (nFd < 0)
		return nFd;

	pFile = fdopen(nFd, FILE_OPEN_MODE);
	if (pFile == NULL) {
		close(nFd);
		return errno;
	}

	nResult = ReadFileData(pFile, pFileData);

	fclose(pFile);
	close(nFd);

	return nResult;
}
#endif

static void ReleaseFileData(struct FILE_DATA *pFileData)
{
	if (pFileData->pbData != NULL)
		free(pFileData->pbData);

	pFileData->pbData = NULL;
	pFileData->nSize = 0;
}

static int AddGroupInfo(char *pbName, int nStartPos, int nEndPos)
{
	int i;
	struct GROUP_INFO *pTemp;

	pa_log_debug("%s", pbName);

	if (gnAllocGroupInfoNum <= (gnGroupInfoNum + 1)) {
		gnAllocGroupInfoNum = gnGroupInfoNum * 2;
		pTemp = malloc(
			sizeof(struct GROUP_INFO) * gnAllocGroupInfoNum);
		if (pTemp == NULL)
			return ERROR;

		for (i = 0; i < gnGroupInfoNum; ++i) {
			strncpy(pTemp[i].abName,
					gpGroupInfo[i].abName, MAX_NAME);
			pTemp[i].nStartPos = gpGroupInfo[i].nStartPos;
			pTemp[i].nEndPos = gpGroupInfo[i].nEndPos;
			pa_log_debug("abName[%s]:nStartPos[%d]:nEndPos[%d]",
							gpGroupInfo[i].abName,
							gpGroupInfo[i].nStartPos,
							gpGroupInfo[i].nEndPos);
		}

		free(gpGroupInfo);
		gpGroupInfo = pTemp;
	}

	strncpy(gpGroupInfo[gnGroupInfoNum].abName, pbName, MAX_NAME-1);
	gpGroupInfo[gnGroupInfoNum].abName[MAX_NAME-1] = '\0';
	gpGroupInfo[gnGroupInfoNum].nStartPos = nStartPos;
	gpGroupInfo[gnGroupInfoNum].nEndPos = nEndPos;

	gnGroupInfoNum++;
	return gnGroupInfoNum;
}

static int ConfigAnalyze(struct FILE_DATA *pFileData,
					char *pbRootDir, int nRootDirSize)
{
	int nPos;
	int nRemain;
	int nResult;
	int nYmcStart;
	int nGroupStart;
	char abName[MAX_NAME];
	char *pbStart;
	int nSize;

	pbStart = pFileData->pbData;
	nSize = pFileData->nSize;

	nPos = 0;
	nYmcStart = 0;
	nGroupStart = 0;
	while (nPos < nSize) {
		nRemain = (nSize - nPos);
		if (isblank(pbStart[nPos])) {
			nPos++;
		} else if ((5 <= nRemain) &&
			(strncmp(&pbStart[nPos], "<ymc>", 5) == 0)) {
			if (nYmcStart != 0)
				return ERROR;
			nPos += 5;
			nYmcStart = nPos;
		} else if ((6 <= nRemain) &&
			(strncmp(&pbStart[nPos], "</ymc>", 6) == 0)) {
			if (nYmcStart == 0)
				return ERROR;
			nPos += 6;
			nYmcStart = 0;
		} else if (nYmcStart == 0) {
			nPos++;
		} else if ((19 <= nRemain) &&
				(strncmp(&pbStart[nPos],
					"<parameter rootdir=", 19) == 0)) {
			if (nGroupStart != 0)
				return ERROR;

			nPos += 19;
			nRemain = (nSize - nPos);
			nResult = GetValue(&pbStart[nPos], nRemain,
						pbRootDir, nRootDirSize);
			if (nResult < 0)
				return nResult;
			else
				pa_log_debug("pbRootDir:%s", pbRootDir);

			nPos += nResult;
			while (nPos < nSize) {
				nRemain = (nSize - nPos);
				if (isblank(pbStart[nPos])) {
					nPos++;
				} else if ((2 <= nRemain) &&
					(strncmp(&pbStart[nPos], "/>", 2) == 0)) {
					nPos += 2;
					break;
				} else {
					return ERROR;
				}
			}
		} else if ((12 <= nRemain) &&
			(strncmp(&pbStart[nPos], "<group name=", 12) == 0)) {
			if (nGroupStart != 0)
				return ERROR;

			nPos += 12;
			nRemain = (nSize - nPos);
			nResult = GetValue(&pbStart[nPos], nRemain,
						abName, sizeof(abName));
			if (nResult < 0)
				return nResult;
			else
				pa_log_debug("abName:%s", abName);

			nPos += nResult;
			while (nPos < nSize) {
				if (isblank(pbStart[nPos])) {
					nPos++;
				} else if (pbStart[nPos] == '>') {
					nPos++;
					break;
				} else {
					return ERROR;
				}
			}
			nGroupStart = nPos;
		} else if ((8 <= nRemain) &&
			(strncmp(&pbStart[nPos], "</group>", 8) == 0)) {
			if (nGroupStart == 0)
				return ERROR;
			if (nPos <= 0)
				return ERROR;

			nResult = AddGroupInfo(abName,
						nGroupStart, (nPos - 1));
			if (nResult < 0)
				return nResult;

			nPos += 8;
			nGroupStart = 0;
		} else {
			nPos++;
		}
	}

	if ((nGroupStart != 0) || (nYmcStart != 0))
		return ERROR;

	return 0;
}

static int HwdevOpen()
{
	int handle;
	char hwdev_path[MAX_NAME];

	snprintf(hwdev_path, MAX_NAME, "/dev/snd/hwC%uD%u",
		DEVFILE_HW_CARD_NUMBER, DEVFILE_HW_DEVICE_NUMBER);
	handle = open(hwdev_path, O_RDWR);
	return handle;
}

static void HwdevClose(int handle)
{
        close(handle);
}

static unsigned long GetOption(char *pbName)
{
	unsigned long dResult;
	dResult = YMC_DSP_OPTION_ERROR;

	if (strcasecmp(pbName, DSP_OUTPUT_NAME) == 0)
		dResult = YMC_DSP_OUTPUT_BASE;
	else if (strcasecmp(pbName, DSP_INPUT_NAME) == 0)
		dResult = YMC_DSP_INPUT_BASE;
	else if (strcasecmp(pbName, DSP_VOICECALL_NAME) == 0)
		dResult = YMC_DSP_VOICECALL_BASE_COMMON;
	else if (strcasecmp(pbName, DSP_VOICECALL_1MIC_NAME) == 0)
		dResult = YMC_DSP_VOICECALL_BASE_1MIC;
	else if (strcasecmp(pbName, DSP_VOICECALL_2MIC_NAME) == 0)
		dResult = YMC_DSP_VOICECALL_BASE_2MIC;

	return dResult;
}

static int SetParameter(char *pbName, char *pbPathName)
{
	int nResult;
	int handle;
	struct FILE_DATA Parameter;
	ymc_ctrl_args_t args;

	pa_log_debug("Name[%s] PathName[%s]", pbName, pbPathName);

	args.option = GetOption(pbName);
	if (args.option == YMC_DSP_OPTION_ERROR)
		return ERROR;

	nResult = ReadFile(pbPathName, &Parameter);
	if (nResult < 0)
		return nResult;
	else
		pa_log_debug("Read %s %d bytes done",
					pbPathName, Parameter.nSize);

	args.param = Parameter.pbData;
	args.size = Parameter.nSize;

	handle = HwdevOpen();
	if (handle >= 0) {
		nResult = ioctl(handle, YMC_IOCTL_SET_CTRL, &args);
		if (nResult < 0)
			pa_log_error("ioctl YMC_IOCTL_SET_CTRL has failed with %s", strerror(errno));
		HwdevClose(handle);
	} else {
		pa_log_error("HwdevOpen failed, ret handle is %d, err is %s", handle, strerror(errno));
	}

	ReleaseFileData(&Parameter);

	return nResult;
}

static snd_mixer_t *MixOpen(char* ifaceName)
{
	int nResult;
	snd_mixer_t *handle;

	nResult = snd_mixer_open(&handle, 0);
	if (nResult < 0)
		return NULL;

	nResult = snd_mixer_attach(handle, ifaceName);
	if (nResult < 0) {
		snd_mixer_close(handle);
		return NULL;
	}

	nResult = snd_mixer_selem_register(handle, NULL, NULL);
	if (nResult < 0) {
		snd_mixer_close(handle);
		return NULL;
	}

	nResult = snd_mixer_load(handle);
	if (nResult < 0) {
		snd_mixer_close(handle);
		return NULL;
	}

	return handle;
}

static int get_mixer_elem(snd_mixer_t *handle,
					char *name, snd_mixer_elem_t **elem)
{
	int nResult;
	snd_mixer_selem_id_t* sid;
	sid = NULL;

	nResult = snd_mixer_selem_id_malloc(&sid);
	if (nResult < 0)
		return ERROR;

	snd_mixer_selem_id_set_index(sid, 0);
	snd_mixer_selem_id_set_name(sid, name);
	*elem = snd_mixer_find_selem(handle, sid);
	snd_mixer_selem_id_free(sid);
	if (*elem == NULL)
		return ERROR;

	return 0;
}

static int SetMixEnum(snd_mixer_t *handle, char *name, int index)
{
	int nResult;
	int items;
	snd_mixer_elem_t *elem;

	nResult = get_mixer_elem(handle, name, &elem);
	if (nResult < 0)
		return nResult;

	if (!snd_mixer_selem_is_enumerated(elem)) {
		return ERROR;
	}

	items = snd_mixer_selem_get_enum_items(elem);
	if (items <= 0)
		return ERROR;

	if (items <= index)
		return ERROR;

	nResult = snd_mixer_selem_set_enum_item(elem, 0, index);

	return nResult;
}

static void MixClose(snd_mixer_t *handle)
{
    snd_mixer_close(handle);
}

static int SetMixer(char *pbName, char *pbValue0, char *pbValue1)
{
	int nResult;
	snd_mixer_t *handle;

	(void)pbValue1;

	handle = MixOpen(MIXER_NAME_OUT);
	if (handle == NULL)
		return ERROR;

	nResult = SetMixEnum(handle, pbName, atoi(pbValue0));

	MixClose(handle);

	return nResult;
}

static int SetParamMix(char *pbStart, int nSize, char *pbRootDir, int nTarget)
{
	int nPos;
	int nResult;
	int nRootDirSize;
	int nValidValue;
	int nRemain;
	char abName[MAX_NAME];
	char abPathName[MAX_PATH_NAME];
	char abValue0[MAX_VALUE_LEN];
	char abValue1[MAX_VALUE_LEN];

	memset(abName, 0x00, sizeof(abName));
	memset(abPathName, 0x00, sizeof(abPathName));
	memset(abValue0, 0x00, sizeof(abValue0));
	memset(abValue1, 0x00, sizeof(abValue1));

	snprintf(abPathName, MAX_PATH_NAME, "%s/", pbRootDir);
	nRootDirSize = (int)strlen(abPathName);

	nPos = 0;
	nValidValue = 0;
	while (nPos < nSize) {
		if ((pbStart[nPos] == '\r') || (pbStart[nPos] == '\n'))
			return ERROR;

		nRemain = (nSize - nPos);
		if (isblank(pbStart[nPos])) {
			nPos++;
		} else if ((2 <= nRemain) &&
			(strncmp(&pbStart[nPos], "/>", 2) == 0)) {
			nPos += 2;
			nValidValue = 1;
			break;

		} else if ((5 <= nRemain) &&
			(strncmp(&pbStart[nPos], "name=", 5) == 0)) {
			if (strlen(abName) != 0)
				return ERROR;

			nPos += 5;
			nRemain = (nSize - nPos);
			nResult = GetValue(&pbStart[nPos], nRemain,
						abName, sizeof(abName));
			if (nResult < 0)
				return nResult;

			nPos += nResult;
		} else if ((5 <= nRemain) &&
			(strncmp(&pbStart[nPos], "file=", 5) == 0)) {
			if (nRootDirSize < (int)strlen(abPathName))
				return ERROR;

			nPos += 5;
			nRemain = (nSize - nPos);
			nResult = GetValue(&pbStart[nPos], nRemain,
					&abPathName[nRootDirSize],
					(sizeof(abPathName) - nRootDirSize));
			if (nResult < 0)
				return nResult;

			nPos += nResult;
		} else if ((7 <= nRemain) &&
			(strncmp(&pbStart[nPos], "value0=", 7) == 0)) {

			if (strlen(abValue0) != 0)
				return ERROR;

			nPos += 7;
			nRemain = (nSize - nPos);
			nResult = GetValue(&pbStart[nPos], nRemain,
						abValue0, sizeof(abValue0));
			if (nResult < 0)
				return nResult;

			nPos += nResult;
		} else if ((7 <= nRemain) &&
			(strncmp(&pbStart[nPos], "value1=", 7) == 0)) {

			if (strlen(abValue1) != 0)
				return ERROR;

			nPos += 7;
			nRemain = (nSize - nPos);
			nResult = GetValue(&pbStart[nPos], nRemain,
						abValue1, sizeof(abValue1));
			if (nResult < 0)
				return nResult;

			nPos += nResult;
		} else {
			return ERROR;
		}
	}

	if (nValidValue == 0)
		return ERROR;

	switch(nTarget) {
	case TARGET_PARAMETER:
		nResult = SetParameter(abName, abPathName);
		break;

	case TARGET_MIXER:
		nResult = SetMixer(abName, abValue0, abValue1);
		break;

	default:
		nResult = ERROR;
		break;
	}

	return (nResult < 0) ? nResult : nPos;
}

static int SetGroup(char *pbStart, int nSize, char *pbRootDir)
{
	int nPos;
	int nRemain;
	int nResult;

	nPos = 0;
	while (nPos < nSize) {
		nRemain = (nSize - nPos);
		if (isblank(pbStart[nPos]) ||
			pbStart[nPos] == '\r' || pbStart[nPos] == '\n') {
			nPos++;
		} else if ((11 <= nRemain) &&
			(strncmp(&pbStart[nPos], "<parameter ", 11) == 0)) {
			nPos += 11;
			nRemain = (nSize - nPos);

			nResult = SetParamMix(&pbStart[nPos], nRemain,
						pbRootDir, TARGET_PARAMETER);
			if (nResult < 0)
				return nResult;

			nPos += nResult;
		} else if ((7 <= nRemain) &&
				(strncmp(&pbStart[nPos], "<mixer ", 7) == 0)) {
			nPos += 7;
			nRemain = (nSize - nPos);

			nResult = SetParamMix(&pbStart[nPos], nRemain,
						pbRootDir, TARGET_MIXER);
			if (nResult < 0)
				return nResult;

			nPos += nResult;
		} else {
			return ERROR;
		}
	}

	return 0;
}

#ifdef USE_SCN_TO_GRP_FILE
static char *FindGroup(char *ascn_str)
{
	struct SCENARIO_GROUP *pScnToGrp;

	for (pScnToGrp = gpScnToGrp; pScnToGrp; pScnToGrp = pScnToGrp->pNext) {
		if (strncmp(ascn_str, pScnToGrp->pbScenario, MAX_NAME) == 0)
			return pScnToGrp->pbGroup;
	}
	return NULL;
}
#else
static int GetGroup(char *ascn_str)
{
	int i = 0;
	int nNum = sizeof(gaScnToGrp) / sizeof(struct SCENARIO_GROUP);

	for (i = 0; i < nNum; ++i)
		if (strncmp(ascn_str, gaScnToGrp[i].pbScenario, MAX_NAME) == 0)
			break;

	return (i == nNum) ? NOT_FINED : i;
}
#endif

static int Init()
{
	int nResult;

	gnGroupInfoNum = 0;
	gnAllocGroupInfoNum = 64;
	gpGroupInfo = malloc(
			sizeof(struct GROUP_INFO) * gnAllocGroupInfoNum);
	if (gpGroupInfo == NULL)
		return ERROR;

	memset(gpGroupInfo, 0x00,
			sizeof(struct GROUP_INFO) * gnAllocGroupInfoNum);

	nResult = ReadFile(YSOUND_CONF, &gsFileData);
	if (nResult < 0) {
		free(gpGroupInfo);
		gpGroupInfo = NULL;
		return nResult;
	}

	nResult = ConfigAnalyze(
				&gsFileData, gabRootDir, sizeof(gabRootDir));
	if (nResult < 0) {
		free(gpGroupInfo);
		gpGroupInfo = NULL;
		return nResult;
	}

	return nResult;
}

static void Term()
{
	if (gsFileData.pbData != NULL)
		free(gsFileData.pbData);

	gsFileData.pbData = NULL;
	gsFileData.nSize = 0;

	free(gpGroupInfo);
	gpGroupInfo = NULL;
}

int ymu831_set_scenario(char *ascn_str)
{
	char *pbData;
	int nGroup;
	int nSize;
	int i;
	int nResult;
#ifdef USE_SCN_TO_GRP_FILE
	char *pbGroup;

	if (!initialized) {
		pa_log_warn("Initialize ymu831 scenario files\n");
		nResult = Init();
		if (nResult < 0) {
			pa_log_error("Init() failed with %d\n", nResult);
			return nResult;
		}
		InitScenarioGroup();
		initialized = 1;
	}

	pbGroup = FindGroup(ascn_str);
	if (!pbGroup)
		return 0;

	pa_log_warn("scn:%s grp:%s\n", ascn_str, pbGroup);

	for (i = 0; i < gnGroupInfoNum; ++i)
		if (strncmp(gpGroupInfo[i].abName, pbGroup, MAX_NAME) == 0)
			break;

	if (gnGroupInfoNum <= i)
		return ERROR;

#else
	/* scenario->group */
	nGroup = GetGroup(ascn_str);
	if (nGroup == NOT_FINED)
		return 0;

	pa_log_warn("%s\n", gaScnToGrp[nGroup].pbScenario);

	nResult = Init();
	if (nResult < 0) {
		pa_log_error ("Init() failed with %d", nResult);
		return nResult;
	}

	for (i = 0; i < gnGroupInfoNum; ++i)
		if (strncmp(gpGroupInfo[i].abName,
				gaScnToGrp[nGroup].pbGroup, MAX_NAME) == 0)
			break;

	if (gnGroupInfoNum <= i) {
		Term();
		return ERROR;
	}
#endif

	pbData = &gsFileData.pbData[gpGroupInfo[i].nStartPos];
	nSize = gpGroupInfo[i].nEndPos - gpGroupInfo[i].nStartPos + 1;
	nResult = SetGroup(pbData, nSize, gabRootDir);

#ifndef USE_SCN_TO_GRP_FILE
	Term();
#endif

	return nResult;
}
