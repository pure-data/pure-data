This directory contains translation files for the Pure Data GUI:

* po files: per-language translations
* template.pot: template for creating a new po translation
* pd_dynamic_strings.tcl: strings sent from the Pd core
* iemgui_dynamic_strings.tcl: IEM GUI strings sent from the Pd core

Localizations are built using GNU gettext: https://www.gnu.org/software/gettext

Pd GUI strings are written in English and denoted in Tcl using the "_" prefix:

    [_ "Audio Settings"]

gettext searches the GUI Tcl files for these marked strings and uses them as
unique identifiers to create the .pot template file which is simply a list of
string ids and translation stubs with associated metadata:

    msgid "Audio Settings"
    msgstr ""

To create a new translation, copy & rename the .pot template file using the
appropriate ISO 639 2 or 3 letter language code (ie. German is "de.po"). Note
that some languages also have specific country distinctions, ie. Brazilian
Portuguese and Portuguese are named pt_br.po pt_pt.po.

Next, fill out the msgstr portion for each msgid with it's translation:

    msgid "Audio Settings"
    msgstr "Audioeinstellungen"

The translation process can either be done by hand with a text editor or via GUI
tools created for this purpose such as poedit: https://poedit.net. Once the .po
translation file is ready, add it to this directory and check if it needs to be
added to the ALL_LINGUAS language list in po/Makefile.am.

When building Pd, these .po translation files are used to generate .msg files
which are then loaded by the Pd GUI at runtime depending on the current locale.

If the .tcl files have changed, the make process regenerates the .pot template
file and merges any changes into the .po files. Strings which have changed are
marked as "fuzzy" and should be reviewed updated and by a translator.

Note: As the GUI is separate from the Pd core, strings which are sent from the
core to the GUI are translated "on the fly" when they are received. These are
given to gettext via the dummy Tcl files in this directory to make sure they are
added to the .pot & .po files since the Pd core .c source files are not indexed.
This means that if a "dynamic" string sent from the core is changed at some
point, it needs to be manually updated here. 
