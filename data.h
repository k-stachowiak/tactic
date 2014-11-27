#ifndef DATA_H
#define DATA_H

#include <stdbool.h>
#include "config.h"

enum scan_field {
    SF_UNSCANNED = '~',
    SF_SPACE = ' ',
    SF_FOG = '.',
    SF_ASTEROID = '#',
    SF_PLAYER = '*',
    SF_PATH = '`'
};

enum move_result {
    MR_CLEAR,
    MR_BLOCK,
    MR_SHIP
};

enum enemy_behavior {
    EB_IDLE,
    EB_HUNT
};

struct Data {

    struct { int x1, y1, x2, y2; } asteroids[ASTEROIDS_MAX];
    int asteroids_count;

    struct {
        int x, y;
        enum enemy_behavior behavior;
        int *hunt_path;
        int hunt_path_length;
        int hunt_path_step;
    } enemies[ENEMIES_MAX];
    int enemies_count;

    struct {
        int x, y;
        double health;
    } player;

    char map_buffer[MAP_WIDTH * MAP_HEIGHT];

};

void data_init_map(struct Data *d);
void data_init_asteroids(struct Data *d);
void data_init_enemies(struct Data *d);
void data_init_player(struct Data *d);

/** @brief Finds a random field that is not occupied.
  * @param[in] max_seeks The maximum number of acceptable fails.
  * @param[out] out_x The x coordinate of the found point.
  * @param[out] out_y The y coordinate of the found point.
  * @return True if seek was successfull, false otherwise.
  */
bool data_find_empty_field(
    struct Data *d,
    int max_seeks,
    int *out_x, int *out_y);

#endif
