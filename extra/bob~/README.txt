The bob~ object.  BSD licensed; Copyright notice is in bob~ source code.

Imitates a Moog resonant filter by Runge-Kutte numerical integration of
a differential equation approximately describing the dynamics of the circuit.

Useful references:

Tim Stilson
Analyzing the Moog VCF with Considerations for Digital Implementation
https://ccrma.stanford.edu/~stilti/papers/moogvcf.ps.gz

(sections 1 and 2 are a reasonably good introduction but the model they use
is highly idealized.)

Timothy E. Stinchcombe
Analysis of the Moog Transistor Ladder and Derivative Filters

(long, but a very thorough description of how the filter works including
its nonlinearities)

Antti Huovilainen
Non-linear digital implementation of the moog ladder filter

(comes close to giving a differential equation for a reasonably realistic
model of the filter).

Th differential equations are:

y1' = k * (S(x - r * y4) - S(y1))
y2' = k * (S(y1) - S(y2))
y3' = k * (S(y2) - S(y3))
y4' = k * (S(y3) - S(y4))

where k controls the cutoff frequency, r is feedback (<= 4 for
stability), and S(x) is a saturation function.
