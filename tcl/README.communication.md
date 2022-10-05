Pd-GUI
======

# Core->GUI communication


## widgets

### widget types

| type          | description |
|---------------|-------------|
| `connection`  | a connection cord |
| `object`      | a standard Pd object |
| `message`     | a message box |
| `floatatom`   | a numberbox (TODO) |
| `symbolatom`  | a symbolbox (TODO) |
| `listatom`    | a listbox (TODO) |
| `comment`     | a comment (TODO) |
| `bang`        | a [bng]] |
| `canvas`      | a [cnv] |
| `radio`       | a [hradio] resp. [vradio] |
| `slider`      | a [hsl] resp. [vsl] (TODO) |
| `toggle`      | a [tgl] |
| `vumeter`     | a [vu] resp. [vradio] (TODO) |



### widget commands

| command                        | description |
|--------------------------------|-------------|
| `::pd::widget::create`         | create a new widget of a given type on a canvas |
| `::pd::widget::destroy`        | destroy all instances of the widget |
| `::pd::widget::config`         | change properties of a widget |
| `::pd::widget::select`         | display a widget as (de)selected |
| `::pd::widget::editmode`       | display a widget as editable |
| `::pd::widget::displace`       | move a widget by an offset |
| `::pd::widget::moveto`         | move a widget to an absolute position |
| `::pd::widget::create_inlets`  | create inlets for a widget |
| `::pd::widget::create_outlets` | create outlets for a widget |
| `::pd::widget::show_iolets`    | show/hide in- and outlets of a widgets |


#### `::pd::widget::create <type> <obj> <cnv> <posX> <posY>`

creates a new widget with the id `<obj>`,
using the widgettype `<type>`.
The object is created on `<cnv>`
(if an object of the same ID already exists on the canvas, the old one is deleted first).

The string `<obj>` needs to be a unique identifier of the object.
The object's initial position is `<posX> <posY>`.

No guarantees are made about the actual value of the `<obj>` ID or the `<cnv>`.

Example: `::pd::widget::create bang 0x55559a95ae40 .x55559a94db20.c 40 40`

#### `::pd::widget::destroy <obj>`

destroys the widget with id `<obj>`.


Example: `::pd::widget::destroy 0x55559a95ae40`

#### `::pd::widget::config <obj> <args>`



#### `::pd::widget::select <obj> <state>`

If `<state`> is `1`, this makes the object appear as "selected".
If `<state`> is `0`, this makes the object appear as "unselected".

Example: `::pd::widget::select 0x55559a95ae40 1`


#### `::pd::widget::editmode <obj> <state>`

If `<state`> is `1`, this makes the object appear as "editable".
If `<state`> is `0`, this makes the object appear as "runnable".

Example: `::pd::widget::editmode 0x55559a95ae40 1`

Some objects might want to change their appearance depending on whether
a canvas is in edit-mode or not.
The default for this is a no-op.


#### `::pd::widget::displace <obj> <dx> <dy>`

moves the object relative to it's current position.

Example: `::pd::widget::displace 0x55559a95ae40 10 0`


#### `::pd::widget::moveto <obj> <cnv> <x> <y>`

moves the object to the absolute position

Example: `::pd::widget::moveto 0x55559a95ae40 200 100`


#### `::pd::widget::create_inlets <obj> <args>`

creates inlets for the given `<obj>`.
The `<args>` denote the inlet types.
Valid types are:

| type-ID | type          |
|---------|---------------|
| `0`     | message inlet |
| `1`     | signal inlet  |


Example: `::pd::widget::create_inlets 0x55559a95ae40 0 0 0`

This creates three message inlets

#### `::pd::widget::create_outlets <obj> <args>`

Same as with `::pd::widget::create_inlets`, but this time creates outlets.

Example: `::pd::widget::create_outlets 0x55559a95ae40 1 0`

This creates a signal outlet and a message outlet.

#### `::pd::widget::show_iolets <obj> <show_inlets> <show_outlets>`

shows/hides inlets and outlets.

The `<show_inlets>` resp  `<show_outlets>` are boolean integers indicating whether to show (or not).

Example: `::pd::widget::show_iolets 0x55559a95ae40 1 0`

This shows the inlets and hides the outlets.

The default state for iolets (after they are created) is being *shown*.

## more ideas

these are just ideas. (not ready for implementation)

### connections
Ideally, we could just tell the GUI to connect a given outlet with a given inlet
(similar to Pd's `connect` message for the canvas).

| command                        | description |
|--------------------------------|-------------|
| `::pd::widget::connect`        | connect an object's inlet to another object's outlet |
| `::pd::widget::disconnect`     | disconnect |

Unfortunately, this would break compatibility with existing GUI-externals
(as it requires that the GUI knows which item is an iolet and to which object
it belongs to).

For now, it's probably better to create a `connection` widget (which is really
just the cord), and move it around without the GUI knowing what it actually is...

#### `::pd::widget::connect <src> <outlet> <dst> <inlet>`
TODO

#### `::pd::widget::disconnect <src> <outlet> <dst> <inlet>`
TODO
