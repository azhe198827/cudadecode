/*
 * Copyright 1993-2015 NVIDIA Corporation.  All rights reserved.
 *
 * Please refer to the NVIDIA end user license agreement (EULA) associated
 * with this source code for terms and conditions that govern your use of
 * this software. Any use, reproduction, disclosure, or distribution of
 * this software and related documentation outside the terms of the EULA
 * is strictly prohibited.
 *
 */

/* This example demonstrates how to use the Video Decode Library with CUDA
 * bindings to interop between NVCUVID(CUDA) and OpenGL (PBOs).  Post-Processing
 * video (de-interlacing) is supported with this sample.
 */

#include "cudaDecode.h"

#if !defined(WIN32) && !defined(_WIN32) && !defined(WIN64) && !defined(_WIN64)
typedef unsigned char BYTE;
#define S_OK true;
#endif
#ifdef _DEBUG
#define ENABLE_DEBUG_OUT    0
#else
#define ENABLE_DEBUG_OUT    0
#endif





#if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
//typedef bool (APIENTRY *PFNWGLSWAPINTERVALFARPROC)(int);
//PFNWGLSWAPINTERVALFARPROC wglSwapIntervalEXT = 0;



#ifndef STRCASECMP
#define STRCASECMP  _stricmp
#endif
#ifndef STRNCASECMP
#define STRNCASECMP _strnicmp
#endif

#else
void setVSync(int interval)
{
}

#ifndef STRCASECMP
#define STRCASECMP  strcasecmp
#endif
#ifndef STRNCASECMP
#define STRNCASECMP strncasecmp
#endif

#endif



bool cudaDecode::initCudaResources(int gpuID)
{


    CUdevice cuda_device;
	CUdevice Device = 0;
	cuda_device = findCudaDeviceDRV(gpuID);
	checkCudaErrors(cuDeviceGet(&Device, cuda_device));
    // get compute capabilities and the devicename
    int major, minor;
    size_t totalGlobalMem;

    char deviceName[256];
	checkCudaErrors(cuDeviceComputeCapability(&major, &minor, Device));
	checkCudaErrors(cuDeviceGetName(deviceName, 256, Device));
    printf("> Using GPU Device: %s has SM %d.%d compute capability\n", deviceName, major, minor);

	checkCudaErrors(cuDeviceTotalMem(&totalGlobalMem, Device));
    printf("  Total amount of global memory:     %4.4f MB\n", (float)totalGlobalMem/(1024*1024));

	
	checkCudaErrors(cuCtxCreate(&m_oContext, CU_CTX_BLOCKING_SYNC, Device));
    // Now we create the CUDA resources and the CUDA decoder context
    initCudaVideo();



    CUcontext cuCurrent = NULL;
    CUresult result = cuCtxPopCurrent(&cuCurrent);

    if (result != CUDA_SUCCESS)
    {
        printf("cuCtxPopCurrent: %d\n", result);
        assert(0);
    }

    /////////////////////////////////////////
    return (m_pVideoDecoder? true : false);
}



void cudaDecode::parseCommandLineArguments(char* filename, int gpuID)
{
	m_sFileName = filename;
    m_eVideoCreateFlags = cudaVideoCreate_PreferCUVID;
	m_DeviceID = gpuID;
    // Now verify the video file is legit
	FILE *fp = fopen(filename, "r");

	if (filename == NULL && fp == NULL)
    {
        printf("[%s]: unable to find file:\nExiting...\n", filename);
        exit(EXIT_FAILURE);
    }

    if (fp)
    {
        fclose(fp);
    }

    // default video file loaded by this sample
	printf(" input file: <%s>\n",  m_sFileName.c_str());
}

void cudaDecode::init(char *filename, int gpuID)
{
	parseCommandLineArguments(filename, gpuID);
	loadVideoSource(m_sFileName.c_str(), m_nVideoWidth, m_nVideoHeight);

	// Determine the proper window size needed to create the correct *client* area
	// that is of the size requested by m_dimensions.
#if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
	RECT adjustedWindowSize;
	DWORD dwWindowStyle = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
	SetRect(&adjustedWindowSize, 0, 0, m_nVideoWidth, m_nVideoHeight);
	AdjustWindowRect(&adjustedWindowSize, dwWindowStyle, false);
#endif


	// Initialize the CUDA Device
	cuInit(0);

	// Initialize CUDA and try to connect with an OpenGL context
	// Other video memory resources will be available
	int bTCC = 0;

	initCudaResources(gpuID);


	m_pVideoSource->start();
}

