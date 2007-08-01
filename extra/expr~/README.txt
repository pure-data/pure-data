
You can get more information on the expr object at
http://www.crca.ucsd.edu/~yadegari/expr.html

-----------

New if Version 0.4

-access to variables (made by value object)
-multiple expression separated by ;
-added the following shorthands:
	$y or $y1 = $y1[-1] and $y2 = $y2[-1]
-new functions:
	if - conditional evaluation
	cbrt - cube root
	erf - error function
	erfc - complementary error function
	expm1 - exponential minus 1,
	log1p - logarithm of 1 plus
	isinf - is the value infinite,
	finite - is the value finite
	isnan -- is the resut a nan (Not a number)
	copysign - copy sign of a number
	ldexp  -  multiply floating-point number by integral power of 2
	imodf -	get signed integral value from floating-point number
	modf - get signed fractional value from floating-point number
	drem - floating-point remainder function

   Thanks to Orm Finnendahl for adding the following functions:
	fmod - floating-point remainder function
	ceil - ceiling function: smallest integral value not less than argument 
	floor - largest integral value not greater than argument

------------

New in Version 0.3
-Full function functionality

------------

The object "expr" is used for expression evaluaion of control data.

Expr~ and fexpr~ are extentions to the expr object to work with vectors.
The expr~ object is designed to efficiently combine signal and control
stream processing by vector operations on the basis of the block size of
the environment.

fexpr~ object provides a flexible mechanism for building FIR and
IIR filters by evaluating expressions on a sample by sample basis
and providing access to prior samples of the input and output audio
streams. When fractional offset is used, fexpr~ uses linear interpolation
to determine the value of the indexed sample. fexpr~ evaluates the
expression for every single sample and at every evaluation previous
samples (limited by the audio vector size) can be accessed. $x is used to
denote a singnal input whose samples we would like to access. The syntax
is $x followed by the inlet number and indexed by brackets, for example
$x1[-1] specifies the previous sample of the first inlet. Therefore,
if we are to build a simple filter which replaces every sample by
the average of that sample and its previous one, we would use "fexpr~
($x1[0]+$x1[-1])/2 ". For ease of when the brackets are omitted, the
current sample is implied, so we can right the previous filter expression
as follows:  " fexpr~ ($x1+$x1[-1])/2". To build IIR filters $y is used
to access the previous samples of the output stream.

The three objects expr, expr~, and fexpr~ are implemented in the same object
so the files expr~.pd_linux and fexpr~.pd_linux are links to expr.pd_linux
This release has been compiled and tested on Linux 6.0.

--------

Here are some syntax information: (refer to help-expr.pd for examples)
 
Syntyax:
The syntax is very close to how expression are written in
C. Variables are specified as follows where the '#' stands
for the inlet number:
$i#: integer input variable
$f#: float input variable
$s#: symbol input variable

Used for expr~ only:
$v#: signal (vector) input (vector by vector evaluation)

Used for fexpr~ only:
$x#[n]: the sample from inlet # indexed by n, where n has to
	satisfy 0 => n >= -vector size,
	($x# is a shorthand for $x#[0], specifying the current sample)

$y#[n]: the output value indexed by n, where n has to
	satisfy 0 > n >= -vector size,
	$y[n] is a shorthand for $y1[n]


I'll appreciate hearing about bugs, comments, suggestions, ...

Shahrokh Yadegari (sdy@ucsd.edu)
7/10/02
