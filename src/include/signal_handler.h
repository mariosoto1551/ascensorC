/* =============================================================================
 * signal_handler.h -- Gestor sincrono de senales
 * =============================================================================
 *
 * RESPONSABILIDAD
 *   Hilo dedicado a esperar senales con sigwait(), evitando la complejidad de
 *   los manejadores asincronos (async-signal-safety).
 *   Traduce senales del SO en operaciones del dominio.
 *
 * SENALES ATENDIDAS
 *   SIGUSR1           -> elevator_crear_persona()
 *   SIGINT / SIGTERM  -> shared_set_terminando() + shared_broadcast()
 *
 * API PUBLICA PLANIFICADA
 *   void *signal_handler_run(void *arg);
 *
 * NOTA DE DISENO
 *   El hilo necesita acceso tanto a shared_t como a elevator_t. Se resuelve
 *   con una estructura de contexto pasada como arg (a definir al implementar).
 *   El bloqueo de senales con pthread_sigmask() debe hacerse en main ANTES de
 *   crear cualquier hilo, para que todos las hereden bloqueadas.
 *
 * Ver: docs/diseno_logico.md, secciones 3.4 y 5.3.
 * -----------------------------------------------------------------------------
 * TODO: implementar -- ver el estado de fases en docs/BITACORA.md
 * ========================================================================== */
#ifndef SIGNAL_HANDLER_H
#define SIGNAL_HANDLER_H

#include "shared.h"
#include "elevator.h"

typedef struct signal_handler
{
    shared_t *shared;
    elevator_t *elevator;
} signal_handler_t;

int signal_handler_init(signal_handler_t *sh, shared_t *shared, elevator_t *elevator);
void *signal_handler_routine(void *arg);
void signal_handler_destroy(signal_handler_t *sh);

#endif