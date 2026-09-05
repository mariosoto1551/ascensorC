/* =============================================================================
 * elevator.c -- Logica del ascensor
 * =============================================================================
 *
 * Colas por piso, estado del ascensor y algoritmo de movimiento SCAN.
 *
 * Contrato publico: ver include/elevator.h
 * Diseno detallado: docs/diseno_logico.md, secciones 3.3, 4.1 y 5.2
 *
 * -----------------------------------------------------------------------------
 * PROTOCOLO DE TRANSPORTE (contrato con persona.c)
 *
 *   Una persona sabe que llego a destino cuando  p->piso_actual == p->destino.
 *   Nadie mas escribe ese campo: el ascensor lo actualiza UNICAMENTE al
 *   descargarla. Mientras espera en la cola o viaja en la cabina, piso_actual
 *   conserva el piso donde subio, de modo que el predicado es falso.
 *
 *   persona.c hace:
 *       elevator_encolar_persona(e, p, <piso elegido>);
 *       elevator_esperar_llegada(e, p);     <- vuelve cuando ya llego
 *
 *   persona.c NO escribe p->destino ni p->piso_actual por su cuenta: los lee
 *   el ascensor con el mutex tomado, asi que escribirlos desde el hilo de la
 *   persona sin el candado seria una condicion de carrera. Los campos que si
 *   son suyos y nadie mas mira son jornada_restante y tiempo_trabajo.
 *
 * -----------------------------------------------------------------------------
 * REGLAS DE MUTEX QUE SIGUE ESTE ARCHIVO (seccion 6.1)
 *
 *   - Todo acceso a colas, cabina y banderas ocurre con el mutex de shared.
 *   - Ningun sleep() ocurre con el mutex tomado.
 *   - Las funciones estaticas de este archivo ASUMEN que el llamante ya tiene
 *     el mutex. Las publicas lo toman ellas mismas.
 * ========================================================================== */

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>

#include "elevator.h"

/* =============================================================================
 * Utilidades internas
 * ========================================================================== */

/* Duerme sin retener el mutex. Reintenta si una senal interrumpe la espera:
   en este programa las senales son frecuentes (SIGUSR1) y sin este bucle los
   tiempos de viaje se acortarian de forma impredecible. */
static void dormir_ms(int ms)
{
    struct timespec ts;
    ts.tv_sec  = (time_t)(ms / 1000);
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;

    while (nanosleep(&ts, &ts) == -1 && errno == EINTR) {
        /* ts quedo con el tiempo restante: seguimos durmiendo */
    }
}

/* La cola de `piso` que corresponde a `direccion`. Requiere el mutex. */
static cola_t *cola_de(elevator_t *e, int piso, int direccion)
{
    return (direccion == ASCENSOR_SUBIENDO) ? &e->colas_subida[piso]
                                            : &e->colas_bajada[piso];
}

/* Cuanta gente espera en un piso, sumando ambas direcciones. Requiere el mutex. */
static int esperando_en(const elevator_t *e, int piso)
{
    return cola_cantidad(&e->colas_subida[piso])
         + cola_cantidad(&e->colas_bajada[piso]);
}

/* Gente esperando en todo el edificio. Requiere el mutex. */
static int total_esperando(const elevator_t *e)
{
    int total = 0;
    for (int i = 0; i < e->num_pisos; i++) {
        total += esperando_en(e, i);
    }
    return total;
}

/* =============================================================================
 * Algoritmo SCAN
 * ========================================================================== */

/* Hay motivo para seguir subiendo: un pasajero a bordo con destino mas arriba,
   o alguien esperando en un piso superior. Requiere el mutex. */
static bool hay_trabajo_arriba(const elevator_t *e)
{
    int piso = e->ascensor.piso_actual;

    for (int i = 0; i < CAPACIDAD; i++) {
        const persona_t *p = e->ascensor.pasajeros[i];
        if (p != NULL && p->destino > piso) {
            return true;
        }
    }
    for (int i = piso + 1; i < e->num_pisos; i++) {
        if (esperando_en(e, i) > 0) {
            return true;
        }
    }
    return false;
}

/* Simetrica de la anterior. Requiere el mutex. */
static bool hay_trabajo_abajo(const elevator_t *e)
{
    int piso = e->ascensor.piso_actual;

    for (int i = 0; i < CAPACIDAD; i++) {
        const persona_t *p = e->ascensor.pasajeros[i];
        if (p != NULL && p->destino < piso) {
            return true;
        }
    }
    for (int i = piso - 1; i >= 0; i--) {
        if (esperando_en(e, i) > 0) {
            return true;
        }
    }
    return false;
}

