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

#include "VideoSource.h"

#include "FrameQueue.h"
#include "VideoParser.h"

#include <assert.h>
extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
}

AVFormatContext* pFormatCtx;
int videoindex;
AVCodecContext* pCodecCtx;
CUVIDEOFORMAT g_stFormat;
AVBitStreamFilterContext *h264bsfc;

bool VideoSource::init(const std::string sFileName, FrameQueue *pFrameQueue)
{
	assert(0 != pFrameQueue);
	oSourceData_.hVideoParser = 0;
	oSourceData_.pFrameQueue = pFrameQueue;

	int                i;
	AVCodec            *pCodec;

	av_register_all();
	avformat_network_init();
	pFormatCtx = avformat_alloc_context();

	if (avformat_open_input(&pFormatCtx, sFileName.c_str(), NULL, NULL) != 0){
		printf("Couldn't open input stream.\n");
		return false;
	}
	if (avformat_find_stream_info(pFormatCtx, NULL) < 0){
		printf("Couldn't find stream information.\n");
		return false;
	}
	videoindex = -1;
	for (i = 0; i < pFormatCtx->nb_streams; i++)
		if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO){
			videoindex = i;
			break;
		}

	if (videoindex == -1){
		printf("Didn't find a video stream.\n");
		return false;
	}

	pCodecCtx = pFormatCtx->streams[videoindex]->codec;



	pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
	if (pCodec == NULL){
		printf("Codec not found.\n");
		return false;
	}

	//Output Info-----------------------------
	printf("--------------- File Information ----------------\n");
	av_dump_format(pFormatCtx, 0, sFileName.c_str(), 0);
	
	printf("-------------------------------------------------\n");

	memset(&g_stFormat, 0, sizeof(CUVIDEOFORMAT));

	switch (pCodecCtx->codec_id) {
	case AV_CODEC_ID_H263:
		g_stFormat.codec = cudaVideoCodec_MPEG4;
		break;

	case AV_CODEC_ID_H264:
		g_stFormat.codec = cudaVideoCodec_H264;
		break;

	case AV_CODEC_ID_HEVC:
		g_stFormat.codec = cudaVideoCodec_HEVC;
		break;

	case AV_CODEC_ID_MJPEG:
		g_stFormat.codec = cudaVideoCodec_JPEG;
		break;

	case AV_CODEC_ID_MPEG1VIDEO:
		g_stFormat.codec = cudaVideoCodec_MPEG1;
		break;

	case AV_CODEC_ID_MPEG2VIDEO:
		g_stFormat.codec = cudaVideoCodec_MPEG2;
		break;

	case AV_CODEC_ID_MPEG4:
		g_stFormat.codec = cudaVideoCodec_MPEG4;
		break;

		/*case AV_CODEC_ID_VP8:
			g_stFormat.codec = cudaVideoCodec_VP8;
			break;

			case AV_CODEC_ID_VP9:
			g_stFormat.codec = cudaVideoCodec_VP9;
			break;*/

	case AV_CODEC_ID_VC1:
		g_stFormat.codec = cudaVideoCodec_VC1;
		break;
	default:
		return false;
	}

	//这个地方的FFmoeg与cuvid的对应关系不是很确定，不过用这个参数似乎最靠谱
	switch (pCodecCtx->sw_pix_fmt)
	{
	case AV_PIX_FMT_YUV420P:
		g_stFormat.chroma_format = cudaVideoChromaFormat_420;
		break;
	case AV_PIX_FMT_YUV422P:
		g_stFormat.chroma_format = cudaVideoChromaFormat_422;
		break;
	case AV_PIX_FMT_YUV444P:
		g_stFormat.chroma_format = cudaVideoChromaFormat_444;
		break;
	default:
		g_stFormat.chroma_format = cudaVideoChromaFormat_420;
		break;
	}

	//找了好久，总算是找到了FFmpeg中标识场格式和帧格式的标识位
	//场格式是隔行扫描的，需要做去隔行处理
	switch (pCodecCtx->field_order)
	{
	case AV_FIELD_PROGRESSIVE:
	case AV_FIELD_UNKNOWN:
		g_stFormat.progressive_sequence = true;
		break;
	default:
		g_stFormat.progressive_sequence = false;
		break;
	}

	pCodecCtx->thread_safe_callbacks = 1;

	g_stFormat.coded_width = pCodecCtx->coded_width;
	g_stFormat.coded_height = pCodecCtx->coded_height;

	g_stFormat.display_area.right = pCodecCtx->width;
	g_stFormat.display_area.left = 0;
	g_stFormat.display_area.bottom = pCodecCtx->height;
	g_stFormat.display_area.top = 0;
	if (pCodecCtx->codec_id == AV_CODEC_ID_H264 || pCodecCtx->codec_id == AV_CODEC_ID_HEVC) {
		if (pCodecCtx->codec_id == AV_CODEC_ID_H264)
			h264bsfc = av_bitstream_filter_init("h264_mp4toannexb");
		else
			h264bsfc = av_bitstream_filter_init("hevc_mp4toannexb");
	}
	if (strcmp("rtsp", sFileName.c_str()) <= 0)
	{
		h264bsfc = 0;
	}
	//printf("code id = %d ,h264=%ld,w=%d,h=%d\n", pCodecCtx->codec_id, h264bsfc, g_stFormat.coded_width, g_stFormat.coded_height);
	//FILE* fp = fopen("temp.data", "rb");
	//fread(pCodecCtx->extradata, 1,51, fp);
	//fclose(fp);
	//printf("size=%d\n", pCodecCtx->extradata_size);
	//exit(1);
	return true;
}

