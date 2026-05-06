#include <gint/display.h>
#include <gint/keyboard.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "tinfl.h"

#define TARGET_W        128
#define TARGET_H        64
#define BYTES_PER_FRAME (TARGET_W * TARGET_H / 8)
#define MAX_COMP_FRAME  1536

static uint8_t comp_buf[MAX_COMP_FRAME];
static uint8_t frame_buf[BYTES_PER_FRAME];

void display_mono(uint8_t *pixels) {
    for(int y = 0; y < TARGET_H; y++) {
        for(int x = 0; x < TARGET_W; x++) {
            int byte_idx = y * (TARGET_W >> 3) + (x >> 3);
            int bit_idx  = 7 - (x & 7);
            if(pixels[byte_idx] & (1 << bit_idx))
                dpixel(x, y, C_BLACK);
        }
    }
}

static uint32_t read_u32_le(uint8_t *b) {
    return b[0] | (b[1]<<8) | (b[2]<<16) | (b[3]<<24);
}

static uint16_t read_u16_le(uint8_t *b) {
    return b[0] | (b[1]<<8);
}

int main(void)
{
    dclear(C_WHITE);
    dtext(1, 1,  C_BLACK, "Bad Apple fx-9860G");
    dtext(1, 15, C_BLACK, "Ouverture fichier...");
    dupdate();

    int fd = open("/fls0/bad-apple.bin", O_RDONLY);
    if(fd < 0) {
        dclear(C_WHITE);
        dtext(1, 1,  C_BLACK, "Erreur: fichier");
        dtext(1, 15, C_BLACK, "/fls0/bad-apple.bin");
        dtext(1, 30, C_BLACK, "introuvable !");
        dtext(1, 45, C_BLACK, "AC/ON pour quitter");
        dupdate();
        while(1) {
            key_event_t ev = pollevent();
            if(ev.type == KEYEV_DOWN && ev.key == KEY_ACON) return 1;
        }
    }

    // Lire header (16 octets)
    uint8_t header[16];
    read(fd, header, 16);

    if(memcmp(header, "FXZL", 4) != 0) {
        dclear(C_WHITE);
        dtext(1, 1,  C_BLACK, "Erreur: format");
        dtext(1, 15, C_BLACK, "invalide !");
        dtext(1, 30, C_BLACK, "AC/ON pour quitter");
        dupdate();
        close(fd);
        while(1) {
            key_event_t ev = pollevent();
            if(ev.type == KEYEV_DOWN && ev.key == KEY_ACON) return 1;
        }
    }

    uint32_t fps         = read_u32_le(header + 4);
    uint32_t frame_count = read_u32_le(header + 8);

    // Afficher infos + attendre EXE
    char info1[32], info2[32];
    dclear(C_WHITE);
    dtext(1, 1,  C_BLACK, "Bad Apple fx-9860G");
    snprintf(info1, sizeof(info1), "%lu frames @ %lu fps", frame_count, fps);
    snprintf(info2, sizeof(info2), "Duree: %lu s", frame_count / fps);
    dtext(1, 15, C_BLACK, info1);
    dtext(1, 25, C_BLACK, info2);
    dtext(1, 40, C_BLACK, "EXE=play");
    dtext(1, 50, C_BLACK, "AC/ON=quit");
    dupdate();

    while(1) {
        key_event_t ev = pollevent();
        if(ev.type == KEYEV_DOWN && ev.key == KEY_EXE)  break;
        if(ev.type == KEYEV_DOWN && ev.key == KEY_ACON) {
            close(fd);
            return 1;
        }
    }

    // Boucle lecture + affichage

    for(uint32_t i = 0; i < frame_count; i++) {
        // Lire taille compressée
        uint8_t size_buf[2];
        if(read(fd, size_buf, 2) < 2) break;
        uint16_t comp_size = read_u16_le(size_buf);

        if(comp_size > MAX_COMP_FRAME) {
            // Skip frame corrompue
            uint8_t tmp;
            for(int s = 0; s < comp_size; s++) read(fd, &tmp, 1);
            continue;
        }

        // Lire données compressées
        if(read(fd, comp_buf, comp_size) < comp_size) break;

        // Décompresser avec tinfl
        size_t out_len = tinfl_decompress_mem_to_mem(frame_buf, BYTES_PER_FRAME, comp_buf, comp_size, TINFL_FLAG_PARSE_ZLIB_HEADER);

        if(out_len > 0) {
            dclear(C_WHITE);
            display_mono(frame_buf);
            dupdate();
        }

        // Vérifier quitter
        key_event_t ev = pollevent();
        if(ev.type == KEYEV_DOWN && ev.key == KEY_ACON) break;

        // Timing ~10fps : busy wait calibré pour SH3 ~30MHz
        // 30000000 / 10 = 3000000 cycles, ~2 cycles/iter → 1500000 iter
        // Ajuste ce facteur si trop rapide/lent
        for(volatile int t = 0; t < 800000; t++);
    }

    close(fd);

    dclear(C_WHITE);
    dtext(1, 1,  C_BLACK, "Fin !");
    dtext(1, 15, C_BLACK, "AC/ON pour quitter");
    dupdate();

    while(1) {
        key_event_t ev = pollevent();
        if(ev.type == KEYEV_DOWN && ev.key == KEY_ACON) break;
    }

    return 1;
}