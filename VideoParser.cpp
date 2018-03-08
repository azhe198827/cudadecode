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

#include "VideoParser.h"

#include "VideoDecoder.h"
#include "FrameQueue.h"
#include "cuda_runtime.h"
#include <cstring>
#include <cassert>

//#include "opencv2/opencv.hpp"
#define DISPLAY

#ifdef DISPLAY
#include "opencv2/opencv.hpp"
#endif

VideoParser::VideoParser(VideoDecoder *pVideoDecoder, FrameQueue *pFrameQueue, CUcontext *pCudaContext): hParser_(0)
{
    assert(0 != pFrameQueue);
    oParserData_.pFrameQueue   = pFrameQueue;
    assert(0 != pVideoDecoder);
    oParserData_.pVideoDecoder = pVideoDecoder;
    oParserData_.pContext      = pCudaContext;

    CUVIDPARSERPARAMS oVideoParserParameters;
    memset(&oVideoParserParameters, 0, sizeof(CUVIDPARSERPARAMS));
    oVideoParserParameters.CodecType              = pVideoDecoder->codec();
    oVideoParserParameters.ulMaxNumDecodeSurfaces = pVideoDecoder->maxDecodeSurfaces();
    oVideoParserParameters.ulMaxDisplayDelay      = 1;  // this flag is needed so the parser will push frames out to the decoder as quickly as it can
    oVideoParserParameters.pUserData              = &oParserData_;
    oVideoParserParameters.pfnSequenceCallback    = HandleVideoSequence;    // Called before decoding frames and/or whenever there is a format change
    oVideoParserParameters.pfnDecodePicture       = HandlePictureDecode;    // Called when a picture is ready to be decoded (decode order)
    oVideoParserParameters.pfnDisplayPicture      = HandlePictureDisplay;   // Called whenever a picture is ready to be displayed (display order)
    CUresult oResult = cuvidCreateVideoParser(&hParser_, &oVideoParserParameters);
    assert(CUDA_SUCCESS == oResult);
}

int
CUDAAPI
VideoParser::HandleVideoSequence(void *pUserData, CUVIDEOFORMAT *pFormat)
{
    VideoParserData *pParserData = reinterpret_cast<VideoParserData *>(pUserData);

    if ((pFormat->codec         != pParserData->pVideoDecoder->codec())         // codec-type
        || (pFormat->coded_width   != pParserData->pVideoDecoder->frameWidth())
        || (pFormat->coded_height  != pParserData->pVideoDecoder->frameHeight())
        || (pFormat->chroma_format != pParserData->pVideoDecoder->chromaFormat()))
    {
        // We don't deal with dynamic changes in video format
        return 0;
    }

    return 1;
}

int
CUDAAPI
VideoParser::HandlePictureDecode(void *pUserData, CUVIDPICPARAMS *pPicParams)
{
    VideoParserData *pParserData = reinterpret_cast<VideoParserData *>(pUserData);

    bool bFrameAvailable = pParserData->pFrameQueue->waitUntilFrameAvailable(pPicParams->CurrPicIdx);

    if (!bFrameAvailable)
        return false;

    pParserData->pVideoDecoder->decodePicture(pPicParams, pParserData->pContext);

    return true;
}
int frame_num = 0;
int
CUDAAPI
VideoParser::HandlePictureDisplay(void *pUserData, CUVIDPARSERDISPINFO *pPicParams)
{
    // std::cout << *pPicParams << std::endl;

    VideoParserData *pParserData = reinterpret_cast<VideoParserData *>(pUserData);
	//printf("frame = %d\n", frame_num++);
	char * temp_gpu = NULL;


	CUVIDPROCPARAMS oVideoProcessingParameters;	CUdeviceptr		pSrc = 0;
	unsigned int	nPitch = 0;
	memset(&oVideoProcessingParameters, 0, sizeof(CUVIDPROCPARAMS));
	oVideoProcessingParameters.progressive_frame = pPicParams->progressive_frame;
	oVideoProcessingParameters.second_field = 0;
	oVideoProcessingParameters.top_field_first = pPicParams->top_field_first;
	oVideoProcessingParameters.unpaired_field = (pPicParams->progressive_frame == 1);
	pParserData->pVideoDecoder->mapFrame(pPicParams->picture_index, &pSrc, &nPitch, &oVideoProcessingParameters);
#ifdef DISPLAY	
	cv::Mat imageNV12(cv::Size(1536, 720 * 3 / 2), CV_8UC1);
	cv::Mat imageRgb(cv::Size(1536, 720), CV_8UC3);
	cudaMemcpy(imageNV12.data, (uchar*)pSrc, 1536 * 720 * 3 / 2, cudaMemcpyDeviceToHost);
	cv::cvtColor(imageNV12, imageRgb, cv::COLOR_YUV2RGB_NV21);
	cv::imshow("1", imageRgb);
	cv::waitKey(5);
#endif
	pParserData->pVideoDecoder->unmapFrame(pSrc);
    //pParserData->pFrameQueue->enqueue(pPicParams);
	cudaFree(temp_gpu);
    return 1;
}

std::ostream &
operator << (std::ostream &rOutputStream, const CUVIDPARSERDISPINFO &rParserDisplayInfo)
{
    rOutputStream << "Picture Index: " << rParserDisplayInfo.picture_index << "\n";
    rOutputStream << "Progressive frame: ";

    if (rParserDisplayInfo.progressive_frame)
        rOutputStream << "true\n";
    else
        rOutputStream << "false\n";

    rOutputStream << "Top field first: ";

    if (rParserDisplayInfo.top_field_first)
        rOutputStream << "true\n";
    else
        rOutputStream << "false\n";

    rOutputStream << "Repeat first field: ";

    if (rParserDisplayInfo.repeat_first_field)
        rOutputStream << "true\n";
    else
        rOutputStream << "false\n";

    rOutputStream << "Time stamp: " << rParserDisplayInfo.timestamp << "\n";

    return rOutputStream;
}

