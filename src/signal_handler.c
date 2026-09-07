/* =============================================================================
 * signal_handler.c -- Gestor sincrono de senales
 * =============================================================================*/

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include "signal_handler.h"

void signal_handler_init(signal_handler_t *sh, shared_t *shared, elevator_t *elevator)
{
    if (!sh || !shared || !elevator)
        return;
    sh->shared = shared;
    sh->elevator = elevator;
}

void *signal_handler_routine(void *arg)
{
    signal_handler_t *sh = (signal_handler_t *)arg;
    sigset_t set;
    int sig;

    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);

    while (!shared_is_terminando(sh->shared))
    {
        if (sigwait(&set, &sig) != 0)
        {
            perror("sigwait");
            continue;
        }

        switch (sig)
        {
        case SIGUSR1:
            if (elevator_crear_persona(sh->elevator) != ELEVATOR_OK)
            {
                fprintf(stderr, "Error al crear persona (señal SIGUSR1)\n");
            }
            break;
        case SIGINT:
        case SIGTERM:
            shared_set_terminando(sh->shared);
            break;
        default:
            fprintf(stderr, "Señal desconocida: %d\n", sig);
            break;
        }
    }
    return NULL;
}

void signal_handler_destroy(signal_handler_t *sh)
{
    (void)sh;
}