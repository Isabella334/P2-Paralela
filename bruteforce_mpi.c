/* ============================================================================
 * bruteforce_mpi.c  -  PROGRAMA BASE (version paralela con MPI)
 * ----------------------------------------------------------------------------
 * Version del bruteforce.c del catedratico corregida para que COMPILE y CORRA.
 * Conserva la estructura y semantica del original (misma transformacion de
 * llave, mismo cipher[], misma subcadena " the ", misma logica MPI de reparto
 * y aviso con MPI_Irecv / MPI_Send / MPI_Wait).
 *
 * UNICO cambio de fondo: se reemplaza <rpc/des_crypt.h> (ecb_crypt /
 * des_setparity), eliminado de glibc moderno, por des.h (DES propio,
 * byte-compatible con ecb_crypt: descifra el cipher a "Save the planet ").
 *
 * Ajustes menores para robustez:
 *   - Cota de busqueda configurable por argumento (por defecto 2^56 como el
 *     original). El original recorreria 2^56 llaves; para las pruebas se pasa
 *     una cota menor.
 *   - Se usa long long en los mensajes para transportar la llave hallada.
 *
 * Compilar:  mpicc -O3 -o bruteforce_mpi bruteforce_mpi.c
 * Ejecutar:  mpirun -np 4 ./bruteforce_mpi 24     (busca en [0, 2^24))
 * ========================================================================== */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <mpi.h>
#include "des.h"

char search[] = " the ";
unsigned char cipher[] = {108,245,65,63,125,200,150,66,17,170,207,170,34,31,70,215,0};

/* Transformacion de llave EXACTA del original. */
static void key_to_bytes(long key, uint8_t kb[8]) {
    long k = 0;
    for (int i = 0; i < 8; ++i) {
        key <<= 1;
        k += (key & (0xFE << i * 8));
    }
    for (int i = 0; i < 8; ++i) kb[i] = (uint8_t)((k >> (8 * i)) & 0xFF);
}

void decrypt(long key, char *ciph, int len) {
    uint8_t kb[8]; key_to_bytes(key, kb);
    for (int off = 0; off + 8 <= len; off += 8)
        des_decrypt_block(kb, (uint8_t *)ciph + off);
}
void encrypt(long key, char *ciph, int len) {
    uint8_t kb[8]; key_to_bytes(key, kb);
    for (int off = 0; off + 8 <= len; off += 8)
        des_encrypt_block(kb, (uint8_t *)ciph + off);
}
int tryKey(long key, char *ciph, int len) {
    char temp[256];
    if (len > 255) len = 255;
    memcpy(temp, ciph, len); temp[len] = 0;
    decrypt(key, temp, len);
    return strstr((char *)temp, search) != NULL;
}

int main(int argc, char *argv[]) {
    int N, id;
    /* Cota superior de busqueda: 2^bits. Por defecto 2^56 (como el original). */
    int bits   = (argc >= 2) ? atoi(argv[1]) : 56;
    long upper = 1L << bits;
    long mylower, myupper;
    MPI_Status st;
    MPI_Request req;
    int flag;
    int ciphlen = strlen((char *)cipher);
    MPI_Comm comm = MPI_COMM_WORLD;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(comm, &N);
    MPI_Comm_rank(comm, &id);

    long range_per_node = upper / N;
    mylower = range_per_node * id;
    myupper = range_per_node * (id + 1) - 1;
    if (id == N - 1) myupper = upper;          /* compensar residuo */

    long long found = 0;
    double t0 = MPI_Wtime();

    /* Recepcion NO bloqueante: escuchar la llave hallada por cualquier proceso. */
    MPI_Irecv(&found, 1, MPI_LONG_LONG, MPI_ANY_SOURCE, MPI_ANY_TAG, comm, &req);

    for (long i = mylower; i < myupper && (found == 0); ++i) {
        /* Revisar periodicamente si otro proceso ya encontro la llave. */
        if ((i & 0x3FF) == 0) {
            MPI_Test(&req, &flag, &st);
            if (flag && found != 0) break;
        }
        if (tryKey(i, (char *)cipher, ciphlen)) {
            found = i;
            for (int node = 0; node < N; ++node)
                MPI_Send(&found, 1, MPI_LONG_LONG, node, 0, comm);
            break;
        }
    }

    /* Asegurar que la recepcion pendiente termine (o cancelarla). */
    MPI_Test(&req, &flag, &st);
    if (!flag) { MPI_Cancel(&req); MPI_Wait(&req, &st); }

    double t1 = MPI_Wtime();

    /* El proceso 0 reporta el resultado. */
    long long global = 0;
    MPI_Reduce(&found, &global, 1, MPI_LONG_LONG, MPI_MAX, 0, comm);
    double tlocal = t1 - t0, tmax = 0;
    MPI_Reduce(&tlocal, &tmax, 1, MPI_DOUBLE, MPI_MAX, 0, comm);

    if (id == 0) {
        printf("Procesos: %d  |  cota: 2^%d\n", N, bits);
        if (global > 0) {
            char temp[256];
            memcpy(temp, cipher, ciphlen); temp[ciphlen] = 0;
            decrypt((long)global, temp, ciphlen);
            printf("%lld %s\n", global, temp);
        } else {
            printf("Llave no encontrada en la cota dada.\n");
        }
        printf("Tiempo (s): %.4f\n", tmax);
    }

    MPI_Finalize();
    return 0;
}
