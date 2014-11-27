#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "xeno.h"
#include "data.h"

void data_init_map(struct Data *d)
{
    memset(d->map_buffer, SF_UNSCANNED, sizeof(d->map_buffer));
}

void data_init_asteroids(struct Data *d)
{
    int i;
    d->asteroids_count = XENO_rand_range(ASTEROIDS_MIN, ASTEROIDS_MAX);
    printf("Generating %d asteroids.\n", d->asteroids_count);
    for (i = 0; i < d->asteroids_count; ++i) {
        int width = XENO_rand_range(ASTEROID_SIDE_MIN, ASTEROID_SIDE_MAX);
        int height = XENO_rand_range(ASTEROID_SIDE_MIN, ASTEROID_SIDE_MAX);
        int x = XENO_rand_range(0, MAP_WIDTH - width);
        int y = XENO_rand_range(0, MAP_HEIGHT - height);
        d->asteroids[i].x1 = x;
        d->asteroids[i].y1 = y;
        d->asteroids[i].x2 = x + width;
        d->asteroids[i].y2 = y + height;
    }
}

void data_init_enemies(struct Data *d)
{
    int i;
    int new_count = XENO_rand_range(ENEMIES_MIN, ENEMIES_MAX);
    d->enemies_count = 0;
    for (i = 0; i < new_count; ++i) {
        if (!data_find_empty_field(d,
                MAX_RANDOM_SEEKS,
                &(d->enemies[i].x),
                &(d->enemies[i].y))) {
            fprintf(stderr, "ERROR: Failed finding random free field too many times.\n");
            exit(1);
        }
        d->enemies[i].behavior = EB_IDLE;
        d->enemies[i].hunt_path = NULL;
        d->enemies[i].hunt_path_length = 0;
        d->enemies[i].hunt_path_step = 0;
        ++(d->enemies_count);
    }
}

void data_init_player(struct Data *d)
{
    d->player.health = 100.0;
    if (!data_find_empty_field(d,
            MAX_RANDOM_SEEKS,
            &(d->player.x),
            &(d->player.y))) {
        fprintf(stderr, "ERROR: Failed finding random free field too many times.\n");
        exit(1);
    }
    printf("Generating player at (%d, %d).\n", d->player.x, d->player.y);
}

bool data_find_empty_field(
    struct Data *d,
    int max_seeks,
    int *out_x, int *out_y)
{
    int i, x, y;
    bool found = false;
    int seeks = 0;
    while (!found) {
seek_fail:
        if (++seeks > max_seeks) {
            return false;
        }
        x = XENO_rand_range(0, MAP_WIDTH);
        y = XENO_rand_range(0, MAP_HEIGHT);
        for (i = 0; i < d->asteroids_count; ++i) {
            if (x >= d->asteroids[i].x1 && x <= d->asteroids[i].x2 &&
                y >= d->asteroids[i].y1 && y <= d->asteroids[i].y2) {
                found = false;
                goto seek_fail;
            }
        }
        for (i = 0; i < d->enemies_count; ++i) {
            if (x == d->enemies[i].x && y == d->enemies[i].y) {
                found = false;
                goto seek_fail;
            }
        }
        found = true;
    }
    *out_x = x;
    *out_y = y;
    return true;
}

