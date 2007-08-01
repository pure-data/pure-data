#define DLL_EXPORT __declspec( dllexport )

typedef void (*close_fn_ptr_type)();

DLL_EXPORT pm_set_close_function(close_fn_ptr_type close_fn_ptr);
