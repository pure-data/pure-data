/* In MSW, this is all there is to pd; the rest sits in a "pdlib" dll so
that externs can link back to functions defined in pd. */

int sys_main(int argc, char **argv);

/*
 * gcc does not support the __try stuff, only MSVC.  Also, MinGW allows you to
 * use main() instead of WinMain(). <hans@at.or.at>
 */
#if defined(_MSC_VER) && !defined(COMMANDVERSION)
#include <windows.h>
#include <stdio.h>

int WINAPI WinMain(HINSTANCE hInstance,
                               HINSTANCE hPrevInstance,
                               LPSTR lpCmdLine,
                               int nCmdShow)
{
    __try {
        sys_main(__argc,__argv);
    }
    __finally
    {
        printf("caught an exception; stopping\n");
    }
    return (0);
}

#else /* not _MSC_VER ... */
int main(int argc, char **argv)
{
    return (sys_main(argc, argv));
}
#endif /* _MSC_VER */


