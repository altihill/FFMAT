/*
----------------------------------------------------------------------------
ffmat
----------------------------------------------------------------------------
Packed ffmpeg demuxing/decoding API for MATLAB

Version: 2.1
Date: 10/23/2017
Author: Gao Shan
E-mail: altihill@gmail.com
----------------------------------------------------------------------------
ffmpeg version: 3.0.9 64bit
ffmpeg API version: 3.0
Tested C/C++ compliler: 
    visual studio 2015/2017 (Windows 10)
    Xcode 9.0 with clang (macOS 10.13)
    gcc 5.4.0 (ubuntu 16.04)
Tested MATLAB version: 
    R2016a 64bit, R2015a 64bit, R2014a 64bit, R2012a 64bit on Windows
    R2016b 64bit on macOS
    R2016b 64bit on ubuntu
----------------------------------------------------------------------------
Copyright 2017 Gao Shan

 * Permission is hereby granted, free of charge, to any person 
 * obtaining a copy of this software and associated documentation 
 * files (the "Software"), to deal in the Software without restriction, 
 * including without limitation the rights to use, copy, modify, 
 * merge, publish, distribute, sublicense, and/or sell copies of 
 * the Software, and to permit persons to whom the Software is 
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be 
 * included in all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, 
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND 
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT 
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, 
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR 
 * OTHER DEALINGS IN THE SOFTWARE.
--------------------------------------------------------
Change Log:
version 2.1
    -- better performance during sparsely picking
    -- fix building errors in macOS/Linux environments

version 2.0
	-- change command name:
	   open->openvideo, get->getprop, read->readframe, 
	   pick->pickframe, close->closevideo
	   previous command names are still valid.
	-- add command: seekframe
	-- add AspectRatio property
	-- fix bug: frame number mismatch
	-- fix bug: time stamp may be negative

version 1.1
	-- add output pixel format option (Gray/RGB/YUV)
	-- fix some mistakes in help file
*/

extern "C" {
	#include "libavcodec/avcodec.h"
	#include "libavformat/avformat.h"
	#include "libswscale/swscale.h"
	#include "libavutil/imgutils.h"
}// ffmpeg must be complied according to C

#include <string.h>
#include "crossplatform.h"
#include "mex.h" // for mexfunction

// Common sources
static AVFormatContext *pFormatCtx = NULL;
static AVCodecContext *pCodecCtx = NULL;
static AVStream *pStream = NULL;
static struct SwsContext *pSwsCtx = NULL;
static int StreamIdx = -1;
static unsigned long src_w, src_h, dst_w, dst_h;
static int out_channel = -1;

// buffers
static uint8_t *rawdata[4] = {NULL};
static int rawdata_linesize[4];
static AVFrame *frame = NULL;
static AVPacket *pkt;

void GS_Open(char *filename, int w, int h, AVPixelFormat dst_pix_fmt) {
	// register all formats and codecs
	av_register_all();
	// open input file, and allocate format context
	if (avformat_open_input(&pFormatCtx, filename, NULL, NULL) != 0) {
		mexErrMsgTxt("Failed to open video file");
	}
	// retrieve stream information
	if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
		mexErrMsgTxt("Failed to get video stream information");
    }
	// initiate codec
	AVCodec *pCodec = NULL;
	StreamIdx = av_find_best_stream(pFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &pCodec, 0);
	if (StreamIdx < 0) {
		mexErrMsgTxt("Failed to find valid video stream");
	}
	pStream = pFormatCtx->streams[StreamIdx];
	pCodecCtx = pStream->codec;
	AVDictionary *pOption = NULL;
	av_dict_set(&pOption, "refcounted_frames", "1", 0);
	if (avcodec_open2(pCodecCtx, pCodec, &pOption) < 0) {
		mexErrMsgTxt("Failed to open codec");
	}
	if (!av_reduce(
		&(pStream->display_aspect_ratio.num),
		&(pStream->display_aspect_ratio.den),
		pCodecCtx->sample_aspect_ratio.num*pCodecCtx->width,
		pCodecCtx->sample_aspect_ratio.den*pCodecCtx->height,
		2^31-1)) {
		mexWarnMsgTxt("Aspect ratio may be invalid");
	}
	// allocate buffer
	// input buffer
    pkt = av_packet_alloc();
	if (!pkt) {
		mexErrMsgTxt("Failed to allocate input buffer");
	}
    pkt->data = NULL;
    pkt->size = 0;
	// decoded buffer
	frame = av_frame_alloc();
	if (!frame) {
		mexErrMsgTxt("Failed to allocate decode buffer");
	}
	// output buffer
	src_w = pCodecCtx->width;
	src_h = pCodecCtx->height;
	dst_w = w == 0 ? src_w : w;
	dst_h = h == 0 ? src_h : h;
	if (av_image_alloc(rawdata, rawdata_linesize, dst_w, dst_h, dst_pix_fmt, 4) < 0) {
		mexErrMsgTxt("Failed to allocate raw video buffer");
	}
	// initiate scaling context
	pSwsCtx = sws_getContext(src_w, src_h, pCodecCtx->pix_fmt, 
							 dst_w, dst_h, dst_pix_fmt, 
							 SWS_BILINEAR, NULL, NULL, NULL);
	if (!pSwsCtx) {
		mexErrMsgTxt("Impossible to convert source to grayscale image");
	}
	return;
}