#if 0
void VideoSource::internal_thread_entry()
{
	AVPacket *avpkt;
	avpkt = (AVPacket *)av_malloc(sizeof(AVPacket));
	CUVIDSOURCEDATAPACKET cupkt;
	int iPkt = 0;
	CUresult oResult;
	bool first = true;
	printf("start thread\n");
	while (av_read_frame(pFormatCtx, avpkt) >= 0){
		//if (bThreadExit){
			//break;
		//}
		//bStarted = true;
		//if (first)
		printf("111\n");
		if (avpkt->stream_index == videoindex){

			//cuCtxPushCurrent(g_oContext);

			if (avpkt && avpkt->size) {
				if (h264bsfc)
				{
					av_bitstream_filter_filter(h264bsfc, pFormatCtx->streams[videoindex]->codec, NULL, &avpkt->data, &avpkt->size, avpkt->data, avpkt->size, 0);

				}
				printf("222-1\n");
				cupkt.payload_size = (unsigned long)avpkt->size;
				cupkt.payload = (const unsigned char*)avpkt->data;

				if (avpkt->pts != AV_NOPTS_VALUE) {
					cupkt.flags = CUVID_PKT_TIMESTAMP;
					if (pCodecCtx->pkt_timebase.num && pCodecCtx->pkt_timebase.den){
						AVRational tb;
						tb.num = 1;
						tb.den = AV_TIME_BASE;
						cupkt.timestamp = av_rescale_q(avpkt->pts, pCodecCtx->pkt_timebase, tb);
						printf("222-2\n");
					}
					else
						cupkt.timestamp = avpkt->pts;
				}
			}
			else {
				cupkt.flags = CUVID_PKT_ENDOFSTREAM;
			}

			oResult = cuvidParseVideoData(oSourceData_.hVideoParser, &cupkt);
			printf("222-3\n");
			if ((cupkt.flags & CUVID_PKT_ENDOFSTREAM) || (oResult != CUDA_SUCCESS)){
				break;
			}
			iPkt++;
			//printf("Succeed to read avpkt %d !\n", iPkt);
			//checkCudaErrors(cuCtxPopCurrent(NULL));
		}
		printf("222\n");
		av_free_packet(avpkt);
		printf("333\n");
	}

	oSourceData_.pFrameQueue->endDecode();
	//bStarted = false;
	if (pCodecCtx->codec_id == AV_CODEC_ID_H264 || pCodecCtx->codec_id == AV_CODEC_ID_HEVC) {
		av_bitstream_filter_close(h264bsfc);
	}
}
#endif
#if 1
void VideoSource::internal_thread_entry()
{
	AVPacket *avpkt;
	avpkt = (AVPacket *)av_malloc(sizeof(AVPacket));
	CUVIDSOURCEDATAPACKET cupkt;
	CUresult oResult;
	bool first = true;
	while (av_read_frame(pFormatCtx, avpkt) >= 0){
	LOOP0:
		/*if (bThreadExit){
			break;
		}*/
		
		if (avpkt->stream_index == videoindex)
		{
			AVPacket new_pkt = *avpkt;
			if (avpkt && avpkt->size)
			{
				if (h264bsfc){

					int a = av_bitstream_filter_filter(h264bsfc, pFormatCtx->streams[videoindex]->codec, NULL,
						&new_pkt.data, &new_pkt.size,
						avpkt->data, avpkt->size,
						avpkt->flags & AV_PKT_FLAG_KEY);
					if (a > 0){
						if (new_pkt.data != avpkt->data)//-added this
						{
							av_free_packet(avpkt);

							avpkt->data = new_pkt.data;
							avpkt->size = new_pkt.size;
						}
					}
					else if (a < 0){
						goto LOOP0;
					}

					*avpkt = new_pkt;
				}

				cupkt.payload_size = (unsigned long)avpkt->size;
				cupkt.payload = (const unsigned char*)avpkt->data;

				if (avpkt->pts != AV_NOPTS_VALUE)
				{
					cupkt.flags = CUVID_PKT_TIMESTAMP;
					if (pCodecCtx->pkt_timebase.num && pCodecCtx->pkt_timebase.den)
					{
						AVRational tb;
						tb.num = 1;
						tb.den = AV_TIME_BASE;
						cupkt.timestamp = av_rescale_q(avpkt->pts, pCodecCtx->pkt_timebase, tb);
					}
					else
						cupkt.timestamp = avpkt->pts;
				}
			}
			else
			{
				cupkt.flags = CUVID_PKT_ENDOFSTREAM;
			}

			oResult = cuvidParseVideoData(oSourceData_.hVideoParser, &cupkt);
			if ((cupkt.flags & CUVID_PKT_ENDOFSTREAM) || (oResult != CUDA_SUCCESS))
			{
				printf("handle = %d ,break =%d,%d\n", oSourceData_.hVideoParser, cupkt.flags, oResult);
				break;
			}

			av_free(new_pkt.data);
		}
		else
			av_free_packet(avpkt);
	}
	oSourceData_.pFrameQueue->endDecode();
	//bStarted = false;
	printf("moon decode over!\n");
	
	if (pCodecCtx->codec_id == AV_CODEC_ID_H264 || pCodecCtx->codec_id == AV_CODEC_ID_HEVC) {
		av_bitstream_filter_close(h264bsfc);
	}
	oSourceData_.pFrameQueue->isDecodeFinished();
}
#endif
VideoSource::VideoSource() : hVideoSource_(0)
{

}
VideoSource::~VideoSource()
{
	uninit_cuvid();
}

