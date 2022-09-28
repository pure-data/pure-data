widget implementation in Tcl/Tk
===============================

there's a number of procedures that help with implementing the GUI in Tcl/Tk

# registering new widgets

In order for the GUI to know a widget type, it *must* be made known.
Also, most widgets probably need to override one or more of the
generic "widget commands".

| command                       | description |
|-------------------------------|-------------|
| `::pd::widget::register`      | register a constructor for a new widget type |
| `::pd::widget::widgetbehavior`| register a new widget behavior override |

## `::pd::widget::register <type> <ctor>`

registers a constructor for a new widget `<type>`

Example: `::pd::widget::register bang ::pd::widget::bang::create`

this registers a new widget `bang`.
Whenever the `bang` widget is to be created (e.g. via `::pd::widget::create bang ...`)
the constructor `::pd::widget::bang::create` is called.

This proc should get called when starting the GUI (or loading the GUI-external).

## `::pd::widget::widgetbehavior <obj> <behavior> <behaviorproc>`

registers a new widget behavior for the given object
(see the communication's [widget commands](README.communication.md#widget-commands) for valid behaviors).

All calls to `::pd::widget::<behavior> <obj> ...` (from Pd-core),
are forwarded to the registered `<behaviorproc>`.

If no behaviorproc is registered for a given behavior, a fallback is used
(which might work for a given widget, or not).

This is to be called from the constructor of the `<obj>`.

Example: `::pd::widget::widgetbehavior $obj select ::pd::widget::bang::select`
(with `$obj` set to `0x55559a95ae40`)

this will turn a Pd-core call `::pd::widget::select 0x55559a95ae40 1` into
`::pd::widget::bang::select 0x55559a95ae40 1`



# widget helpers

| command                        | description |
|--------------------------------|-------------|
| `::pd::widget::base_tag`       | get a Tcl/Tk conformant tag for an object |
| `::pd::widget::get_canvases`   | get all (valid) canvases that hold a given object  |
| `::pd::widget::refresh_iolets` | redraw iolets (after a size change) |
| `::pd::widget::parseargs`      | parse flag-like arguments |
| `::pd::widget::get_iolets`     | get the types of existing iolets - REMOVE? |
| `::pd::widget::create_iolets`  | recreate iolets (after a size change) - REMOVE? |

## `::pd::widget::base_tag <obj>`

Mangles the object ID `<obj>` into a string that can be used as a Tcl/Tk tag.

## `::pd::widget::get_canvases <obj>`

Returns all (valid) canvases where `<obj>` has been created.

## `::pd::widget::refresh_iolets <obj>`
## `::pd::widget::parseargs <optionspec> <argv>`


## `::pd::widget::get_iolets <obj> <type>`
## `::pd::widget::create_iolets <obj> <inlets> <outlets>`



# specific widgets

## `connection`

### configuration
| flag        | arguments           | description |
|-------------|---------------------|-------------|
| `-position` | <x0> <y0> <x1> <y1> | start and end position of the cord |
| `-type`     | <type>              | whether this is a message (0) or signal (1) connection

## `object`
### configuration
| flag        | arguments           | description |
|-------------|---------------------|-------------|
| `-size`     | <width> <height>    | size of the objectbox
| `-text`     | <txt>               | text to be displayed

## `message`
### configuration
| flag        | arguments           | description |
|-------------|---------------------|-------------|
| `-size`     | <width> <height>    | size of the msgbox
| `-text`     | <txt>               | text to be displayed

### `::pd::widget::message::activate <obj> <flashtime>`

Flashes the msgbox for `<flashtime>` milliseconds.


## `floatatom`

## `symbolatom`

## `listatom`

## `comment`

## `bang`
### configuration
| flag         | arguments         | description |
|--------------|-------------------|-------------|
| `-labelpos`  | <dx> <dy>         | relative position of the label |
| `-size`      | <w> <h>           | object size |
| `-colors`    | <fg> <bg> <label> | foreground/background/label colours |
| `-font`      | <family> <size>   | label font |
| `-label`     | <txt>             | label text |

### `:pd::widget::bang::activate <obj> <state> <color>`

Flashes the bang with `<color>`.


## `canvas`
### configuration
| flag         | arguments         | description |
|--------------|-------------------|-------------|
| `-labelpos`  | <dx> <dy>         | relative position of the label |
| `-size`      | <w> <h>           | object size |
| `-colors`    | <fg> <bg> <label> | foreground/background/label colours |
| `-font`      | <family> <size>   | label font |
| `-label`     | <txt>             | label text |
| `-visible`   | <w> <h>           | size of the visible rectangle |

## `radio`
### configuration
| flag         | arguments         | description |
|--------------|-------------------|-------------|
| `-labelpos`  | <dx> <dy>         | relative position of the label |
| `-size`      | <w> <h>           | object size |
| `-colors`    | <fg> <bg> <label> | foreground/background/label colours |
| `-font`      | <family> <size>   | label font |
| `-label`     | <txt>             | label text |
| `-number`    | <cols> <rows>     | number of columns/rows or radio-buttons |

### `::pd::widget::radio::activate <obj> <num> <color>`
displays the given `<num>` radio as active (using `<color>`),
and deactivates any previously active radio(s).

## `slider`

## `toggle`
### configuration
| flag         | arguments         | description |
|--------------|-------------------|-------------|
| `-labelpos`  | <dx> <dy>         | relative position of the label |
| `-size`      | <w> <h>           | object size |
| `-colors`    | <fg> <bg> <label> | foreground/background/label colours |
| `-font`      | <family> <size>   | label font |
| `-label`     | <txt>             | label text |

### `::pd::widget::radio::activate <obj> <state> <color>`
shows the toggle as active (if `<state>` is `1`) using the `<color>`.
If state is `0`, deactivates the toggle.

## `vumeter`

