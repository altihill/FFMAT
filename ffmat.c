#include "ffmat.h"

int GS_Open_sw(char *filename) {
	AVCodec *pCodec = NULL;
    AVCodecParameters *pCodecPara = NULL;
    int i, j;
	uint8_t *pData = NULL;
	mwSize dims[3] = {0};
	// open input file, and allocate format context
	if (avformat_open_input(&FormatCtx, filename, NULL, NULL) < 0) {
		mexWarnMsgTxt("Failed to open video file");
		goto fail;
	}
	// retrieve stream information
	if (avformat_find_stream_info(FormatCtx, NULL) < 0) {
		mexWarnMsgTxt("Failed to retrieve video stream information");
		goto fail;
	}
	// find video stream
	StreamIdx = av_find_best_stream(FormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &pCodec, 0);
	if (StreamIdx < 0) {
		mexWarnMsgTxt("Failed to find valid video stream");
		goto fail;
	}
	// initiate codec context
	CodecCtx = avcodec_alloc_context3(pCodec);
	if (!CodecCtx) {
		mexWarnMsgTxt("Failed to allocate codec context");
		goto fail;
	}
    Stream = FormatCtx->streams[StreamIdx];
	pCodecPara = Stream->codecpar;
	if (avcodec_parameters_to_context(CodecCtx, pCodecPara) < 0) {
		mexWarnMsgTxt("Failed to fill codec context");
		goto fail;
	}
    av_opt_set_int(CodecCtx, "refcounted_frames", 1, 0);
    // open codec
	if (avcodec_open2(CodecCtx, pCodec, NULL) < 0) {
		mexWarnMsgTxt("Failed to open codec");
		goto fail;
	}
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
	if (!pkt) {
		mexWarnMsgTxt("Failed to allocate input buffer");
		goto fail;
	}
    pkt->data = NULL;
    pkt->size = 0;
	// decoded buffer
	frame = av_frame_alloc();
	if (!frame) {
		mexWarnMsgTxt("Failed to allocate decode buffer");
		goto fail;
	}
	// output buffer
	src_w = CodecCtx->width;
	src_h = CodecCtx->height;
	dst_w = dst_w == 0 ? src_w : dst_w;
	dst_h = dst_h == 0 ? src_h : dst_h;
	switch (dst_pix_fmt) {
		case AV_PIX_FMT_GRAY8:
			mxin[0] = mxCreateNumericMatrix(dst_w,dst_h,mxUINT8_CLASS,mxREAL);
			pData = (uint8_t *)mxGetData(mxin[0]);
			rawdata[0] = pData;
			rawdata_linesize[0] = dst_w;
			mxin[1] = mxCreateDoubleMatrix(2,1,mxREAL);
			mxGetPr(mxin[1])[0] = 2;
			mxGetPr(mxin[1])[1] = 1;
			break;
		case AV_PIX_FMT_RGB24:
			dims[0] = 3;
			dims[1] = dst_w;
			dims[2] = dst_h;
			mxin[0] = mxCreateNumericArray(3,dims,mxUINT8_CLASS,mxREAL);
			pData = (uint8_t *)mxGetData(mxin[0]);
			rawdata[0] = pData;
			rawdata_linesize[0] = 3*dst_w;
			mxin[1] = mxCreateDoubleMatrix(3,1,mxREAL);
			mxGetPr(mxin[1])[0] = 3;
			mxGetPr(mxin[1])[1] = 2;
			mxGetPr(mxin[1])[2] = 1;
			break;
		case AV_PIX_FMT_YUV444P:
			dims[0] = dst_w;
			dims[1] = dst_h;
			dims[2] = 3;
			mxin[0] = mxCreateNumericArray(3,dims,mxUINT8_CLASS,mxREAL);
			pData = (uint8_t *)mxGetData(mxin[0]);
			rawdata[0] = pData;
			rawdata[1] = pData + dst_w*dst_h;
			rawdata[2] = pData + 2*dst_w*dst_h;
			rawdata_linesize[0] = dst_w;
			rawdata_linesize[1] = dst_w;
			rawdata_linesize[2] = dst_w;
			mxin[1] = mxCreateDoubleMatrix(3,1,mxREAL);
			mxGetPr(mxin[1])[0] = 2;
			mxGetPr(mxin[1])[1] = 1;
			mxGetPr(mxin[1])[2] = 3;
	}
	mexMakeArrayPersistent(mxin[0]);
	mexMakeArrayPersistent(mxin[1]);
    // create swscale ctx
	SwsCtx = sws_getContext(src_w, src_h, CodecCtx->pix_fmt, 
							dst_w, dst_h, dst_pix_fmt, 
							SWS_BILINEAR, NULL, NULL, NULL);
	if (!SwsCtx) {
		mexWarnMsgTxt("Impossible to convert source to target pix_fmt");
		goto fail;
	}
    // for pts calculation
    b = Stream->avg_frame_rate.den * Stream->time_base.den;
	c = Stream->avg_frame_rate.num * Stream->time_base.num;
	if (CodecCtx->refs && CodecCtx->gop_size)
        steps = CodecCtx->refs * CodecCtx->gop_size;
    mexLock();
    return 1;

	fail:
	GS_Close();
	return FFMAT_ERR_OPEN;
}