void VideoSource::init_cuvid(const std::string sFileName, FrameQueue *pFrameQueue)
{
    // fill in SourceData struct as much as we can
    // right now. Client must specify parser at a later point
    // to avoid crashes (see setParser() method).
    assert(0 != pFrameQueue);
    oSourceData_.hVideoParser = 0;
    oSourceData_.pFrameQueue = pFrameQueue;

    CUVIDSOURCEPARAMS oVideoSourceParameters;
    // Fill parameter struct
    memset(&oVideoSourceParameters, 0, sizeof(CUVIDSOURCEPARAMS));
    oVideoSourceParameters.pUserData = &oSourceData_;               // will be passed to data handlers
    oVideoSourceParameters.pfnVideoDataHandler = HandleVideoData;   // our local video-handler callback
    oVideoSourceParameters.pfnAudioDataHandler = 0;
    // now create the actual source
    CUresult oResult = cuvidCreateVideoSource(&hVideoSource_, sFileName.c_str(), &oVideoSourceParameters);
	if (oResult != CUDA_SUCCESS)
	{
		printf("result=%d\n", oResult);
	}
	assert(CUDA_SUCCESS == oResult);
}



void VideoSource::uninit_cuvid()
{
    cuvidDestroyVideoSource(hVideoSource_);
}

void
VideoSource::ReloadVideo(const std::string sFileName, FrameQueue *pFrameQueue, VideoParser *pVideoParser)
{
    // fill in SourceData struct as much as we can right now. Client must specify parser at a later point
    assert(0 != pFrameQueue);
    oSourceData_.hVideoParser = pVideoParser->hParser_;
    oSourceData_.pFrameQueue  = pFrameQueue;

    cuvidDestroyVideoSource(hVideoSource_);

    CUVIDSOURCEPARAMS oVideoSourceParameters;
    // Fill parameter struct
    memset(&oVideoSourceParameters, 0, sizeof(CUVIDSOURCEPARAMS));
    oVideoSourceParameters.pUserData = &oSourceData_;               // will be passed to data handlers
    oVideoSourceParameters.pfnVideoDataHandler = HandleVideoData;   // our local video-handler callback
    oVideoSourceParameters.pfnAudioDataHandler = 0;
    // now create the actual source
    CUresult oResult = cuvidCreateVideoSource(&hVideoSource_, sFileName.c_str(), &oVideoSourceParameters);
    assert(CUDA_SUCCESS == oResult);
}


CUVIDEOFORMAT
VideoSource::format()
const
{
    CUVIDEOFORMAT oFormat;
    //CUresult oResult = cuvidGetSourceVideoFormat(hVideoSource_, &oFormat, 0);
   // assert(CUDA_SUCCESS == oResult);
	return g_stFormat;
    //return oFormat;
}

void
VideoSource::getSourceDimensions(unsigned int &width, unsigned int &height)
{
    CUVIDEOFORMAT rCudaVideoFormat=  format();

    //width  = rCudaVideoFormat.coded_width;
    //height = rCudaVideoFormat.coded_height;
	width = g_stFormat.coded_width;
	height = g_stFormat.coded_height;
}

