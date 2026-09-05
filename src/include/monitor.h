/* =============================================================================
 * monitor.h -- Visualizacion en tiempo real (ncurses)
 * =============================================================================
 *
 * RESPONSABILIDAD
 *   Hilo que dibuja el estado del sistema con ncurses.
 *   SOLO LEE: obtiene un Snapshot y lo pinta. Nunca modifica datos de dominio.
 *
 * API PUBLICA PLANIFICADA
 *   void *monitor_run(void *arg);   // arg = elevator_t *
 *
 * BUCLE
 *   1. elevator_get_snapshot()  (toma y suelta el mutex internamente)
 *   2. dibuja pisos, colas de espera, posicion del ascensor, ocupacion
 *   3. refresca cada ~200 ms
 *   4. sale cuando shared_is_terminando() y restaura la terminal (endwin)
 *
 * REGLA
 *   Ninguna llamada a ncurses se hace con el mutex tomado.
 *
 * Ver: docs/diseno_logico.md, secciones 3.6 y 4.4.
 * -----------------------------------------------------------------------------
 * TODO: implementar -- ver el estado de fases en docs/BITACORA.md
 * ========================================================================== */
#ifndef MONITOR_H
#define MONITOR_H

#include "elevator.h"

void *monitor_run(void *arg);

#endif