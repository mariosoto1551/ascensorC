#ifndef SIGNAL_HANDLER_H
#define SIGNAL_HANDLER_H

#include "shared.h"
#include "elevator.h"

typedef struct signal_handler
{
    shared_t *shared;
    elevator_t *elevator;
} signal_handler_t;

void signal_handler_init(signal_handler_t *sh, shared_t *shared, elevator_t *elevator);
void *signal_handler_routine(void *arg);
void signal_handler_destroy(signal_handler_t *sh);

#endif