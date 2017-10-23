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
MIT License

Copyright 2017 Gao Shan

Permission is hereby granted, free of charge, to any person 
obtaining a copy of this software and associated documentation 
files (the "Software"), to deal in the Software without restriction, 
including without limitation the rights to use, copy, modify, 
merge, publish, distribute, sublicense, and/or sell copies of 
the Software, and to permit persons to whom the Software is 
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be 
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, 
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND 
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT 
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, 
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR 
OTHER DEALINGS IN THE SOFTWARE.

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

----------------------------------------------------------------------------
Compilation Guide:

Requirements:
1, Windows/macOS/Linux 64bit
2, MATLAB 64bit (2014+ prefered)
3, mex compatiable C/C++ compiler (https://www.mathworks.com/support/compilers.html)
4, all dependencies requried for building ffmpeg

Steps:
1, Build ffmpeg 3.0.9 with "--enable-shared" configuration.
   Source:
   https://www.ffmpeg.org/releases/ffmpeg-3.0.9.tar.gz
   ffmpeg compilation guide: 
   https://trac.ffmpeg.org/wiki/CompilationGuide
   
   P.S.
   Starting from ffmpeg 3.1, new decoding/encoding APIs are introduced. 
   However, they actually perform slower than the legacy ones in my cases.
   FFMAT will not support newer ffmpeg until it matches 3.0.9 in speed.

2, Run "build_script.m" after modifying the sourcepath variable as the parent 
   path of "include" & "lib" folders.
   
3, Run "ffmat_demo.m" to test. See help file for detailed sytax.