int main(int argc, char *argv[])
{

#if defined(__linux__)
    setenv ("DISPLAY", ":0", 0);
#endif

	cudaDecode cudadecode_;

	int GPUID = 0;
	char filename[] = "/home/al/video/short.mp4";
    // parse the command line arguments
	cudadecode_.init(filename, GPUID);

	while (true)
	{
		if (cudadecode_.check_decode_end())
		{
			break;
		}
	}
	cudadecode_.uninit();


   
    return 0;
}

void cudaDecode::uninit()
{
	m_pFrameQueue->endDecode();
	m_pVideoSource->stop();
	// clean up CUDA and OpenGL resources
	cleanup(true);
}



bool
cudaDecode::loadVideoSource(const char *video_file,
                unsigned int &width    , unsigned int &height)
{
    std::auto_ptr<FrameQueue> apFrameQueue(new FrameQueue);
    std::auto_ptr<VideoSource> apVideoSource(new VideoSource(video_file, apFrameQueue.get()));

    // retrieve the video source (width,height)
    apVideoSource->getSourceDimensions(width, height);

    std::cout << apVideoSource->format() << std::endl;

    m_pFrameQueue  = apFrameQueue.release();
    m_pVideoSource = apVideoSource.release();

    if (m_pVideoSource->format().codec == cudaVideoCodec_JPEG ||
        m_pVideoSource->format().codec == cudaVideoCodec_MPEG2)
    {
        m_eVideoCreateFlags = cudaVideoCreate_PreferCUDA;
    }

    bool IsProgressive = 0;
    m_pVideoSource->getProgressive(IsProgressive);
    return IsProgressive;
}

void
cudaDecode::initCudaVideo()
{
    // bind the context lock to the CUDA context
    CUresult result = cuvidCtxLockCreate(&m_CtxLock, m_oContext);

    if (result != CUDA_SUCCESS)
    {
        printf("cuvidCtxLockCreate failed: %d\n", result);
        assert(0);
    }

    size_t totalGlobalMem;
    size_t freeMem;

    cuMemGetInfo(&freeMem,&totalGlobalMem);
    printf("  Free memory:     %4.4f MB\n", (float)freeMem/(1024*1024));

    std::auto_ptr<VideoDecoder> apVideoDecoder(new VideoDecoder(m_pVideoSource->format(), m_oContext, m_eVideoCreateFlags, m_CtxLock));
    std::auto_ptr<VideoParser> apVideoParser(new VideoParser(apVideoDecoder.get(), m_pFrameQueue, &m_oContext));
    m_pVideoSource->setParser(*apVideoParser.get());

    m_pVideoParser  = apVideoParser.release();
    m_pVideoDecoder = apVideoDecoder.release();

}


void
cudaDecode::freeCudaResources(bool bDestroyContext)
{
    if (m_pVideoParser)
    {
        delete m_pVideoParser;
    }

    if (m_pVideoDecoder)
    {
        delete m_pVideoDecoder;
    }

    if (m_pVideoSource)
    {
        delete m_pVideoSource;
    }

    if (m_pFrameQueue)
    {
        delete m_pFrameQueue;
    }


    if (m_CtxLock)
    {
        checkCudaErrors(cuvidCtxLockDestroy(m_CtxLock));
    }

    if (m_oContext && bDestroyContext)
    {
        checkCudaErrors(cuCtxDestroy(m_oContext));
        m_oContext = NULL;
    }
}


// Release all previously initialized objects
bool cudaDecode::cleanup(bool bDestroyContext)
{
    if (bDestroyContext)
    {
        // Attach the CUDA Context (so we may properly free memory)
        checkCudaErrors(cuCtxPushCurrent(m_oContext));
        // Detach from the Current thread
        checkCudaErrors(cuCtxPopCurrent(NULL));
    }


    freeCudaResources(bDestroyContext);

    return true;
}

// Launches the CUDA kernels to fill in the texture data
// returns true if it's the end of the decode and false otherwise
bool cudaDecode::check_decode_end()
{
	return m_pFrameQueue->isDecodeFinished();
}

