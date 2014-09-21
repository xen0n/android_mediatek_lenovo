/*
**
** Copyright 2008, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#include "camera_custom_types.h"
#include "string.h"
#ifdef WIN32
#else
#include "camera_custom_nvram.h"
#endif
#include "flash_feature.h"
#include "flash_param.h"
#include "flash_tuning_custom.h"

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
/*
int getDefaultStrobeNVRam(int sensorType, void* data, int* ret_size)
{
	//static NVRAM_CAMERA_STROBE_STRUCT strobeNVRam;
	NVRAM_CAMERA_STROBE_STRUCT* p;
	p = (NVRAM_CAMERA_STROBE_STRUCT*)data;

	p->u4Version = NVRAM_CAMERA_STROBE_FILE_VERSION;
	p->engTab.exp = 20000;
	p->engTab.afe_gain = 1024;
	p->engTab.isp_gain = 1024;
	p->engTab.distance = 300;
	static short ytab[]={
	100,	200,	600,	4,	5,	6,	7,	8,	9,	10,	11,	12,	13,	14,	15,	16,	17,	18,	19,	20,	21,	22,	23,	24,	25,	26,	27,	28,	29,	30,	31,	1,	
	1,	2,	3,	4,	5,	6,	7,	8,	9,	10,	11,	12,	13,	14,	15,	16,	17,	18,	19,	20,	21,	22,	23,	24,	25,	26,	27,	28,	29,	30,	31,	2,	
	1,	2,	3,	4,	5,	6,	7,	8,	9,	10,	11,	12,	13,	14,	15,	16,	17,	18,	19,	20,	21,	22,	23,	24,	25,	26,	27,	28,	29,	30,	31,	3,	
	1,	2,	3,	4,	5,	6,	7,	8,	9,	10,	11,	12,	13,	14,	15,	16,	17,	18,	19,	20,	21,	22,	23,	24,	25,	26,	27,	28,	29,	30,	31,	4,	
	1,	2,	3,	4,	5,	6,	7,	8,	9,	10,	11,	12,	13,	14,	15,	16,	17,	18,	19,	20,	21,	22,	23,	24,	25,	26,	27,	28,	29,	30,	31,	5,	
	1,	2,	3,	4,	5,	6,	7,	8,	9,	10,	11,	12,	13,	14,	15,	16,	17,	18,	19,	20,	21,	22,	23,	24,	25,	26,	27,	28,	29,	30,	31,	6,	
	1,	2,	3,	4,	5,	6,	7,	8,	9,	10,	11,	12,	13,	14,	15,	16,	17,	18,	19,	20,	21,	22,	23,	24,	25,	26,	27,	28,	29,	30,	31,	7,	
	1,	2,	3,	4,	5,	6,	7,	8,	9,	10,	11,	12,	13,	14,	15,	16,	17,	18,	19,	20,	21,	22,	23,	24,	25,	26,	27,	28,	29,	30,	31,	8,	
	};
	memcpy(p->engTab.yTab, ytab, 512);

	p->tuningPara[0].yTar = 188;
	p->tuningPara[0].antiIsoLevel = -5;
	p->tuningPara[0].antiExpLevel = -5;
	p->tuningPara[0].antiStrobeLevel = -10;
	p->tuningPara[0].antiUnderLevel = -2;
	p->tuningPara[0].antiOverLevel = 2;
	p->tuningPara[0].foregroundLevel = 1;
	p->tuningPara[0].isRefAfDistance = 0;
	p->tuningPara[0].accuracyLevel = -10;

	p->tuningPara[1].yTar = 188;
	p->tuningPara[1].antiIsoLevel = -5;
	p->tuningPara[1].antiExpLevel = -5;
	p->tuningPara[1].antiStrobeLevel = -10;
	p->tuningPara[1].antiUnderLevel = -2;
	p->tuningPara[1].antiOverLevel = 2;
	p->tuningPara[1].foregroundLevel = 1;
	p->tuningPara[1].isRefAfDistance = 0;
	p->tuningPara[1].accuracyLevel = -10;

	p->tuningPara[2].yTar = 188;
	p->tuningPara[2].antiIsoLevel = -5;
	p->tuningPara[2].antiExpLevel = -5;
	p->tuningPara[2].antiStrobeLevel = -10;
	p->tuningPara[2].antiUnderLevel = -2;
	p->tuningPara[2].antiOverLevel = 2;
	p->tuningPara[2].foregroundLevel = 1;
	p->tuningPara[2].isRefAfDistance = 0;
	p->tuningPara[2].accuracyLevel = -10;

	p->tuningPara[3].yTar = 188;
	p->tuningPara[3].antiIsoLevel = -5;
	p->tuningPara[3].antiExpLevel = -5;
	p->tuningPara[3].antiStrobeLevel = -10;
	p->tuningPara[3].antiUnderLevel = -2;
	p->tuningPara[3].antiOverLevel = 2;
	p->tuningPara[3].foregroundLevel = 1;
	p->tuningPara[3].isRefAfDistance = 0;
	p->tuningPara[3].accuracyLevel = -10;

	p->tuningPara[4].yTar = 0;
	p->tuningPara[4].antiIsoLevel = 0;
	p->tuningPara[4].antiExpLevel = 0;
	p->tuningPara[4].antiStrobeLevel = 0;
	p->tuningPara[4].antiUnderLevel = 0;
	p->tuningPara[4].antiOverLevel = 0;
	p->tuningPara[4].foregroundLevel = 0;
	p->tuningPara[4].isRefAfDistance = 0;
	p->tuningPara[4].accuracyLevel = 0;

	p->tuningPara[5].yTar = 0;
	p->tuningPara[5].antiIsoLevel = 0;
	p->tuningPara[5].antiExpLevel = 0;
	p->tuningPara[5].antiStrobeLevel = 0;
	p->tuningPara[5].antiUnderLevel = 0;
	p->tuningPara[5].antiOverLevel = 0;
	p->tuningPara[5].foregroundLevel = 0;
	p->tuningPara[5].isRefAfDistance = 0;
	p->tuningPara[5].accuracyLevel = 0;

	p->isTorchEngUpdate = 0;
	p->isAfEngUpdate = 0;
	p->isNormaEnglUpdate = 0;
	p->isLowBatEngUpdate = 0;
	p->isBurstEngUpdate = 0;


	*ret_size = sizeof(NVRAM_CAMERA_STROBE_STRUCT);
	return 0;
}
*/
