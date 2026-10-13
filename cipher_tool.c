/* cipher_tool - cifra/descifra un archivo de texto con DES usando una llave
 * entera como parametro. Secuencial, sin MPI.*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "bruteforce_common.h"

static void usage(const char *prog) {
    fprintf(stderr,
        "uso:\n"
        "  %s encrypt <entrada.txt> <salida.bin> <llave>\n"
        "  %s decrypt <entrada.bin> <llave> [palabra_clave]\n", prog, prog);
}

static int do_encrypt(const char *in, const char *out, long key) {
    uint8_t buf[BF_MAXLEN + 1];
    int len;
    if (load_text_file(in, buf, BF_MAXLEN, &len) != 0) return 1;

    encrypt(key, buf, len);

    FILE *f = fopen(out, "wb");
    if (!f) { fprintf(stderr, "error: no se pudo escribir '%s'\n", out); return 1; }
    size_t w = fwrite(buf, 1, (size_t)len, f);
    fclose(f);
    if ((int)w != len) { fprintf(stderr, "error: escritura incompleta\n"); return 1; }

    printf("Cifrado OK\n");
    printf("  Archivo entrada : %s\n", in);
    printf("  Archivo salida  : %s (%d bytes, %d bloques)\n", out, len, len / 8);
    printf("  Llave           : %ld\n", key);
    return 0;
}

static int do_decrypt(const char *in, long key, const char *keyword) {
    uint8_t buf[BF_MAXLEN + 1];
    int len;
    if (load_text_file(in, buf, BF_MAXLEN, &len) != 0) return 1;

    decrypt(key, buf, len);
    buf[len] = 0;

    printf("Descifrado OK\n");
    printf("  Archivo         : %s (%d bytes)\n", in, len);
    printf("  Llave           : %ld\n", key);
    printf("  Texto           : \"%s\"\n", (char *)buf);
    if (keyword) {
        int ok = strstr((char *)buf, keyword) != NULL;
        printf("  Palabra clave   : \"%s\" -> %s\n",
               keyword, ok ? "ENCONTRADA" : "no aparece");
    }
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 2) { usage(argv[0]); return 1; }

    if (strcmp(argv[1], "encrypt") == 0) {
        if (argc != 5) { usage(argv[0]); return 1; }
        return do_encrypt(argv[2], argv[3], atol(argv[4]));
    }
    if (strcmp(argv[1], "decrypt") == 0) {
        if (argc != 4 && argc != 5) { usage(argv[0]); return 1; }
        return do_decrypt(argv[2], atol(argv[3]), argc == 5 ? argv[4] : NULL);
    }
    usage(argv[0]);
    return 1;
}
