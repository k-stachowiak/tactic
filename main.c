#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <termios.h>
#include <unistd.h>

#define ASTEROIDS_MAX 5 
#define MAP_SIDE 20
#define MAX_RANDOM_SEEKS (10 * MAP_SIDE * MAP_SIDE)
#define MAX_SCAN_LINE_SIZE ((int)(MAP_SIDE * 1.5))

#if MAP_SIDE < 2
#	error Map side too small!
#endif

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

enum scan_field {
	SF_UNSCANNED = 'X',
	SF_SPACE = ' ',
	SF_FOG = '.',
	SF_ASTEROID = '@',
	SF_PLAYER = '*',
	SF_ENEMY = '$'
};

static struct {

	struct {
		int x1, y1, x2, y2;
	} asteroids[ASTEROIDS_MAX];
	int asteroids_count;

	struct {
		int x;
		int y;
	} player;

	char map_buffer[MAP_SIDE * MAP_SIDE];

} data;

/*
 * Xeno code.
 * ==========
 */

int XENO_getch(void)
{
	struct termios oldattr, newattr;
	int ch;
	tcgetattr( STDIN_FILENO, &oldattr );
	newattr = oldattr;
	newattr.c_lflag &= ~( ICANON | ECHO );
	tcsetattr( STDIN_FILENO, TCSANOW, &newattr );
	ch = getchar();
	tcsetattr( STDIN_FILENO, TCSANOW, &oldattr );
	return ch;
}

/*
 * Data related operations.
 * ========================
 */

void data_init_asteroids(void)
{
	int i, x1, y1, x2, y2;
	data.asteroids_count = rand() % ASTEROIDS_MAX;
	printf("Generating %d asteroids.\n", data.asteroids_count);
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
seek_fail:
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
				goto seek_fail;
			}
		}
		found = true;
	}
	data.player.x = x;
	data.player.y = y;
	printf("Generating player at (%d, %d).\n", data.player.x, data.player.y);
}

void data_init(void)
{
	data_init_asteroids();
	data_init_player();
}

/*
 * Scanning related operation.
 * ===========================
 */

void generic_scan(int x1, int y1, int x2, int y2, bool (*func)(int, int))
{
	int dx, dy;
	int i, max_i;
	double x, y, fdx, fdy;

	dx = x2 - x1;
	dy = y2 - y1;

	if (dx == 0 && dy == 0) {
		func(x1, y1);
		return;
	}

	x = x1;
	y = y1;

	if (abs(dx) > abs(dy)) {
		fdx = (x2 > x1) ? 1.0 : -1.0;
		fdy = (double)dy / (double)dx;
		max_i = abs(dx);
	} else {
		fdx = (double)dx / (double)dy;
		fdy = (y2 > y1) ? 1.0 : -1.0;
		max_i = abs(dy);
	}

	for (i = 0; i <= max_i; ++i) {
		if (!func(x, y)) {
			return;
		}
		x += fdx;
		y += fdy;
	}
}

bool tactical_scan(int x, int y)
{
	int i;

	// Check asteroids collision.
	for (i = 0; i < data.asteroids_count; ++i) {
		if (x >= data.asteroids[i].x1 && x <= data.asteroids[i].x2 &&
		    y >= data.asteroids[i].y1 && y <= data.asteroids[i].y2) {
			data.map_buffer[y * MAP_SIDE + x] = SF_ASTEROID;
			return false;
		}
	}

	// No collision, mark empty space.
	data.map_buffer[y * MAP_SIDE + x] = SF_SPACE;
	return true;
}

/*
 * Plotting operations.
 * ====================
 */

void plot_asteroids(void)
{
	int i, x, y;
	for (i = 0; i < data.asteroids_count; ++i) {
		for (x = data.asteroids[i].x1; x <= data.asteroids[i].x2; ++x) {
			for (y = data.asteroids[i].y1; y <= data.asteroids[i].y2; ++y) {
				data.map_buffer[y * MAP_SIDE + x] = SF_ASTEROID;
			}
		}
	}
}

void plot_map(void)
{
	int x, y;
	for (x = 0; x < MAP_SIDE; ++x) {
		generic_scan(data.player.x, data.player.y, x, 0, tactical_scan);
		generic_scan(data.player.x, data.player.y, x, MAP_SIDE - 1, tactical_scan);
	}

	for (y = 1; y < (MAP_SIDE - 1); ++y) {
		generic_scan(data.player.x, data.player.y, 0, y, tactical_scan);
		generic_scan(data.player.x, data.player.y, MAP_SIDE - 1, y, tactical_scan);
	}

	data.map_buffer[data.player.y * MAP_SIDE + data.player.x] = SF_PLAYER;
}

/*
 * Prining operations.
 * ===================
 */

void print_welcome(void)
{
	printf("Welcome to the Space Tactical Battle!\n");
}

void print_status(void)
{
	int i;
	memset(data.map_buffer, SF_UNSCANNED, sizeof(data.map_buffer));

	printf("Tactical status:\n\n");
	printf("Map:\n");
	// plot_asteroids();
	plot_map();

	for (i = 0; i < MAP_SIDE; ++i) {
		printf("%.*s\n", MAP_SIDE, data.map_buffer + (i * MAP_SIDE));
	}
}

/*
 * Game engine.
 * ============
 */

void game_loop(void)
{
	int c = 0;

	print_status();
	while ((c = XENO_getch()) != 'q') {
		switch (c) {
		case 'h':
			data.player.x -= 1;
			break;
		case 'j':
			data.player.y += 1;
			break;
		case 'k':
			data.player.y -= 1;
			break;
		case 'l':
			data.player.x += 1;
			break;
		default:
			break;
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
