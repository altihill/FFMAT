#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
#include "libavutil/hwcontext.h"
#include "libavutil/opt.h"

#include <string.h>
#include "mex.h"
#include "crossplatform.h"

static AVFormatContext *FormatCtx = NULL;
static AVCodecContext *CodecCtx = NULL;
static AVStream *Stream = NULL;
static struct SwsContext *SwsCtx = NULL;
static int StreamIdx = -1;
static int src_w, src_h, dst_w, dst_h;
static int out_channel = -1;
static int64_t b, c;

static enum AVPixelFormat HWPixFmt;

static uint8_t *rawdata[4] = {NULL};
static int rawdata_linesize[4];
static AVFrame *frame = NULL;
static AVFrame *hwframe = NULL;
static AVPacket *pkt;

static enum AVPixelFormat get_hw_format(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts) {
    const enum AVPixelFormat *p;
    for (p = pix_fmts; *p != -1; p++)
        if (*p == HWPixFmt) return *p;
    mexErrMsgTxt("Failed to get HW surface format.\n");
    return AV_PIX_FMT_NONE;
}

int GS_Load() {
	int ValidRead, ValidSend;
	if (pkt->data) {
		ValidSend = avcodec_send_packet(CodecCtx, pkt);
		if (ValidSend == 0)  {av_packet_unref(pkt); return 0;}
        else if (ValidSend == AVERROR(EAGAIN))		return 0;
		else if (ValidSend < 0)			            return -2;
	}
	ValidRead = av_read_frame(FormatCtx, pkt);
	if (ValidRead < 0) return -1;
    else if (pkt->stream_index != StreamIdx) av_packet_unref(pkt);
	return GS_Load();
}

double GS_Read() {
	int GotFrame, ValidLoad;
	ValidLoad = GS_Load();
	if (ValidLoad >= -1){
		GotFrame = avcodec_receive_frame(CodecCtx, hwframe);
		if (GotFrame == 0) {
            if (av_hwframe_transfer_data(frame, hwframe, 0) < 0) return -3;
            else frame->pts = hwframe->pts;
			return (frame->pts - Stream->start_time) * av_q2d(Stream->time_base);
		}else if (GotFrame == AVERROR(EAGAIN)) return GS_Read();
		else return -3;
	}else return ValidLoad;
}

double GS_Pick(int64_t SeekFrame, int64_t TargetFrame, int FailCount) {
	double dret;
	int sret;
	int64_t SeekPts,TargetPts;
	// caculate pts of the seekframe and the targetframe
    if (SeekFrame<1 || TargetFrame<1) return -3;
	TargetPts = av_rescale(TargetFrame-1,b,c) + Stream->start_time;
	if (SeekFrame<20) SeekPts = Stream->first_dts;
    else SeekPts = av_rescale(SeekFrame-1,b,c) + Stream->start_time;
	// seek to the seekframe
	sret = av_seek_frame(FormatCtx, StreamIdx, SeekPts, AVSEEK_FLAG_BACKWARD);
	if (sret < 0) return -3;
    av_packet_unref(pkt);
	avcodec_flush_buffers(CodecCtx);
	// read and decode until targetframe
	do dret = GS_Read();
	while (dret > -1 && frame->pts < TargetPts);
	if (frame->pts == TargetPts) return dret;
    else if (dret <= -1)
        if (FailCount<3) return GS_Pick((SeekFrame-2)>0 ? (SeekFrame-2):1, TargetFrame,FailCount++);
        else return dret;
	else
		mexPrintf("Pick:%d\n",TargetFrame);
        if (FailCount<10) return GS_Pick((SeekFrame-4)>0 ? (SeekFrame-4):1, TargetFrame,FailCount++);
        else return -3;
}

