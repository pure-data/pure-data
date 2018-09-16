This directory contains translation files for the Pure Data GUI:

* po files: per-language translations
* template.pot: template for creating a new po translation
* pd_dynamic_strings.tcl: strings sent from the Pd core
* iemgui_dynamic_strings.tcl: IEM GUI strings sent from the Pd core

Localizations are built using GNU gettext: https://www.gnu.org/software/gettext

## Overview

Pd GUI strings are written in English and denoted in Tcl using the "_" prefix:

    [_ "Audio Settings"]

gettext searches the GUI Tcl files for these marked strings and uses them as
unique identifiers to create the .pot template file which is simply a list of
string ids and translation stubs with associated metadata:

    msgid "Audio Settings"
    msgstr ""

To create a new translation, either copy & rename the .pot template file using
the appropriate ISO 639 2 or 3 letter language code (ie. German is "de.po") or
use the gettext msginit utility to initialize a new translation:

    msginit --locale=de --input=template.pot

Note that some languages also have specific country distinctions, ie. Brazilian
Portuguese and Portuguese are named "pt_br.po" & "pt_pt.po", respectively.  

Next, fill out the msgstr portion for each msgid with it's translation:

    msgid "Audio Settings"
    msgstr "Audioeinstellungen"

The translation process can either be done by hand with a text editor or via GUI
tools created for this purpose such as poedit: https://poedit.net. Once the .po
translation file is ready, add it to this directory and check if it needs to be
added to the ALL_LINGUAS language list in po/Makefile.am.

When building Pd, these .po translation files are used to generate .msg files
which are then loaded by the Pd GUI at runtime depending on the current locale.
You can manually set the locale when running Pd from the commandline with the
LANG environment variable, in this case German (de):

    LANG=de.UTF-8 pd

This also works when opening a macOS .app bundle:

    LANG=de.UTF-8 open Pd.0.47.1.app

On Windows, you can also set LANG on the commandline or set it within a .bat
wrapper file to launch Pd. Place the following in a file called "pd-lang.bat"
in the Pd bin directory and double click to run:

    SET LANG="de.UTF-8"
    start "" "pd.exe"

If the Tcl files have changed, the make process regenerates the .pot template
file and merges any changes into the .po files. Strings which have changed are
marked as "fuzzy" and should be reviewed updated and by a translator. Once the 
update is approved, the "fuzzy" keyword and/or comment can be removed. Also,
any strings which do not require translation can be left with an empty msgstr
which tells gettext to use the default, non translated string.

## Developer Info

This is system is largely automated using make, however there are times when it
is useful to do things manually. The applicable make targets are:

* template: re-builds the template if one of the Tcl files has changed
* po: updates the .po files if the template has been changed
* msg: generates the .msg files if the .po files have changed

You can manually trigger a template refresh by updating the timestamp of one
of the Tcl files:

    touch ../tcl/pd-gui.tcl
    make template

The .po file refresh can be triggered by updating the template timestamp:

    touch template.pot
    make po

To see the location of translation strings in the Tcl source files, run the
following in this directory:

    make locations

## On "Dynamic" Strings

As the GUI is separate from the Pd core, strings which are sent from the
core to the GUI are translated "on the fly" when they are received. These are
given to gettext via the dummy Tcl files in this directory to make sure they are
added to the .pot & .po files since the Pd core .c source files are not indexed.
This means that if a "dynamic" string sent from the core is changed at some
point, it needs to be manually updated here.
