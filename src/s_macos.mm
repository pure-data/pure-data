#import <Cocoa/Cocoa.h>

static volatile int sys_eventloop_running;

extern "C" void sys_eventloop_setup(void)
{
    [NSApplication sharedApplication];
}

extern "C" void sys_eventloop_run(void)
{
    /* this doesn't work...
     * [NSApp run];
     * Kudos to https://www.cocoawithlove.com/2009/01/demystifying-nsapplication-by.html */
    NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];

    [NSApp finishLaunching];
    sys_eventloop_running = 1;

    while (sys_eventloop_running)
    {
        [pool release];
        pool = [[NSAutoreleasePool alloc] init];
        NSEvent* event = [NSApp nextEventMatchingMask:NSAnyEventMask
                                            untilDate:[NSDate distantFuture]
                                               inMode:NSDefaultRunLoopMode
                                              dequeue:YES];
        if (event)
        {
            [NSApp sendEvent:event];
            [NSApp updateWindows];
        }
    }
    [pool release];
}

extern "C" void sys_eventloop_quit(void)
{
    /* break from event loop instead of [NSApp terminate:nil] */
    sys_eventloop_running = 0;
    /* send dummy event to wake up event loop */
    NSEvent* event = [NSEvent otherEventWithType:NSApplicationDefined
                                        location:NSMakePoint(0, 0)
                                   modifierFlags:0
                                       timestamp:0
                                    windowNumber:0
                                         context:nil
                                         subtype:0
                                           data1:0
                                           data2:0];
    [NSApp postEvent:event atStart:NO];
}
