/* =============================================================================
 * main.c -- Orquestador
 * =============================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>

#include "shared.h"
#include "elevator.h"
#include "signal_handler.h"
#include "monitor.h"

int main(int argc, char *argv[])
{
    /* --- Validar argumentos --- */
    if (argc != 2)
    {
        fprintf(stderr, "Uso: %s <num_pisos> (entre %d y %d)\n", argv[0], MIN_PISOS, MAX_PISOS);
        exit(EXIT_FAILURE);
    }
    int num_pisos = atoi(argv[1]);
    if (num_pisos < MIN_PISOS || num_pisos > MAX_PISOS)
    {
        fprintf(stderr, "Error: número de pisos debe estar entre %d y %d\n", MIN_PISOS, MAX_PISOS);
        exit(EXIT_FAILURE);
    }

    /* --- 1. Inicializar shared --- */
    shared_t shared;
    if (shared_init(&shared) != SHARED_OK)
    {
        fprintf(stderr, "Error al inicializar shared\n");
        exit(EXIT_FAILURE);
    }

    /* --- 2. Crear elevator (con la nueva API) --- */
    elevator_t *elevator = NULL;
    elevator_status_t err = elevator_create(&elevator, num_pisos, &shared);
    if (err != ELEVATOR_OK)
    {
        fprintf(stderr, "Error al crear elevator: código %d\n", err);
        shared_destroy(&shared);
        exit(EXIT_FAILURE);
    }

    /* --- 3. Bloquear señales en el hilo principal --- */
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &set, NULL);

    /* --- 4. Inicializar signal_handler --- */
    signal_handler_t sig_handler;
    signal_handler_init(&sig_handler, &shared, elevator);

    /* --- 5. Crear hilos permanentes --- */
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

    /* --- 6. Esperar a que los hilos terminen --- */
    pthread_join(th_elevator, NULL);
    pthread_join(th_signals, NULL);
    pthread_join(th_monitor, NULL);

    /* --- 7. Limpiar recursos (orden inverso) --- */
    signal_handler_destroy(&sig_handler);
    elevator_destroy(elevator);
    shared_destroy(&shared);

    printf("Sistema terminado correctamente.\n");
    return EXIT_SUCCESS;
}