double GS_Read() {
	av_packet_unref(pkt);
	int ValidRead, ValidDecode, GotFrame;
	// read packet from source
	ValidRead = av_read_frame(pFormatCtx, pkt);
	if (ValidRead < 0){
		// try to retrieve remaining frames after sending all packets
		av_packet_unref(pkt);
		avcodec_decode_video2(pCodecCtx, frame, &GotFrame, pkt);
		if (GotFrame) {
			// success and return frame time
			return (frame->pkt_pts - pStream->start_time) * av_q2d(pStream->time_base);
		}else {
			// may be the end of the video
			return -1;
		}
	}else if (pkt->stream_index != StreamIdx) {
		// try to read video packet if not
		return GS_Read();
	}else {
		// decode frame
		ValidDecode = avcodec_decode_video2(pCodecCtx, frame, &GotFrame, pkt);
		if (ValidDecode<0) {
			// decoding failed
			return -2;
		}else if (!GotFrame || frame->pkt_pts < pStream->start_time) {
			// send more packets to decode a valid frame
			return GS_Read();
		}else {
			// success and return frame time
			return (frame->pkt_pts - pStream->start_time) * av_q2d(pStream->time_base);
		}
	}
}

double GS_Pick(int64_t SeekFrame, const int64_t TargetFrame) {
	double dret;
	int sret;
	int64_t sa,ta,b,c,SeekPts,TargetPts;
	// caculate pts of the seekframe and the targetframe
	sa = SeekFrame-1;
	ta = TargetFrame-1;
	b = pStream->avg_frame_rate.den * pStream->time_base.den;
	c = pStream->avg_frame_rate.num * pStream->time_base.num;
	SeekPts = av_rescale(sa,b,c) + pStream->start_time;
	TargetPts = av_rescale(ta,b,c) + pStream->start_time;
	if (SeekFrame<1 || TargetFrame<1){
		// invalid seekframe or targetframe
		return -3;
	}
	if (frame->pkt_pts == TargetPts){
		// no need to seek
		return (frame->pkt_pts - pStream->start_time) * av_q2d(pStream->time_base);
	}
	if (TargetPts < frame->pkt_pts || TargetPts > frame->pkt_pts + av_rescale(20,b,c)) {
		// seek to the seekframe
		if (SeekFrame<20) {
			// seek to the very begining of the file
			SeekPts = pStream->first_dts;
		}
		sret = av_seek_frame(pFormatCtx, StreamIdx, SeekPts, AVSEEK_FLAG_BACKWARD);
		if (sret < 0) {
			// seek failed
			return -3;
		}
		avcodec_flush_buffers(pCodecCtx);
	}
	// read and decode until targetframe
	do {
		dret = GS_Read();
	}while (dret > -1 && frame->pkt_pts < TargetPts);
	if (dret <= -1 || frame->pkt_pts == TargetPts) {
		// success and return frame time
		return dret;
	}else {
		// need to read backward to decode targetframe
		return GS_Pick((SeekFrame-4)>0 ? (SeekFrame-4):1, TargetFrame);
	}
}

