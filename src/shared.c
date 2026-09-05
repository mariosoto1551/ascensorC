#include <errno.h>
#include "include/shared.h"

shared_status_t shared_init(shared_t *s)
{
    if (!s)
        return SHARED_ERR_MUTEX_INIT; // No se pudo inicializar mutex porque el puntero es NULL

    if (pthread_mutex_init(&s->mutex, NULL) != 0)
        return SHARED_ERR_MUTEX_INIT;

    if (pthread_cond_init(&s->cond, NULL) != 0)
    {
        pthread_mutex_destroy(&s->mutex);
        return SHARED_ERR_COND_INIT;
    }

    s->terminando = 0;       // No está terminando
    s->personas_activas = 0; // No hay personas creadas
    s->hay_llamadas = false; // No existen llamadas

    return SHARED_OK;
}

shared_status_t shared_destroy(shared_t *s)
{
    if (!s)
        return SHARED_ERR_MUTEX_DESTROY;

    if (pthread_mutex_destroy(&s->mutex) != 0)
        return SHARED_ERR_MUTEX_DESTROY; // Error al destruir mutex

    if (pthread_cond_destroy(&s->cond) != 0)
        return SHARED_ERR_COND_DESTROY; // Error al destruir condición

    return SHARED_OK;
}

void shared_set_terminando(shared_t *s)
{
    pthread_mutex_lock(&s->mutex);
    s->terminando = 1;
    pthread_cond_broadcast(&s->cond);
    pthread_mutex_unlock(&s->mutex);
}

int shared_get_personas_activas(shared_t *s)
{
    int count;
    pthread_mutex_lock(&s->mutex);
    count = s->personas_activas;
    pthread_mutex_unlock(&s->mutex);
    return count;
}

void shared_set_hay_llamadas(shared_t *s, bool valor)
{
    pthread_mutex_lock(&s->mutex);
    s->hay_llamadas = valor;
    if (valor)
        pthread_cond_signal(&s->cond);
    pthread_mutex_unlock(&s->mutex);
}