void
VideoSource::getDisplayDimensions(unsigned int &width, unsigned int &height)
{
    CUVIDEOFORMAT rCudaVideoFormat=  format();

    width  = abs(rCudaVideoFormat.display_area.right  - rCudaVideoFormat.display_area.left);
    height = abs(rCudaVideoFormat.display_area.bottom - rCudaVideoFormat.display_area.top);
}

void
VideoSource::getProgressive(bool &progressive)
{
    CUVIDEOFORMAT rCudaVideoFormat=  format();
    progressive = (rCudaVideoFormat.progressive_sequence != 0);
}

void
VideoSource::setParser(VideoParser &rVideoParser)
{
    oSourceData_.hVideoParser = rVideoParser.hParser_;
	//printf("set handle = %d\n",oSourceData_.hVideoParser);
}

void
VideoSource::start()
{
	start_internal_thread();
	//CUresult oResult = cuvidSetVideoSourceState(hVideoSource_, cudaVideoState_Started);
    //assert(CUDA_SUCCESS == oResult);
}

void
VideoSource::stop()
{
    CUresult oResult = cuvidSetVideoSourceState(hVideoSource_, cudaVideoState_Stopped);
    assert(CUDA_SUCCESS == oResult);
}

bool
VideoSource::isStarted()
{
    return (cuvidGetVideoSourceState(hVideoSource_) == cudaVideoState_Started);
}

int
VideoSource::HandleVideoData(void *pUserData, CUVIDSOURCEDATAPACKET *pPacket)
{
    VideoSourceData *pVideoSourceData = (VideoSourceData *)pUserData;

    // Parser calls back for decode & display within cuvidParseVideoData
    if (!pVideoSourceData->pFrameQueue->isDecodeFinished())
    {
        CUresult oResult = cuvidParseVideoData(pVideoSourceData->hVideoParser, pPacket);

        if ((pPacket->flags & CUVID_PKT_ENDOFSTREAM) || (oResult != CUDA_SUCCESS))
            pVideoSourceData->pFrameQueue->endDecode();
    }

    return !pVideoSourceData->pFrameQueue->isDecodeFinished();
}

std::ostream &
operator << (std::ostream &rOutputStream, const CUVIDEOFORMAT &rCudaVideoFormat)
{
    rOutputStream << "\tVideoCodec      : ";

    if ((rCudaVideoFormat.codec <= cudaVideoCodec_UYVY) &&
        (rCudaVideoFormat.codec >= cudaVideoCodec_MPEG1) &&
        (rCudaVideoFormat.codec != cudaVideoCodec_NumCodecs))
    {
        rOutputStream << eVideoFormats[rCudaVideoFormat.codec].name << "\n";
    }
    else
    {
        rOutputStream << "unknown\n";
    }

    rOutputStream << "\tFrame rate      : " << rCudaVideoFormat.frame_rate.numerator << "/" << rCudaVideoFormat.frame_rate.denominator;
    rOutputStream << "fps ~ " << rCudaVideoFormat.frame_rate.numerator/static_cast<float>(rCudaVideoFormat.frame_rate.denominator) << "fps\n";
    rOutputStream << "\tSequence format : ";

    if (rCudaVideoFormat.progressive_sequence)
        rOutputStream << "Progressive\n";
    else
        rOutputStream << "Interlaced\n";

    rOutputStream << "\tCoded frame size: [" << rCudaVideoFormat.coded_width << ", " << rCudaVideoFormat.coded_height << "]\n";
    rOutputStream << "\tDisplay area    : [" << rCudaVideoFormat.display_area.left << ", " << rCudaVideoFormat.display_area.top;
    rOutputStream << ", " << rCudaVideoFormat.display_area.right << ", " << rCudaVideoFormat.display_area.bottom << "]\n";
    rOutputStream << "\tChroma format   : ";

    switch (rCudaVideoFormat.chroma_format)
    {
        case cudaVideoChromaFormat_Monochrome:
            rOutputStream << "Monochrome\n";
            break;

        case cudaVideoChromaFormat_420:
            rOutputStream << "4:2:0\n";
            break;

        case cudaVideoChromaFormat_422:
            rOutputStream << "4:2:2\n";
            break;

        case cudaVideoChromaFormat_444:
            rOutputStream << "4:4:4\n";
            break;

        default:
            rOutputStream << "unknown\n";
    }

    rOutputStream << "\tBitrate         : ";

    if (rCudaVideoFormat.bitrate == 0)
        rOutputStream << "unknown\n";
    else
        rOutputStream << rCudaVideoFormat.bitrate/1024 << "kBit/s\n";

    rOutputStream << "\tAspect ratio    : " << rCudaVideoFormat.display_aspect_ratio.x << ":" << rCudaVideoFormat.display_aspect_ratio.y << "\n";

    return rOutputStream;
}