static enum AVPixelFormat get_hw_format(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts) {
    const enum AVPixelFormat *p;
    for (p = pix_fmts; *p != -1; p++)
        if (*p == HWPixFmt) return *p;
    mexWarnMsgTxt("Failed to get HW surface format");
    return AV_PIX_FMT_NONE;
}

int GS_Load() {
	int ValidRead, ValidSend;
	if (pkt->data) {
		ValidSend = avcodec_send_packet(CodecCtx, pkt);
		if (ValidSend == 0)  {av_packet_unref(pkt); return 1;}
        else if (ValidSend == AVERROR(EAGAIN))		return 1;
		else if (ValidSend < 0)			            return FFMAT_ERR_FEED;
	}
	ValidRead = av_read_frame(FormatCtx, pkt);
	if (ValidRead < 0) return FFMAT_ERR_READ;
    else if (pkt->stream_index != StreamIdx) av_packet_unref(pkt);
	return GS_Load();
}

double GS_Read_sw() {
	int GotFrame, ValidLoad;
	ValidLoad = GS_Load();
	if (ValidLoad > FFMAT_ERR){
		GotFrame = avcodec_receive_frame(CodecCtx, frame);
		if (GotFrame == 0) 
			return (frame->pts - Stream->start_time) * av_q2d(Stream->time_base);
		else if (GotFrame == AVERROR(EAGAIN)) return GS_Read_sw();
		else return FFMAT_ERR_DECODE;
	}else if(ValidLoad == FFMAT_ERR_READ){
		GotFrame = avcodec_receive_frame(CodecCtx, frame);
		if (GotFrame == 0)
			return (frame->pts - Stream->start_time) * av_q2d(Stream->time_base);
		else if (GotFrame == AVERROR(EAGAIN)) return ValidLoad;
		else return FFMAT_ERR_DECODE;
	}else return ValidLoad;
}

double GS_Read_hw() {
	int GotFrame, ValidLoad;
	ValidLoad = GS_Load();
	if (ValidLoad > FFMAT_ERR){
		GotFrame = avcodec_receive_frame(CodecCtx, hwframe);
		if (GotFrame == 0) {
            if (av_hwframe_transfer_data(frame, hwframe, 0) < 0) return FFMAT_ERR_HWT;
            else frame->pts = hwframe->pts;
			return (frame->pts - Stream->start_time) * av_q2d(Stream->time_base);
		}else if (GotFrame == AVERROR(EAGAIN)) return GS_Read_hw();
		else return FFMAT_ERR_DECODE;
	}else if(ValidLoad == FFMAT_ERR_READ){
		GotFrame = avcodec_receive_frame(CodecCtx, hwframe);
		if (GotFrame == 0) {
            if (av_hwframe_transfer_data(frame, hwframe, 0) < 0) return FFMAT_ERR_HWT;
            else frame->pts = hwframe->pts;
			return (frame->pts - Stream->start_time) * av_q2d(Stream->time_base);
		}else if (GotFrame == AVERROR(EAGAIN)) return ValidLoad;
		else return FFMAT_ERR_DECODE;
	}else return ValidLoad;
}