mxArray* GS_memcpy() {
	mxArray* Ret;
	uint8_t *pOutputData;
	if (out_channel<3) {
		Ret = mxCreateNumericMatrix(dst_h,dst_w,mxUINT8_CLASS,mxREAL);
		pOutputData = (uint8_t *)mxGetData(Ret);
		for (int i=0; i<dst_w; i++)
			for (int j=0; j<dst_h; j++) {
				*pOutputData = *(rawdata[0]+(j*dst_w+i)*3+out_channel);
				pOutputData++;
			}
	}else if (out_channel == 3) {
		const mwSize dims[] = {dst_h,dst_w,3};
		Ret = mxCreateNumericArray(3,dims,mxUINT8_CLASS,mxREAL);
		pOutputData = (uint8_t *)mxGetData(Ret);
		for (int ij=0; ij<3; ij++)
			for (int i=0; i<dst_w; i++)
				for (int j=0; j<dst_h; j++) {
					*pOutputData = *(rawdata[0]+(j*dst_w+i)*3+ij);
					pOutputData++;
				}
	}else if (out_channel<7) {
		Ret = mxCreateNumericMatrix(dst_h,dst_w,mxUINT8_CLASS,mxREAL);
		pOutputData = (uint8_t *)mxGetData(Ret);
		for (int i=0; i<dst_w; i++)
			for (int j=0; j<dst_h; j++) {
				*pOutputData = *(rawdata[out_channel-4]+j*dst_w+i);
				pOutputData++;
			}
	}else if (out_channel == 7) {
		const mwSize dims[] = {dst_h,dst_w,3};
		Ret = mxCreateNumericArray(3,dims,mxUINT8_CLASS,mxREAL);
		pOutputData = (uint8_t *)mxGetData(Ret);
		for (int ij=0; ij<3; ij++)
			for (int i=0; i<dst_w; i++)
				for (int j=0; j<dst_h; j++) {
					*pOutputData = *(rawdata[ij]+j*dst_w+i);
					pOutputData++;
				}
	}else {
		mexErrMsgTxt("Invalid output channel.");
	}
	return Ret;
}

void GS_Close() {
	// free memory
	if (pCodecCtx)		avcodec_close(pCodecCtx);
	if (pFormatCtx)		avformat_close_input(&pFormatCtx);
	if (pkt)			av_packet_free(&pkt);
	if (frame)			av_frame_free(&frame);
	av_freep(&rawdata);
	if (pSwsCtx)		{sws_freeContext(pSwsCtx); pSwsCtx = NULL;}
	return;
}

void mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[]) {
	// read command name
	char *FunctionName;
	if (nrhs < 1) {
		mexErrMsgTxt("Must specify command to excute");
	}else if (!mxIsChar(prhs[0])) {
		mexErrMsgTxt("The first input argument must be string, representing command name");
	}else {
		FunctionName = mxArrayToString(prhs[0]);
	}
	// allocate status output
	plhs[0] = mxCreateNumericMatrix(1,1,mxDOUBLE_CLASS,mxREAL);
	double *Status = mxGetPr(plhs[0]);
	*Status = 1;
	// check command
	if (!strncasecmp(FunctionName, "openvideo", 4)) {
		int h=0, w=0;
		char *FileName;
		char *pixfmt_str = NULL;
		AVPixelFormat out_pix_fmt;
		if (pFormatCtx) {
			GS_Close();
			mexWarnMsgTxt("Previous video has been closed.");
		}
		// check input parameters
		switch (nrhs) {
			case 5:
				if (!mxIsChar(prhs[4])) {
					mexErrMsgTxt("The fourth argument after 'open' command must be string, representing output pixel format");
				}else {
					pixfmt_str = mxArrayToString(prhs[4]);
				}
			case 4:
				if (!mxIsNumeric(prhs[3])) {
					mexErrMsgTxt("The third argument after 'open' command must be numeric type, representing height of the output frame");
				}else {
					h = (int) mxGetScalar(prhs[3]);
				}
			case 3:
				if (!mxIsNumeric(prhs[2])) {
					mexErrMsgTxt("The second argument after 'open' command must be numeric type, representing width of the output frame");
				}else {
					w = (int) mxGetScalar(prhs[2]);
				}
			case 2:
				if (!mxIsChar(prhs[1])) {
					mexErrMsgTxt("The first argument after 'open' command must be string, representing filename");
				}else {
					FileName = mxArrayToString(prhs[1]);
				}
			break;
			/*case 1:{
				mxArray *mxon[2] = {mxCreateString(NULL),mxCreateString(NULL)};
				mxArray *mxop[2] = {mxCreateString("*.*"),mxCreateString("Open Video File")};
				mexCallMATLAB(2,mxon,2,mxop,"uigetfile");
				mxArray *mxcn[1] = {mxCreateString(NULL)};
				mxArray *mxcp[2] = {mxon[1],mxon[0]};
				mexCallMATLAB(1,mxcn,2,mxcp,"strcat");
				FileName = mxArrayToString(mxcn[0]); 
				mxDestroyArray(mxon[0]);
				mxDestroyArray(mxon[1]);
				mxDestroyArray(mxop[0]);
				mxDestroyArray(mxop[1]);
				mxDestroyArray(mxcn[0]);
				break;
			}*/
			default:
				mexErrMsgTxt("'open' command can have 1~3 additional arguments.");
		}
        if (!pixfmt_str)    {out_channel = 4; out_pix_fmt = AV_PIX_FMT_GRAY8;}
		else if (!strcasecmp(pixfmt_str,"GRAY"))		{out_channel = 4; out_pix_fmt = AV_PIX_FMT_GRAY8;}
		else if (!strcasecmp(pixfmt_str,"R"))		{out_channel = 0; out_pix_fmt = AV_PIX_FMT_RGB24;}
		else if (!strcasecmp(pixfmt_str,"G"))		{out_channel = 1; out_pix_fmt = AV_PIX_FMT_RGB24;}
		else if (!strcasecmp(pixfmt_str,"B"))		{out_channel = 2; out_pix_fmt = AV_PIX_FMT_RGB24;}
		else if (!strcasecmp(pixfmt_str,"RGB"))	{out_channel = 3; out_pix_fmt = AV_PIX_FMT_RGB24;}
		else if (!strcasecmp(pixfmt_str,"Y"))		{out_channel = 4; out_pix_fmt = AV_PIX_FMT_YUV420P;}
		else if (!strcasecmp(pixfmt_str,"U"))		{out_channel = 5; out_pix_fmt = AV_PIX_FMT_YUV444P;}
		else if (!strcasecmp(pixfmt_str,"V"))		{out_channel = 6; out_pix_fmt = AV_PIX_FMT_YUV444P;}
		else if (!strcasecmp(pixfmt_str,"YUV"))	{out_channel = 7; out_pix_fmt = AV_PIX_FMT_YUV444P;}
		else									mexErrMsgTxt("Invalid output pixel format.");
		GS_Open(FileName, w, h, out_pix_fmt);
		mxFree(FileName);
		if (nrhs>=5) mxFree(pixfmt_str);
	}else if (!strncasecmp(FunctionName, "getprop", 3)) {
		if (nlhs != 2) {
			mexErrMsgTxt("'get' command must have 2 output arguments");
		}
		if (!pFormatCtx) {
			*Status = -4;// closed video
			plhs[1] = mxCreateNumericMatrix(1,1,mxDOUBLE_CLASS,mxREAL);
			*(mxGetPr(plhs[1])) = -1;
		}else {
			const char * propname[] = {"FrameRate","Height","Width","Duration","TotalFrames",
			                           "NextFrame","FileName","BitRate","AspectRatio"};
			plhs[1] = mxCreateStructMatrix(1, 1, sizeof(propname)/sizeof(propname[0]), propname);
			mxArray *mxparaval = NULL;
			// frame rate
			mxparaval = mxCreateNumericMatrix(1,1,mxDOUBLE_CLASS,mxREAL);
			*(mxGetPr(mxparaval)) = av_q2d(pStream->avg_frame_rate);
			mxSetFieldByNumber(plhs[1], 0, 0, mxparaval);
			if (*(mxGetPr(mxparaval))<=0) {
				mexWarnMsgTxt("FrameRate may not be valid.");
			}
			// height
			mxparaval = mxCreateNumericMatrix(1,1,mxDOUBLE_CLASS,mxREAL);
			*(mxGetPr(mxparaval)) = (double) pCodecCtx->height;
			mxSetFieldByNumber(plhs[1], 0, 1, mxparaval);
			if (*(mxGetPr(mxparaval))<=0) {
				mexWarnMsgTxt("Height may not be valid.");
			}
			// width
			mxparaval = mxCreateNumericMatrix(1,1,mxDOUBLE_CLASS,mxREAL);
			*(mxGetPr(mxparaval)) = (double) pCodecCtx->width;
			mxSetFieldByNumber(plhs[1], 0, 2, mxparaval);
			if (*(mxGetPr(mxparaval))<=0) {
				mexWarnMsgTxt("Width may not be valid.");
			}
			// duration
			mxparaval = mxCreateNumericMatrix(1,1,mxDOUBLE_CLASS,mxREAL);
			*(mxGetPr(mxparaval)) = (double) pStream->duration * av_q2d(pStream->time_base);
			mxSetFieldByNumber(plhs[1], 0, 3, mxparaval);
			if (*(mxGetPr(mxparaval))<=0) {
				mexWarnMsgTxt("Duration may not be valid.");
			}
			// total frames
			mxparaval = mxCreateNumericMatrix(1,1,mxDOUBLE_CLASS,mxREAL);
			AVRational frametime = av_inv_q(pStream->avg_frame_rate);
			if (pStream->nb_frames > 0) {
				*(mxGetPr(mxparaval)) = (double) pStream->nb_frames;
			}else {
				*(mxGetPr(mxparaval)) = (double) av_rescale_q(pStream->duration, pStream->time_base, frametime);
			}
			mxSetFieldByNumber(plhs[1], 0, 4, mxparaval);
			if (*(mxGetPr(mxparaval))<=0) {
				mexWarnMsgTxt("TotalFrames may not be valid.");
			}
			// next frame
			mxparaval = mxCreateNumericMatrix(1,1,mxDOUBLE_CLASS,mxREAL);
			if (pCodecCtx->pts_correction_last_pts >= pStream->start_time && 
			    pCodecCtx->pts_correction_last_pts < pStream->duration + pStream->start_time) {
				*(mxGetPr(mxparaval)) = (double) av_rescale_q(pCodecCtx->pts_correction_last_pts - pStream->start_time, 
												 pStream->time_base, frametime) + 2;
			}else if(pCodecCtx->pts_correction_last_pts == pStream->duration + pStream->start_time) {
				*(mxGetPr(mxparaval)) = -1;
				mexWarnMsgTxt("NextFrame is invalid because it is the end of the video.");
			}else {
				*(mxGetPr(mxparaval)) = 1;
			}
			mxSetFieldByNumber(plhs[1], 0, 5, mxparaval);
			// file name
			mxSetFieldByNumber(plhs[1], 0, 6, mxCreateString(pFormatCtx->filename));
			// bit rate
			mxparaval = mxCreateNumericMatrix(1,1,mxDOUBLE_CLASS,mxREAL);
			*(mxGetPr(mxparaval)) = (double) pFormatCtx->bit_rate;
			mxSetFieldByNumber(plhs[1], 0, 7, mxparaval);
			if (*(mxGetPr(mxparaval))<=0) {
				mexWarnMsgTxt("BitRate may not be valid.");
			}
			// aspect ratio
			mxparaval = mxCreateNumericMatrix(2,1,mxDOUBLE_CLASS,mxREAL);
			(mxGetPr(mxparaval))[0] = pStream->display_aspect_ratio.num;
			(mxGetPr(mxparaval))[1] = pStream->display_aspect_ratio.den;
			mxSetFieldByNumber(plhs[1], 0, 8, mxparaval);
			if ((mxGetPr(mxparaval)[0])<=0 || (mxGetPr(mxparaval)[1])<=0) {
				mexWarnMsgTxt("AspectRatio may not be valid.");
			}
		}
	}else if (!strncasecmp(FunctionName, "readframe", 4)) {
		// check output
		if (nlhs != 2) {
			mexErrMsgTxt("'read' command must have 2 output arguments");
		}
		// read, decode and rescale frame
		if (!pFormatCtx) {
			*Status = -4;//closed video
			plhs[1] = mxCreateNumericMatrix(1,1,mxDOUBLE_CLASS,mxREAL);
			*(mxGetPr(plhs[1])) = -1;
		}else {
			double ReadRet = GS_Read();
			*Status = ReadRet;
			if (ReadRet > -1) {
				sws_scale(pSwsCtx, frame->data, frame->linesize, 0, src_h, rawdata, rawdata_linesize);
				plhs[1] = GS_memcpy();
			}else {
				plhs[1] = mxCreateNumericMatrix(1,1,mxDOUBLE_CLASS,mxREAL);
				*(mxGetPr(plhs[1])) = -1;
			}
		}
	}else if (!strncasecmp(FunctionName, "pickframe", 4)) {
		// check input and output
		if (nlhs != 2) {
			mexErrMsgTxt("'pick' command must have 2 output arguments");
		}
		if (nrhs != 2) {
			mexErrMsgTxt("'pick' command must have 1 input arguments");
		}
		if (!mxIsNumeric(prhs[1])) {
			mexErrMsgTxt("The first argument after 'pick' command must be numeric, representing frame number");
		}
		// seek, read, decode and rescale frame
		if (!pFormatCtx) {
			*Status = -4;//closed video
			plhs[1] = mxCreateNumericMatrix(1,1,mxDOUBLE_CLASS,mxREAL);
			*(mxGetPr(plhs[1])) = -1;
		}else {
			int64_t FrameNum = (int64_t) mxGetScalar(prhs[1]);
			double PickRet;
			if (pCodecCtx->codec_id == AV_CODEC_ID_H264) {
				PickRet = GS_Pick(((FrameNum-14)>0)?(FrameNum-14):1, FrameNum);
			}else {
				PickRet = GS_Pick(FrameNum, FrameNum);
			}
			*Status = PickRet;
			if (PickRet > -1) {
				sws_scale(pSwsCtx, frame->data, frame->linesize, 0, src_h, rawdata, rawdata_linesize);
				plhs[1] = GS_memcpy();
			}else {
				plhs[1] = mxCreateNumericMatrix(1,1,mxDOUBLE_CLASS,mxREAL);
				*(mxGetPr(plhs[1])) = -1;
			}
		}
	}else if (!strncasecmp(FunctionName, "seekframe", 4)) {
		// check input and output
		if (nrhs != 2) {
			mexErrMsgTxt("'seek' command must have 1 input arguments");
		}
		if (!mxIsNumeric(prhs[1])) {
			mexErrMsgTxt("The first argument after 'seek' command must be numeric, representing frame number");
		}
		// seek to frame
		if (!pFormatCtx) {
			*Status = -4;//closed video
		}else {
			int64_t FrameNum = (int64_t) mxGetScalar(prhs[1])-1;
			if (FrameNum == 0) {
				if (av_seek_frame(pFormatCtx, StreamIdx, pStream->first_dts, AVSEEK_FLAG_BACKWARD) < 0) {
					// seek failed
					*Status = -3;
				}else {
					avcodec_flush_buffers(pCodecCtx);
					*Status = (pStream->first_dts-pStream->start_time) * av_q2d(pStream->time_base);
				}
			}else {
				double PickRet;
				if (pCodecCtx->codec_id == AV_CODEC_ID_H264) {
					PickRet = GS_Pick(((FrameNum-14)>0)?(FrameNum-14):1, FrameNum);
				}else {
					PickRet = GS_Pick(FrameNum, FrameNum);
				}
				*Status = PickRet;
			}
		}
	}else if (!strncasecmp(FunctionName, "closevideo", 5)) {
		// close video
		GS_Close();
	}else {
		mexErrMsgTxt("Invalid ffmat command");
	}
	mxFree(FunctionName);
	return;
}
