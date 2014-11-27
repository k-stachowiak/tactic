#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <unistd.h>

#include "config.h"
#include "xeno.h"
#include "data.h"
#include "scan.h"

#define MIN(MACRO_x, MACRO_y) ((MACRO_x) < (MACRO_y) ? (MACRO_x) : (MACRO_y))
#define MAX(MACRO_x, MACRO_y) ((MACRO_x) > (MACRO_y) ? (MACRO_x) : (MACRO_y))

#define PRINT_HR(MACRO_width)\
    do {\
        int MACRO_i;\
        for (MACRO_i = 0; MACRO_i < (MACRO_width); ++MACRO_i) {\
            printf("=");\
        }\
        printf("\n");\
    } while(0)

struct Data data;

static struct {
    int pred_map[MAP_WIDTH * MAP_HEIGHT];
    bool close_map[MAP_WIDTH * MAP_HEIGHT];
    double cost_map[MAP_WIDTH * MAP_HEIGHT];
} relax;

static void data_init(void)
{
    data_init_map(&data);
    data_init_asteroids(&data);
    data_init_enemies(&data);
    data_init_player(&data);
}

/*
 * Plotting operations.
 * ====================
 */

static void plot_fog(void)
{
    int i;
    for (i = 0; i < MAP_WIDTH * MAP_HEIGHT; ++i) {
        if (data.map_buffer[i] == SF_SPACE ||
            data.map_buffer[i] == SF_PLAYER ||
            (data.map_buffer[i] >= '0' && data.map_buffer[i] <= '9')) {
                data.map_buffer[i] = SF_FOG;
        }
    }
}

static void plot_map(void)
{
    int x, y;
    for (x = 0; x < MAP_WIDTH; ++x) {
        for (y = 0; y < MAP_HEIGHT; ++y) {
            scan_generic(data.player.x, data.player.y, x, y, &data, scan_plot);
        }
    }
    data.map_buffer[data.player.y * MAP_WIDTH + data.player.x] = SF_PLAYER;
}

static void plot_paths(void)
{
    int e, i;
    for (e = 0; e < data.enemies_count; ++e) {
        for (i = 0; i < data.enemies[e].hunt_path_length; ++i) {
            int step = data.enemies[e].hunt_path[i];
            data.map_buffer[step] = SF_PATH;
        }
    }
}

/*
 * Prining operations.
 * ===================
 */

static void print_welcome(void)
{
    printf("Welcome to the Space Tactical Battle!\n");
}

static int print_laser_prompt(void)
{
    int scan_result = 0;
    int target = -1;

    while (scan_result != 1) {
        printf("Fire laser, select target [0-%d] (negative value to cancel): ", data.enemies_count - 1);
        scan_result = scanf("%d", &target);
        printf("\n");
        if (target < 0) {
            return -1;
        }
        if (target < 0 || target >= data.enemies_count) {
            scan_result = 0;
        }
    }

    return target;
}

static void print_status(void)
{
    int i;

    printf("Tactical status:\n");
    printf("\n");

    printf("Aseroids: %d\n", data.asteroids_count);
    printf("Enemies : %d\n", data.enemies_count);
    printf("\n");

    plot_fog();
    plot_paths();
    plot_map();

    PRINT_HR(MAP_WIDTH);
    for (i = 0; i < MAP_HEIGHT; ++i) {
        printf("%.*s\n", MAP_WIDTH, data.map_buffer + (i * MAP_WIDTH));
    }
    PRINT_HR(MAP_WIDTH);
}

/*
 * Game engine.
 * ============
 */

static void game_set_hunt_path_INIT(int source)
{
    int i;
    for (i = 0; i < MAP_WIDTH * MAP_HEIGHT; ++i) {
        relax.pred_map[i] = i;
        relax.close_map[i] = false;
        relax.cost_map[i] = INFINITY;
    }

    relax.cost_map[source] = 0;
}

