#include <stdlib.h>
#include "scan.h"

int scan_generic(
		int x1, int y1, int x2, int y2,
		struct Data* d, int(*func)(struct Data*, int, int))
{
    int dx, dy;
    int i, max_i;
    double x, y, fdx, fdy;
    int scan_result;

    dx = x2 - x1;
    dy = y2 - y1;

    if (dx == 0 && dy == 0) {
        return func(d, x1, y1);
    }

    x = (double)x1 + 0.5;
    y = (double)y1 + 0.5;

    if (abs(dx) > abs(dy)) {
        if (x2 > x1) {
            fdx = 1.0;
            fdy = (double)dy / (double)dx;
            max_i = dx;
        } else {
            fdx = -1.0;
            fdy = -((double)dy / (double)dx);
            max_i = -dx;
        }
    } else {
        if (y2 > y1) {
            fdx = (double)dx / (double)dy;
            fdy = 1.0;
            max_i = dy;
        } else {
            fdx = -((double)dx / (double)dy);
            fdy = -1.0;
            max_i = -dy;
        }
    }

    // Skip source field.
    x += fdx;
    y += fdy;

    for (i = 1; i <= max_i; ++i) {
        if ((scan_result = func(d, x, y)) != 0) {
            return scan_result;
        }
        x += fdx;
        y += fdy;
    }

    return 0;
}

int scan_plot(struct Data *d, int x, int y)
{
    int i;

    for (i = 0; i < d->asteroids_count; ++i) {
        if (x >= d->asteroids[i].x1 && x <= d->asteroids[i].x2 &&
            y >= d->asteroids[i].y1 && y <= d->asteroids[i].y2) {
            d->map_buffer[y * MAP_WIDTH + x] = SF_ASTEROID;
            return 1;
        }
    }

    for (i = 0; i < d->enemies_count; ++i) {
        if (x == d->enemies[i].x && y == d->enemies[i].y) {
            d->map_buffer[y * MAP_WIDTH + x] = '0' + i;
            return 1;
        }
    }

    d->map_buffer[y * MAP_WIDTH + x] = SF_SPACE;
    return 0;
}

int scan_visibility(struct Data *d, int x, int y)
{
    int i;

    for (i = 0; i < d->asteroids_count; ++i) {
        if (x >= d->asteroids[i].x1 && x <= d->asteroids[i].x2 &&
            y >= d->asteroids[i].y1 && y <= d->asteroids[i].y2) {
            return FAKE_ASTEROID_INDEX;
        }
    }

    for (i = 0; i < d->enemies_count; ++i) {
        if (x == d->enemies[i].x && y == d->enemies[i].y) {
            return i + 1; // Solve case when enemy 0 hit
        }
    }

    if (x == d->player.x && y == d->player.y) {
        return FAKE_PLAYER_INDEX;
    }

    return 0;
}

