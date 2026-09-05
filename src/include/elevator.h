/* =============================================================================
 * elevator.h -- Logica de movimiento del ascensor
 * =============================================================================
 *
 * RESPONSABILIDAD
 *   Dueno de los datos de dominio: las colas por piso, la estructura del
 *   ascensor y la logica de movimiento (algoritmo SCAN).
 *   Usa el mutex de shared para proteger sus datos; no crea uno propio.
 *
 * TIPOS PRINCIPALES
 *   ascensor_t { piso_actual, estado, ocupacion, pasajeros[] }
 *   elevator_t { num_pisos, colas_subida, colas_bajada, ascensor, shared }
 *   Snapshot   { piso_ascensor, estado_ascensor, ocupacion,
 *                colas_subida[], colas_bajada[] }
 *
 * API PUBLICA PLANIFICADA
 *   elevator_t *elevator_create(int num_pisos, shared_t *shared);
 *   void        elevator_destroy(elevator_t *e);
 *   void        elevator_crear_persona(elevator_t *e);
 *   void       *elevator_run(void *arg);          // bucle principal del hilo
 *   Snapshot    elevator_get_snapshot(elevator_t *e);
 *
 * COMPORTAMIENTO
 *   Cuando no hay llamadas ni pasajeros llama a shared_wait() y duerme.
 *   El tiempo de viaje se simula con sleep() FUERA del mutex.
 *
 * Ver: docs/diseno_logico.md, secciones 3.3, 4.1 y 5.2.
 * -----------------------------------------------------------------------------
 * TODO: implementar -- ver el estado de fases en docs/BITACORA.md
 *
 * ========================================================================== */

#ifndef ELEVATOR_H
#define ELEVATOR_H

#include "shared.h"
#include <stdbool.h>

typedef struct elevator elevator_t; // Opaco

elevator_t *elevator_create(int num_pisos, shared_t *shared);
void elevator_destroy(elevator_t *e);
void elevator_crear_persona(elevator_t *e);
void *elevator_run(void *arg);

#endif
