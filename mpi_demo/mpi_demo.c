#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <mpi.h>

#define TAG_FOUND 0
#define CHECK_EVERY 16      /* cada cuantos intentos se consulta MPI_Test */

static double t0;           /* referencia de tiempo comun para los logs */

#define LOG(id, ...) do {                                       \
        printf("[%8.4f s] P%d: ", MPI_Wtime() - t0, (id));      \
        printf(__VA_ARGS__);                                    \
        printf("\n");                                           \
        fflush(stdout);                                         \
    } while (0)

/* "tryKey" simulado: en el programa real aqui se descifra y se busca " the ". */
static int try_candidate(long candidate, long target, int delay_us) {
    if (delay_us > 0) usleep(delay_us);
    return candidate == target;
}

int main(int argc, char *argv[]) {
    int N, id;
    MPI_Comm comm = MPI_COMM_WORLD;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(comm, &N);
    MPI_Comm_rank(comm, &id);

    long target   = (argc >= 2) ? atol(argv[1]) : 700;
    long space    = (argc >= 3) ? atol(argv[2]) : 1000;
    int  delay_us = (argc >= 4) ? atoi(argv[3]) : 1000;

    if (target < 0 || target >= space) {
        if (id == 0) fprintf(stderr, "target debe estar en [0, espacio)\n");
        MPI_Finalize();
        return 1;
    }

    /* Todos arrancan el reloj juntos para que los logs sean comparables. */
    MPI_Barrier(comm);
    t0 = MPI_Wtime();

    /* ---- Reparto del espacio (mismo esquema que bruteforce.c) ---------- */
    long range_per_node = space / N;
    long mylower = range_per_node * id;
    long myupper = (id == N - 1) ? space : range_per_node * (id + 1);
    LOG(id, "rango [%ld, %ld)", mylower, myupper);

    /* ---- 1. MPI_Irecv: escuchar el aviso de cualquier proceso ---------- */
    long recv_found = -1;
    MPI_Request req;
    MPI_Status st;
    int flag = 0;
    MPI_Irecv(&recv_found, 1, MPI_LONG, MPI_ANY_SOURCE, TAG_FOUND, comm, &req);
    LOG(id, "MPI_Irecv publicado (ANY_SOURCE), comienza la busqueda");

    /* ---- Busqueda local ------------------------------------------------ */
    long tried = 0;
    int i_found_it = 0;
    for (long i = mylower; i < myupper; ++i) {
        if (tried % CHECK_EVERY == 0) {
            MPI_Test(&req, &flag, &st);
            if (flag) {
                LOG(id, "MPI_Test: P%d ya encontro %ld -> detengo la busqueda "
                        "(intento %ld de %ld)",
                    st.MPI_SOURCE, recv_found, tried, myupper - mylower);
                break;
            }
        }
        ++tried;
        if (try_candidate(i, target, delay_us)) {
            i_found_it = 1;
            long key = i;
            LOG(id, "ENCONTRE %ld -> MPI_Send a los %d procesos", key, N);

            /* ---- 2. MPI_Send: avisar a todos (incluido a si mismo) ------ */
            for (int node = 0; node < N; ++node)
                MPI_Send(&key, 1, MPI_LONG, node, TAG_FOUND, comm);
            break;
        }
    }

    /* ---- 3. MPI_Wait: completar la recepcion ---------------------------- */
    if (!flag) {
        if (!i_found_it) LOG(id, "rango agotado, espero con MPI_Wait");
        MPI_Wait(&req, &st);
    }

    if (id == 0) {
        LOG(id, "MPI_Wait completo: llave = %ld, enviada por P%d (tag %d)",
            recv_found, st.MPI_SOURCE, st.MPI_TAG);
    }

    /* Resumen ordenado: cuantos candidatos probo cada proceso. */
    long *all_tried = (id == 0) ? malloc(sizeof(long) * N) : NULL;
    MPI_Gather(&tried, 1, MPI_LONG, all_tried, 1, MPI_LONG, 0, comm);
    if (id == 0) {
        printf("\nResumen: target=%ld espacio=%ld procesos=%d\n", target, space, N);
        for (int p = 0; p < N; ++p)
            printf("  P%d probo %ld candidatos%s\n", p, all_tried[p],
                   p == st.MPI_SOURCE ? "  <- encontro la llave" : "");
        free(all_tried);
    }

    MPI_Finalize();
    return 0;
}
