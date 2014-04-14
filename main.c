#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <termios.h>
#include <unistd.h>

#define ASTEROIDS_MIN 3
#define ASTEROIDS_MAX 5 
#define ASTEROID_SIDE_MIN 3
#define ASTEROID_SIDE_MAX 7
#define MAP_WIDTH 40
#define MAP_HEIGHT 20
#define MAX_RANDOM_SEEKS (10 * MAP_WIDTH * MAP_HEIGHT)
#define MAX_SCAN_LINE_SIZE ((int)(MAP_SIDE * 1.5))

#if MAP_WIDTH < 2 || MAP_HEIGHT < 2
#	error Map side too small!
#endif

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

enum scan_field {
	SF_UNSCANNED = '~',
	SF_SPACE = ' ',
	SF_FOG = '.',
	SF_ASTEROID = '@',
	SF_PLAYER = '*',
	SF_ENEMY = '$'
};

static struct {

	struct { int x1, y1, x2, y2; } asteroids[ASTEROIDS_MAX];
	int asteroids_count;

	struct { int x, y; } player;

	char map_buffer[MAP_WIDTH * MAP_HEIGHT];

} data;

/*
 * Xeno code.
 * ==========
 */

static int XENO_getch(void)
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

static int XENO_rand_range (unsigned int min, unsigned int max)
{
	int base_random = rand(); /* in [0, RAND_MAX] */
	int range = max - min;
	int remainder = RAND_MAX % range;
	int bucket = RAND_MAX / range;

	/* Prevent max to ensure [0, RAND_MAX) */
	if (RAND_MAX == base_random) {
		return XENO_rand_range(min, max);
	}

	/* There are range buckets, plus one smaller interval within remainder of RAND_MAX */
	if (base_random < RAND_MAX - remainder) {
		return min + base_random/bucket;
	} else {
		return XENO_rand_range(min, max);
	}
}

/*
 * Data related operations.
 * ========================
 */

static void data_init_map(void)
{
	memset(data.map_buffer, SF_UNSCANNED, sizeof(data.map_buffer));
}

static void data_init_asteroids(void)
{
	int i;
	data.asteroids_count = XENO_rand_range(ASTEROIDS_MIN, ASTEROIDS_MAX);
	printf("Generating %d asteroids.\n", data.asteroids_count);
	for (i = 0; i < data.asteroids_count; ++i) {
		int width = XENO_rand_range(ASTEROID_SIDE_MIN, ASTEROID_SIDE_MAX);
		int height = XENO_rand_range(ASTEROID_SIDE_MIN, ASTEROID_SIDE_MAX);
		int x = XENO_rand_range(0, MAP_WIDTH - width);
		int y = XENO_rand_range(0, MAP_HEIGHT - height);
		data.asteroids[i].x1 = x;
		data.asteroids[i].y1 = y;
		data.asteroids[i].x2 = x + width;
		data.asteroids[i].y2 = y + height;
	}
}

static void data_init_player(void)
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
		x = XENO_rand_range(0, MAP_WIDTH);
		y = XENO_rand_range(0, MAP_HEIGHT);
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

static void data_init(void)
{
	data_init_map();
	data_init_asteroids();
	data_init_player();
}

/*
 * Scanning related operation.
 * ===========================
 */

static void generic_scan(int x1, int y1, int x2, int y2, bool (*func)(int, int))
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

	for (i = 0; i <= max_i; ++i) {
		if (!func(x, y)) {
			return;
		}
		x += fdx;
		y += fdy;
	}
}

static bool tactical_scan(int x, int y)
{
	int i;

	// Check asteroids collision.
	for (i = 0; i < data.asteroids_count; ++i) {
		if (x >= data.asteroids[i].x1 && x <= data.asteroids[i].x2 &&
		    y >= data.asteroids[i].y1 && y <= data.asteroids[i].y2) {
			data.map_buffer[y * MAP_WIDTH + x] = SF_ASTEROID;
			return false;
		}
	}

	// No collision, mark empty space.
	data.map_buffer[y * MAP_WIDTH + x] = SF_SPACE;
	return true;
}

/*
 * Plotting operations.
 * ====================
 */

static void plot_fog(void)
{
	int i;
	for (i = 0; i < MAP_WIDTH * MAP_HEIGHT; ++i) {
		if (data.map_buffer[i] == SF_SPACE || data.map_buffer[i] == SF_PLAYER) {
			data.map_buffer[i] = SF_FOG;
		}
	}
}

static void plot_asteroids(void)
{
	/*
	int i, x, y;
	for (i = 0; i < data.asteroids_count; ++i) {
		for (x = data.asteroids[i].x1; x <= data.asteroids[i].x2; ++x) {
			for (y = data.asteroids[i].y1; y <= data.asteroids[i].y2; ++y) {
				data.map_buffer[y * MAP_WIDTH + x] = SF_ASTEROID;
			}
		}
	}
	//*/
}

static void plot_map(void)
{
	int x, y;
	
	plot_fog();
	plot_asteroids();

	for (x = 0; x < MAP_WIDTH; ++x) {
		for (y = 0; y < MAP_HEIGHT; ++y) {
			generic_scan(data.player.x, data.player.y, x, y, tactical_scan);
		}
	}

	data.map_buffer[data.player.y * MAP_WIDTH + data.player.x] = SF_PLAYER;
}

/*
 * Prining operations.
 * ===================
 */

static void print_welcome(void)
{
	printf("Welcome to the Space Tactical Battle!\n");
}

static void print_status(void)
{
	int i;

	printf("Tactical status:\n\n");
	printf("Map:\n");

	plot_map();

	for (i = 0; i < MAP_HEIGHT; ++i) {
		printf("%.*s\n", MAP_WIDTH, data.map_buffer + (i * MAP_WIDTH));
	}
}

/*
 * Game engine.
 * ============
 */

static void game_loop(void)
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
