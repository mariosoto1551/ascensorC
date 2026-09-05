/* =============================================================================
 * elevator.c -- Lógica del ascensor
 * =============================================================================
 */
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>

#include "elevator.h"

/* =============================================================================
 * Utilidades internas
 * ========================================================================== */

/* Duerme sin retener el mutex. Reintenta si una señal interrumpe la espera. */
static void dormir_ms(int ms)
{
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;

    for (;;)
    {
        if (nanosleep(&ts, &ts) == 0)
            break; // Dormido completo
        if (errno != EINTR)
            break; // Error grave, salimos
        // Si fue EINTR, ts ya tiene el tiempo restante, continuamos
    }
}

// Obtener la cola de subida o bajada de un piso según la dirección. Requiere el mutex.
static cola_t *cola_de(elevator_t *e, int piso, int direccion)
{
    return (direccion == ASCENSOR_SUBIENDO) ? &e->colas_subida[piso]
                                            : &e->colas_bajada[piso];
}

// Obtener la cantidad de personas esperando en un piso (subida + bajada). Requiere el mutex.
static int esperando_en(const elevator_t *e, int piso)
{
    return cola_cantidad(&e->colas_subida[piso]) + cola_cantidad(&e->colas_bajada[piso]);
}

// Obtener el total de personas esperando en todo el edificio (subida + bajada). Requiere el mutex.
static int total_esperando(const elevator_t *e)
{
    int total = 0;
    for (int i = 0; i < e->num_pisos; i++)
    {
        total += esperando_en(e, i);
    }
    return total;
}

/* =============================================================================
 * Algoritmo SCAN
 * ========================================================================== */

static bool hay_trabajo_arriba(const elevator_t *e)
{
    int piso = e->ascensor.piso_actual; // Obtener piso actual de referencia

    // Verificar si hay personas dentro del ascensor que quieran ir a pisos superiores
    for (int i = 0; i < CAPACIDAD; i++)
    {
        const persona_t *p = e->ascensor.pasajeros[i];
        if (p != NULL && p->destino > piso)
            return true;
    }

    // Verificar si hay personas esperando en pisos superiores
    for (int i = piso + 1; i < e->num_pisos; i++)
    {
        if (esperando_en(e, i) > 0)
            return true;
    }
    return false;
}

static bool hay_trabajo_abajo(const elevator_t *e)
{
    int piso = e->ascensor.piso_actual; // Obtener piso actual de referencia

    // Verificar si hay personas dentro del ascensor que quieran ir a pisos inferiores
    for (int i = 0; i < CAPACIDAD; i++)
    {
        const persona_t *p = e->ascensor.pasajeros[i];
        if (p != NULL && p->destino < piso)
            return true;
    }

    // Verificar si hay personas esperando en pisos inferiores
    for (int i = piso - 1; i >= 0; i--)
    {
        if (esperando_en(e, i) > 0)
            return true;
    }
    return false;
}

// Verifica si el ascensor se utiliza o se duerme
static bool hay_trabajo(const elevator_t *e)
{
    return e->ascensor.personas_dentro > 0 || total_esperando(e) > 0;
}

static int decidir_direccion(const elevator_t *e)
{
    switch (e->ascensor.estado)
    {
    case ASCENSOR_SUBIENDO:
        if (hay_trabajo_arriba(e))
            return ASCENSOR_SUBIENDO;
        if (hay_trabajo_abajo(e))
            return ASCENSOR_BAJANDO;
        return ASCENSOR_DETENIDO;

    case ASCENSOR_BAJANDO:
        if (hay_trabajo_abajo(e))
            return ASCENSOR_BAJANDO;
        if (hay_trabajo_arriba(e))
            return ASCENSOR_SUBIENDO;
        return ASCENSOR_DETENIDO;

    default:
        if (hay_trabajo_arriba(e))
            return ASCENSOR_SUBIENDO;
        if (hay_trabajo_abajo(e))
            return ASCENSOR_BAJANDO;
        return ASCENSOR_DETENIDO;
    }
}

static void ajustar_rumbo_si_vacio(elevator_t *e)
{
    if (e->ascensor.personas_dentro != 0)
        return;

    int piso = e->ascensor.piso_actual;
    int dir = e->ascensor.estado;

    if ((dir == ASCENSOR_SUBIENDO && hay_trabajo_arriba(e)) ||
        (dir == ASCENSOR_BAJANDO && hay_trabajo_abajo(e)))
        return;

    // Prioridad local en el mismo piso
    if (cola_cantidad(&e->colas_subida[piso]) > 0)
        dir = ASCENSOR_SUBIENDO;
    else if (cola_cantidad(&e->colas_bajada[piso]) > 0)
        dir = ASCENSOR_BAJANDO;

    // Si no hay trabajo en el piso actual, buscar trabajo arriba o abajo
    else if (hay_trabajo_arriba(e))
        dir = ASCENSOR_SUBIENDO;
    else if (hay_trabajo_abajo(e))
        dir = ASCENSOR_BAJANDO;

    // Si no hay trabajo en ningún lado, detenerse
    else
        dir = ASCENSOR_DETENIDO;

    e->ascensor.estado = dir;
}