void GS_Open(char *filename, int w, int h, enum AVPixelFormat dst_pix_fmt) {
	AVCodec *pCodec = NULL;
    AVCodecParameters *pCodecPara = NULL;
    AVBufferRef *HWDevCtx = NULL;
    AVBufferRef *HWFrameCtx = NULL;
	AVDictionary *pOption = NULL;
    enum AVPixelFormat *src_pix_fmts = NULL;
    enum AVPixelFormat src_pix_fmt;
    int i, j;
    // find available hwaccel dev
    enum AVHWDeviceType type = AV_HWDEVICE_TYPE_NONE;
    enum AVHWDeviceType HWDevs[] = {
        AV_HWDEVICE_TYPE_CUDA,
        AV_HWDEVICE_TYPE_D3D11VA,
        AV_HWDEVICE_TYPE_DXVA2
    };
    bool HWDevFound[sizeof(HWDevs)] = {false};
    bool HWDevCodec[sizeof(HWDevs)] = {false};
    enum AVPixelFormat hw_pix_fmts[sizeof(HWDevs)] = {AV_HWDEVICE_TYPE_NONE};
    while((type = av_hwdevice_iterate_types(type)) != AV_HWDEVICE_TYPE_NONE)
        for (i=0;i<sizeof(HWDevs);i++)
            if (type == HWDevs[i])
                HWDevFound[i] = true;
    for (i=0;i<sizeof(HWDevs) && !HWDevs[i];i++);
    if (i==sizeof(HWDevs))
        mexErrMsgTxt("No hardware acceleration device is found");
	// open input file, and allocate format context
	if (avformat_open_input(&FormatCtx, filename, NULL, NULL) < 0)
		mexErrMsgTxt("Failed to open video file");
	// retrieve stream information
	if (avformat_find_stream_info(FormatCtx, NULL) < 0)
		mexErrMsgTxt("Failed to retrieve video stream information");
	// find video stream
	StreamIdx = av_find_best_stream(FormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &pCodec, 0);
	if (StreamIdx < 0)
		mexErrMsgTxt("Failed to find valid video stream");
    // determine whether codec support existing hwaccel devs
    for (i=0;;i++) {
        const AVCodecHWConfig *config = avcodec_get_hw_config(pCodec, i);
        if (!config) break;
        else if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX)
            for (j=0;j<sizeof(HWDevs);j++)
                if (config->device_type == HWDevs[j]) {
                    HWDevCodec[j] = true;
                    hw_pix_fmts[j] = config->pix_fmt;
                    break;
                }
    }
    for (i=0;i<sizeof(HWDevs) && (!HWDevFound[i] || !HWDevCodec[i]);i++);
    if (i==sizeof(HWDevs))
        mexErrMsgTxt("Codec does not support existing hardware device(s)");
    else {
        type = HWDevs[i];
        HWPixFmt = hw_pix_fmts[i];
    }
	// initiate codec context
	CodecCtx = avcodec_alloc_context3(pCodec);
	if (!CodecCtx)
		mexErrMsgTxt("Failed to allocate codec context");
    Stream = FormatCtx->streams[StreamIdx];
	pCodecPara = Stream->codecpar;
	if (avcodec_parameters_to_context(CodecCtx, pCodecPara) < 0)
		mexErrMsgTxt("Failed to fill codec context");
    CodecCtx->get_format = get_hw_format;
    av_opt_set_int(CodecCtx, "refcounted_frames", 1, 0);
	// open hardware device
    if (av_hwdevice_ctx_create(&HWDevCtx, type, NULL, NULL, 0) < 0)
        mexErrMsgTxt("Failed to open hardware decoder");
    CodecCtx->hw_device_ctx = av_buffer_ref(HWDevCtx);
    // open codec
	if (avcodec_open2(CodecCtx, pCodec, NULL) < 0)
		mexErrMsgTxt("Failed to open codec");
	if (!av_reduce(
		&(Stream->display_aspect_ratio.num),
		&(Stream->display_aspect_ratio.den),
		CodecCtx->sample_aspect_ratio.num*CodecCtx->width,
		CodecCtx->sample_aspect_ratio.den*CodecCtx->height,
		2^31-1))
		mexWarnMsgTxt("Aspect ratio may be invalid");
	// allocate buffer
	// input buffer
    pkt = av_packet_alloc();
	if (!pkt)
		mexErrMsgTxt("Failed to allocate input buffer");
    pkt->data = NULL;
    pkt->size = 0;
	// decoded buffer
	frame = av_frame_alloc();
	if (!frame)
		mexErrMsgTxt("Failed to allocate decode buffer");
    hwframe = av_frame_alloc();
	if (!frame)
		mexErrMsgTxt("Failed to allocate hardware decode buffer");
	// output buffer
	src_w = CodecCtx->width;
	src_h = CodecCtx->height;
	dst_w = w == 0 ? src_w : w;
	dst_h = h == 0 ? src_h : h;
	if (av_image_alloc(rawdata, rawdata_linesize, dst_w, dst_h, dst_pix_fmt, 4) < 0)
		mexErrMsgTxt("Failed to allocate raw video buffer");
	// negotiate transfer pixfmt
    GS_Read();
    if (av_hwframe_transfer_get_formats(hwframe->hw_frames_ctx,
                            AV_HWFRAME_TRANSFER_DIRECTION_FROM,
                            &src_pix_fmts,0) < 0)
        mexErrMsgTxt("Failed to get valid source pixel format");
    for (i=0;;i++)
        if (src_pix_fmts[i] == dst_pix_fmt){
            src_pix_fmt = dst_pix_fmt;
            break;
        }else if (src_pix_fmts[i] == AV_PIX_FMT_NONE){
            src_pix_fmt = src_pix_fmts[0];
            break;
        }
    av_freep(&src_pix_fmts);
    frame->format = src_pix_fmt;
    if (av_seek_frame(FormatCtx, StreamIdx, Stream->first_dts, AVSEEK_FLAG_BACKWARD) < 0)
        mexErrMsgTxt("Failed to seek to the very beginning");
    else avcodec_flush_buffers(CodecCtx);
    // create swscale ctx
	SwsCtx = sws_getContext(src_w, src_h, src_pix_fmt, 
							dst_w, dst_h, dst_pix_fmt, 
							SWS_BILINEAR, NULL, NULL, NULL);
	if (!SwsCtx)
		mexErrMsgTxt("Impossible to convert source to target pix_fmt");
    // for pts calculation
    b = Stream->avg_frame_rate.den * Stream->time_base.den;
	c = Stream->avg_frame_rate.num * Stream->time_base.num;
    mexLock();
    return;
}

