p-net Profinet device stack
===========================

This is a fork of original p-net Profinet device stack.
I am trying to modify the source code such as the device is able to pass the test cases of the Automated RT Tester v 2.4.1.3.<br>
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

When the original p-net source changes, I manually merge the changes as much as possible (source code only).
Actual merge if from the master uploaded on the 1st of July 2020.


Follows the original Readme.md file:

Web resources
-------------

* Source repository: [https://github.com/rtlabs-com/p-net](https://github.com/rtlabs-com/p-net)
* Documentation: [https://rt-labs.com/docs/p-net](https://rt-labs.com/docs/p-net)
* Continuous integration: [https://travis-ci.org/rtlabs-com/p-net](https://travis-ci.org/rtlabs-com/p-net)
* rt-labs: [https://rt-labs.com](https://rt-labs.com)

[![Build Status](https://travis-ci.org/rtlabs-com/p-net.svg?branch=master)](https://travis-ci.org/rtlabs-com/p-net)

p-net
-----
The rt-labs Profinet stack p-net is used for Profinet device
implementations. It is easy to use and provides a small footprint. It
is especially well suited for embedded systems where resources are
limited and efficiency is crucial.

It is written in C and can be run on bare-metal hardware, an RTOS such as
rt-kernel, or on Linux. The main requirement is that the
platform can send and receive raw Ethernet Layer 2 frames. The
p-net stack is supplied with full sources including a porting
layer.

Also C++ (any version) is supported.

rt-labs p-net is developed according to specification 2.3:

 * Conformance Class A (Class B upon request)
 * Real Time Class 1

Features:

 * TCP/IP
 * RT (real-time)
 * Address resolution
 * Parameterization
 * Process IO data exchange
 * Alarm handling
 * Configurable number of modules and sub-modules
 * Bare-metal or OS
 * Porting layer provided

Limitations:

* IPv4 only
* Only a single Ethernet interface (no media redundancy)

This software is dual-licensed, with GPL version 3 and a commercial license.
See LICENSE.md for more details.


Requirements
------------
The platform must be able to send and receive raw Ethernet Layer 2 frames,
and the Ethernet driver must be able to handle full size frames. It
should also avoid copying data, for performance reasons.

* cmake 3.13 or later

For Linux:

* gcc 4.6 or later
* See the "Real-time properties of Linux" page in the documentation on how to
  improve Linux timing

For rt-kernel:

* Workbench 2017.1 or later

An example of microcontroller we have been using is the Infineon XMC4800,
which has an ARM Cortex-M4 running at 144 MHz, with 2 MB Flash and 352 kB RAM.
It runs rt-kernel, and we have tested it with 9 Profinet slots each
having 8 digital inputs and 8 digital outputs (one bit each). The values are
sent and received each millisecond (PLC watchdog setting 3 ms).


Contributions
--------------
Contributions are welcome. If you want to contribute you will need to
sign a Contributor License Agreement and send it to us either by
e-mail or by physical mail. More information is available
on [https://rt-labs.com/contribution](https://rt-labs.com/contribution).
