This is the README file for the "extra" library, consisting of Pd
objects which are too specialized or otherwise non-canonical for
inclusion into Pd proper.   These files are open source; see 
LICENSE.txt in this distribution for details.

This package should run in Pd under linux, MSW, or Mac OSX.
You can additionally compile fiddle~, bonk~, and paf~ for Max/MSP.

contents:

externs:
fiddle~ -- pitch tracker
bob~ - moog analog resonant filter simulation
bonk~ - percussion detector
choice - find the "best fit" of incoming list
paf~ - phase aligned formant generator
loop~ - sample looper
pique - fft-based peak finder
pd~ - run a pd sub-process
stdout - write messages to standard output
lrshift~ - left or right shift an audio vector

abstractions:
hilbert~ - Hilbert transform for SSB modulation
complex-mod~ - ring modulation for complex (real+imaginary) audio signals
rev1~, etc. - reverberators

The extra library is part of the regular Pd distribution as of Pd version
0.30.

- msp@ucsd.edu
