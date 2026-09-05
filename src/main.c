/* =============================================================================
 * main.c -- Orquestador
 * =============================================================================
 *
 * Inicializa shared_t, crea elevator, lanza los hilos permanentes (ascensor, senales, monitor) y espera su terminacion ordenada. Sin logica de sincronizacion ni de negocio.
 *
 * Punto de entrada del programa. No expone header propio.
 * Diseno detallado: docs/diseno_logico.md, seccion 3.1
 * -----------------------------------------------------------------------------
 * TODO: implementar -- ver el estado de fases en docs/BITACORA.md
 * ========================================================================== */
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>

#include "include/shared.h"
#include "include/elevator.h"
#include "include/signal_handler.h"
#include "include/monitor.h"

int main(int argc, char *argv[])
{
    // 1. Validar argumentos (ej. número de pisos)
    if (argc != 2)
    {
        fprintf(stderr, "Uso: %s <num_pisos>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    int num_pisos = atoi(argv[1]);
    if (num_pisos < 3 || num_pisos > 10)
    {
        fprintf(stderr, "Error: número de pisos debe estar entre 3 y 10\n");
        exit(EXIT_FAILURE);
    }

    // 2. Inicializar shared (mutex, cond, banderas)
    shared_t shared;
    if (shared_init(&shared) != SHARED_OK)
    {
        fprintf(stderr, "Error al inicializar shared\n");
        exit(EXIT_FAILURE);
    }

    // 3. Crear elevator (pasar shared)
    elevator_t *elevator = elevator_create(num_pisos, &shared);
    if (!elevator)
    {
        fprintf(stderr, "Error al crear elevator\n");
        shared_destroy(&shared);
        exit(EXIT_FAILURE);
    }

    // 4. Bloquear señales en el hilo principal
    //    (para que solo el hilo de señales las reciba)
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &set, NULL);

    // 5. Inicializar signal_handler (pasar shared y elevator)
    signal_handler_t sig_handler;
    if (signal_handler_init(&sig_handler, &shared, elevator) != 0)
    {
        fprintf(stderr, "Error al inicializar signal_handler\n");
        elevator_destroy(elevator);
        shared_destroy(&shared);
        exit(EXIT_FAILURE);
    }

    // 6. Crear hilos permanentes
    pthread_t th_elevator, th_signals, th_monitor;

    if (pthread_create(&th_elevator, NULL, elevator_run, elevator) != 0)
    {
        fprintf(stderr, "Error al crear hilo elevator\n");
        elevator_destroy(elevator);
        shared_destroy(&shared);
        exit(EXIT_FAILURE);
    }

    if (pthread_create(&th_signals, NULL, signal_handler_routine, &sig_handler) != 0)
    {
        fprintf(stderr, "Error al crear hilo signal_handler\n");
        // En un sistema real, aquí deberíamos cancelar th_elevator
        pthread_cancel(th_elevator);
        elevator_destroy(elevator);
        shared_destroy(&shared);
        exit(EXIT_FAILURE);
    }

    if (pthread_create(&th_monitor, NULL, monitor_run, elevator) != 0)
    {
        fprintf(stderr, "Error al crear hilo monitor\n");
        pthread_cancel(th_signals);
        pthread_cancel(th_elevator);
        elevator_destroy(elevator);
        shared_destroy(&shared);
        exit(EXIT_FAILURE);
    }

    // 7. Esperar a que todos los hilos terminen
    pthread_join(th_elevator, NULL);
    pthread_join(th_signals, NULL);
    pthread_join(th_monitor, NULL);

    // 8. Limpiar recursos
    signal_handler_destroy(&sig_handler);
    elevator_destroy(elevator);
    shared_destroy(&shared);

    printf("Sistema terminado correctamente\n");
    return EXIT_SUCCESS;
}