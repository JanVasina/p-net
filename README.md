p-net Profinet device stack
===========================

This is a fork of original p-net Profinet device stack.
I am trying to modify the source code such as the device is able to pass the test cases of the Automated RT Tester v 2.41.0.2.<br>
**All the tests of the Automated RT Tester pass.**<br>
**The Security Level 1 Test (SL1) passes.**<br>

There are some hacks in the source code (just to pass the tests) which probably break the design of the original p-net stack library.
See the comments in the source code.

The code is modified for Zynqberry module from Trenz Electronics (TE-0726) which is very close to Raspberry 2B.

There are four input modules (size 32, 64, 128, 256 bytes) and four output modules (32, 64, 128, bytes). Modules have
fixed slot positions to ease programming of the application.

I have created a simple makefile for everyday compile and cross-compile, please change the paths in it if you want to use it directly.
Cross compiling is done on Windows with the raspberry gcc 8.3.0 taken from https://gnutoolchains.com/raspberry/.

The Profinet device application (sample_app) uses shared linux memory where the data of inputs/outputs are stored/read. 
Another separate Linux process (virtual PLC - not part of this project) then works with this data and can create a customized behavior 
(together with a Simatic S7 PLC or other IO-controller).

I tried to comment my changes to the original source code with C++ comments // in contrast to original C-like comments /*.

The code is fixed now and is ready for certification in the PI-LAB. It is an approximate merge from original p-net stack code from beginning of July 2020 and then developed by my own way to pass all the certification tests.

The latest original p-net README and stack code can be found here: https://github.com/rtlabs-com/p-net