/* Queda algo por hacer en el edificio. Requiere el mutex. */
static bool hay_trabajo(const elevator_t *e)
{
    return e->ascensor.ocupacion > 0 || total_esperando(e) > 0;
}

/*
 * Corazon del SCAN (seccion 5.2, punto 2): se mantiene el rumbo mientras quede
 * algo que atender en esa direccion; recien cuando se agota, se invierte.
 * Esto es lo que evita que el ascensor oscile y lo que acota la espera maxima.
 *
 * Devuelve ASCENSOR_DETENIDO solo si no hay absolutamente nada que hacer.
 * Requiere el mutex.
 */
static int decidir_direccion(const elevator_t *e)
{
    switch (e->ascensor.estado) {
    case ASCENSOR_SUBIENDO:
        if (hay_trabajo_arriba(e)) return ASCENSOR_SUBIENDO;
        if (hay_trabajo_abajo(e))  return ASCENSOR_BAJANDO;
        return ASCENSOR_DETENIDO;

    case ASCENSOR_BAJANDO:
        if (hay_trabajo_abajo(e))  return ASCENSOR_BAJANDO;
        if (hay_trabajo_arriba(e)) return ASCENSOR_SUBIENDO;
        return ASCENSOR_DETENIDO;

    default: /* estaba detenido: toma lo que haya, empezando por arriba */
        if (hay_trabajo_arriba(e)) return ASCENSOR_SUBIENDO;
        if (hay_trabajo_abajo(e))  return ASCENSOR_BAJANDO;
        return ASCENSOR_DETENIDO;
    }
}

/*
 * Con la cabina vacia el rumbo anterior ya no obliga a nada, asi que se puede
 * adoptar el de quien este esperando en este mismo piso. Pero solo si en el
 * rumbo actual ya no queda trabajo: si todavia hay gente mas arriba y veniamos
 * subiendo, se sigue subiendo (SCAN puro) y a los que bajan se los recoge a la
 * vuelta.
 *
 * Requiere el mutex.
 */
static void ajustar_rumbo_si_vacio(elevator_t *e)
{
    if (e->ascensor.ocupacion != 0) {
        return;
    }

    int piso = e->ascensor.piso_actual;
    int dir  = e->ascensor.estado;

    if (dir == ASCENSOR_SUBIENDO && hay_trabajo_arriba(e)) return;
    if (dir == ASCENSOR_BAJANDO  && hay_trabajo_abajo(e))  return;

    if      (cola_cantidad(&e->colas_subida[piso]) > 0) dir = ASCENSOR_SUBIENDO;
    else if (cola_cantidad(&e->colas_bajada[piso]) > 0) dir = ASCENSOR_BAJANDO;
    else if (hay_trabajo_arriba(e))                     dir = ASCENSOR_SUBIENDO;
    else if (hay_trabajo_abajo(e))                      dir = ASCENSOR_BAJANDO;
    else                                                dir = ASCENSOR_DETENIDO;

    e->ascensor.estado = dir;
}

/* =============================================================================
 * Carga y descarga
 * ========================================================================== */

/* Baja a todos los pasajeros cuyo destino es este piso.
   Devuelve cuantos bajaron. Requiere el mutex. */
static int descargar(elevator_t *e)
{
    int piso    = e->ascensor.piso_actual;
    int bajaron = 0;

    for (int i = 0; i < CAPACIDAD; i++) {
        persona_t *p = e->ascensor.pasajeros[i];
        if (p != NULL && p->destino == piso) {
            p->piso_actual = piso;          /* protocolo de llegada */
            e->ascensor.pasajeros[i] = NULL;
            e->ascensor.ocupacion--;
            bajaron++;
        }
    }
    return bajaron;
}

/* Sube gente de la cola de este piso que va en la direccion actual, mientras
   quede lugar. Si el ascensor esta lleno simplemente no sube a nadie y el
   recorrido continua (seccion 5.2, punto 5).
   Devuelve cuantos subieron. Requiere el mutex. */
