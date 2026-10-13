/* bruteforce_mpi_master - Estrategia maestro-trabajador*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <mpi.h>
#include "bruteforce_common.h"

#define TAG_REQUEST 1   /* trabajador -> maestro: pido trabajo               */
#define TAG_ASSIGN  2   /* maestro -> trabajador: bloque [lo,hi) o sentinela */
#define TAG_FOUND   3   /* trabajador -> maestro: encontre la llave          */

/* cipher real del catedratico (16 bytes + terminador). */
static const uint8_t CIPHER_DEMO[] =
    {108,245,65,63,125,200,150,66,17,170,207,170,34,31,70,215,0};

/* Texto interno para el modo de prueba */
static const char PLAIN_DEMO[] = "Esta es una prueba de proyecto 2";

typedef struct {
    const char *file;      /* ruta del .txt, o NULL          */
    long        key;       /* llave para self-cipher, o -1   */
    const char *keyword;   /* frase clave a buscar           */
    int         bits;      /* cota: espacio = 2^bits         */
    long        chunk;     /* bloque que entrega el maestro  */
} Config;

/* Parsea argumentos con valores por defecto sensatos. Devuelve 0 o <0. */
static int parse_args(int argc, char **argv, Config *c) {
    c->file = NULL; c->key = -1; c->keyword = NULL; c->bits = 28; c->chunk = 4096;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--file")    && i + 1 < argc) c->file = argv[++i];
        else if (!strcmp(argv[i], "--key")    && i + 1 < argc) c->key = atol(argv[++i]);
        else if (!strcmp(argv[i], "--keyword")&& i + 1 < argc) c->keyword = argv[++i];
        else if (!strcmp(argv[i], "--bits")   && i + 1 < argc) c->bits = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--chunk")  && i + 1 < argc) c->chunk = atol(argv[++i]);
        else { fprintf(stderr, "argumento invalido: %s\n", argv[i]); return -1; }
    }
    if (c->bits < 1 || c->bits > 60) { fprintf(stderr, "--bits fuera de rango\n"); return -1; }
    if (c->chunk < 1) { fprintf(stderr, "--chunk debe ser > 0\n"); return -1; }
    return 0;
}

/* Prepara el buffer 'cipher' (len multiplo de 8) y el nombre de la fuente */
static int build_cipher(const Config *c, uint8_t *cipher, int *len,
                        char *source, size_t srcsz, const char **search) {
    *search = c->keyword ? c->keyword : (c->file || c->key >= 0 ? "es una prueba de" : " the ");

    if (c->file) {                       /* texto desde archivo */
        if (load_text_file(c->file, cipher, BF_MAXLEN, len) != 0) return -1;
        long k = c->key >= 0 ? c->key : 42;
        encrypt(k, cipher, *len);
        snprintf(source, srcsz, "%s (cifrado con llave %ld)", c->file, k);
    } else if (c->key >= 0) {            /* texto interno self-cipher */
        *len = (int)strlen(PLAIN_DEMO);
        memcpy(cipher, PLAIN_DEMO, *len);
        while (*len % 8 != 0) cipher[(*len)++] = ' ';
        encrypt(c->key, cipher, *len);
        snprintf(source, srcsz, "<texto interno> (cifrado con llave %ld)", c->key);
    } else {                            /* cipher embebido del catedratico */
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
    long found_key = -1;

    if (N < 2) {
        /* Sin trabajadores: el rank 0 recorre todo el espacio (defensivo). */
        for (long i = 0; i < upper; ++i)
            if (try_key(i, cipher, ciphlen, search)) { found_key = i; break; }
    } else if (id == 0) {
        long next = 0;
        int  done_workers = 0, nworkers = N - 1;
        while (done_workers < nworkers) {
            long msg[2];
            MPI_Status st;
            MPI_Recv(msg, 2, MPI_LONG, MPI_ANY_SOURCE, MPI_ANY_TAG, comm, &st);

            if (st.MPI_TAG == TAG_FOUND) {
                if (found_key < 0) found_key = msg[0];  /* primer hallazgo gana */
                done_workers++;                         /* ese trabajador ya paro */
            } else { /* TAG_REQUEST */
                long block[2];
                if (found_key >= 0 || next >= upper) {
                    block[0] = block[1] = -1;           /* sentinela: no hay mas */
                    done_workers++;
                } else {
                    block[0] = next;
                    block[1] = (next + cfg.chunk < upper) ? next + cfg.chunk : upper;
                    next = block[1];
                }
                MPI_Send(block, 2, MPI_LONG, st.MPI_SOURCE, TAG_ASSIGN, comm);
            }
        }
    } else {
        int working = 1;
        while (working) {
            long req[2] = {0, 0};
            MPI_Send(req, 2, MPI_LONG, 0, TAG_REQUEST, comm);

            long block[2];
            MPI_Recv(block, 2, MPI_LONG, 0, TAG_ASSIGN, comm, MPI_STATUS_IGNORE);
            if (block[0] < 0) break;                    /* sentinela: terminar */

            for (long i = block[0]; i < block[1]; ++i) {
                if (try_key(i, cipher, ciphlen, search)) {
                    long msg[2] = {i, 0};
                    MPI_Send(msg, 2, MPI_LONG, 0, TAG_FOUND, comm);
                    working = 0;                         /* dejo de pedir trabajo */
                    break;
                }
            }
        }
    }

    double t1 = MPI_Wtime();
    double tlocal = t1 - t0, tmax = 0;
    long global = 0;
    MPI_Reduce(&tlocal, &tmax, 1, MPI_DOUBLE, MPI_MAX, 0, comm);
    MPI_Reduce(&found_key, &global, 1, MPI_LONG, MPI_MAX, 0, comm);

    if (id == 0) {
        printf("=== Maestro-trabajador | procesos=%d  cota=2^%d  chunk=%ld ===\n",
               N, cfg.bits, cfg.chunk);
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
        /* linea CSV para scripts de benchmark*/
        fprintf(stderr, "CSV,master,%d,%d,%ld,%.4f\n",
                N, cfg.bits, global, tmax);
    }

    MPI_Finalize();
    return 0;
}
