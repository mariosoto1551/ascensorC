/* =============================================================================
 * elevator.h -- Logica de movimiento del ascensor
 * =============================================================================
 * */

#ifndef ELEVATOR_H
#define ELEVATOR_H

#include <stdbool.h>
#include <pthread.h>

#include "shared.h"
#include "colas.h"
#include "persona.h"

/* -------------------------------------------------------------------------- */
#define MIN_PISOS 3 // n mínimo
#define MAX_PISOS 7 // n máximo
#define CAPACIDAD 8 // capacidad máxima del ascensor
#define PISO_PB 0   // Planta Baja (PB) = 0

/* ----------------------------------------------------------------------------
 * Tiempos de simulacion, en milisegundos.
 * -------------------------------------------------------------------------- */
#define MS_VIAJE_PISO 300 // moverse un piso (subir o bajar)
#define MS_PUERTAS 5      // abrir/cerrar puertas y subir/bajar gente

/* -----------------------------------------------------------------------------
 * Códigos de error específicos del módulo elevator
 * -------------------------------------------------------------------------- */
typedef enum
{
    ELEVATOR_OK = 0,
    ELEVATOR_ERR_NULL,          // Puntero nulo
    ELEVATOR_ERR_INVALID_PISOS, // Número de pisos fuera de rango
    ELEVATOR_ERR_MEMORY,        // Fallo en malloc/calloc
    ELEVATOR_ERR_TERMINATING,   // Intento de crear persona durante apagado
    ELEVATOR_ERR_THREAD_CREATE, // Fallo en pthread_create
    ELEVATOR_ERR_UNKNOWN        // Error desconocido
} elevator_status_t;

/* -----------------------------------------------------------------------------
 * Estados del ascensor
 * -----------------------------------------------------------------------------*/
enum
{
    ASCENSOR_BAJANDO = -1,
    ASCENSOR_DETENIDO = 0,
    ASCENSOR_SUBIENDO = +1
};

/* -----------------------------------------------------------------------------
 * Estructura del ascensor (seccion 4.1)
 * -------------------------------------------------------------------------- */
typedef struct
{
    int piso_actual;
    int estado;                      // ASCENSOR_SUBIENDO / ASCENSOR_BAJANDO / ASCENSOR_DETENIDO
    int personas_dentro;             //
    persona_t *pasajeros[CAPACIDAD]; /* NULL = asiento libre               */
} ascensor_t;

/* -----------------------------------------------------------------------------
 * Elevador
 * -------------------------------------------------------------------------- */
typedef struct elevator
{
    int num_pisos;
    cola_t *colas_subida;
    cola_t *colas_bajada;
    ascensor_t ascensor;
    shared_t *shared; // Puntero a la estructura compartida (para lock/cond/contadores)

    /* --- Anadidos respecto de la seccion 3.3 (ver docs/BITACORA.md) --- */
    int siguiente_id;         /* contador de ids de persona         */
    bool evacuacion_iniciada; /* para no evacuar dos veces          */
} elevator_t;

/* -----------------------------------------------------------------------------
 * SNAPSHOT
 * -------------------------------------------------------------------------- */
typedef struct
{
    int piso_ascensor;
    int estado_ascensor;
    int personas_dentro;
    int capacidad; /* copia de CAPACIDAD, por comodidad  */
    int num_pisos;
    int personas_activas; /* total dentro del edificio          */
    int total_esperando;  /* suma de todas las colas            */
    bool terminando;
    bool evacuando;
    int colas_subida[MAX_PISOS]; /* cuantos esperan por piso           */
    int colas_bajada[MAX_PISOS];
    int destinos[CAPACIDAD]; /* destino de cada pasajero; -1 libre */
} Snapshot;

/* =============================================================================
 * API publica
 * ========================================================================== */

elevator_status_t elevator_create(elevator_t **out, int num_pisos, shared_t *shared);

void elevator_destroy(elevator_t *e);

void *elevator_run(void *arg);

elevator_status_t elevator_crear_persona(elevator_t *e);

void elevator_encolar_persona(elevator_t *e, persona_t *p, int destino);

void elevator_esperar_llegada(elevator_t *e, persona_t *p);

Snapshot elevator_get_snapshot(elevator_t *e);

const char *elevator_estado_texto(int estado);

#endif
