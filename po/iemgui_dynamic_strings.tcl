# These are strings that are passed from C to Tcl but the msgcat
# procedure call '[_ $string]' is on the Tcl side, so that the
# automatic parsing done by xgettext doesn't pick them up as strings
# to be localized.  Therefore we just list them here to make xgettext
# happy.
#
# If these are changed in the src/g_*.c files, then need to be
# manually updated here.

# these are labels for elements of the Properties panel
## nbx2/slider scale
puts [_ "linear"]
puts [_ "logarithmic"]
## radio (compat mode)
puts [_ "new-only"]
puts [_ "new&old"]
## VU-meter
puts [_ "no scale"]
puts [_ "scale"]
