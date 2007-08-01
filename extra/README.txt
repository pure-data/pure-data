This is the README file for the "extras" library, consisting of Pd
objects which are too specialized or otherwise non-canonical for
inclusion into Pd proper.   These files are open source; see 
LICENSE.txt in this distribution for details.
Note however that "expr" is GPL (the rest is all BSD).

This package should run in Pd under linux, MSW, or Mac OSX.
You can additionally compile fiddle~. bonk~, and paf~ for Max/MSP.

contents:

externs:
fiddle~ -- pitch tracker
bonk~ - percussion detector
choose - find the "best fit" of incoming vector with stored profiles
paf~ -- phase aligned formant generator
loop~ -- sample looper
expr -- arithmetic expression evaluation (Shahrokh Yadegari)
pique - fft-based peak finder
lrshift~ - left or right shift an audio vector

abstractions:
hilbert~ - Hilbert transform for SSB modulation
complex-mod~ - ring modulation for complex (real+imaginary) audio signals
rev1~, etc. - reverberators

These objects are part of the regular Pd distribution as of Pd version
0.30.  Macintosh versions of fiddle~, bonk~, and paf~ are available
from http://www.crca.ucsd.edu/~tapel
- msp@ucsd.edu