static int cargar(elevator_t *e)
{
    int piso = e->ascensor.piso_actual;
    int dir  = e->ascensor.estado;

    if (dir == ASCENSOR_DETENIDO) {
        return 0;
    }

    cola_t *cola     = cola_de(e, piso, dir);
    int     subieron = 0;

    while (e->ascensor.ocupacion < CAPACIDAD && !cola_vacia(cola)) {
        persona_t *p = cola_desencolar(cola);
        if (p == NULL) {
            break;                          /* defensivo: cola inconsistente */
        }
        for (int i = 0; i < CAPACIDAD; i++) {
            if (e->ascensor.pasajeros[i] == NULL) {
                e->ascensor.pasajeros[i] = p;
                break;
            }
        }
        e->ascensor.ocupacion++;
        subieron++;
    }
    return subieron;
}

/* =============================================================================
 * Evacuacion (seccion 5.3)
 * ========================================================================== */

/*
 * Se ejecuta una sola vez, en cuanto el ascensor observa que el sistema esta
 * terminando. Reorienta a todo el edificio hacia planta baja:
 *
 *   - Los pasajeros a bordo cambian su destino a PB.
 *   - Los que esperan para subir pasan a la cola de bajada de su piso.
 *   - Los que ya estan en PB esperando subir se dan por llegados y pueden irse.
 *
 * Esta es la unica parte del programa donde el ascensor escribe campos de una
 * persona que no sea piso_actual. Se hace aca, y no en persona.c, porque esa
 * gente esta guardada dentro de las colas, que son datos privados de este
 * modulo: persona.c no puede sacarlas de ahi sin romper el encapsulamiento.
 *
 * Requiere el mutex.
 */
static void iniciar_evacuacion(elevator_t *e)
{
    for (int i = 0; i < CAPACIDAD; i++) {
        persona_t *p = e->ascensor.pasajeros[i];
        if (p != NULL) {
            p->destino   = PISO_PB;
            p->direccion = ASCENSOR_BAJANDO;
        }
    }

    for (int piso = 0; piso < e->num_pisos; piso++) {
        while (!cola_vacia(&e->colas_subida[piso])) {
            persona_t *p = cola_desencolar(&e->colas_subida[piso]);
            if (p == NULL) {
                break;
            }
            p->destino = PISO_PB;

            if (piso == PISO_PB) {
                /* Ya esta donde tiene que estar: se da por llegada y sale. */
                p->piso_actual = PISO_PB;
                p->direccion   = ASCENSOR_DETENIDO;
            } else {
                p->direccion = ASCENSOR_BAJANDO;
                cola_encolar(&e->colas_bajada[piso], p);
            }
        }
    }

    e->evacuacion_iniciada = true;
}

/* Ya no queda nadie: ni a bordo, ni en las colas, ni trabajando en un piso.
   Requiere el mutex. */
static bool fin_de_jornada(const elevator_t *e)
{
    return shared_is_terminando_locked(e->shared)
        && shared_get_personas_locked(e->shared) == 0
        && e->ascensor.ocupacion == 0
        && total_esperando(e) == 0;
}

/* =============================================================================
 * Ciclo de vida
 * ========================================================================== */

elevator_t *elevator_create(int num_pisos, shared_t *shared)
{
    if (shared == NULL || num_pisos < MIN_PISOS || num_pisos > MAX_PISOS) {
        return NULL;
    }

    elevator_t *e = calloc(1, sizeof *e);
    if (e == NULL) {
        return NULL;
    }

    e->colas_subida = calloc((size_t)num_pisos, sizeof *e->colas_subida);
    e->colas_bajada = calloc((size_t)num_pisos, sizeof *e->colas_bajada);
    if (e->colas_subida == NULL || e->colas_bajada == NULL) {
        free(e->colas_subida);
        free(e->colas_bajada);
        free(e);
        return NULL;
    }

    for (int i = 0; i < num_pisos; i++) {
        cola_init(&e->colas_subida[i]);
        cola_init(&e->colas_bajada[i]);
    }

    e->num_pisos           = num_pisos;
    e->shared              = shared;
    e->siguiente_id        = 1;
    e->evacuacion_iniciada = false;

    e->ascensor.piso_actual = PISO_PB;
    e->ascensor.estado      = ASCENSOR_DETENIDO;
    e->ascensor.ocupacion   = 0;
    for (int i = 0; i < CAPACIDAD; i++) {
        e->ascensor.pasajeros[i] = NULL;
    }

    return e;
}

