# These are strings that are passed from C to Tcl but the msgcat
# procedure call '[_ $string]' is on the Tcl side, so that the
# automatic parsing done by xgettext doesn't pick them up as strings
# to be localized.  Therefore we just list them here to make xgettext
# happy.
#
# If these are changed in the src/g_*.c files, then need to be
# manually updated here.

# these are headers for the tops of the Properties panel
puts [_ "-------dimensions(digits)(pix):-------"]
puts [_ "--------dimensions(pix)(pix):--------"]
puts [_ "----------dimensions(pix):-----------"]
puts [_ "--------flash-time(ms)(ms):---------"]
puts [_ "-----------output-range:-----------"]
puts [_ "------selectable_dimensions(pix):------"]
puts [_ "------visible_rectangle(pix)(pix):------"]

# these are labels for elements of the Properties panel
puts [_ "bottom:"]
puts [_ "height:"]
puts [_ "hold:"]
puts [_ "intrrpt:"]
puts [_ "left:"]
puts [_ "lin"]
puts [_ "log"]
puts [_ "log-height:"]
puts [_ "max:"]
puts [_ "min:"]
puts [_ "right:"]
puts [_ "size:"]
puts [_ "top:"]
puts [_ "value:"]
puts [_ "width:"]
