
package provide pd_connect 0.1

namespace eval ::pd_connect:: {
    variable pd_socket

    namespace export to_pd
    namespace export create_socket
    namespace export pdsend
}

# TODO figure out how to escape { } properly

proc ::pd_connect::configure_socket {sock} {
    fconfigure $sock -blocking 0 -buffering line -encoding utf-8;
    fileevent $sock readable {::pd_connect::pd_readsocket ""}
}

# if pd opens first, it starts pd-gui, then pd-gui connects to the port pd sent
proc ::pd_connect::to_pd {port} {
    # puts "::pd_connect::to_pd"
    variable pd_socket
    # puts stderr "Connecting to localhost $port ..."
    if {[catch {set pd_socket [socket localhost $port]}]} {
        puts stderr "WARNING: connect to pd failed, retrying port $port."
        after 1000 ::pd_connect::to_pd $port
        return
    }
    ::pd_connect::configure_socket $pd_socket
}

# if pd-gui opens first, it creates socket and requests a port.  The function
# then returns the portnumber it receives. pd then connects to that port.
proc ::pd_connect::create_socket {} {
    if {[catch {set sock [socket -server ::pd_connect::from_pd -myaddr localhost 0]}]} {
        puts stderr "ERROR: failed to allocate port, exiting!"
        exit 3
    }
    return [lindex [fconfigure $sock -sockname] 2]
}

proc ::pd_connect::from_pd {channel clientaddr clientport} {
    puts "::pd_connect::from_pd"
    variable pd_socket $channel
    puts "Connection from $clientaddr:$clientport registered"
    ::pd_connect::configure_socket $pd_socket
}

# send a pd/FUDI message from Tcl to Pd. This function aims to behave like a
# [; message( in Pd.  Basically, whatever is in quotes after the proc name
# will be sent as if it was sent from a message box with a leading semi-colon
proc ::pd_connect::pdsend {message} {
    variable pd_socket
    append message \;
    if {[catch {puts $pd_socket $message} errorname]} {
        puts stderr "pdsend errorname: >>$errorname<<"
        error "Not connected to 'pd' process"
    }
}

proc ::pd_connect::pd_readsocket {cmd_from_pd} {
    variable pd_socket
    if {[eof $pd_socket]} {
        # if we lose the socket connection, that means pd quit, so we quit
        close $pd_socket
        exit
    } 
    append cmd_from_pd [read $pd_socket]
    while {![info complete $cmd_from_pd] || \
        [string index $cmd_from_pd end] ne "\n"} {
        append cmd_from_pd [read $pd_socket]
        if {[eof $pd_socket]} {
        close $pd_socket
        exit
        }
    }
#        puts stderr [concat CMD: $cmd_from_pd :CMD]
    if {[catch {uplevel #0 $cmd_from_pd} errorname]} {
        global errorInfo
        puts stderr "errorname: >>$errorname<<"
        switch -regexp -- $errorname { 
            "missing close-brace" {
                # TODO consider using [info complete $cmd_from_pd] in a loop
                pd_readsocket $cmd_from_pd
            } "^invalid command name" {
                puts stderr "INVALID COMMAND NAME: $errorInfo"
            } default {
                puts stderr "UNHANDLED ERROR: $errorInfo"
            }
        }
    }
}
