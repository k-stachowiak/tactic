#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <math.h>

#define ASTEROIDS_MAX 20
#define MAP_SIDE 20
#define MAX_RANDOM_SEEKS (10 * MAP_SIDE * MAP_SIDE)
#define MAX_SCAN_LINE_SIZE ((int)(MAP_SIDE * 1.5))

#if MAP_SIDE < 2
#	error Map side too small!
#endif

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

enum scan_field {
	SF_UNSCANNED = '.',
	SF_SPACE = ' ',
	SF_ASTEROID = '@',
	SF_PLAYER = '*',
	SF_ENEMY = '$'
};

struct {

	struct {
		int x1, y1, x2, y2;
	} asteroids[ASTEROIDS_MAX];
	int asteroids_count;

	struct {
		int x;
		int y;
	} player;

	int map_buffer[MAP_SIDE * MAP_SIDE];

} data;

void data_init_asteroids(void)
{
	int i, x1, y1, x2, y2;
	data.asteroids_count = rand() % ASTEROIDS_MAX;
	for (i = 0; i < data.asteroids_count; ++i) {
		x1 = rand() % MAP_SIDE;
		y1 = rand() % MAP_SIDE;
		x2 = rand() % MAP_SIDE;
		y2 = rand() % MAP_SIDE;
		data.asteroids[i].x1 = MIN(x1, x2);
		data.asteroids[i].y1 = MIN(y1, y2);
		data.asteroids[i].x2 = MAX(x1, x2);
		data.asteroids[i].y2 = MAX(y1, y2);
	}
}

void data_init_player(void)
{
	int i, x, y;
	bool found = false;
	int seeks = 0;
	while (!found) {
		if (++seeks > MAX_RANDOM_SEEKS) {
			fprintf(stderr, "ERROR: Failed finding place for player too many times.\n");
			exit(1);
		}
		x = rand() % MAP_SIDE;
		y = rand() % MAP_SIDE;
		for (i = 0; i < data.asteroids_count; ++i) {
			if (x >= data.asteroids[i].x1 && x <= data.asteroids[i].x2 &&
				y >= data.asteroids[i].y1 && y <= data.asteroids[i].y2) {
				found = false;
				continue;
			}
		}
		found = true;
	}
	data.player.x = x;
	data.player.y = y;
}

void data_init(void)
{
	data_init_asteroids();
	data_init_player();
}

void tactical_scan(int x1, int y1, int x2, int y2)
{
}

void print_welcome(void)
{
	printf("Welcome to the Space Tactical Battle!\n");
}

void pint_map(void)
{
	int i, x, y;

	memset(map_buffer, ' ', sizeof(map_buffer));

	for (x = 0; x < MAP_SIDE; ++x) {
		tactical_scan(data.player.x, data.player.y, x, 0);
		tactical_scan(data.player.x, data.player.y, x, MAP_SIDE - 1);
	}

	for (y = 1; y < (MAP_SIDE - 1); ++y) {
		tactical_scan(data.player.x, data.player.y, 0, y);
		tactical_scan(data.player.x, data.player.y, MAP_SIDE - 1, y);
	}

	for (i = 0; i < MAP_SIDE; ++i) {
		printf("%s.*s\n", MAP_SIDE, data.map_buffer + i * MAP_SIDE);
	}
}

void print_status(void)
{
	printf("Tactical status:\n");
	print_map();
}

void game_loop(void)
{
	print_status();
}

int main()
{
	srand(time(NULL));

	data_init();
	print_welcome();
	game_loop();
	return 0;
}
