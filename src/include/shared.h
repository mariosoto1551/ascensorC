#ifndef SHARED_H
#define SHARED_H

#include <pthread.h>
#include <stdbool.h>

/* Códigos de retorno específicos del módulo shared */
typedef enum
{
    SHARED_OK = 0,                // Éxito
    SHARED_ERR_MUTEX_INIT = 1,    // Fallo al inicializar mutex
    SHARED_ERR_COND_INIT = 2,     // Fallo al inicializar condición
    SHARED_ERR_MUTEX_DESTROY = 3, // Fallo al destruir mutex
    SHARED_ERR_COND_DESTROY = 4   // Fallo al destruir condición
} shared_status_t;

/*Estructura compartida*/
typedef struct shared
{
    pthread_mutex_t mutex; // mutex global que protege todos los hilos
    pthread_cond_t cond;   // variable de condición para notificar a los hilos que hay llamadas
    int terminando;        // 1 = apagado solicitado, 0 = en marcha
    int personas_activas;  // contador de hilos persona vivos dentro del edificio
    bool hay_llamadas;     // true si hay algún hilo esperando en alguna cola
} shared_t;

/*Inicialización y destrucción*/
shared_status_t shared_init(shared_t *s);    // Inicializa la estructura compartida
shared_status_t shared_destroy(shared_t *s); // Libera los recursos de la estructura compartida

/* Funciones de alto nivel (encapsulan lock + modificación) */
void shared_set_terminando(shared_t *s);               // Control sobre la bandera terminando (modificación)
void shared_set_hay_llamadas(shared_t *s, bool valor); // Control sobre la bandera hay_llamadas (modificación)

/*Lectura del contador de personas activas*/
int shared_get_personas_activas(shared_t *s); // Obtener la cantidad exacta de personas activas

#endif