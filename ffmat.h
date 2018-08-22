// ffmpeg headers
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
#include "libavutil/hwcontext.h"
#include "libavutil/opt.h"
// other headers
#include <string.h>
#include "mex.h"
#include "crossplatform.h"
// Error Code
#define FFMAT_ERR -1
#define FFMAT_ERR_OPEN -1
#define FFMAT_ERR_READ -2
#define FFMAT_ERR_FEED -3
#define FFMAT_ERR_DECODE -4
#define FFMAT_ERR_HWT -5
#define FFMAT_ERR_SEEK -6
#define FFMAT_ERR_CLOSE -7
#define FFMAT_ERR_CLOSED -8
// demuxing and decoding contexts
static AVFormatContext *FormatCtx = NULL;
static AVCodecContext *CodecCtx = NULL;
static AVStream *Stream = NULL;
static struct SwsContext *SwsCtx = NULL;
static int StreamIdx = -1;
static int src_w, src_h, dst_w, dst_h;
static enum AVPixelFormat dst_pix_fmt;
static int64_t b, c;
static int steps = 24;
// hwaccel related
static bool HwAccel = true;
static enum AVPixelFormat HWPixFmt;
// buffers
static mxArray *mxin[2];
static uint8_t *rawdata[4] = {NULL};
static int rawdata_linesize[4] = {0};
static AVFrame *frame = NULL;
static AVFrame *hwframe = NULL;
static AVPacket *pkt;
// functions
static enum AVPixelFormat get_hw_format(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts);
int GS_Open(char *filename);
int GS_Open_sw(char *filename);
int GS_Load();
double GS_Read();
double GS_Read_sw();
double GS_Read_hw();
double GS_Pick(int64_t SeekFrame, int64_t TargetFrame, int FailCount);
int GS_Close();
void mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[]);