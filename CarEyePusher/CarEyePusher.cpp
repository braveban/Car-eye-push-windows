/*
* CarEyePusher.cpp
*
* Author: Wgj
* Date: 2018-03-06 21:37
*
* 推流接口测试演示程序
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "CarEyeRtspAPI.h"

#ifdef _WIN32
#include <windows.h>
char *key = "J38U1uiVIUHV2uH7%7%U%6H7%7%Q$rh7gSI7k71U1uiVIUHV2uH8%VHV1uLuHVITJuHVLuH4B";
#else
#include <pthread.h>
#include <ctype.h>
#include <sys/time.h>
char *key = ")0FE_dWD9EXDYdXfvfWF0A,fvfv>^Bwfxa9fyf_E_dWD9EXDYdX|vDXD_d.dXD956";
#endif

using namespace std;

#define TEST_NATIVE_FILE
//#define TEST_RTSP
//#define TEST_MULTI_CHN

char ServerIP[] = "www.car-eye.cn"; // CarEye服务器
const unsigned short ServerPort = 10554;
char SdpName[] = "careye_pusher.sdp";
char TestFile[] = "./test.mp4";
char TestH264[] = "./test.264";

bool threadIsWork = false;
CarEye_MediaInfo mediaInfo;

#define ENUM_CHIP_TYPE_CASE(x)   case x: return(#x);

char* GetEnumString(CarEyePusherType aEnum)
{
	switch (aEnum)
	{
		ENUM_CHIP_TYPE_CASE(PUSHER_RTSP)
		ENUM_CHIP_TYPE_CASE(PUSHER_NATIVEFILE_RTSP)
		ENUM_CHIP_TYPE_CASE(PUSHER_RTMP)
		ENUM_CHIP_TYPE_CASE(PUSHER_NATIVEFILE_RTMP)
	}

	return "Already released";
}

/*
* Comments: 推流器状态更改事件
* Param :
* @Return int
*/
int CarEyePusher_StateChangedEvent(int channel, CarEyeStateType state, CarEyePusherType type)
{
	switch (state)
	{
	case CAREYE_STATE_CONNECTING:
		printf("Chn[%d] %s Connecting...\n", channel, GetEnumString(type));
		break;

	case CAREYE_STATE_CONNECTED:
		printf("Chn[%d] %s Connected\n", channel, GetEnumString(type));
		break;

	case CAREYE_STATE_CONNECT_FAILED:
		printf("Chn[%d] %s Connect failed\n", channel, GetEnumString(type));
		break;

	case CAREYE_STATE_CONNECT_ABORT:
		printf("Chn[%d] %s Connect abort\n", channel, GetEnumString(type));
		break;

	case CAREYE_STATE_PUSHING:

		break;

	case CAREYE_STATE_DISCONNECTED:
		printf("Chn[%d] %s Disconnect.\n", channel, GetEnumString(type));
		break;

	case CAREYE_STATE_FILE_FINISHED:
		printf("Chn[%d] %s Push native file finished.\n", channel, GetEnumString(type));
		CarEyeRTSP_StopNativeFile(channel);
		break;

	default:
		break;
	}

	return 0;
}

