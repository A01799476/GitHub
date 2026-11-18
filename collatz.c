/* Práctica: Cálculo paralelo de la longitud máxima de la secuencia de Collatz */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct {
    long numero;
    long longitud;
} Resultado;

/* Devuelve el número de pasos necesarios para llegar a 1. */
static long longitud_collatz(long numero)
{
    long pasos = 0;

    while (numero != 1L) {
        if ((numero % 2L) == 0L) {
            numero /= 2L;
        } else {
            numero = (3L * numero) + 1L;
        }
        pasos++;
    }

    return pasos;
}

/* Encuentra el número con la secuencia más larga en un rango inclusivo. */
static Resultado buscar_maximo(long inicio, long fin)
{
    Resultado mejor;
    long numero;

    mejor.numero = inicio;
    mejor.longitud = longitud_collatz(inicio);

    for (numero = inicio + 1L; numero <= fin; numero++) {
        long longitud = longitud_collatz(numero);

        if (longitud > mejor.longitud) {
            mejor.numero = numero;
            mejor.longitud = longitud;
        }
    }

    return mejor;
}

int main(void)
{
    const long limite_inferior_hijo = 1L;
    const long limite_superior_hijo = 25000000L;
    const long limite_inferior_padre = 25000001L;
    const long limite_superior_padre = 50000000L;
    Resultado *resultado_compartido;
    Resultado resultado_padre;
    Resultado resultado_final;
    pid_t pid;
    pid_t hijo_esperado;
    int estado;

    resultado_compartido = mmap(NULL, sizeof(*resultado_compartido),
                                PROT_READ | PROT_WRITE,
                                MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    if (resultado_compartido == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    resultado_compartido->numero = 0L;
    resultado_compartido->longitud = 0L;

    pid = fork();

    if (pid == -1) {
        perror("fork");
        if (munmap(resultado_compartido, sizeof(*resultado_compartido)) == -1) {
            perror("munmap");
        }
        exit(1);
    }

    if (pid == 0) {
        Resultado resultado_hijo = buscar_maximo(limite_inferior_hijo,
                                                  limite_superior_hijo);

        /* El hijo publica su resultado en la región de memoria compartida. */
        resultado_compartido->numero = resultado_hijo.numero;
        resultado_compartido->longitud = resultado_hijo.longitud;

        if (munmap(resultado_compartido, sizeof(*resultado_compartido)) == -1) {
            perror("munmap (hijo)");
            exit(1);
        }

        exit(0);
    }

    /* El padre calcula su mitad mientras el hijo trabaja en la primera. */
    resultado_padre = buscar_maximo(limite_inferior_padre,
                                    limite_superior_padre);

    /* Espera a que el hijo termine; reintenta si wait() fue interrumpida. */
    do {
        hijo_esperado = wait(&estado);
    } while (hijo_esperado == -1 && errno == EINTR);

    if (hijo_esperado == -1) {
        perror("wait");
        if (munmap(resultado_compartido, sizeof(*resultado_compartido)) == -1) {
            perror("munmap (padre)");
        }
        exit(1);
    }

    if (!WIFEXITED(estado) || WEXITSTATUS(estado) != 0) {
        fprintf(stderr, "Error: el proceso hijo terminó de forma anormal.\n");
        if (munmap(resultado_compartido, sizeof(*resultado_compartido)) == -1) {
            perror("munmap (padre)");
        }
        exit(1);
    }

    /* Compara el resultado del hijo con el calculado por el padre. */
    resultado_final = resultado_padre;
    if (resultado_compartido->longitud > resultado_final.longitud ||
        (resultado_compartido->longitud == resultado_final.longitud &&
         resultado_compartido->numero < resultado_final.numero)) {
        resultado_final.numero = resultado_compartido->numero;
        resultado_final.longitud = resultado_compartido->longitud;
    }

    if (munmap(resultado_compartido, sizeof(*resultado_compartido)) == -1) {
        perror("munmap (padre)");
        exit(1);
    }

    printf("N = %ld Longitud de secuencia de Collatz = %ld\n",
           resultado_final.numero, resultado_final.longitud);

    exit(0);
}
