// SPDX-License-Identifier: MIT
// Demo: lectura "a medias" entre dos hilos, con y sin mutex.
// Compilar en Ubuntu: gcc -O2 -Wall -Wextra -pthread race_demo.c -o race_demo
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>

#define NUM_REGS   4
#define ITERATIONS 5000000L

static volatile uint16_t regs[NUM_REGS];   /* "tabla de registros" compartida */
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static int use_mutex;
static long torn_reads;

/* Escribe los 4 registros con el mismo valor: equivale a volcar una trama CAN */
static void *writer(void *arg)
{
    (void)arg;
    for (long n = 0; n < ITERATIONS; n++) {
        uint16_t v = (uint16_t)(n & 0xFFFF);
        if (use_mutex)
            pthread_mutex_lock(&lock);
        for (int i = 0; i < NUM_REGS; i++)
            regs[i] = v;
        if (use_mutex)
            pthread_mutex_unlock(&lock);
    }
    return NULL;
}

/* Lee los 4 registros: si no son iguales, ha leido una trama a medias */
static void *reader(void *arg)
{
    (void)arg;
    for (long n = 0; n < ITERATIONS; n++) {
        uint16_t snap[NUM_REGS];
        if (use_mutex)
            pthread_mutex_lock(&lock);
        for (int i = 0; i < NUM_REGS; i++)
            snap[i] = regs[i];
        if (use_mutex)
            pthread_mutex_unlock(&lock);

        for (int i = 1; i < NUM_REGS; i++) {
            if (snap[i] != snap[0]) {
                torn_reads++;
                break;
            }
        }
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    use_mutex = (argc > 1 && strcmp(argv[1], "mutex") == 0);

    pthread_t w, r;
    pthread_create(&w, NULL, writer, NULL);
    pthread_create(&r, NULL, reader, NULL);
    pthread_join(w, NULL);
    pthread_join(r, NULL);

    printf("Modo: %-10s | lecturas: %ld | lecturas a medias: %ld\n",
           use_mutex ? "CON mutex" : "SIN mutex", ITERATIONS, torn_reads);
    return 0;
}
