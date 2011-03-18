# These are strings that are passed from C to Tcl but the msgcat
# procedure call '[_ $string]' is on the Tcl side, so that the
# automatic parsing done by xgettext doesn't pick them up as strings
# to be localized.  Therefore we just list them here to make xgettext
# happy.
#
# If these are changed in the src/*.c files, then need to be
# manually updated here.

puts [_ "Do you want to save the changes you made in '%s'?"]
puts [_ "Discard changes to '%s'?"]

puts [_ "Undo clear"]
puts [_ "Undo connect"]
puts [_ "Undo cut"]
puts [_ "Undo disconnect"]
puts [_ "Undo duplicate"]
puts [_ "Undo motion"]
puts [_ "Undo paste"]
puts [_ "Undo typing"]

puts [_ "Redo clear"]
puts [_ "Redo connect"]
puts [_ "Redo cut"]
puts [_ "Redo disconnect"]
puts [_ "Redo duplicate"]
puts [_ "Redo motion"]
puts [_ "Redo paste"]
puts [_ "Redo typing"]
