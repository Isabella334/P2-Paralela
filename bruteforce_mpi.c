/* bruteforce_mpi - Acercamiento NAIVE (linea base de comparacion)
 * Divide el espacio [0, 2^bits) en N segmentos CONTIGUOS iguales: el proceso
 * 'id' recorre [range*id, range*(id+1)). El ultimo rank absorbe el residuo.
 * Es la version de referencia contra la que se comparan round-robin y
 * maestro-trabajador, por lo que acepta los MISMOS flags que ellas. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <mpi.h>
#include "bruteforce_common.h"

#define TAG_FOUND 7   /* aviso: encontre la llave, dejen de buscar */

/* cipher real del catedratico (16 bytes + terminador). */
static const uint8_t CIPHER_DEMO[] =
    {108,245,65,63,125,200,150,66,17,170,207,170,34,31,70,215,0};

/* Texto interno para el modo de prueba (igual que master/roundrobin). */
static const char PLAIN_DEMO[] = "Esta es una prueba de proyecto 2";

typedef struct {
    const char *file;      /* ruta del .txt, o NULL        */
    long        key;       /* llave para self-cipher, o -1 */
    const char *keyword;   /* frase clave a buscar         */
    int         bits;      /* cota: espacio = 2^bits       */
} Config;

/* Parsea argumentos con los mismos valores por defecto que master/roundrobin.
 * (El naive no usa --chunk porque el reparto es estatico.) */
static int parse_args(int argc, char **argv, Config *c) {
    c->file = NULL; c->key = -1; c->keyword = NULL; c->bits = 28;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--file")     && i + 1 < argc) c->file = argv[++i];
        else if (!strcmp(argv[i], "--key")     && i + 1 < argc) c->key = atol(argv[++i]);
        else if (!strcmp(argv[i], "--keyword") && i + 1 < argc) c->keyword = argv[++i];
        else if (!strcmp(argv[i], "--bits")    && i + 1 < argc) c->bits = atoi(argv[++i]);
        else { fprintf(stderr, "argumento invalido: %s\n", argv[i]); return -1; }
    }
    if (c->bits < 1 || c->bits > 60) { fprintf(stderr, "--bits fuera de rango\n"); return -1; }
    return 0;
}

/* Prepara el buffer 'cipher' y la frase de busqueda (igual que master/roundrobin). */
static int build_cipher(const Config *c, uint8_t *cipher, int *len,
                        char *source, size_t srcsz, const char **search) {
    *search = c->keyword ? c->keyword : (c->file || c->key >= 0 ? "es una prueba de" : " the ");

    if (c->file) {
        if (load_text_file(c->file, cipher, BF_MAXLEN, len) != 0) return -1;
        long k = c->key >= 0 ? c->key : 42;
        encrypt(k, cipher, *len);
        snprintf(source, srcsz, "%s (cifrado con llave %ld)", c->file, k);
    } else if (c->key >= 0) {
        *len = (int)strlen(PLAIN_DEMO);
        memcpy(cipher, PLAIN_DEMO, *len);
        while (*len % 8 != 0) cipher[(*len)++] = ' ';
        encrypt(c->key, cipher, *len);
        snprintf(source, srcsz, "<texto interno> (cifrado con llave %ld)", c->key);
    } else {
        *len = (int)strlen((const char *)CIPHER_DEMO);
        memcpy(cipher, CIPHER_DEMO, *len);
        snprintf(source, srcsz, "<cipher embebido del catedratico>");
    }
    return 0;
}

int main(int argc, char *argv[]) {
    int N, id;
    MPI_Comm comm = MPI_COMM_WORLD;
    MPI_Init(&argc, &argv);
    MPI_Comm_size(comm, &N);
    MPI_Comm_rank(comm, &id);

    Config cfg;
    if (parse_args(argc, argv, &cfg) != 0) { MPI_Finalize(); return 1; }

    uint8_t cipher[BF_MAXLEN + 1];
    int ciphlen = 0;
    char source[256];
    const char *search = NULL;
    if (build_cipher(&cfg, cipher, &ciphlen, source, sizeof(source), &search) != 0) {
        MPI_Finalize();
        return 1;
    }
    long upper = 1L << cfg.bits;

    MPI_Barrier(comm);
    double t0 = MPI_Wtime();
    long answer = -1;

    if (N < 2) {
        /* Un solo proceso: recorre todo (linea base secuencial). */
        for (long i = 0; i < upper; ++i)
            if (try_key(i, cipher, ciphlen, search)) { answer = i; break; }
    } else {
        /* Reparto NAIVE: N segmentos contiguos iguales. */
        long range   = upper / N;
        long mylower = range * id;
        long myupper = (id == N - 1) ? upper : range * (id + 1);

        /* Escucha no bloqueante por si otro proceso encuentra la llave. */
        long recv_key = -1;
        MPI_Request req;
        MPI_Irecv(&recv_key, 1, MPI_LONG, MPI_ANY_SOURCE, TAG_FOUND, comm, &req);

        for (long i = mylower; i < myupper && answer < 0; ++i) {
            /* Revisar periodicamente si otro proceso ya encontro la llave. */
            if ((i & 0x3FF) == 0) {
                int flag = 0;
                MPI_Test(&req, &flag, MPI_STATUS_IGNORE);
                if (flag) { answer = recv_key; break; }
            }
            if (try_key(i, cipher, ciphlen, search)) {
                answer = i;
                for (int node = 0; node < N; ++node)      /* avisar a los demas */
                    if (node != id)
                        MPI_Send(&answer, 1, MPI_LONG, node, TAG_FOUND, comm);
                break;
            }
        }

        /* Cerrar la recepcion pendiente (cancelar si nadie me escribio). */
        int flag = 0;
        MPI_Test(&req, &flag, MPI_STATUS_IGNORE);
        if (!flag) { MPI_Cancel(&req); MPI_Wait(&req, MPI_STATUS_IGNORE); }
    }

    double t1 = MPI_Wtime();
    double tlocal = t1 - t0, tmax = 0;
    long global = 0;
    MPI_Reduce(&tlocal, &tmax, 1, MPI_DOUBLE, MPI_MAX, 0, comm);
    MPI_Reduce(&answer, &global, 1, MPI_LONG, MPI_MAX, 0, comm);

    if (id == 0) {
        printf("=== Naive | procesos=%d  cota=2^%d ===\n", N, cfg.bits);
        printf("Fuente        : %s\n", source);
        printf("Palabra clave : \"%s\"\n", search);
        if (global >= 0) {
            uint8_t temp[BF_MAXLEN + 1];
            memcpy(temp, cipher, ciphlen); temp[ciphlen] = 0;
            decrypt(global, temp, ciphlen);
            printf("Llave hallada : %ld\n", global);
            printf("Texto         : \"%s\"\n", (char *)temp);
        } else {
            printf("Llave NO encontrada en la cota dada.\n");
        }
        printf("Tiempo (s)    : %.4f\n", tmax);
        fprintf(stderr, "CSV,naive,%d,%d,%ld,%.4f\n", N, cfg.bits, global, tmax);
    }

    MPI_Finalize();
    return 0;
}
