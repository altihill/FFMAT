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

---

- ffmpeg version: 3.0.9 64bit
- ffmpeg API version: 3.0
- Tested C/C++ compliler
  - visual studio 2015/2017 (Windows 10)
  - Xcode 9.0 with clang (macOS 10.13)
  - gcc 5.4.0 (ubuntu 16.04)
- Tested MATLAB version
    - R2016a 64bit, R2015a 64bit, R2014a 64bit, R2012a 64bit on Windows
    - R2016b 64bit on macOS
    - R2016b 64bit on ubuntu

## Change Log

---

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

---

### Requirements

1. Windows/macOS/Linux 64bit
1. MATLAB 64bit (2014+ prefered)
1. mex compatiable C/C++ compiler, [see here for detail](https://www.mathworks.com/support/compilers.html)
1. all dependencies requried for building ffmpeg

### Compilation Steps

1. Build ffmpeg 3.0.9 with "*--enable-shared*" configuration.
    - Download Source:
    <https://www.ffmpeg.org/releases/ffmpeg-3.0.9.tar.gz>
    - ffmpeg compilation guide:
    <https://trac.ffmpeg.org/wiki/CompilationGuide>
    - P.S.
    Starting from ffmpeg 3.1, new decoding/encoding APIs are introduced. However, they actually perform slower than the legacy ones in my cases.
    FFMAT will not support newer ffmpeg until it matches 3.0.9 in speed.

1. Run "*build_script.m*" after modifying the sourcepath variable as the parent path of "*include*" & "*lib*" folders, in which headers and library files of ffmpeg are installed.
1. Run "*ffmat_demo.m*" to test.

## Usage

---

### General Syntax

`[output1, output2, ...] = ffmat('command',argument1,argument2,...);`

For different command, argument number and output number may be variant.

### Command List

- openvideo
  - syntax
    - `Status = ffmat('openvideo',filename);`
    - `Status = ffmat('openvideo',filename,outputW);`
    - `Status = ffmat('openvideo',filename,   0   ,outputH);`
    - `Status = ffmat('openvideo',filename,outputW,outputH);`
    - `Status = ffmat('openvideo',filename,   0   ,   0   ,outputPixFmt);`
    - `Status = ffmat('openvideo',filename,   0   ,outputH,outputPixFmt);`
    - `Status = ffmat('openvideo',filename,outputW,   0   ,outputPixFmt);`
    - `Status = ffmat('openvideo',filename,outputW,outputH,outputPixFmt);`
  - function
    - Open video file, should be called before using other commands.
  - input arguments
    - *filename* is the whole videofile name, and should be string. *outputW*, *outputH* are the output resolution, and should be numeric. If one of those are not given or zero, default value would be applied. *outputPixFmt* is the output pixel format, and should be string. **'Gray','R','G','B','RGB','Y','U','V','YUV'** are all the valid value currently.
  - return
    - *Status* is 1 if success. Otherwise it is negative.
- getprop
  - syntax
    - `[Status, Property] = ffmat('getprop');`
  - function
    - Get the property values of the opened video file.
  - return
    - *Status* is 1 if success. Otherwise it is negative. *Property* is a 1x1 structure array, while its fields contains the property values of the video file.
- readframe
  - syntax
    - `[TimeStamp, RawImage] = ffmat('readframe');`
  - function
    - Read and decode the next frame.
  - return
    - *TimeStamp* is the timestamp of the returned frame in seconds, if success. Otherwise it is negative. -1 means end of the file. -2 means decoding error. *RawImage* is a HxW/HxWx3 uint8 array, which is the decoded image.
- pickframe
  - syntax
    - `[TimeStamp, RawImage] = ffmat('pickframe',frameidx);`
  - function
    - Pick and decode one specific frame. This will also lead to the consequence that the next "readframe" call will return the frame next to the picked frame.
  - input arguments
    - frameidx is the index of the required frame. The frame index starts from 1, not 0.
  - return
    - *TimeStamp* is the timestamp of the returned frame in seconds, if success. Otherwise it is negative. -1 means end of the file. -2 means decoding error. -3 means seeking error. *RawImage* is an HxW/HxWx3 uint8 array, which is the decoded image.
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

---

Zhu Lab @ IBP