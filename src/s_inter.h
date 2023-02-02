#pragma once
#include <m_pd.h>

typedef void (*pd_gui_callback)(void*, char const*, t_atom*, t_atom*, t_atom*);
typedef void (*pd_message_callback)(void*, void*, t_symbol*, int, t_atom*);

void register_gui_triggers(t_pdinstance* instance, void* target, pd_gui_callback gui_callback, pd_message_callback message_callback);

void create_panel(int openpanel, char const* path, char const* snd);
void trigger_open_file(const char* file);
void update_gui();

void plugdata_forward_message(void *x, t_symbol *s, int argc, t_atom *argv);
