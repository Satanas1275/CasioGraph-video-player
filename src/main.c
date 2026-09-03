#include <gint/display.h>
#include <gint/keyboard.h>
#include <gint/timer.h>
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

typedef struct {
    const char *name;
    const char *path;
} video_entry;

static video_entry video_list[] = {
    { "Bad Apple",    "/fls0/bad-apple.bin" },
    { "Rick Roll",    "/fls0/rick-roll.bin" },
};
static const int video_count = sizeof(video_list) / sizeof(video_list[0]);

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

int play_video(const char *path, const char *name)
{
    int fd = open(path, O_RDONLY);
    if(fd < 0) return -1;

    uint8_t header[16];
    read(fd, header, 16);

    if(memcmp(header, "FXZL", 4) != 0) {
        close(fd);
        return -1;
    }

    uint32_t fps         = read_u32_le(header + 4);
    uint32_t frame_count = read_u32_le(header + 8);

    // Afficher infos + attendre EXE
    char info1[32], info2[32];
    dclear(C_WHITE);
    dtext(1, 1,  C_BLACK, name);
    snprintf(info1, sizeof(info1), "%lu frames @ %lu fps", frame_count, fps);
    snprintf(info2, sizeof(info2), "Duree: %lu s", frame_count / fps);
    dtext(1, 15, C_BLACK, info1);
    dtext(1, 25, C_BLACK, info2);
    dtext(1, 40, C_BLACK, "EXE=play");
    dtext(1, 50, C_BLACK, "AC/ON=menu");
    dupdate();

    while(1) {
        key_event_t ev = pollevent();
        if(ev.type == KEYEV_DOWN && ev.key == KEY_EXE)  break;
        if(ev.type == KEYEV_DOWN && ev.key == KEY_ACON) {
            close(fd);
            return 0;
        }
    }

    // Boucle lecture (en boucle)
    while(1) {
        lseek(fd, 16, SEEK_SET);

        for(uint32_t i = 0; i < frame_count; i++) {
            uint8_t size_buf[2];
            if(read(fd, size_buf, 2) < 2) break;
            uint16_t comp_size = read_u16_le(size_buf);

            if(comp_size > MAX_COMP_FRAME) {
                uint8_t tmp;
                for(int s = 0; s < comp_size; s++) read(fd, &tmp, 1);
                continue;
            }

            if(read(fd, comp_buf, comp_size) < comp_size) break;

            size_t out_len = tinfl_decompress_mem_to_mem(frame_buf, BYTES_PER_FRAME,
                             comp_buf, comp_size, TINFL_FLAG_PARSE_ZLIB_HEADER);

            if(out_len > 0) {
                dclear(C_WHITE);
                display_mono(frame_buf);
                dupdate();
            }

            // Timing précis
            uint64_t delay_us = 1000000 / fps;
            int t = timer_configure(TIMER_ANY, delay_us, GINT_CALL_NULL);
            if(t >= 0) {
                timer_spinwait(t);
                timer_stop(t);
            }

            // Vérifier AC/ON
            key_event_t ev = pollevent();
            if(ev.type == KEYEV_DOWN && ev.key == KEY_ACON) break;
        }

        // Fin de lecture ou AC/ON
        dclear(C_WHITE);
        dtext(1, 1, C_BLACK, "Fin de la video");
        dtext(1, 15, C_BLACK, "AC/ON=menu");
        dtext(1, 25, C_BLACK, "EXE=rejouer");
        dupdate();

        key_event_t ev;
        do { ev = pollevent(); } while(ev.type != KEYEV_DOWN);

        if(ev.key == KEY_ACON) break;
        // EXE relance la boucle
    }

    close(fd);
    return 0;
}

int main(void)
{
    int selected = 0;

    while(1) {
        // Menu de sélection
        dclear(C_WHITE);
        dtext(1, 1, C_BLACK, "Video Player");
        for(int i = 0; i < video_count; i++) {
            if(i == selected)
                dtext(1, 15 + i * 10, C_BLACK, ">");
            dtext(10, 15 + i * 10, C_BLACK, video_list[i].name);
        }
        dtext(1, 50, C_BLACK, "FLECHES=naviguer");
        dtext(1, 60, C_BLACK, "EXE=play AC/ON=menu");
        dupdate();

        key_event_t ev;
        do { ev = pollevent(); } while(ev.type != KEYEV_DOWN);

        if(ev.key == KEY_UP)   selected = (selected - 1 + video_count) % video_count;
        if(ev.key == KEY_DOWN) selected = (selected + 1) % video_count;
        if(ev.key == KEY_EXE)  play_video(video_list[selected].path, video_list[selected].name);
        if(ev.key == KEY_ACON) return 1; // Retour au menu calculatrice
    }
}