double GS_Read() {
	if (HwAccel) return GS_Read_hw();
	else return GS_Read_sw();
}

double GS_Pick(int64_t SeekFrame, int64_t TargetFrame, int FailCount) {
	double dret;
	int sret;
	int64_t SeekPts,TargetPts;
	// caculate pts of the targetframe
    if (SeekFrame<1 || TargetFrame<1) return FFMAT_ERR_SEEK;
	TargetPts = av_rescale(TargetFrame-1,b,c) + Stream->start_time;
	// seek to the seekframe
	if (TargetPts == frame->pts)
		return (frame->pts - Stream->start_time) * av_q2d(Stream->time_base);
	else if (TargetPts<frame->pts || TargetPts>frame->pts+av_rescale(steps,b,c)) { 
		if (SeekFrame<steps) SeekPts = Stream->first_dts;
    	else SeekPts = av_rescale(SeekFrame-1,b,c) + Stream->start_time;
		sret = av_seek_frame(FormatCtx, StreamIdx, SeekPts, AVSEEK_FLAG_BACKWARD);
		if (sret < 0) return FFMAT_ERR_SEEK;
		av_packet_unref(pkt);
		avcodec_flush_buffers(CodecCtx);
	}
	// read and decode until targetframe
	do dret = GS_Read();
	while (dret > FFMAT_ERR && frame->pts < TargetPts);
	if (frame->pts == TargetPts) return dret;
    else if (FailCount<15) 
		return GS_Pick((SeekFrame-2)>0 ? (SeekFrame-2):1, TargetFrame, ++FailCount);
    else return FFMAT_ERR_SEEK;
}

int GS_Close() {
    if (mexIsLocked())				mexUnlock();
	else 							return 1;
	mxDestroyArray(mxin[0]);
	mxDestroyArray(mxin[1]);
    if (CodecCtx->hw_device_ctx)    av_buffer_unref(&CodecCtx->hw_device_ctx);
	if (CodecCtx) {
		pkt->data = NULL;
		pkt->size = 0;
		avcodec_send_packet(CodecCtx,pkt);
		avcodec_free_context(&CodecCtx);
	}
	if (FormatCtx)		            avformat_close_input(&FormatCtx);
	if (pkt)			            av_packet_free(&pkt);
	if (frame)			            av_frame_free(&frame);
    if (hwframe)		            av_frame_free(&hwframe);
	if (SwsCtx)		                {sws_freeContext(SwsCtx); SwsCtx = NULL;}
	return 1;
}

