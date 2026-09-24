#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include "des.h"

char search[] = " the ";

/* Cipher REAL entregado por el catedratico (16 bytes + terminador). */
unsigned char cipher[] = {108,245,65,63,125,200,150,66,17,170,207,170,34,31,70,215,0};

/* Transformacion de llave EXACTA del bruteforce.c original.
 * Toma una llave entera y produce los 8 bytes de la llave DES. */
static void key_to_bytes(long key, uint8_t kb[8]) {
    long k = 0;
    for (int i = 0; i < 8; ++i) {
        key <<= 1;
        k += (key & (0xFE << i * 8));   /* verbatim del original */
    }
    for (int i = 0; i < 8; ++i) kb[i] = (uint8_t)((k >> (8 * i)) & 0xFF);
}

/* Descifra 'len' bytes (multiplo de 8) bloque a bloque, in place. */
void decrypt(long key, char *ciph, int len) {
    uint8_t kb[8]; key_to_bytes(key, kb);
    for (int off = 0; off + 8 <= len; off += 8)
        des_decrypt_block(kb, (uint8_t *)ciph + off);
}

/* Cifra 'len' bytes (multiplo de 8) bloque a bloque, in place. */
void encrypt(long key, char *ciph, int len) {
    uint8_t kb[8]; key_to_bytes(key, kb);
    for (int off = 0; off + 8 <= len; off += 8)
        des_encrypt_block(kb, (uint8_t *)ciph + off);
}

/* Prueba una llave: descifra una COPIA y busca la subcadena. */
int tryKey(long key, char *ciph, int len) {
    char temp[256];
    if (len > 255) len = 255;
    memcpy(temp, ciph, len);
    temp[len] = 0;
    decrypt(key, temp, len);
    return strstr((char *)temp, search) != NULL;
}

int main(int argc, char *argv[]) {
    /* ---------- Modo BENCH: rendimiento en peor caso ---------- */
    if (argc >= 3 && strcmp(argv[1], "bench") == 0) {
        int bits = atoi(argv[2]);
        uint64_t upper = (uint64_t)1 << bits;

        /* Texto conocido de 16 bytes (2 bloques, IGUAL que el cipher real)
         * cifrado con una llave FUERA del rango [0,2^bits), usando la MISMA
         * transformacion, para forzar un barrido completo y que el rendimiento
         * sea comparable al del crack real. */
        char buf[24];
        memset(buf, ' ', sizeof(buf));
        memcpy(buf, "Save the world..", 16);
        int len = 16;               /* 2 bloques, como el cipher del profe */
        long outkey = 999999999L;   /* fuera de cualquier 2^bits usado */
        encrypt(outkey, buf, len);

        struct timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC, &t0);
        long long found = -1;
        for (uint64_t k = 0; k < upper; ++k)
            if (tryKey((long)k, buf, len)) { found = (long long)k; break; }
        clock_gettime(CLOCK_MONOTONIC, &t1);

        double secs = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9;
        printf("[bench] bits=%d  espacio=2^%d=%llu  probadas=%llu  "
               "tiempo=%.4fs  %.0f llaves/s  (found=%lld)\n",
               bits, bits, (unsigned long long)upper,
               (unsigned long long)upper, secs,
               secs > 0 ? upper / secs : 0.0, found);
        fprintf(stderr, "CSV,%d,%llu,%.4f,%.0f\n", bits,
                (unsigned long long)upper, secs, secs > 0 ? upper / secs : 0.0);
        return 0;
    }

    /* ---------- Modo normal: descifrar el cipher REAL del profe ---------- */
    int len = (int)strlen((char *)cipher);          /* 16 bytes = 2 bloques */
    int bits = (argc >= 2) ? atoi(argv[1]) : 28;    /* cota superior 2^bits */
    uint64_t upper = (uint64_t)1 << bits;

    printf("=== Fuerza bruta SECUENCIAL sobre DES (cipher del catedratico) ===\n");
    printf("Longitud del cipher: %d bytes (%d bloques)\n", len, len / 8);
    printf("Subcadena buscada  : \"%s\"\n", search);
    printf("Cota de busqueda   : 2^%d = %llu llaves\n",
           bits, (unsigned long long)upper);
    fflush(stdout);

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    long long found = -1;
    unsigned long long tested = 0;
    for (uint64_t k = 0; k < upper; ++k) {
        ++tested;
        if (tryKey((long)k, (char *)cipher, len)) { found = (long long)k; break; }
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double secs = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9;

    printf("------------------------------------------------------------\n");
    if (found >= 0) {
        char temp[256];
        memcpy(temp, cipher, len); temp[len] = 0;
        decrypt(found, temp, len);
        printf("Llave ENCONTRADA   : %lld\n", found);
        printf("Texto descifrado   : \"%s\"\n", temp);
    } else {
        printf("Llave NO encontrada en la cota dada.\n");
    }
    printf("Llaves probadas    : %llu\n", tested);
    printf("Tiempo (s)         : %.4f\n", secs);
    printf("Rendimiento        : %.0f llaves/seg\n",
           secs > 0 ? tested / secs : 0.0);
    return 0;
}