static int game_set_hunt_path_CHEAPEST(void)
{
    int i, cheap = -1;
    double cheap_cost = INFINITY;
    for (i = 0; i < MAP_WIDTH * MAP_HEIGHT; ++i) {
        if (relax.close_map[i] == false &&
            relax.cost_map[i] < cheap_cost) {
                cheap_cost = relax.cost_map[i];
                cheap = i;
        }
    }
    return cheap;
}

static void game_set_hunt_path_RELAX(int x1, int y1, int x2, int y2)
{
    const int src = y1 * MAP_WIDTH + x1;
    const int dst = y2 * MAP_WIDTH + x2;
    const double src_cost = relax.cost_map[src];
    const double dst_cost = relax.cost_map[dst];

    if ((src_cost + 1.0) < dst_cost) {
        relax.cost_map[dst] = src_cost + 1.0;
        relax.pred_map[dst] = src;
    }
}

static void game_set_hunt_path_BUILD(int index)
{
    const int src = data.enemies[index].y * MAP_WIDTH + data.enemies[index].x;
    const int dst = data.player.y * MAP_WIDTH + data.player.x;
    int path_length = 0;
    int write_index = 0;
    int cur = src;

    while (cur != dst) {
        ++path_length;
        cur = relax.pred_map[cur];
    }
    ++path_length;

    data.enemies[index].hunt_path_length = path_length;
    data.enemies[index].hunt_path = malloc(
            path_length * sizeof(*data.enemies[index].hunt_path));

    cur = src;
    while (cur != dst) {
        data.enemies[index].hunt_path[write_index++] = cur;
        cur = relax.pred_map[cur];
    }
    data.enemies[index].hunt_path[write_index++] = cur;

    data.enemies[index].hunt_path_step = 0;
}

static void game_set_hunt_path(int index)
{
    const int px = data.player.x;
    const int py = data.player.y;
    const int ex = data.enemies[index].x;
    const int ey = data.enemies[index].y;
    const int src = py * MAP_WIDTH + px;
    const int dst = ey * MAP_WIDTH + ex;
    int cur, cur_x, cur_y;

    game_set_hunt_path_INIT(src);

    cur = src;
    cur_x = ex;
    cur_y = ey;

    while (cur != dst) {

        cur = game_set_hunt_path_CHEAPEST();

        cur_x = cur % MAP_WIDTH;
        cur_y = cur / MAP_WIDTH;

        if (cur_x> 0) {
            game_set_hunt_path_RELAX(cur_x, cur_y, cur_x - 1, cur_y);
        }
        if (cur_x < (MAP_WIDTH - 1)) {
            game_set_hunt_path_RELAX(cur_x, cur_y, cur_x + 1, cur_y);
        }
        if (cur_y > 0) {
            game_set_hunt_path_RELAX(cur_x, cur_y, cur_x, cur_y - 1);
        }
        if (cur_y < (MAP_HEIGHT - 1)) {
            game_set_hunt_path_RELAX(cur_x, cur_y, cur_x, cur_y + 1);
        }

        relax.close_map[cur] = true;
    }

    game_set_hunt_path_BUILD(index);
}

static enum move_result game_try_move(int new_x, int new_y)
{
    const int new_field = data.map_buffer[new_y * MAP_WIDTH + new_x];

    const bool obstacle = new_field == SF_ASTEROID;
    const bool enemy = (new_field >= '0' && new_field <= '9');
    const bool player = (new_x == data.player.x && new_y == data.player.y);
    const bool outside = new_x < 0 || new_x >= MAP_WIDTH ||
                   new_y < 0 || new_y >= MAP_HEIGHT;

    if (obstacle || outside) {
        return MR_BLOCK;
    } else if (enemy || player) {
        return MR_SHIP;
    } else {
        return MR_CLEAR;
    }
}

static void game_move_player(int dx, int dy)
{
    const int new_x = data.player.x + dx;
    const int new_y = data.player.y + dy;

    switch (game_try_move(new_x, new_y)) {
    case MR_CLEAR:
        data.player.x = new_x;
        data.player.y = new_y;
        /* Intentional fall-through! */
    case MR_BLOCK:
        break;
    case MR_SHIP:
        data.player.health = 0.0;
        break;
    }
}

