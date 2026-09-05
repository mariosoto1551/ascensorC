/* =============================================================================
 * elevator.c -- Logica del ascensor
 * =============================================================================
 *
 * Colas por piso, estado del ascensor y algoritmo de movimiento SCAN.
 *
 * Contrato publico: ver include/elevator.h
 * Diseno detallado: docs/diseno_logico.md, seccion 3.3
 * -----------------------------------------------------------------------------
 * TODO: implementar -- ver el estado de fases en docs/BITACORA.md
 * ========================================================================== */
#include "include/elevator.h"
#include <stdio.h>
#include <stdlib.h>

struct elevator
{
    int num_pisos;
    shared_t *shared;
    // ... colas, ascensor, etc.
};

elevator_t *elevator_create(int num_pisos, shared_t *shared)
{
    elevator_t *e = malloc(sizeof(elevator_t));
    if (!e)
        return NULL;
    e->num_pisos = num_pisos;
    e->shared = shared;
    // TODO: inicializar colas, ascensor, etc.
    printf("Elevator creado con %d pisos\n", num_pisos);
    return e;
}

void elevator_destroy(elevator_t *e)
{
    // TODO: liberar colas, etc.
    free(e);
    printf("Elevator destruido\n");
}

void elevator_crear_persona(elevator_t *e)
{
    // TODO: crear hilo persona y encolarla
    printf("Elevator: persona creada (stub)\n");
}

void *elevator_run(void *arg)
{
    elevator_t *e = (elevator_t *)arg;
    // TODO: bucle principal de movimiento
    printf("Elevator: iniciando bucle (stub)\n");
    while (!shared_is_terminando(e->shared))
    {
        // Simular trabajo
        sleep(1);
    }
    printf("Elevator: terminando\n");
    return NULL;
}