// 推流测试线程
#ifdef _WIN32
DWORD WINAPI PushThreadEntry(LPVOID arg)
#else
void *PushThreadEntry(void *arg)
#endif
{
	int chn = *((int *)arg);

	int buf_size = 1024 * 512;
	char *pbuf = (char *)malloc(buf_size);
	FILE *fES = NULL;
	int position = 0;
	int iFrameNo = 0;
	int timestamp = 0;
	int i = 0;

	fES = fopen(TestH264, "rb");
	if (NULL == fES)
	{
		printf("Test push file has not found.\n");
		return -1;
	}

	while (threadIsWork)
	{
		if (CarEyeRTSP_PusherIsReady(chn))
		{
			break;
#ifndef _WIN32
			usleep(1000);
#else
			Sleep(1);
#endif
		}
	}

	while (threadIsWork)
	{
		int nReadBytes = fread(pbuf + position, 1, 1, fES);
		if (nReadBytes < 1)
		{
			if (feof(fES))
			{
				position = 0;
				fseek(fES, 0, SEEK_SET);
				memset(pbuf, 0x00, buf_size);
				continue;
			}
			printf("Chn[%d] Read file error!", chn);
			break;
		}

		position++;

		if (position > 5)
		{
			unsigned char naltype = ((unsigned char)pbuf[position - 1] & 0x1F);

			if ((unsigned char)pbuf[position - 5] == 0x00 &&
				(unsigned char)pbuf[position - 4] == 0x00 &&
				(unsigned char)pbuf[position - 3] == 0x00 &&
				(unsigned char)pbuf[position - 2] == 0x01 &&
				//(((unsigned char)pbuf[position-1] == 0x61) ||
				//((unsigned char)pbuf[position-1] == 0x67) ) )
				(naltype == 0x07 || naltype == 0x01))
			{
				int framesize = position - 5;
				CarEye_AV_Frame avFrame;

				naltype = (unsigned char)pbuf[4] & 0x1F;

				memset(&avFrame, 0x00, sizeof(CarEye_AV_Frame));
				avFrame.FrameFlag = CAREYE_VFRAME_FLAG;
				avFrame.FrameLen = framesize;
				avFrame.Buffer = (unsigned char*)pbuf;
				avFrame.VFrameType = (naltype == 0x07) ? VIDEO_FRAME_I : VIDEO_FRAME_P;
				avFrame.Second = timestamp / 1000;
				avFrame.USecond = (timestamp % 1000) * 1000;
				CarEyeRTSP_PushData(chn, &avFrame);
				timestamp += 1000 / mediaInfo.VideoFps;
#ifndef _WIN32
				usleep(40 * 1000);
#else
				Sleep(30);
#endif

				memmove(pbuf, pbuf + position - 5, 5);
				position = 5;

				iFrameNo++;

				if (iFrameNo == 100 || iFrameNo == 200)
				{
					for (i = 0; i < 8 && threadIsWork; i++)
					{
#ifndef _WIN32
						usleep(100000);
#else
						Sleep(100);
#endif
					}
				}
			}
		}
	}

	return NULL;
}

#ifdef TEST_NATIVE_FILE

// 本地MP4文件推流测试程序
int main()
{
	if (CarEyeRTSP_Register(key) != CAREYE_NOERROR)
	{
		printf("Register pusher failed.\n");
		return -1;
	}

	CarEyeRTSP_RegisterStateChangedEvent(CarEyePusher_StateChangedEvent);

	int chn = CarEyeRTSP_StartNativeFile(ServerIP, ServerPort, SdpName, TestFile, 0, 0);
	if (chn < 0)
	{
		printf("Start native file rtsp failed %d.\n", chn);
		return -1;
	}

	printf("Wait key stop channel[%d] rtsp...\n", chn);
	getchar();

	CarEyeRTSP_StopNativeFile(chn);
	printf("Wait key exit program...\n");
	getchar();

	return 0;
}

#elif defined TEST_RTSP
// 媒体流推送测试程序
int main()
{
#ifdef _WIN32
	// 解码视频并推送的线程句柄
	HANDLE		thread_id;
#else
	pthread_t	thread_id;
#endif

	if (CarEyeRTSP_Register(key) != CAREYE_NOERROR)
	{
		printf("Register pusher failed.\n");
		return -1;
	}

	CarEyeRTSP_RegisterStateChangedEvent(CarEyePusher_StateChangedEvent);
	mediaInfo.VideoCodec = CAREYE_VCODE_H264;
	mediaInfo.VideoFps = 25;
	mediaInfo.AudioCodec = CAREYE_ACODE_G711U;
	mediaInfo.AudioChannel = 1;
	mediaInfo.AudioSamplerate = 8000;
	int chn = CarEyeRTSP_StartPusher(ServerIP, ServerPort, SdpName, mediaInfo);
	if (chn < 0)
	{
		printf("Start push rtsp failed %d.\n", chn);
		return -1;
	}

	threadIsWork = true;
#ifdef _WIN32
	thread_id = CreateThread(NULL, 0, PushThreadEntry, &chn, 0, NULL);
	if (thread_id == NULL)
	{
		printf("Create push thread failed.\n");
		return -1;
	}
#else
	if (pthread_create(&thread_id, NULL, PushThreadEntry, &chn) != 0)
	{
		printf("Create push thread failed: %d.\n", errno);
		return -1;
	}
	pthread_detach(thread_id);
#endif

	printf("Wait key exit program...\n");
	getchar();
	threadIsWork = false;

	CarEyeRTSP_StopPusher(chn);
	return 0;
}

