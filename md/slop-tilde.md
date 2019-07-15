---
comment:  This document is in 'markdown' format for use with pandoc
---

### [slop~ - slew-limiting low-pass filter](#topics-slop)

Tasks such as envelope following, dynamics processing, and soft saturation
often rely on low-pass filtering in which the cutoff frequency of the filter
(which you can alternatively think of as its reaction speed) varies according to
whether the input is rising, stable, or falling.  For example, a VU meter design
might call for an envelope follower whose output can rise quickly but then drops
off more slowly.  To make this we could use a low-pass filter to make a moving
average of the instantaneous signal level, but the moving average should react
faster on rising inputs than on falling ones.

The simplest type of digital low-pass filter can be understood as a moving
average:

$$y[n] = y[n-1] + k \cdot (x[n] - y[n-1])$$

where $0 \le k \le 1$ is an averaging factor,  usually much closer to zero than
one.  When the value of $k$ is small enough (less than 1/2, say), it is
approximately equal to the filter's rolloff frequency in units of radians per
sample.  (The theory behind this is explained in
[Theory and Techniques of Electronic Music](http://msp.ucsd.edu/techniques.htm), section 8.3, "designing filters").

For our purposes we'll rewrite this equation as:

$$y[n] - y[n-1] = f (x[n] - y[n-1])$$

where the function $f$ is linear:

$$f(x) = k \cdot x$$

In words, this equation says, "increment your output by $k$ times the distance
you have to travel to reach the goal $x[n]$".  (So far, we've described the
action of the linear lop~ object.)  In the slop~ object, this linear function is
replaced by a nonlinear one with three segments, one for an interval $(-n, p)$
containing zero, and two others joining this one at the input values $-n$ and
$p$.  The three segments have slopes equal to $k_n$, $k$, and $k_p$ for the
negative, middle, and positive regions:


![test](slop-tilde-1-curves.png)


_Rationale._  In general, $k$ could depend on both the previous output $y[n-1]$
and on the current input $x$.  This would require that the invoking patch somehow
specify a function of two variables, a feat for which Pd is ill suited.  In slop~
we make the simplifying assumption that adding an offset to both the filter's
state and its input should result in adding the same offset to the output; that
is, the filter should be translation-invariant.  (As will be seen below, through a
bit of skulduggery we can still make translation-dependent effects such as soft
saturation).  One could also ask why we don't allow the function $f$ to refer to a
stored array instead of restricting it to a 5-parameter family of piecewise linear
functions.  The reason for choosing the approach taken is that it is often
desirable to modulate the parameters at audio rates, and that would be difficult
if we used an array.

##### Examples.

[Using slop~ to make a compressor-expander-limiter](compander-limiter.htm).

