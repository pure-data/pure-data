---
title: FUDI
---

# FUDI

**FUDI** is a the networking protocol used by Pd internally to communicate between the GUI process and the DSP process. The same protocol is also used for saving patches to the Pd file.
FUDI stand for 'Fast Unified Digital Interface' (or according to Miller Puckette, whatever other acronym people can come up with :)


### Format:

**FUDI** is a packet oriented protocol.
Each message consists of one or more **atoms**, separated by one or more **whitespace** characters, and it's terminated by a **semicolon** character.
An **atom** is a sequence of one or more characters; whitespaces inside atoms can be escaped by the **backslash** (ascii 92) character (see Examples below).

A **whitespace** is either a space (ascii 32), a tab (ascii 9) or a newline (ascii 10).

A **semicolon** (ascii 59) is mandatory to terminate (and send) a message. A **newline** is just treated as whitespace and not needed for message termination.

### Implementations:

**netsend** / **netreceive**

Those classes can be used to transport Pd-messages over a TCP or UDP socket. Both are Pd's built-in objects.


### Example messages:

```
test/blah 123.45314;
```

```
my-slider 12;
```

```
hello this is a message;
```

```
this message continues
in the following
line;
```

```
you; can; send; multiple messages; in a line;
```

```
this\ is\ one\ whole\ atom;
```

```
this_atom_contains_a\
newline_character_in_it;
```


### Message conversion:

**fudiformat** / **fudiparse**

These classes can be used to convert Pd-messages to FUDI and vice-versa.
Both are Pd's built-in objects.


**fudiformat**: convert Pd messages to FUDI packets.

**fudiparse**: parse FUDI packets into Pd messages


----------------------------



### Using other languages or tools:


### pdsend

usage: pdsend <portnumber> [host] [udp|tcp] (default is localhost and tcp)

Example:

```
echo "list foo bar;" | pdsend 8888
```

### Tcl

Just create a socket object and write data to it:

```
set sock [socket localhost 8888]
puts $sock "test/blah 123.45314;"
```

note that since newline is not mandatory, you can send messages without newline at the end, but you have to flush the socket buffer (or change buffering mode - not recommended):

```
set sock [socket localhost 8888]
puts -nonewline $sock "test/blah 123.45314;"
flush $sock
```

### netcat

netcat (aka nc) is a handy command line tool for networking. You can use it to send FUDI messages:

```
echo "blah;" | nc localhost 8888
```


---------------------------

### Related:

Pd also interfaces via *OpenSoundControl* (aka **OSC**)

[https://en.wikipedia.org/wiki/Open_Sound_Control](https://en.wikipedia.org/wiki/Open_Sound_Control)


Pd's built-in objects:

**oscforma**t - convert Pd lists to OSC packets

**oscparse** - parse OSC packets into Pd messages