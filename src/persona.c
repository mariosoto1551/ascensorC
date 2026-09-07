/* =============================================================================
 * persona.c -- Rutina del trabajador
 * ============================================================================= */

#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "persona.h"
#include "elevator.h"

static void dormir_ms(int ms)
{
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;

    while (nanosleep(&ts, &ts) == -1 && errno == EINTR)
        ;
}

static int aleatorio_entre(int min, int max)
{
    return min + (rand() % (max - min + 1));
}

persona_t *persona_create(struct elevator *e, int id, int piso_inicial)
{
    if (e == NULL)
        return NULL;

    persona_t *p = calloc(1, sizeof *p);
    if (p == NULL)
        return NULL;

    p->id = id;
    p->piso_actual = piso_inicial;
    p->destino = piso_inicial;
    p->direccion = ASCENSOR_DETENIDO;
    p->jornada_restante = aleatorio_entre(2, 4);
    p->tiempo_trabajo = aleatorio_entre(80, 220);
    p->elevator = e;

    return p;
}

void persona_destroy(persona_t *p)
{
    free(p);
}

void *persona_run(void *arg)
{
    persona_t *p = (persona_t *)arg;
    if (p == NULL || p->elevator == NULL)
        return NULL;

    elevator_t *e = p->elevator;
    shared_t *s = e->shared;

    for (;;)
    {
        if (p->piso_actual == p->destino)
        {
            int nuevo_destino = aleatorio_entre(1, e->num_pisos - 1);
            if (p->jornada_restante <= 0)
                nuevo_destino = PISO_PB;

            elevator_encolar_persona(e, p, nuevo_destino);
            elevator_esperar_llegada(e, p);
        }

        if (p->piso_actual == p->destino && p->destino != PISO_PB)
        {
            for (int paso = 0; paso < p->tiempo_trabajo; paso += 10)
            {
                if (shared_is_terminando(s))
                    break;
                dormir_ms(10);
            }

            p->jornada_restante--;

            if (p->jornada_restante <= 0)
            {
                elevator_encolar_persona(e, p, PISO_PB);
                elevator_esperar_llegada(e, p);

                pthread_mutex_lock(&s->mutex);
                if (s->personas_activas > 0)
                    s->personas_activas--;
                pthread_cond_broadcast(&s->cond);
                pthread_mutex_unlock(&s->mutex);
                persona_destroy(p);
                break;
            }
        }

        if (p->piso_actual == PISO_PB && p->destino == PISO_PB)
        {
            pthread_mutex_lock(&s->mutex);
            if (s->personas_activas > 0)
                s->personas_activas--;
            pthread_cond_broadcast(&s->cond);
            pthread_mutex_unlock(&s->mutex);
            persona_destroy(p);
            break;
        }
    }

    return NULL;
}
