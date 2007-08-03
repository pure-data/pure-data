/*
====================================================================
DLL to perform action when program shuts down
====================================================================
*/

#include "windows.h"
#include "pmdll.h"

static close_fn_ptr_type close_function = NULL;


DLL_EXPORT pm_set_close_function(close_fn_ptr_type close_fn_ptr)
{
    close_function = close_fn_ptr;
}


static void Initialize( void ) {
    return;
}

static void Terminate( void ) {
    if (close_function) {
        (*close_function)();
    }
}


BOOL WINAPI DllMain(HINSTANCE hinstDLL, //DLL module handle
					DWORD fdwReason,	//for calling function
					LPVOID lbpvReserved)//reserved
{
	switch(fdwReason) {
		case DLL_PROCESS_ATTACH:
			/* when DLL starts, run this */
			Initialize();
			break;
		case DLL_PROCESS_DETACH:
			/* when DLL ends, this run (note: verified this	run */
			Terminate();
			break;
		default:
			break;
	}
	return TRUE;
}


