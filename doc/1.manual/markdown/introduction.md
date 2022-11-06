---
title: introduction
---

## Pd Manual: introduction

[back to table of contents](index.html)


This is the HTML documentation for the Pd computer program. Pd is free
and can be downloaded from the internet; go to
<http://msp.ucsd.edu/software.html> to get it.

### Guide to Pd's documentation

Pd's documentation lives in the "doc" folder of the distribution and
consists of:

-   this HTML manual

        Pd/
        ├── doc/
            ├── 1.manual/ 

-   "reference" (or 'help') patches, for all objects in Pd

        Pd/
        ├── doc/
            ├── 5.reference/ 

-   "example" patches showing how to do things

        Pd/
        ├── doc/
            ├── 2.control.examples/
            ├── 3.audio.examples/
            ├── 4.data.structures/ 

-   sample C code on how to write externals

        Pd/
        ├── doc/
            ├── 6.externs/

This Manual has six sections:

1.  this introduction
1.  [a theory of operations, explaining how Pd works]({{< mdlink "theory of operation" "">}})
1.  [instructions on installing Pd and getting it to run]({{< mdlink "getting Pd to run" "">}})
1.  [externals]({{< mdlink "externals" "">}})
1.  [installing from source]({{< mdlink "installing from source" "">}})
1.  [release notes and known bugs]({{< mdlink "current status" "">}})

For a list of all the objects you can use menu **Help/List of objects**.
To get help on any Pd object you can right click on it; or you can
browse the help patches by choosing menu **Help/Browser** and looking in
"Pure Data/5.reference".

The "example" patches are also available from Pd's browser. They
appear in subdirectories named "2.control.examples",
"3.audio.examples" and "4.data.structures". Some additional patches
in "7.stuff" might also be helpful.

To get started writing your own C extensions, refer to "6.externs".
For more about externals, please refer to the [externals]({{< mdlink "externals" "">}}) 
section of this manual.

### other resources

There is a very extensive Pd community web site,
[pure-data.info](http://www.pure-data.info/), which aims to be the
central resource for Pd, from documentation and downloads; to forums,
member pages, and a patch exchange. You can check
<https://puredata.info/docs/> for more documentation.

Most of the interesting news related to Pd shows up on the Pd mailing
list, maintained by IOhannes zmölnig. To subscribe or browse the
archives visit: <https://lists.puredata.info/listinfo/pd-list>. This is
the best source of recent information regarding installation problems
and bugs. It is perfectly reasonable to post "beginner" questions on
this list; alternatively you can contact msp@ucsd.edu for help.

Many extensions to Pd are announced on the mailing list. In particular,
for people interested in graphics, there is a 3D graphics rendering
package, named GEM, based on OpenGL, written by Mark Danks, adapted to
Linux by Guenter Geiger, and now maintained by IOhannes zmölnig. You can
get it from: <http://gem.iem.at/>, via "Find externals" in the 'Help
Menu' or package manager of your Linux distribution. Another option is
Ofelia, a Pd external that allows you to use openFrameworks and Lua
within Pd for creating audiovisual artwork or multimedia applications
such as games. Get it also via via "Find externals" or from
<https://github.com/cuinjune/Ofelia>.



[next chapter]({{< mdlink "theory of operation" "">}}) \
[back to table of contents](index.html)


