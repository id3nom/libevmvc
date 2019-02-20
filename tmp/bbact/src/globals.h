#ifndef BBACT_GLOBALS_H
#define BBACT_GLOBALS_H

#include "stable_headers.h"

struct event_base* ev_base()
{
    static struct event_base* _ev_base = nullptr;
    if(_ev_base == nullptr){
        event_enable_debug_mode();
        _ev_base = event_base_new();
    }
        
    return _ev_base;
}

#endif // BBACT_GLOBALS_H
