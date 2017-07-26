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

# preferences feedback strings in s_file.c

puts [_ "no Pd settings to clear"]
puts [_ "removed .pdsettings file"]
puts [_ "couldn't delete .pdsettings file"]
puts [_ "failed to erase Pd settings"]
puts [_ "erased Pd settings"]
puts [_ "no Pd settings to erase"]
puts [_ "skipping loading preferences... Pd seems to have crashed on startup."]
puts [_ "(re-save preferences to reinstate them)"]

# These are strings which, for some reason or another, are used in the Pd GUI
# Tcl, but xgettext doesn't find them.

# Menu Titles
puts [_ "File"]
puts [_ "Edit"]
puts [_ "Put"]
puts [_ "Find"]
puts [_ "Media"]
puts [_ "Window"]
puts [_ "Help"]

# These are strings built into Tk that msgcat should know about.

puts [_ "Show &Hidden Files and Directories"]