mxArray* GS_memcpy() {
	mxArray* Ret;
	uint8_t *pOutputData;
	if (out_channel<3) {
		Ret = mxCreateNumericMatrix(dst_h,dst_w,mxUINT8_CLASS,mxREAL);
		pOutputData = (uint8_t *)mxGetData(Ret);
		for (int i=0; i<dst_w; i++)
			for (int j=0; j<dst_h; j++, pOutputData++)
				*pOutputData = *(rawdata[0]+(j*dst_w+i)*3+out_channel);
	}else if (out_channel == 3) {
		const mwSize dims[] = {dst_h,dst_w,3};
		Ret = mxCreateNumericArray(3,dims,mxUINT8_CLASS,mxREAL);
		pOutputData = (uint8_t *)mxGetData(Ret);
		for (int ij=0; ij<3; ij++)
			for (int i=0; i<dst_w; i++)
				for (int j=0; j<dst_h; j++, pOutputData++)
					*pOutputData = *(rawdata[0]+(j*dst_w+i)*3+ij);
	}else if (out_channel<7) {
		Ret = mxCreateNumericMatrix(dst_h,dst_w,mxUINT8_CLASS,mxREAL);
		pOutputData = (uint8_t *)mxGetData(Ret);
		for (int i=0; i<dst_w; i++)
			for (int j=0; j<dst_h; j++, pOutputData++)
				*pOutputData = *(rawdata[out_channel-4]+j*dst_w+i);
	}else if (out_channel=7) {
		const mwSize dims[] = {dst_h,dst_w,3};
		Ret = mxCreateNumericArray(3,dims,mxUINT8_CLASS,mxREAL);
		pOutputData = (uint8_t *)mxGetData(Ret);
		for (int ij=0; ij<3; ij++)
			for (int i=0; i<dst_w; i++)
				for (int j=0; j<dst_h; j++, pOutputData++)
					*pOutputData = *(rawdata[ij]+j*dst_w+i);
	}else
		mexErrMsgTxt("Invalid output channel.");
	return Ret;
}

void GS_Close() {
    mexUnlock();
    pkt->data = NULL;
    pkt->size = 0;
    avcodec_send_packet(CodecCtx,pkt);
    if (pkt)			            av_packet_free(&pkt);
    if (CodecCtx->hw_device_ctx)    av_buffer_unref(&CodecCtx->hw_device_ctx);
	if (CodecCtx)		            avcodec_free_context(&CodecCtx);
	if (FormatCtx)		            avformat_close_input(&FormatCtx);
	if (frame)			            av_frame_free(&frame);
    if (hwframe)		            av_frame_free(&hwframe);
	if (rawdata)		            av_freep(&rawdata);
	if (SwsCtx)		                {sws_freeContext(SwsCtx); SwsCtx = NULL;}
	return;
}

void mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[]) {
	// read command name
	char *FunctionName;
	if (nrhs < 1)
        mexErrMsgTxt("Must specify command to excute");
	else if (!mxIsChar(prhs[0]))
        mexErrMsgTxt("The first input argument must be string, representing command name");
	else
		FunctionName = mxArrayToString(prhs[0]);
	// allocate status output
	plhs[0] = mxCreateNumericMatrix(1,1,mxDOUBLE_CLASS,mxREAL);
	double *Status = mxGetPr(plhs[0]);
	*Status = 1;
	// parse command
	if (!strncasecmp(FunctionName, "openvideo", 4)) {
		int h=0, w=0;
		char *FileName;
		char *pixfmt_str = "GRAY";
		enum AVPixelFormat out_pix_fmt;
		if (FormatCtx) {
			GS_Close();
			mexWarnMsgTxt("Previous video has been closed.");
		}
		// parse input parameters
		switch (nrhs) {
			case 5:
				if (!mxIsChar(prhs[4]))
					mexErrMsgTxt("The fourth argument after 'open' command must be string, representing output pixel format");
				else
					pixfmt_str = mxArrayToString(prhs[4]);
			case 4:
				if (!mxIsNumeric(prhs[3]))
					mexErrMsgTxt("The third argument after 'open' command must be numeric type, representing height of the output frame");
				else
					h = (int) mxGetScalar(prhs[3]);
			case 3:
				if (!mxIsNumeric(prhs[2]))
					mexErrMsgTxt("The second argument after 'open' command must be numeric type, representing width of the output frame");
				else
					w = (int) mxGetScalar(prhs[2]);
			case 2:
				if (!mxIsChar(prhs[1]))
					mexErrMsgTxt("The first argument after 'open' command must be string, representing filename");
				else
					FileName = mxArrayToString(prhs[1]);
			    break;
			case 1:
				mxArray *mxon[2];
				mxArray *mxop[2] = {mxCreateString("*.*"),mxCreateString("Open Video File")};
				mexCallMATLAB(2,mxon,2,mxop,"uigetfile");
				mxArray *mxcn[1];
				mxArray *mxcp[2] = {mxon[1],mxon[0]};
				mexCallMATLAB(1,mxcn,2,mxcp,"fullfile");
				FileName = mxArrayToString(mxcn[0]); 
				mxDestroyArray(mxon[0]);
				mxDestroyArray(mxon[1]);
				mxDestroyArray(mxop[0]);
				mxDestroyArray(mxop[1]);
				mxDestroyArray(mxcn[0]);
				break;
			default:
				mexErrMsgTxt("'openvideo' command can have 0~4 additional arguments.");
		}
		if (!strcasecmp(pixfmt_str,"GRAY"))		    {out_channel = 4; out_pix_fmt = AV_PIX_FMT_GRAY8;}
		else if (!strcasecmp(pixfmt_str,"R"))		{out_channel = 0; out_pix_fmt = AV_PIX_FMT_RGB24;}
		else if (!strcasecmp(pixfmt_str,"G"))		{out_channel = 1; out_pix_fmt = AV_PIX_FMT_RGB24;}
		else if (!strcasecmp(pixfmt_str,"B"))		{out_channel = 2; out_pix_fmt = AV_PIX_FMT_RGB24;}
		else if (!strcasecmp(pixfmt_str,"RGB"))	    {out_channel = 3; out_pix_fmt = AV_PIX_FMT_RGB24;}
		else if (!strcasecmp(pixfmt_str,"Y"))		{out_channel = 4; out_pix_fmt = AV_PIX_FMT_YUV420P;}
		else if (!strcasecmp(pixfmt_str,"U"))		{out_channel = 5; out_pix_fmt = AV_PIX_FMT_YUV444P;}
		else if (!strcasecmp(pixfmt_str,"V"))		{out_channel = 6; out_pix_fmt = AV_PIX_FMT_YUV444P;}
		else if (!strcasecmp(pixfmt_str,"YUV"))	    {out_channel = 7; out_pix_fmt = AV_PIX_FMT_YUV444P;}
		else									    mexErrMsgTxt("Invalid output pixel format.");
		GS_Open(FileName, w, h, out_pix_fmt);
		mxFree(FileName);
		if (nrhs>=5) mxFree(pixfmt_str);
	}else if (!strncasecmp(FunctionName, "getprop", 3)) {
		if (nlhs != 2)
			mexErrMsgTxt("'getprop' command must have 2 output arguments");
		if (!FormatCtx) {
			*Status = -4;
			plhs[1] = mxCreateNumericMatrix(1,1,mxDOUBLE_CLASS,mxREAL);
			*(mxGetPr(plhs[1])) = -1;
		}else {
			const char * propname[] = {"FrameRate","Height","Width","Duration","TotalFrames",
			                           "NextFrame","FileName","BitRate","AspectRatio"};
			plhs[1] = mxCreateStructMatrix(1, 1, sizeof(propname)/sizeof(propname[0]), propname);
			mxArray *mxparaval;
			// frame rate
			mxparaval = mxCreateNumericMatrix(1,1,mxDOUBLE_CLASS,mxREAL);
			*(mxGetPr(mxparaval)) = av_q2d(Stream->avg_frame_rate);
			mxSetFieldByNumber(plhs[1], 0, 0, mxparaval);
			if (*(mxGetPr(mxparaval))<=0) mexWarnMsgTxt("FrameRate may not be valid.");
			// height
			mxparaval = mxCreateNumericMatrix(1,1,mxDOUBLE_CLASS,mxREAL);
			*(mxGetPr(mxparaval)) = (double) dst_h;
			mxSetFieldByNumber(plhs[1], 0, 1, mxparaval);
			if (*(mxGetPr(mxparaval))<=0) mexWarnMsgTxt("Height may not be valid.");
			// width
			mxparaval = mxCreateNumericMatrix(1,1,mxDOUBLE_CLASS,mxREAL);
			*(mxGetPr(mxparaval)) = (double) dst_w;
			mxSetFieldByNumber(plhs[1], 0, 2, mxparaval);
			if (*(mxGetPr(mxparaval))<=0) mexWarnMsgTxt("Width may not be valid.");
			// duration
			mxparaval = mxCreateNumericMatrix(1,1,mxDOUBLE_CLASS,mxREAL);
			*(mxGetPr(mxparaval)) = (double) Stream->duration * av_q2d(Stream->time_base);
			mxSetFieldByNumber(plhs[1], 0, 3, mxparaval);
			if (*(mxGetPr(mxparaval))<=0) mexWarnMsgTxt("Duration may not be valid.");
			// total frames
			mxparaval = mxCreateNumericMatrix(1,1,mxDOUBLE_CLASS,mxREAL);
			AVRational frametime = av_inv_q(Stream->avg_frame_rate);
			if (Stream->nb_frames > 0) *(mxGetPr(mxparaval)) = (double) Stream->nb_frames;
			else *(mxGetPr(mxparaval)) = (double) av_rescale_q(Stream->duration, Stream->time_base, frametime);
			mxSetFieldByNumber(plhs[1], 0, 4, mxparaval);
			if (*(mxGetPr(mxparaval))<=0) mexWarnMsgTxt("TotalFrames may not be valid.");
			// next frame
			mxparaval = mxCreateNumericMatrix(1,1,mxDOUBLE_CLASS,mxREAL);
			if (CodecCtx->pts_correction_last_pts >= Stream->start_time && 
			    CodecCtx->pts_correction_last_pts < Stream->duration + Stream->start_time)
				*(mxGetPr(mxparaval)) = (double) av_rescale_q(CodecCtx->pts_correction_last_pts - Stream->start_time, 
												 Stream->time_base, frametime) + 2;
			else if(CodecCtx->pts_correction_last_pts == Stream->duration + Stream->start_time) {
				*(mxGetPr(mxparaval)) = -1;
				mexWarnMsgTxt("NextFrame is invalid because it is the end of the video.");
			}else *(mxGetPr(mxparaval)) = 1;
			mxSetFieldByNumber(plhs[1], 0, 5, mxparaval);
			// file name
			mxSetFieldByNumber(plhs[1], 0, 6, mxCreateString(FormatCtx->filename));
			// bit rate
			mxparaval = mxCreateNumericMatrix(1,1,mxDOUBLE_CLASS,mxREAL);
			*(mxGetPr(mxparaval)) = (double) FormatCtx->bit_rate;
			mxSetFieldByNumber(plhs[1], 0, 7, mxparaval);
			if (*(mxGetPr(mxparaval))<=0) mexWarnMsgTxt("BitRate may not be valid.");
			// aspect ratio
			mxparaval = mxCreateNumericMatrix(2,1,mxDOUBLE_CLASS,mxREAL);
			(mxGetPr(mxparaval))[0] = Stream->display_aspect_ratio.num;
			(mxGetPr(mxparaval))[1] = Stream->display_aspect_ratio.den;
			mxSetFieldByNumber(plhs[1], 0, 8, mxparaval);
			if ((mxGetPr(mxparaval)[0])<=0 || (mxGetPr(mxparaval)[1])<=0) mexWarnMsgTxt("AspectRatio may not be valid.");
		}
	}else if (!strncasecmp(FunctionName, "readframe", 4)) {
		if (nlhs != 2)
			mexErrMsgTxt("'readframe' command must have 2 output arguments");
		// read, decode and rescale frame
		if (!FormatCtx) {
			*Status = -4;
			plhs[1] = mxCreateNumericMatrix(1,1,mxDOUBLE_CLASS,mxREAL);
			*(mxGetPr(plhs[1])) = -1;
		}else {
			*Status = GS_Read();
			if (*Status > -1) {
				sws_scale(SwsCtx, frame->data, frame->linesize, 0, src_h, rawdata, rawdata_linesize);
				plhs[1] = GS_memcpy();
			}else {
				plhs[1] = mxCreateNumericMatrix(1,1,mxDOUBLE_CLASS,mxREAL);
				*(mxGetPr(plhs[1])) = -1;
			}
		}
	}else if (!strncasecmp(FunctionName, "pickframe", 4)) {
		if (nlhs != 2)
			mexErrMsgTxt("'pickframe' command must have 2 output arguments");
		if (nrhs != 2)
			mexErrMsgTxt("'pickframe' command must have 1 input arguments");
		if (!mxIsNumeric(prhs[1]))
			mexErrMsgTxt("The first argument after 'pickframe' command must be numeric, representing frame number");
		// seek, read, decode and rescale frame
		if (!FormatCtx) {
			*Status = -4;
			plhs[1] = mxCreateNumericMatrix(1,1,mxDOUBLE_CLASS,mxREAL);
			*(mxGetPr(plhs[1])) = -1;
		}else {
			int64_t FrameNum = (int64_t) mxGetScalar(prhs[1]);
			if (CodecCtx->codec_id == AV_CODEC_ID_H264)
				*Status = GS_Pick(((FrameNum-14)>0)?(FrameNum-14):1, FrameNum, 0);
			else
				*Status = GS_Pick(FrameNum, FrameNum, 0);
			if (*Status > -1) {
				sws_scale(SwsCtx, frame->data, frame->linesize, 0, src_h, rawdata, rawdata_linesize);
				plhs[1] = GS_memcpy();
			}else {
				plhs[1] = mxCreateNumericMatrix(1,1,mxDOUBLE_CLASS,mxREAL);
				*(mxGetPr(plhs[1])) = -1;
			}
		}
	}else if (!strncasecmp(FunctionName, "seekframe", 4)) {
		if (nrhs != 2)
			mexErrMsgTxt("'seekframe' command must have 1 input arguments");
		if (!mxIsNumeric(prhs[1]))
			mexErrMsgTxt("The first argument after 'seekframe' command must be numeric, representing frame number");
		// seek to frame
		if (!FormatCtx) *Status = -4;
		else {
			int64_t FrameNum = (int64_t) mxGetScalar(prhs[1])-1;
			if (FrameNum == 0)
				if (av_seek_frame(FormatCtx, StreamIdx, Stream->first_dts, AVSEEK_FLAG_BACKWARD) < 0)
					*Status = -3;
			    else {
					avcodec_flush_buffers(CodecCtx);
					*Status = (Stream->first_dts-Stream->start_time) * av_q2d(Stream->time_base);
				}
			else {
				if (CodecCtx->codec_id == AV_CODEC_ID_H264)
					*Status = GS_Pick(((FrameNum-14)>0)?(FrameNum-14):1, FrameNum, 0);
				else
					*Status = GS_Pick(FrameNum, FrameNum, 0);
			}
		}
	}else if (!strncasecmp(FunctionName, "closevideo", 5)) {
		GS_Close();
	}else {
		mexErrMsgTxt("Invalid ffmat command");
	}
	mxFree(FunctionName);
	return;
}