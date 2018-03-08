/*
* File		: im_conver.h
* Author : Auron
* Time : 2018 - 2 - 24
*/

#ifndef _CUDADECODE_H_
#define _CUDADECODE_H_

// CUDA Header includes
#include <cuda.h>
#include "cuda_runtime.h"
#include <helper_cuda_drvapi.h>
// Includes
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <memory>
#include <iostream>
#include <cassert>

// cudaDecodeGL related helper functions
#include "FrameQueue.h"
#include "VideoSource.h"
#include "VideoParser.h"
#include "VideoDecoder.h"

class cudaDecode
{

public:
	void init(char *filename, int gpuID);
	int get_frame_w();
	int get_frame_h();
	int get_frame_s();
	void* get_frame_data();
	bool check_decode_end();
	void uninit();

private:
	bool loadVideoSource(const char *video_file,
		unsigned int &width, unsigned int &height);
	void initCudaVideo();
	void freeCudaResources(bool bDestroyContext);
	bool cleanup(bool bDestroyContext);
	bool initCudaResources(int gpuID);
	void parseCommandLineArguments(char* filename, int gpuID);

	int                 m_DeviceID = 0;

	cudaVideoCreateFlags m_eVideoCreateFlags = cudaVideoCreate_PreferCUVID;
	CUvideoctxlock       m_CtxLock = NULL;
	CUcontext          m_oContext = 0;
	// System Memory surface we want to readback to
	FrameQueue    *m_pFrameQueue = 0;
	VideoSource   *m_pVideoSource = 0;
	VideoParser   *m_pVideoParser = 0;
	VideoDecoder *m_pVideoDecoder = 0;
	std::string m_sFileName;
	unsigned int m_nVideoWidth = 0;
	unsigned int m_nVideoHeight = 0;

	unsigned int m_FrameCount = 0;
};


#endif