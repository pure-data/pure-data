#import <Cocoa/Cocoa.h>

static volatile int sys_eventloop_running;

    /* Setup the event loop. Must be called on the main thread! */
extern "C" void sys_eventloop_setup(void)
{
    /* create the Cocoa application singleton: */
    [NSApplication sharedApplication];
    /* Usually we would transform the process into a foreground application.
     * For now it's the plugin's responsibility to do this (early or lazily)
     * because we don't want to always show an icon in the docker.
     * ProcessSerialNumber psn = { 0, kCurrentProcess };
     * TransformProcessType(&psn, kProcessTransformToForegroundApplication); */
}

    /* NOTE: Because of a limitation of Cocoa, the event loop must run on the
     * main thread, otherwise we can't receive any input events!
     * This function blocks until the event loop receives a quit notification,
     * see sys_eventloop_quit(). */
extern "C" void sys_eventloop_run(void)
{
    /* this doesn't work...
     * [NSApp run];
     * Kudos to https://www.cocoawithlove.com/2009/01/demystifying-nsapplication-by.html */
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    [NSApp finishLaunching];
    sys_eventloop_running = 1;

    while (sys_eventloop_running)
    {
        [pool release];
        pool = [[NSAutoreleasePool alloc] init];
        NSEvent * event = [NSApp nextEventMatchingMask:NSAnyEventMask
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

    /* Tell sys_eventloop_run() to quit. Can be called from any thread! */
extern "C" void sys_eventloop_quit(void)
{
    /* break from event loop instead of [NSApp terminate:nil] */
    sys_eventloop_running = 0;
    /* send dummy event to wake up event loop */
    NSEvent *event = [NSEvent otherEventWithType:NSApplicationDefined
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
