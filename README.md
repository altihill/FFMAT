# FFMAT
FFMAT, which is short for "ffmpeg for MATLAB", is a fast &amp; easy-to-use video decoding function for MATLAB.
----------------------------------------------------------------------------

Packed ffmpeg demuxing/decoding API for MATLAB

Version: 2.1

Date: 10/23/2017

Author: Gao Shan

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
----------------------------------------------------------------------------
General Syntax: 
	[output1, output2, ...] = ffmat('command',argument1,argument2,...);

For different command, argument number and output number may be variant.

Commands:
********'openvideo'********
	Status = ffmat('openvideo',filename);
	Status = ffmat('openvideo',filename,outputW);
	Status = ffmat('openvideo',filename,   0   ,outputH);
	Status = ffmat('openvideo',filename,outputW,outputH);
	Status = ffmat('openvideo',filename,   0   ,   0   ,outputPixFmt);
	Status = ffmat('openvideo',filename,   0   ,outputH,outputPixFmt);
	Status = ffmat('openvideo',filename,outputW,   0   ,outputPixFmt);
	Status = ffmat('openvideo',filename,outputW,outputH,outputPixFmt);
	function: Open video file, should be called before using other 
		  command.	
	input: filename is the whole videofile name, and should be string.
	       outputW, outputH are the output resolution, and should be
           numeric. If one of those are not given or zero, default value 
	       would be applied. outputPixFmt is the output pixel format, 
	       and should be string. 'Gray','R','G','B','RGB','Y','U','V',
	       'YUV' are all the valid value currently.
	return: Status is 1 if success. Otherwise it is negative.

********'getprop'********
	[Status, Property] = ffmat('getprop');
	function: Get property value of opened video file.
	return: Status is 1 if success. Otherwise it is negative.
		Property is a 1x1 structure array, while its fields contains 
		property values of the video file. Please note that for some
		formats, property valuse may not be 100% correct.

********'readframe'********
	[TimeStamp, RawImage] = ffmat('readframe');
	function: Read and decode the next frame.
	return: TimeStamp is the timestamp of the returned frame in seconds,
		if success. Otherwise it is negative. -1 means end of the 
		file. -2 means decoding error. RawImage is a HxW/HxWx3 uint8
		array, which is the decoded image. 

********'pickframe'********
	[TimeStamp, RawImage] = ffmat('pickframe',frameidx);
	function: Pick and decode one specific frame. This will also lead to 
		  the consequence that the next "readframe" call will return
		  the frame next to the picked frame.
	input: frameidx is the index of the required frame. The frame index
	       starts from 1, not 0.
	return: TimeStamp is the timestamp of the returned frame in seconds,
		if success. Otherwise it is negative. -1 means end of the 
		file. -2 means decoding error. -3 means seeking error. 
		RawImage is an HxW/HxWx3 uint8 array, which is the decoded image.

********'seekframe'********
	TimeStamp = ffmat('seekframe',frameidx);
	function: Seek to specific frame so that the next "readframe" will
		  return the corresponding frame.
	input: frameidx is the index of the frame that the next "readframe" 
	       call will return. The frame index starts from 1, not 0.
	return: TimeStamp is the current timestamp after seeking in seconds, 
		if success. Please note that this timestamp should be 
		smaller than the timestamp of the seeked frame. It is 
		negative, if failed. -1 means end of the file. -2 means 
		decoding error. -3 means seeking error.

********'closevideo'********
	Status = ffmat('closevideo');
	function: Close the video file and release the memory.
	return: Status is 1 if success. Otherwise it is negative.
----------------------------------------------------------------------------
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
