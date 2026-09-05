/* =============================================================================
 * elevator.h -- Logica de movimiento del ascensor
 * =============================================================================
 *
 * RESPONSABILIDAD
 *   Dueno de los datos de dominio: las colas por piso, la estructura del
 *   ascensor y la logica de movimiento (algoritmo SCAN).
 *   Usa el mutex de shared para proteger sus datos; no crea uno propio.
 *
 * Ver: docs/diseno_logico.md, secciones 3.3, 4.1, 4.4 y 5.2.
 * ========================================================================== */

#ifndef ELEVATOR_H
#define ELEVATOR_H

#include <stdbool.h>
#include <pthread.h>

#include "shared.h"
#include "colas.h"
#include "persona.h"

/* -----------------------------------------------------------------------------
 * Limites del modelo (seccion 1 del diseno logico)
 *
 * MAX_PISOS y CAPACIDAD viven aca y no en un config.h aparte porque los unicos
 * que los necesitan son las estructuras de este modulo (ascensor_t y Snapshot).
 * monitor.c los recibe incluyendo este header.
 * -------------------------------------------------------------------------- */
#define MIN_PISOS    3      /* n minimo segun el enunciado                     */
#define MAX_PISOS   10      /* X: n maximo de diseno                           */
#define CAPACIDAD    8      /* Y: capacidad maxima del ascensor                */
#define PISO_PB      0      /* planta baja: entrada y salida del edificio      */

/* -----------------------------------------------------------------------------
 * Tiempos de simulacion, en milisegundos.
 * Se duerme SIEMPRE fuera del mutex (regla de oro, seccion 6.1).
 * -------------------------------------------------------------------------- */
#define MS_VIAJE_PISO   600     /* moverse un piso                             */
#define MS_PUERTAS      400     /* abrir/cerrar puertas al cargar o descargar  */

/* -----------------------------------------------------------------------------
 * Estado del ascensor y direccion de una persona.
 *
 * Los valores son -1 / 0 / +1 a proposito: permiten aritmetica directa
 * (piso_actual += estado) y coinciden con el convenio de persona_t.direccion
 * declarado en la seccion 4.3 del diseno ("+1 subir, -1 bajar").
 * -------------------------------------------------------------------------- */
enum {
    ASCENSOR_BAJANDO  = -1,
    ASCENSOR_DETENIDO =  0,
    ASCENSOR_SUBIENDO = +1
};

/* -----------------------------------------------------------------------------
 * Estructura del ascensor (seccion 4.1)
 * -------------------------------------------------------------------------- */
typedef struct {
    int        piso_actual;
    int        estado;                    /* ASCENSOR_DETENIDO/SUBIENDO/BAJANDO */
    int        ocupacion;                 /* cuantos asientos ocupados          */
    persona_t *pasajeros[CAPACIDAD];      /* NULL = asiento libre               */
} ascensor_t;

/* -----------------------------------------------------------------------------
 * El modulo completo (seccion 3.3)
 *
 * colas_subida y colas_bajada son ARREGLOS de num_pisos colas, reservados
 * dinamicamente en elevator_create(). colas_subida[i] son los que esperan en
 * el piso i para subir; colas_bajada[i], los que esperan ahi para bajar.
 * -------------------------------------------------------------------------- */
typedef struct elevator {
    int         num_pisos;
    cola_t     *colas_subida;
    cola_t     *colas_bajada;
    ascensor_t  ascensor;
    shared_t   *shared;                   /* prestado: NO se libera aqui        */

    /* --- Anadidos respecto de la seccion 3.3 (ver docs/BITACORA.md) --- */
    int         siguiente_id;             /* contador de ids de persona         */
    bool        evacuacion_iniciada;      /* para no evacuar dos veces          */
} elevator_t;

/* -----------------------------------------------------------------------------
 * Snapshot para el monitor (seccion 4.4)
 *
 * Se devuelve POR VALOR: el monitor lo recibe ya copiado y puede dibujarlo sin
 * tomar el mutex. Es lo que permite cumplir "nunca ncurses con el mutex".
 * -------------------------------------------------------------------------- */
typedef struct {
    int  piso_ascensor;
    int  estado_ascensor;
    int  ocupacion;
    int  capacidad;                       /* copia de CAPACIDAD, por comodidad  */
    int  num_pisos;
    int  personas_activas;                /* total dentro del edificio          */
    int  total_esperando;                 /* suma de todas las colas            */
    bool terminando;
    bool evacuando;
    int  colas_subida[MAX_PISOS];         /* cuantos esperan por piso           */
    int  colas_bajada[MAX_PISOS];
    int  destinos[CAPACIDAD];             /* destino de cada pasajero; -1 libre */
} Snapshot;

/* =============================================================================
 * API publica
 * ========================================================================== */

/* --- Ciclo de vida (lo llama main.c) --------------------------------------- */

/* Devuelve NULL si num_pisos esta fuera de [MIN_PISOS, MAX_PISOS], si shared es
   NULL o si falta memoria. El ascensor arranca DETENIDO en planta baja. */
elevator_t *elevator_create(int num_pisos, shared_t *shared);

/* Libera colas y estructura. Precondicion: el edificio ya esta vacio y el hilo
   elevator_run() ya termino. No libera el shared_t. */
void        elevator_destroy(elevator_t *e);

/* Bucle principal del hilo del ascensor. arg = elevator_t *. */
void       *elevator_run(void *arg);

/* --- Entrada de gente (lo llama signal_handler.c ante SIGUSR1) ------------- */

/* Reserva la persona y lanza su hilo (desacoplado con pthread_detach).
   Si el sistema ya esta terminando, no hace nada: durante el apagado no entra
   gente nueva al edificio. */
void        elevator_crear_persona(elevator_t *e);

/* --- Usadas por persona.c -------------------------------------------------- */

/* Fija el destino de la persona, la pone en la cola de su piso actual en la
   direccion que corresponda y despierta al ascensor.

   El destino viaja como PARAMETRO y no se escribe desde persona.c a proposito:
   p->destino lo lee el ascensor con el mutex tomado (en el SCAN y al
   descargar), asi que escribirlo desde el hilo de la persona sin el mutex
   seria una condicion de carrera. Aca se escribe con el candado ya tomado.

   Si destino == p->piso_actual no encola nada: no hay viaje que hacer. */
void        elevator_encolar_persona(elevator_t *e, persona_t *p, int destino);

/* Bloquea al hilo llamante hasta que el ascensor lo deje en su destino.
   Encapsula el protocolo de llegada para que persona.c no tenga que conocerlo. */
void        elevator_esperar_llegada(elevator_t *e, persona_t *p);

/* --- Solo lectura (lo llama monitor.c) ------------------------------------- */

/* Toma y suelta el mutex internamente. NO llamar con el mutex ya tomado. */
Snapshot    elevator_get_snapshot(elevator_t *e);

/* "DETENIDO" / "SUBIENDO" / "BAJANDO". Nunca devuelve NULL. */
const char *elevator_estado_texto(int estado);

#endif /* ELEVATOR_H */