static void game_move_enemy(int index, int dx, int dy)
{
    const int new_x = data.enemies[index].x + dx;
    const int new_y = data.enemies[index].y + dy;

    switch (game_try_move(new_x, new_y)) {
    case MR_CLEAR:
        data.enemies[index].x = new_x;
        data.enemies[index].y = new_y;
        /* Intentional fall-through! */
    case MR_BLOCK:
    case MR_SHIP:
        break;
    }
}

static void game_hit_enemy(int index)
{
    data.enemies[index] = data.enemies[data.enemies_count - 1];
    --data.enemies_count;
}

static void game_hit_player(void)
{
    data.player.health = 100.0;
}

static bool game_fire_laser(void)
{
    int target = - 1, hit = -1, scan_result = -1;
    int x, y;

    if (data.enemies_count == 0) {
        printf("No enemies to target.\n");
        return false;
    }

    if ((target = print_laser_prompt()) == -1) {
        printf("Aborting attack.\n");
        return false;
    }

    x = data.enemies[target].x;
    y = data.enemies[target].y;
    scan_result = scan_generic(data.player.x, data.player.y, x, y, &data, scan_visibility);
    hit = scan_result - 1;

    if (scan_result == 0) {
        printf("Nothing hit.\n");
        return false;

    } else if (scan_result == FAKE_ASTEROID_INDEX) {
        printf("Obstacle hit.\n");
        return false;

    } else if (scan_result == FAKE_PLAYER_INDEX) {
        printf("Player hit player - this shouldn't happen.\n");
        exit(1);
    } else if (hit == target) {
        printf("Target hit.\n");
        game_hit_enemy(hit);
        return true;

    } else {
        printf("Another one hit.\n");
        game_hit_enemy(hit);
        return true;
    }
}

static void game_enemy_idle(int index)
{
    int dx, dy;
    int scan_result;
    const int px = data.player.x;
    const int py = data.player.y;
    const int ex = data.enemies[index].x;
    const int ey = data.enemies[index].y;

    scan_result = scan_generic(ex, ey, px, py, &data, scan_visibility);

    if (scan_result == FAKE_PLAYER_INDEX) {
        // Atack and begin hunt.
        data.enemies[index].behavior = EB_HUNT;
        game_set_hunt_path(index);
        game_hit_player();
    } else {
        // Move cluelessly.
        dx = XENO_rand_range(0, 2) - 1;
        dy = XENO_rand_range(0, 2) - 1;
        game_move_enemy(index, dx, dy);
    }
}

static void game_enemy_hunt()
{
    /* 1. if player spotted : shoot
     * 2. else set new hunt path and follow it.
     * 3. If at the end of the hunt path - goto IDLE sate
     */
}

static void game_enemy_turn(int index)
{
    switch (data.enemies[index].behavior) {
    case EB_IDLE:
        game_enemy_idle(index);
        break;
    case EB_HUNT:
        game_enemy_hunt(index);
        break;
    }
}

static void game_loop(void)
{
    int c = 0, i;
    bool fr;

    print_status();
    while ((c = XENO_getch()) != 'q') {
        switch (c) {
        case 'h':
            game_move_player(-1, 0);
            break;
        case 'j':
            game_move_player(0, 1);
            break;
        case 'k':
            game_move_player(0, -1);
            break;
        case 'l':
            game_move_player(1, 0);
            break;
        case 'L':
            fr = game_fire_laser();
            printf("Attack %s!\n", fr ? "success" : "failure");
            break;
        default:
            break;
        }

        if (data.enemies_count == 0) {
            printf("You are awesome!\n");
            break;
        }

        if (data.player.health <= 0.0) {
            printf("You failed!\n");
            break;
        }

        for (i = 0; i < data.enemies_count; ++i) {
            game_enemy_turn(i);
        }

        print_status();
    }
}

int main()
{
    srand(time(NULL));

    data_init();
    print_welcome();
    game_loop();

    return 0;
}

