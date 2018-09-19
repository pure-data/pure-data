This is the README file for the "extras" library, which contains Pd objects
which are too specialized or otherwise non-canonical for inclusion into Pd
proper.   This library is part of the regular ("vanilla") Pd distribution.  It
is all open source; see LICENSE.txt in the Pd distribution for details.

contents:

generally useful externs:
sigmund~ - pitch and sinusoidal peak analysis
bonk~ - percussion detector
lrshift~ - left or right shift an audio vector (probably should be standard)

abstractions:
hilbert~ - Hilbert transform for SSB modulation
complex-mod~ - ring modulation for complex (real+imaginary) audio signals
rev1~, etc. - reverberators

externs aimed at particular tasks:
pd~ - embed one Pd inside another one
stdout - send messages to standard out (useful in pd~ sub-process)
bob~ - Moog ladder filter simulation using a Runge-Kutte ODE solver
choose - find the best fit of a vector to a set of example vectors
loop~ - sample looper

obsolete:
pique - fft-based peak finder (use sigmund~ instead)
fiddle~ - pitch tracker (use sigmund~ instead)

The sigmund~, bonk~, and fiddle~ objects have also been compiled for Max/MSP
by Ted Apel; see his website for details.