/* =============================================================================
 * Carga y descarga
 * ========================================================================== */

static int descargar(elevator_t *e)
{
    int piso = e->ascensor.piso_actual;
    int personas_bajaron = 0;

    for (int i = 0; i < CAPACIDAD; i++)
    {
        persona_t *p = e->ascensor.pasajeros[i];
        if (p != NULL && p->destino == piso)
        {
            p->piso_actual = piso;           // Actualizar piso actual de la persona
            e->ascensor.pasajeros[i] = NULL; // Libera el asiento
            e->ascensor.personas_dentro--;   // Descuenta la cantidad dentro
            personas_bajaron++;
        }
    }
    return personas_bajaron;
}

static int cargar(elevator_t *e)
{
    int piso = e->ascensor.piso_actual;
    int dir = e->ascensor.estado;

    if (dir == ASCENSOR_DETENIDO)
        return 0;

    cola_t *cola = cola_de(e, piso, dir); // Selección de cola de acuerdo a la dirección del ascensor
    int personas_subieron = 0;

    while (e->ascensor.personas_dentro < CAPACIDAD && !cola_vacia(cola))
    {
        persona_t *p = cola_desencolar(cola); // La persona entra al ascensor
        if (p == NULL)
            break;

        for (int i = 0; i < CAPACIDAD; i++)
        {
            if (e->ascensor.pasajeros[i] == NULL)
            {
                e->ascensor.pasajeros[i] = p; // La persona ahora está en el ascensor
                break;
            }
        }
        e->ascensor.personas_dentro++;
        personas_subieron++;
    }
    return personas_subieron;
}

/* =============================================================================
 * Evacuación (apagado controlado)
 * ========================================================================== */

static void iniciar_evacuacion(elevator_t *e)
{
    /* --- 1. Pasajeros dentro del ascensor --- */
    for (int i = 0; i < CAPACIDAD; i++)
    {
        persona_t *p = e->ascensor.pasajeros[i];
        if (p != NULL)
        {
            p->destino = PISO_PB;
            p->direccion = ASCENSOR_BAJANDO;
            p->jornada_restante = 0;
        }
    }

    /* --- 2. Colas de SUBIDA: mover a BAJADA --- */
    for (int piso = 0; piso < e->num_pisos; piso++)
    {
        while (!cola_vacia(&e->colas_subida[piso]))
        {
            persona_t *p = cola_desencolar(&e->colas_subida[piso]);
            if (p == NULL)
                break;

            p->destino = PISO_PB;
            p->jornada_restante = 0;

            if (piso == PISO_PB)
            {
                p->piso_actual = PISO_PB;
                p->direccion = ASCENSOR_DETENIDO;
            }
            else
            {
                p->direccion = ASCENSOR_BAJANDO;
                cola_encolar(&e->colas_bajada[piso], p);
            }
        }
    }

    /* --- 3. Colas de BAJADA ---*/
    for (int piso = 0; piso < e->num_pisos; piso++)
    {
        int count = cola_cantidad(&e->colas_bajada[piso]);
        for (int j = 0; j < count; j++)
        {
            persona_t *p = cola_desencolar(&e->colas_bajada[piso]);
            if (p == NULL)
                break;
            p->destino = PISO_PB;
            p->jornada_restante = 0;
            cola_encolar(&e->colas_bajada[piso], p);
        }
    }

    e->evacuacion_iniciada = true;
}

static bool fin_de_jornada(const elevator_t *e)
{
    shared_t *s = e->shared;
    return s->terminando && s->personas_activas == 0 && e->ascensor.personas_dentro == 0 && total_esperando(e) == 0;
}

/* =============================================================================
 * Ciclo de vida
 * ========================================================================== */

