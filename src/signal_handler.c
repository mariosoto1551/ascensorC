/* =============================================================================
 * signal_handler.c -- Gestor sincrono de senales
 * =============================================================================
 *
 * Hilo que espera SIGUSR1 / SIGINT / SIGTERM con sigwait() y las traduce a operaciones del dominio.
 *
 * Contrato publico: ver include/signal_handler.h
 * Diseno detallado: docs/diseno_logico.md, seccion 3.4
 * -----------------------------------------------------------------------------
 * TODO: implementar -- ver el estado de fases en docs/BITACORA.md
 * ========================================================================== */
#include "signal_handler.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

int signal_handler_init(signal_handler_t *sh, shared_t *shared, elevator_t *elevator)
{
    if (!sh || !shared || !elevator)
        return -1;
    sh->shared = shared;
    sh->elevator = elevator;
    return 0;
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
            // Crear una nueva persona (elevator lo gestiona)
            elevator_crear_persona(sh->elevator);
            printf("signal_handler: SIGUSR1 recibido, creando persona\n");
            break;

        case SIGINT:
        case SIGTERM:
            // Iniciar apagado
            shared_set_terminando(sh->shared);
            printf("signal_handler: señal de terminación recibida, iniciando shutdown\n");
            break;

        default:
            fprintf(stderr, "signal_handler: señal desconocida %d\n", sig);
            break;
        }
    }

    return NULL;
}

void signal_handler_destroy(signal_handler_t *sh)
{
    // No hay recursos propios que liberar
    (void)sh;
}