int GS_Open(char *filename) {
	// prefer software decoder
	if (!HwAccel) return GS_Open_sw(filename);
	// try hw decoder now
	AVCodec *pCodec = NULL;
    AVCodecParameters *pCodecPara = NULL;
    AVBufferRef *HWDevCtx = NULL;
    AVBufferRef *HWFrameCtx = NULL;
    enum AVPixelFormat *src_pix_fmts = NULL;
    enum AVPixelFormat src_pix_fmt;
    int i, j;
	uint8_t *pData = NULL;
	mwSize dims[3] = {0};
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
    if (i==sizeof(HWDevs)){
        mexWarnMsgTxt("No hardware acceleration device is found");
		goto fallback;
	}
	// open input file, and allocate format context
	if (avformat_open_input(&FormatCtx, filename, NULL, NULL) < 0) {
		mexWarnMsgTxt("Failed to open video file");
		goto fail;
	}
	// retrieve stream information
	if (avformat_find_stream_info(FormatCtx, NULL) < 0) {
		mexWarnMsgTxt("Failed to retrieve video stream information");
		goto fail;
	}
	// find video stream
	StreamIdx = av_find_best_stream(FormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &pCodec, 0);
	if (StreamIdx < 0) {
		mexWarnMsgTxt("Failed to find valid video stream");
		goto fail;
	}
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
    if (i==sizeof(HWDevs)) {
        mexWarnMsgTxt("Codec does not support existing hardware device(s)");
		goto fallback;
	}else {
        type = HWDevs[i];
        HWPixFmt = hw_pix_fmts[i];
    }
	// initiate codec context
	CodecCtx = avcodec_alloc_context3(pCodec);
	if (!CodecCtx) {
		mexWarnMsgTxt("Failed to allocate codec context");
		goto fail;
	}
    Stream = FormatCtx->streams[StreamIdx];
	pCodecPara = Stream->codecpar;
	if (avcodec_parameters_to_context(CodecCtx, pCodecPara) < 0) {
		mexWarnMsgTxt("Failed to fill codec context");
		goto fail;
	}
    CodecCtx->get_format = get_hw_format;
    av_opt_set_int(CodecCtx, "refcounted_frames", 1, 0);
	// open hardware device
    if (av_hwdevice_ctx_create(&HWDevCtx, type, NULL, NULL, 0) < 0)	{
        mexWarnMsgTxt("Failed to open hardware decoder");
		goto fallback;
	}
    CodecCtx->hw_device_ctx = av_buffer_ref(HWDevCtx);
    // open codec
	if (avcodec_open2(CodecCtx, pCodec, NULL) < 0) {
		mexWarnMsgTxt("Failed to open codec");
		goto fail;
	}
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
	if (!pkt) {
		mexWarnMsgTxt("Failed to allocate input buffer");
		goto fail;
	}
    pkt->data = NULL;
    pkt->size = 0;
	// decoded buffer
	frame = av_frame_alloc();
	if (!frame) {
		mexWarnMsgTxt("Failed to allocate decode buffer");
		goto fail;
	}
    hwframe = av_frame_alloc();
	if (!frame) {
		mexWarnMsgTxt("Failed to allocate hardware decode buffer");
		goto fallback;
	}
	// output buffer
	src_w = CodecCtx->width;
	src_h = CodecCtx->height;
	dst_w = dst_w == 0 ? src_w : dst_w;
	dst_h = dst_h == 0 ? src_h : dst_h;
	switch (dst_pix_fmt) {
		case AV_PIX_FMT_GRAY8:
			mxin[0] = mxCreateNumericMatrix(dst_w,dst_h,mxUINT8_CLASS,mxREAL);
			pData = (uint8_t *)mxGetData(mxin[0]);
			rawdata[0] = pData;
			rawdata_linesize[0] = dst_w;
			mxin[1] = mxCreateDoubleMatrix(2,1,mxREAL);
			mxGetPr(mxin[1])[0] = 2;
			mxGetPr(mxin[1])[1] = 1;
			break;
		case AV_PIX_FMT_RGB24:
			dims[0] = 3;
			dims[1] = dst_w;
			dims[2] = dst_h;
			mxin[0] = mxCreateNumericArray(3,dims,mxUINT8_CLASS,mxREAL);
			pData = (uint8_t *)mxGetData(mxin[0]);
			rawdata[0] = pData;
			rawdata_linesize[0] = 3*dst_w;
			mxin[1] = mxCreateDoubleMatrix(3,1,mxREAL);
			mxGetPr(mxin[1])[0] = 3;
			mxGetPr(mxin[1])[1] = 2;
			mxGetPr(mxin[1])[2] = 1;
			break;
		case AV_PIX_FMT_YUV444P:
			dims[0] = dst_w;
			dims[1] = dst_h;
			dims[2] = 3;
			mxin[0] = mxCreateNumericArray(3,dims,mxUINT8_CLASS,mxREAL);
			pData = (uint8_t *)mxGetData(mxin[0]);
			rawdata[0] = pData;
			rawdata[1] = pData + dst_w*dst_h;
			rawdata[2] = pData + 2*dst_w*dst_h;
			rawdata_linesize[0] = dst_w;
			rawdata_linesize[1] = dst_w;
			rawdata_linesize[2] = dst_w;
			mxin[1] = mxCreateDoubleMatrix(3,1,mxREAL);
			mxGetPr(mxin[1])[0] = 2;
			mxGetPr(mxin[1])[1] = 1;
			mxGetPr(mxin[1])[2] = 3;
	}
	mexMakeArrayPersistent(mxin[0]);
	mexMakeArrayPersistent(mxin[1]);
	// negotiate transfer pixfmt
    if (GS_Read_hw() <= -1) goto fallback;
    if (av_hwframe_transfer_get_formats(hwframe->hw_frames_ctx,
                            AV_HWFRAME_TRANSFER_DIRECTION_FROM,
                            &src_pix_fmts,0) < 0) {
        mexWarnMsgTxt("Failed to get valid source pixel format");
		goto fallback;
	}
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
    if (av_seek_frame(FormatCtx, StreamIdx, Stream->first_dts, AVSEEK_FLAG_BACKWARD) < 0) {
        mexWarnMsgTxt("Failed to seek to the very beginning");
		goto fail;
	}
    else avcodec_flush_buffers(CodecCtx);
    // create swscale ctx
	SwsCtx = sws_getContext(src_w, src_h, src_pix_fmt, 
							dst_w, dst_h, dst_pix_fmt, 
							SWS_BILINEAR, NULL, NULL, NULL);
	if (!SwsCtx) {
		mexWarnMsgTxt("Impossible to convert source to target pix_fmt");
		goto fail;
	}
    // for pts calculation
    b = Stream->avg_frame_rate.den * Stream->time_base.den;
	c = Stream->avg_frame_rate.num * Stream->time_base.num;
	if (CodecCtx->refs && CodecCtx->gop_size)
        steps = CodecCtx->refs * CodecCtx->gop_size;
    mexLock();
    return 1;

	fallback:
	mexWarnMsgTxt("Hardware acceleration failed, fallback to software mode");
	GS_Close();
	HwAccel = false;
	return GS_Open_sw(filename);

	fail:
	GS_Close();
	return FFMAT_ERR_OPEN;
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
		char *FileName;
		char *pixfmt_str = "GRAY";
		if (FormatCtx) {
			GS_Close();
			mexWarnMsgTxt("Previous video has been closed.");
		}
		// parse input parameters
		switch (nrhs) {
			case 6:
				if (!mxIsLogicalScalar (prhs[5]))
					mexErrMsgTxt("The fifth argument after 'open' command must be logical scalar, denoting whether to use hardware acceleration");
				else
					HwAccel = (bool) *mxGetLogicals(prhs[5]);
			case 5:
				if (!mxIsChar(prhs[4]))
					mexErrMsgTxt("The fourth argument after 'open' command must be string, denoting output pixel format");
				else
					pixfmt_str = mxArrayToString(prhs[4]);
			case 4:
				if (!mxIsNumeric(prhs[3]))
					mexErrMsgTxt("The third argument after 'open' command must be numeric type, denoting height of the output frame");
				else
					dst_h = (int) mxGetScalar(prhs[3]);
			case 3:
				if (!mxIsNumeric(prhs[2]))
					mexErrMsgTxt("The second argument after 'open' command must be numeric type, denoting width of the output frame");
				else
					dst_w = (int) mxGetScalar(prhs[2]);
			case 2:
				if (!mxIsChar(prhs[1]))
					mexErrMsgTxt("The first argument after 'open' command must be string, denoting filename");
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
		if (!strcasecmp(pixfmt_str,"GRAY"))		    dst_pix_fmt = AV_PIX_FMT_GRAY8;
		else if (!strcasecmp(pixfmt_str,"RGB"))	    dst_pix_fmt = AV_PIX_FMT_RGB24;
		else if (!strcasecmp(pixfmt_str,"YUV"))	    dst_pix_fmt = AV_PIX_FMT_YUV444P;
		else									    mexErrMsgTxt("Invalid output pixel format.");
		*Status = GS_Open(FileName);
		mxFree(FileName);
		if (nrhs>=5) mxFree(pixfmt_str);
	}else if (!strncasecmp(FunctionName, "getprop", 3)) {
		if (nlhs != 2)
			mexErrMsgTxt("'getprop' command must have 2 output arguments");
		if (!FormatCtx) {
			*Status = FFMAT_ERR_CLOSED;
			plhs[1] = mxCreateNumericMatrix(0,0,mxDOUBLE_CLASS,mxREAL);
		}else {
			const char * propname[] = {"FrameRate","Height","Width","PixFmt","Duration","TotalFrames",
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
			// pixfmt
			switch (dst_pix_fmt) {
				case AV_PIX_FMT_GRAY8:
				mxSetFieldByNumber(plhs[1], 0, 3, mxCreateString("GRAY"));
				break;
				case AV_PIX_FMT_RGB24:
				mxSetFieldByNumber(plhs[1], 0, 3, mxCreateString("RGB"));
				break;
				case AV_PIX_FMT_YUV444P:
				mxSetFieldByNumber(plhs[1], 0, 3, mxCreateString("YUV"));
			}
			// duration
			mxparaval = mxCreateNumericMatrix(1,1,mxDOUBLE_CLASS,mxREAL);
			*(mxGetPr(mxparaval)) = (double) Stream->duration * av_q2d(Stream->time_base);
			mxSetFieldByNumber(plhs[1], 0, 4, mxparaval);
			if (*(mxGetPr(mxparaval))<=0) mexWarnMsgTxt("Duration may not be valid.");
			// total frames
			mxparaval = mxCreateNumericMatrix(1,1,mxDOUBLE_CLASS,mxREAL);
			AVRational frametime = av_inv_q(Stream->avg_frame_rate);
			if (Stream->nb_frames > 0) *(mxGetPr(mxparaval)) = (double) Stream->nb_frames;
			else *(mxGetPr(mxparaval)) = (double) av_rescale_q(Stream->duration, Stream->time_base, frametime)-1;
			mxSetFieldByNumber(plhs[1], 0, 5, mxparaval);
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
			mxSetFieldByNumber(plhs[1], 0, 6, mxparaval);
			// file name
			mxSetFieldByNumber(plhs[1], 0, 7, mxCreateString(FormatCtx->filename));
			// bit rate
			mxparaval = mxCreateNumericMatrix(1,1,mxDOUBLE_CLASS,mxREAL);
			*(mxGetPr(mxparaval)) = (double) FormatCtx->bit_rate;
			mxSetFieldByNumber(plhs[1], 0, 8, mxparaval);
			if (*(mxGetPr(mxparaval))<=0) mexWarnMsgTxt("BitRate may not be valid.");
			// aspect ratio
			mxparaval = mxCreateNumericMatrix(2,1,mxDOUBLE_CLASS,mxREAL);
			(mxGetPr(mxparaval))[0] = Stream->display_aspect_ratio.num;
			(mxGetPr(mxparaval))[1] = Stream->display_aspect_ratio.den;
			mxSetFieldByNumber(plhs[1], 0, 9, mxparaval);
			if ((mxGetPr(mxparaval)[0])<=0 || (mxGetPr(mxparaval)[1])<=0) mexWarnMsgTxt("AspectRatio may not be valid.");
		}
	}else if (!strncasecmp(FunctionName, "readframe", 4)) {
		if (nlhs != 2)
			mexErrMsgTxt("'readframe' command must have 2 output arguments");
		// read, decode and rescale frame
		if (!FormatCtx) {
			*Status = FFMAT_ERR_CLOSED;
			plhs[1] = mxCreateNumericMatrix(0,0,mxDOUBLE_CLASS,mxREAL);
		}else {
			*Status = GS_Read();
			if (*Status > FFMAT_ERR) {
				sws_scale(SwsCtx, frame->data, frame->linesize, 0, src_h, rawdata, rawdata_linesize);
				mexCallMATLAB(1,&plhs[1],2,mxin,"permute");
			}else {
				plhs[1] = mxCreateNumericMatrix(0,0,mxDOUBLE_CLASS,mxREAL);
			}
		}
	}else if (!strncasecmp(FunctionName, "pickframe", 4)) {
		if (nlhs != 2)
			mexErrMsgTxt("'pickframe' command must have 2 output arguments");
		if (nrhs != 2)
			mexErrMsgTxt("'pickframe' command must have 1 input arguments");
		if (!mxIsNumeric(prhs[1]))
			mexErrMsgTxt("The first argument after 'pickframe' command must be numeric, denoting frame number");
		// seek, read, decode and rescale frame
		if (!FormatCtx) {
			*Status = FFMAT_ERR_CLOSED;
			plhs[1] = mxCreateNumericMatrix(0,0,mxDOUBLE_CLASS,mxREAL);
		}else {
			int64_t FrameNum = (int64_t) mxGetScalar(prhs[1]);
			if (CodecCtx->codec_id == AV_CODEC_ID_H264 || CodecCtx->codec_id == AV_CODEC_ID_H265)
				*Status = GS_Pick(((FrameNum-steps)>0)?(FrameNum-steps):1, FrameNum, 0);
			else
				*Status = GS_Pick(FrameNum, FrameNum, 0);
			if (*Status > FFMAT_ERR) {
				sws_scale(SwsCtx, frame->data, frame->linesize, 0, src_h, rawdata, rawdata_linesize);
				mexCallMATLAB(1,&plhs[1],2,mxin,"permute");
			}else {
				plhs[1] = mxCreateNumericMatrix(0,0,mxDOUBLE_CLASS,mxREAL);
			}
		}
	}else if (!strncasecmp(FunctionName, "seekframe", 4)) {
		if (nrhs != 2)
			mexErrMsgTxt("'seekframe' command must have 1 input arguments");
		if (!mxIsNumeric(prhs[1]))
			mexErrMsgTxt("The first argument after 'seekframe' command must be numeric, denoting frame number");
		// seek to frame
		if (!FormatCtx) *Status = FFMAT_ERR_CLOSED;
		else {
			int64_t FrameNum = (int64_t) mxGetScalar(prhs[1])-1;
			if (FrameNum == 0)
				if (av_seek_frame(FormatCtx, StreamIdx, Stream->first_dts, AVSEEK_FLAG_BACKWARD) < 0)
					*Status = FFMAT_ERR_SEEK;
			    else {
					av_packet_unref(pkt);
					avcodec_flush_buffers(CodecCtx);
					*Status = (Stream->first_dts-Stream->start_time) * av_q2d(Stream->time_base);
				}
			else {
				if (CodecCtx->codec_id == AV_CODEC_ID_H264 || CodecCtx->codec_id == AV_CODEC_ID_H265)
					*Status = GS_Pick(((FrameNum-steps)>0)?(FrameNum-steps):1, FrameNum, 0);
				else
					*Status = GS_Pick(FrameNum, FrameNum, 0);
			}
		}
	}else if (!strncasecmp(FunctionName, "closevideo", 5)) {
		*Status = GS_Close();
	}else {
		mexErrMsgTxt("Invalid ffmat command");
	}
	mxFree(FunctionName);
	return;
}