elevator_status_t elevator_create(elevator_t **out, int num_pisos, shared_t *shared)
{
    if (out == NULL)
        return ELEVATOR_ERR_NULL;
    if (shared == NULL)
        return ELEVATOR_ERR_NULL;
    if (num_pisos < MIN_PISOS || num_pisos > MAX_PISOS)
        return ELEVATOR_ERR_INVALID_PISOS;

    elevator_t *e = calloc(1, sizeof *e);
    if (e == NULL)
        return ELEVATOR_ERR_MEMORY;

    e->colas_subida = calloc((size_t)num_pisos, sizeof *e->colas_subida);
    e->colas_bajada = calloc((size_t)num_pisos, sizeof *e->colas_bajada);
    if (e->colas_subida == NULL || e->colas_bajada == NULL)
    {
        free(e->colas_subida);
        free(e->colas_bajada);
        free(e);
        return ELEVATOR_ERR_MEMORY;
    }

    for (int i = 0; i < num_pisos; i++)
    {
        cola_init(&e->colas_subida[i]);
        cola_init(&e->colas_bajada[i]);
    }

    e->num_pisos = num_pisos;
    e->shared = shared;
    e->siguiente_id = 1;
    e->evacuacion_iniciada = false;

    e->ascensor.piso_actual = PISO_PB;
    e->ascensor.estado = ASCENSOR_DETENIDO;
    e->ascensor.personas_dentro = 0;
    for (int i = 0; i < CAPACIDAD; i++)
        e->ascensor.pasajeros[i] = NULL;

    *out = e; // <-- Asignamos el puntero de salida
    return ELEVATOR_OK;
}

void elevator_destroy(elevator_t *e)
{
    if (e == NULL)
        return;

    for (int i = 0; i < e->num_pisos; i++)
    {
        cola_destroy(&e->colas_subida[i]);
        cola_destroy(&e->colas_bajada[i]);
    }

    free(e->colas_subida);
    free(e->colas_bajada);
    free(e);
}

/* =============================================================================
 * Entrada de gente (desde signal_handler)
 * ========================================================================== */
elevator_status_t elevator_crear_persona(elevator_t *e)
{
    if (e == NULL)
        return ELEVATOR_ERR_NULL;

    shared_t *s = e->shared;

    pthread_mutex_lock(&s->mutex);
    if (s->terminando)
    {
        pthread_mutex_unlock(&s->mutex);
        return ELEVATOR_ERR_TERMINATING;
    }
    int id = e->siguiente_id++;
    pthread_mutex_unlock(&s->mutex);

    persona_t *p = persona_create(e, id, PISO_PB);
    if (p == NULL)
        return ELEVATOR_ERR_MEMORY;

    pthread_mutex_lock(&s->mutex);
    s->personas_activas++;
    pthread_mutex_unlock(&s->mutex);

    pthread_t hilo;
    if (pthread_create(&hilo, NULL, persona_run, p) != 0)
    {
        pthread_mutex_lock(&s->mutex);
        s->personas_activas--;
        pthread_cond_broadcast(&s->cond);
        pthread_mutex_unlock(&s->mutex);
        persona_destroy(p);
        return ELEVATOR_ERR_THREAD_CREATE;
    }

    pthread_detach(hilo);
    return ELEVATOR_OK;
}

/* =============================================================================
 * Bucle principal del ascensor
 * ========================================================================== */

void *elevator_run(void *arg)
{
    elevator_t *e = (elevator_t *)arg;
    shared_t *s = e->shared;

    for (;;)
    {
        pthread_mutex_lock(&s->mutex);

        /* --- 0. Apagado pedido: evacuar una sola vez --- */
        if (s->terminando && !e->evacuacion_iniciada)
        {
            iniciar_evacuacion(e);
            pthread_cond_broadcast(&s->cond);
        }

        /* --- 1. Atender piso actual (salen primero, luego entran) --- */
        int personas_bajaron = descargar(e);
        ajustar_rumbo_si_vacio(e);
        int personas_subieron = cargar(e);

        // Actualizar el indicador de llamadas pendientes (hay_llamadas)
        s->hay_llamadas = (total_esperando(e) > 0);

        bool hubo_parada = (personas_bajaron > 0 || personas_subieron > 0);
        if (hubo_parada)
        {
            pthread_cond_broadcast(&s->cond); // Despertar a las personas que bajaron y a las que subieron
        }

        /* --- 2. Si no hay trabajo, dormir --- */
        while (!hay_trabajo(e) && !fin_de_jornada(e))
        {
            e->ascensor.estado = ASCENSOR_DETENIDO;
            pthread_cond_wait(&s->cond, &s->mutex);

            // Verificar si se pidió apagado mientras dormía, para iniciar la evacuación
            if (s->terminando && !e->evacuacion_iniciada)
            {
                iniciar_evacuacion(e);
                pthread_cond_broadcast(&s->cond);
            }
        }

        if (fin_de_jornada(e))
        {
            e->ascensor.estado = ASCENSOR_DETENIDO;
            pthread_mutex_unlock(&s->mutex);
            break;
        }

        /* --- 3. Elegir rumbo --- */
        int dir = decidir_direccion(e);
        e->ascensor.estado = dir;

        // Si el ascensor está detenido, no se mueve, pero se simula el tiempo de espera
        if (dir == ASCENSOR_DETENIDO)
        {
            pthread_mutex_unlock(&s->mutex);
            dormir_ms(hubo_parada ? MS_PUERTAS : MS_VIAJE_PISO);
            continue;
        }

        pthread_mutex_unlock(&s->mutex); // El ascensor se mueve, pero no retiene el mutex mientras simula el tiempo de viaje

        /* --- 4. Simular tiempos FUERA del mutex --- */
        if (hubo_parada)
        {
            dormir_ms(MS_PUERTAS);
        }
        dormir_ms(MS_VIAJE_PISO);

        /* --- 5. Avanzar un piso (con mutex) --- */
        pthread_mutex_lock(&s->mutex);
        e->ascensor.piso_actual += dir;
        pthread_mutex_unlock(&s->mutex);
    }

    // Cuando la jornada termina, se despiertan todos los hilos para que puedan salir del edificio
    pthread_mutex_lock(&s->mutex);
    pthread_cond_broadcast(&s->cond);
    pthread_mutex_unlock(&s->mutex);

    return NULL;
}