void elevator_destroy(elevator_t *e)
{
    if (e == NULL) {
        return;
    }

    /* No se liberan las persona_t: cada hilo persona libera la suya al salir.
       Si el apagado fue ordenado, a esta altura las colas ya estan vacias. */
    for (int i = 0; i < e->num_pisos; i++) {
        cola_destroy(&e->colas_subida[i]);
        cola_destroy(&e->colas_bajada[i]);
    }

    free(e->colas_subida);
    free(e->colas_bajada);
    free(e);
}

/* =============================================================================
 * Bucle principal del ascensor
 * ========================================================================== */

/*
 * Una vuelta del bucle = atender el piso donde esta parado + moverse un piso.
 *
 * El orden importa: primero se atiende el piso actual y despues se decide el
 * movimiento. Al reves, el ascensor arrancaria en PB, se iria al piso 1 y
 * recien ahi cargaria, dejando plantada a la gente de PB.
 */
void *elevator_run(void *arg)
{
    elevator_t *e = (elevator_t *)arg;
    shared_t   *s = e->shared;

    for (;;) {
        shared_lock(s);

        /* --- 0. Apagado pedido: reorientar el edificio hacia PB, una vez --- */
        if (shared_is_terminando_locked(s) && !e->evacuacion_iniciada) {
            iniciar_evacuacion(e);
            shared_broadcast(s);
        }

        /* --- 1. Atender el piso actual: primero bajan, despues suben ------- */
        int bajaron = descargar(e);
        ajustar_rumbo_si_vacio(e);
        int subieron = cargar(e);

        shared_set_llamadas_locked(s, total_esperando(e) > 0);

        bool hubo_parada = (bajaron > 0 || subieron > 0);
        if (hubo_parada) {
            /* Despierta a los que acaban de llegar a destino. Es broadcast y no
               signal a proposito: sobre esta unica condicion esperan tanto las
               personas como el ascensor, y un signal podria despertar al hilo
               equivocado y perder el aviso. */
            shared_broadcast(s);
        }

        /* --- 2. Sin trabajo: dormir en la condicion (nunca espera activa) -- */
        while (!hay_trabajo(e) && !fin_de_jornada(e)) {
            e->ascensor.estado = ASCENSOR_DETENIDO;
            shared_wait(s);

            /* Puede haberse pedido el apagado mientras dormiamos. */
            if (shared_is_terminando_locked(s) && !e->evacuacion_iniciada) {
                iniciar_evacuacion(e);
                shared_broadcast(s);
            }
        }

        if (fin_de_jornada(e)) {
            e->ascensor.estado = ASCENSOR_DETENIDO;
            shared_unlock(s);
            break;
        }

        /* --- 3. Elegir rumbo ---------------------------------------------- */
        int dir = decidir_direccion(e);
        e->ascensor.estado = dir;

        if (dir == ASCENSOR_DETENIDO) {
            /* Hay gente, pero toda en este piso y ya la atendimos arriba
               (por ejemplo, la cabina esta llena). Se vuelve a empezar. */
            shared_unlock(s);
            if (hubo_parada) {
                dormir_ms(MS_PUERTAS);
            } else {
                dormir_ms(MS_VIAJE_PISO);
            }
            continue;
        }

        shared_unlock(s);

        /* --- 4. Tiempos de simulacion, siempre fuera del mutex ------------- */
        if (hubo_parada) {
            dormir_ms(MS_PUERTAS);
        }
        dormir_ms(MS_VIAJE_PISO);

        /* --- 5. Avanzar un piso ------------------------------------------- */
        shared_lock(s);
        e->ascensor.piso_actual += dir;
        shared_unlock(s);
    }

    /* Que nadie quede dormido esperando a un ascensor que ya no existe. */
    shared_lock(s);
    shared_broadcast(s);
    shared_unlock(s);

    return NULL;
}

/* =============================================================================
 * Entrada de gente al edificio
 * ========================================================================== */

