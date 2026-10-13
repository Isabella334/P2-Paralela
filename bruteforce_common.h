#ifndef BRUTEFORCE_COMMON_H
#define BRUTEFORCE_COMMON_H

/* Base compartida para las variantes de fuerza bruta sobre DES.
 * Centraliza la preparacion de llave, el cifrado/descifrado por bloques, la
 * prueba de una llave candidata y la carga de un texto desde archivo
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "des.h"

/* Tamano maximo de texto que manejamos (cipher + terminador). */
#define BF_MAXLEN 4096

/* Prepara los 8 bytes de la llave DES a partir de una llave entera*/
static inline void key_to_bytes(long key, uint8_t kb[8]) {
    long k = 0;
    for (int i = 0; i < 8; ++i) {
        key <<= 1;
        k += (key & (0xFE << i * 8));
    }
    for (int i = 0; i < 8; ++i) kb[i] = (uint8_t)((k >> (8 * i)) & 0xFF);
}

/* Descifra 'len' bytes (se procesan los bloques completos de 8) in place. */
static inline void decrypt(long key, uint8_t *ciph, int len) {
    uint8_t kb[8]; key_to_bytes(key, kb);
    for (int off = 0; off + 8 <= len; off += 8)
        des_decrypt_block(kb, ciph + off);
}

/* Cifra 'len' bytes (se procesan los bloques completos de 8) in place. */
static inline void encrypt(long key, uint8_t *ciph, int len) {
    uint8_t kb[8]; key_to_bytes(key, kb);
    for (int off = 0; off + 8 <= len; off += 8)
        des_encrypt_block(kb, ciph + off);
}

/* Prueba una llave: descifra una COPIA y busca 'search' como subcadena.
 * Devuelve 1 si la llave es candidata (aparece la frase), 0 si no. */
static inline int try_key(long key, const uint8_t *ciph, int len, const char *search) {
    uint8_t temp[BF_MAXLEN + 1];
    if (len > BF_MAXLEN) len = BF_MAXLEN;
    memcpy(temp, ciph, len);
    temp[len] = 0;                 /* terminador para strstr */
    decrypt(key, temp, len);
    return strstr((char *)temp, search) != NULL;
}

/* Carga un .txt a 'buf' y lo rellena con espacios hasta multiplo de 8 (DES-ECB
 * trabaja por bloques de 8 bytes). Programacion defensiva: valida puntero,
 * apertura, archivo vacio y desbordamiento.
 * Devuelve 0 y escribe la longitud (ya padeada) en *out_len, o <0 en error. */
static inline int load_text_file(const char *path, uint8_t *buf, int maxlen, int *out_len) {
    if (!path || !buf || !out_len || maxlen < 8) return -1;
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "error: no se pudo abrir '%s'\n", path); return -2; }

    size_t n = fread(buf, 1, (size_t)maxlen, f);
    int extra = getc(f) != EOF;    /* quedo contenido sin leer? */
    fclose(f);

    if (n == 0) { fprintf(stderr, "error: archivo '%s' vacio\n", path); return -3; }
    if (extra || n > (size_t)maxlen - 8) {
        fprintf(stderr, "error: archivo '%s' excede el limite (%d bytes)\n", path, maxlen);
        return -4;
    }

    int len = (int)n;
    while (len % 8 != 0) buf[len++] = ' ';   /* padding a bloque completo */
    *out_len = len;
    return 0;
}

#endif /* BRUTEFORCE_COMMON_H */