/* =============================================================================
 * API para persona.c
 * ========================================================================== */

void elevator_encolar_persona(elevator_t *e, persona_t *p, int destino)
{
    if (e == NULL || p == NULL)
        return;
    if (destino < 0 || destino >= e->num_pisos)
        return;

    shared_t *s = e->shared;

    pthread_mutex_lock(&s->mutex);

    p->destino = destino;

    if (p->destino == p->piso_actual)
    {
        p->direccion = ASCENSOR_DETENIDO;
        pthread_cond_broadcast(&s->cond);
        pthread_mutex_unlock(&s->mutex);
        return;
    }

    p->direccion = (p->destino > p->piso_actual) ? ASCENSOR_SUBIENDO
                                                 : ASCENSOR_BAJANDO;
    cola_encolar(cola_de(e, p->piso_actual, p->direccion), p);

    // Marca llamadas y despierta al ascensor
    s->hay_llamadas = true;
    pthread_cond_signal(&s->cond);

    pthread_mutex_unlock(&s->mutex);
}

void elevator_esperar_llegada(elevator_t *e, persona_t *p)
{
    if (e == NULL || p == NULL)
        return;

    shared_t *s = e->shared;

    pthread_mutex_lock(&s->mutex);
    while (p->piso_actual != p->destino)
    {
        pthread_cond_wait(&s->cond, &s->mutex);
    }
    pthread_mutex_unlock(&s->mutex);
}

/* =============================================================================
 * Lectura para el monitor
 * ========================================================================== */

Snapshot elevator_get_snapshot(elevator_t *e)
{
    Snapshot snap;
    memset(&snap, 0, sizeof snap);

    for (int i = 0; i < CAPACIDAD; i++)
    {
        snap.destinos[i] = -1;
    }
    snap.capacidad = CAPACIDAD;

    if (e == NULL)
        return snap;

    shared_t *s = e->shared;

    pthread_mutex_lock(&s->mutex);

    snap.num_pisos = e->num_pisos;
    snap.piso_ascensor = e->ascensor.piso_actual;
    snap.estado_ascensor = e->ascensor.estado;
    snap.personas_dentro = e->ascensor.personas_dentro;
    snap.personas_activas = s->personas_activas;
    snap.terminando = s->terminando;
    snap.evacuando = e->evacuacion_iniciada;

    for (int i = 0; i < e->num_pisos && i < MAX_PISOS; i++)
    {
        snap.colas_subida[i] = cola_cantidad(&e->colas_subida[i]);
        snap.colas_bajada[i] = cola_cantidad(&e->colas_bajada[i]);
    }
    snap.total_esperando = total_esperando(e);

    for (int i = 0; i < CAPACIDAD; i++)
    {
        const persona_t *p = e->ascensor.pasajeros[i];
        snap.destinos[i] = (p != NULL) ? p->destino : -1;
    }

    pthread_mutex_unlock(&s->mutex);

    return snap;
}

const char *elevator_estado_texto(int estado)
{
    switch (estado)
    {
    case ASCENSOR_SUBIENDO:
        return "SUBIENDO";
    case ASCENSOR_BAJANDO:
        return "BAJANDO";
    default:
        return "DETENIDO";
    }
}