# FFMAT

**FFMAT**, which is short for "*ffmpeg for MATLAB*", is a fast and easy-to-use video decoding function for MATLAB.

- [FFMAT](#ffmat)
    - [Meta info](#meta-info)
    - [Change Log](#change-log)
    - [Compilation Guide](#compilation-guide)
        - [Requirements](#requirements)
        - [Compilation Steps](#compilation-steps)
    - [Usage](#usage)
        - [General Syntax](#general-syntax)
        - [Command List](#command-list)
    - [Acknowledgement](#acknowledgement)

## Meta info

- ffmpeg version: 4.0.2 64bit
- ffmpeg API version: 4.0
- Tested C/C++ compliler
    - visual studio 2017 (Windows 10)
- Tested MATLAB version
    - R2018a 64bit on Windows

## Change Log
- version 4.1
    - significantly improved performance using faster bilinear resize algorism
    - fix bug: int64 type conversion
    - fix bug: color range mismatch using swscale
- version 4.0
    - allow fallback to software mode, if hardware acceleration fails
    - significantly improved speed, achieved by reduced unnecessary memcopy
    - no longer support for pixel formats: "R", "G", "B", "Y", "U", "V"
      currently valid pixel formats: "GRAY", "RGB", "YUV"
    - add "PixFmt" to property list
    - initial optimization for HEVC (h.265)
    - fix fatal bug reading last frame
    - fix bug calculating TotalFrames 
- version 3.1
    - minor improvements during sparsely picking
    - fix bug: fix crash after multiple 'closevideo' calls
- version 3.0
    - update to ffmpeg 4.0
    - support hardware acceleration: 
        - cuda (NVIDIA), 
        - d3d11va (DirectX11), 
        - dxva2 (DirectX9)
	- openvideo command can be called without any additional parameters
	- fix bug: memory overflow
	- fix bug: Height and width props reflect the real output dimensions.
- version 2.1
    - better performance during sparsely picking
    - fix building errors in macOS/Linux environments
- version 2.0
    - change command name (previous command names are still valid)
        - open->openvideo
        - get->getprop
        - read->readframe
        - pick->pickframe
        - close->closevideo
    - add command: seekframe
    - add AspectRatio property
    - fix bug: frame number mismatch
    - fix bug: time stamp may be negative
- version 1.1
    - add output pixel format option (Gray/RGB/YUV)
    - fix some mistakes in help file

## Compilation Guide

### Requirements

1. Windows/macOS/Linux 64bit
2. MATLAB 64bit (2014+ prefered)
3. mex compatiable C/C++ compiler, [see here for detail](https://www.mathworks.com/support/compilers.html)
4. Precompiled ffmpeg dev package, extracted in a folder like "*dev*"
5. Precompiled ffmpeg shared build package, extracted in a folder like "*shared*"

Note: Precompiled ffmpeg can be downloaded [here](https://ffmpeg.zeranoe.com/builds/), or [compiled from ffmpeg source](https://trac.ffmpeg.org/wiki/CompilationGuide).

### Compilation Steps

1. Git clone this repository to a folder like "*ffmat*".
2. Copy folders "*include*" and "*lib*", from "*dev*" to "*ffmat*".
3. In MATLAB, change current directory to "*ffmat*", Run "*build_script.m*". 
4. Copy any files whose names contain "*avcodec*","*avformat*","*avutil*","*swscale*","*swresample*", from "*shared/bin/*" to "*ffmat*"
5. Run "*ffmat_demo.m*" to test.

## Usage

### General Syntax

```Matlab
[output1, output2, ...] = ffmat('command',argument1,argument2,...);
```

For different command, argument number and output number may be variant.

### Command List

- openvideo
    - syntax
        - `Status = ffmat('openvideo');`
        - `Status = ffmat('openvideo',filename);`
        - `Status = ffmat('openvideo',filename,outputW);`
        - `Status = ffmat('openvideo',filename,outputW,outputH);`
        - `Status = ffmat('openvideo',filename,outputW,outputH,outputPixFmt);`
        - `Status = ffmat('openvideo',filename,outputW,outputH,outputPixFmt,EnableHWAccel);`
    - function
        - Open video file, should be called before using other commands.
    - input arguments
        - *filename* is the whole videofile name, and should be string. 
        - *outputW*, *outputH* are the output resolutions, and should be numeric. If one of those are not given or zero, default value(s) will be applied. 
        - *outputPixFmt* is the output pixel format, and should be string. 'Gray','RGB','YUV' are all the valid value currently. Default is 'Gray'.
        - *EnableHWAccel* denotes whether to enable hardware acceleration. Default is true.
    - return
        - *Status* is 1 if success. Otherwise it is negative.
- getprop
    - syntax
        - `[Status, Property] = ffmat('getprop');`
    - function
        - Get the property values of the opened video file.
    - return
        - *Status* is 1 if success. Otherwise it is negative. 
        - *Property* is a 1x1 structure array, while its fields contains the property values of the video file.
- readframe
    - syntax
        - `[TimeStamp, RawImage] = ffmat('readframe');`
    - function
        - Read and decode the next frame.
    - return
        - *TimeStamp* is the timestamp of the returned frame in seconds, if success. Otherwise it is negative. -1 means end of the file. -2 means decoding error. 
        - *RawImage* is a HxW/HxWx3 uint8 array, which is the decoded image, if success. Otherwise it is empty.
- pickframe
    - syntax
        - `[TimeStamp, RawImage] = ffmat('pickframe',frameidx);`
    - function
        - Pick and decode one specific frame. This will also lead to the consequence that the next "readframe" call will return the frame next to the picked frame.
    - input arguments
        - frameidx is the index of the required frame. The frame index starts from 1, not 0.
    - return
        - *TimeStamp* is the timestamp of the returned frame in seconds, if success. Otherwise it is negative. -1 means end of the file. -2 means decoding error. -3 means seeking error.
        - *RawImage* is an HxW/HxWx3 uint8 array, which is the decoded image, if success. Otherwise it is empty.
- seekframe
    - syntax
        - `TimeStamp = ffmat('seekframe',frameidx);`
    - function
        - Seek to specific frame so that the next "readframe" will return the corresponding frame.
    - input arguments
        - *frameidx* is the index of the frame that the next "readframe" call will return. The frame index starts from 1, not 0.
    - return
        - *TimeStamp* is the current timestamp after seeking in seconds, if success. Please note that this timestamp should be smaller than the timestamp of the seeked frame. It is negative, if failed. -1 means end of the file. -2 means decoding error. -3 means seeking error.
- closevideo
    - syntax
        - `Status = ffmat('closevideo');`
    - function
        - Close the video file and release the memory.
    - return
        - *Status* is 1 if success. Otherwise it is negative.

## Acknowledgement

Zhu Lab @ IBP
