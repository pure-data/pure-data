# Pure Data font resources

This directory contains font files for use with the Pure Data GUI.

The default font for Pd is DejaVu Sans Mono (DVSM) and it is included with Pd,
based on the platform.

You can see which font Pd is using by looking for the font information prints
when the log level is set to "4 all."

## Linux

DVSM is usually installed with the system & is a dependency for the puredata
package. The included font file in this directory are not installed with Pd.
The fallback font when not found is Courier.

The default font weight on Linux is bold.

## macOS

DVSM is included within the Pd .app bundle. If Pd is run from the commandline
DVSM needs to be installed to the system otherwise the fallback font is Monaco.

The default font weight on macOS is normal.

## Windows

As of Pd 0.49, DVSM is included within the Pd app directory and loaded by default.
Previous version require it to be installed to the system manually. The fallback font
when not found is Courier.

The default font weight on Windows is bold.

## Experimenting with Different Fonts

Typically any fixed-width font will work with Pd, although the legibility and
object sizing may vary. Non fixed-width fonts will usually render much longer 
object boxes as the sizing does not take into account variable character width.

You can experiment with different fonts and font weights (normal or bold) by
using the following Pd startup flags or when running Pd on the commandline:

    # specify default font (by name)
    pd -font-face Courier

    # specify default font weight (normal or bold)
    pd -font-weight bold

    # specify default font size in points
    pd -font-size 14

Try this for some unusable fun:

    pd -font-face "Zapf Dingbats" -font-weight normal -font-size 16