void elevator_crear_persona(elevator_t *e)
{
    if (e == NULL) {
        return;
    }

    shared_t *s = e->shared;

    /* Durante el apagado no entra gente nueva: si entrara, el contador de
       personas activas podria no llegar nunca a cero y el programa no cerraria. */
    shared_lock(s);
    if (shared_is_terminando_locked(s)) {
        shared_unlock(s);
        return;
    }
    int id = e->siguiente_id++;
    shared_unlock(s);

    persona_t *p = persona_create(e, id, PISO_PB);
    if (p == NULL) {
        return;
    }

    /* Se incrementa ANTES de crear el hilo: si se hiciera despues, existiria un
       instante con una persona viva que el contador no ve, y el ascensor podria
       dar la jornada por terminada. */
    shared_inc_personas(s);

    pthread_t hilo;
    if (pthread_create(&hilo, NULL, persona_run, p) != 0) {
        shared_dec_personas(s);
        persona_destroy(p);

        shared_lock(s);
        shared_broadcast(s);
        shared_unlock(s);
        return;
    }

    /* Desacoplado: nadie hace join de las personas. La sincronizacion del
       apagado se hace con el contador personas_activas, no con pthread_join.
       Ojo: despues de pthread_create no se puede tocar *p desde aca, porque el
       hilo ya puede haber terminado y liberado la estructura. Es persona_run()
       quien guarda su propio pthread_self() en p->hilo. */
    pthread_detach(hilo);
}

/* =============================================================================
 * API para persona.c
 * ========================================================================== */

void elevator_encolar_persona(elevator_t *e, persona_t *p, int destino)
{
    if (e == NULL || p == NULL) {
        return;
    }
    if (destino < 0 || destino >= e->num_pisos) {
        return;                             /* destino invalido: se ignora */
    }

    shared_t *s = e->shared;
    shared_lock(s);

    /* El destino se escribe aca, con el mutex tomado, porque el ascensor lo lee
       protegido. Si lo escribiera persona.c por su cuenta seria un data race. */
    p->destino = destino;

    if (p->destino == p->piso_actual) {
        /* No hay viaje que hacer. Se despierta igual para que persona.c salga
           de su espera en vez de quedarse colgada. */
        p->direccion = ASCENSOR_DETENIDO;
        shared_broadcast(s);
        shared_unlock(s);
        return;
    }

    p->direccion = (p->destino > p->piso_actual) ? ASCENSOR_SUBIENDO
                                                 : ASCENSOR_BAJANDO;
    cola_encolar(cola_de(e, p->piso_actual, p->direccion), p);

    shared_set_llamadas_locked(s, true);
    shared_broadcast(s);
    shared_unlock(s);
}

void elevator_esperar_llegada(elevator_t *e, persona_t *p)
{
    if (e == NULL || p == NULL) {
        return;
    }

    shared_t *s = e->shared;
    shared_lock(s);

    /* while y no if: la condicion se comparte con el ascensor y con las demas
       personas, asi que hay despertares que no son para nosotros (seccion 6.2). */
    while (p->piso_actual != p->destino) {
        shared_wait(s);
    }

    shared_unlock(s);
}

/* =============================================================================
 * Lectura para el monitor
 * ========================================================================== */

Snapshot elevator_get_snapshot(elevator_t *e)
{
    Snapshot snap;
    memset(&snap, 0, sizeof snap);

    for (int i = 0; i < CAPACIDAD; i++) {
        snap.destinos[i] = -1;
    }
    snap.capacidad = CAPACIDAD;

    if (e == NULL) {
        return snap;
    }

    shared_t *s = e->shared;
    shared_lock(s);

    snap.num_pisos        = e->num_pisos;
    snap.piso_ascensor    = e->ascensor.piso_actual;
    snap.estado_ascensor  = e->ascensor.estado;
    snap.ocupacion        = e->ascensor.ocupacion;
    snap.personas_activas = shared_get_personas_locked(s);
    snap.terminando       = shared_is_terminando_locked(s);
    snap.evacuando        = e->evacuacion_iniciada;

    for (int i = 0; i < e->num_pisos && i < MAX_PISOS; i++) {
        snap.colas_subida[i] = cola_cantidad(&e->colas_subida[i]);
        snap.colas_bajada[i] = cola_cantidad(&e->colas_bajada[i]);
    }
    snap.total_esperando = total_esperando(e);

    for (int i = 0; i < CAPACIDAD; i++) {
        const persona_t *p = e->ascensor.pasajeros[i];
        snap.destinos[i] = (p != NULL) ? p->destino : -1;
    }

    shared_unlock(s);
    return snap;
}

const char *elevator_estado_texto(int estado)
{
    switch (estado) {
    case ASCENSOR_SUBIENDO: return "SUBIENDO";
    case ASCENSOR_BAJANDO:  return "BAJANDO";
    default:                return "DETENIDO";
    }
}
