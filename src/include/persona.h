/* =============================================================================
 * persona.h -- Ciclo de vida del trabajador
 * ============================================================================= */

#ifndef PERSONA_H
#define PERSONA_H

#include <pthread.h>

struct elevator;
typedef struct elevator elevator_t;

typedef struct persona
{
   int id;
   int piso_actual;
   int destino;
   int direccion;
   int jornada_restante;
   int tiempo_trabajo;
   pthread_t hilo;
   elevator_t *elevator;
} persona_t;

persona_t *persona_create(struct elevator *e, int id, int piso_inicial);
void persona_destroy(persona_t *p);
void *persona_run(void *arg);

#endif /* PERSONA_H */