#elif defined TEST_MULTI_CHN

// 本地MP4文件与H264多通道推流测试程序
int main()
{
	int chns[4]{ -1, -1, -1, 1 };
	int i;

#ifdef _WIN32
	// 解码视频并推送的线程句柄
	HANDLE		thread_id;
#else
	pthread_t	thread_id;
#endif

	if (CarEyeRTSP_Register(key) != CAREYE_NOERROR)
	{
		printf("Register pusher failed.\n");
		return -1;
	}

	CarEyeRTSP_RegisterStateChangedEvent(CarEyePusher_StateChangedEvent);
	int chn = CarEyeRTSP_StartNativeFile(ServerIP, ServerPort, "careye_pusher0.sdp", TestFile, 12000, 32000);
	if (chn < 0)
	{
		printf("Start chn0 native file rtsp failed %d.\n", chn);
	}
	chns[0] = chn;

	chn = CarEyeRTSP_StartNativeFile(ServerIP, ServerPort, "careye_pusher1.sdp", TestFile, 32000, 0);
	if (chn < 0)
	{
		printf("Start chn1 native file rtsp failed %d.\n", chn);
	}
	chns[1] = chn;

#if 1
	mediaInfo.VideoCodec = CAREYE_VCODE_H264;
	mediaInfo.VideoFps = 25;
	mediaInfo.AudioCodec = CAREYE_ACODE_G711U;
	mediaInfo.AudioChannel = 1;
	mediaInfo.AudioSamplerate = 8000;
	chn = CarEyeRTSP_StartPusher(ServerIP, ServerPort, "careye_pusher2.sdp", mediaInfo);
	if (chn < 0)
	{
		printf("Start chn2 push rtsp failed %d.\n", chn);
	}
	chns[2] = chn;

	threadIsWork = true;
	if (chn >= 0)
	{
#ifdef _WIN32
		thread_id = CreateThread(NULL, 0, PushThreadEntry, &chn, 0, NULL);
		if (thread_id == NULL)
		{
			printf("Create chn2 push thread failed.\n");
		}
#else
		if (pthread_create(&thread_id, NULL, PushThreadEntry, &chn) != 0)
		{
			printf("Create chn2 push thread failed: %d.\n", errno);
		}
		else
		{
			pthread_detach(thread_id);
		}
#endif
	}

	chn = CarEyeRTSP_StartPusher(ServerIP, ServerPort, "careye_pusher3.sdp", mediaInfo);
	if (chn < 0)
	{
		printf("Start chn3 push rtsp failed %d.\n", chn);
	}
	chns[3] = chn;

	if (chn >= 0)
	{
#ifdef _WIN32
		thread_id = CreateThread(NULL, 0, PushThreadEntry, &chn, 0, NULL);
		if (thread_id == NULL)
		{
			printf("Create chn3 push thread failed.\n");
		}
#else
		if (pthread_create(&thread_id, NULL, PushThreadEntry, &chn) != 0)
		{
			printf("Create chn3 push thread failed: %d.\n", errno);
		}
		else
		{
			pthread_detach(thread_id);
		}
#endif
	}
#endif
	printf("Wait key stop pushing...\n");
	getchar();
	threadIsWork = false;

	for (i = 0; i < 4; i++)
	{
		if (i < 2 && chns[i] >= 0)
		{
			CarEyeRTSP_StopNativeFile(chns[i]);
		}
		else if (chns[i] >= 0)
		{
			CarEyeRTSP_StopPusher(chn);
		}
	}
	printf("Wait key exit program...\n");
	getchar();

	return 0;
}

#endif

