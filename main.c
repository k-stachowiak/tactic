#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <termios.h>
#include <unistd.h>

#define MAP_WIDTH 40
#define MAP_HEIGHT 20
#define MAX_RANDOM_SEEKS (10 * MAP_WIDTH * MAP_HEIGHT)
#define MAX_SCAN_LINE_SIZE ((int)(MAP_SIDE * 1.5))
#define ASTEROIDS_MIN 3
#define ASTEROIDS_MAX 5 
#define ASTEROID_SIDE_MIN 3
#define ASTEROID_SIDE_MAX 7
#define ENEMIES_MIN 3
#define ENEMIES_MAX 5

#if MAP_WIDTH < 2 || MAP_HEIGHT < 2
#	error Map side too small!
#endif

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

enum scan_field {
	SF_UNSCANNED = '~',
	SF_SPACE = ' ',
	SF_FOG = '.',
	SF_ASTEROID = '#',
	SF_PLAYER = '*'
};

static struct {

	struct { int x1, y1, x2, y2; } asteroids[ASTEROIDS_MAX];
	int asteroids_count;

	struct { int x, y; } enemies[ENEMIES_MAX];
	int enemies_count;

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

static int XENO_rand_range(unsigned int min, unsigned int max)
{
	int base_random = rand();
	int range = max - min;
	int remainder = RAND_MAX % range;
	int bucket = RAND_MAX / range;

	if (RAND_MAX == base_random) {
		return XENO_rand_range(min, max);
	}

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

static bool data_find_empty_field(int max_seeks, int *out_x, int *out_y)
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
		for (i = 0; i < data.asteroids_count; ++i) {
			if (x >= data.asteroids[i].x1 && x <= data.asteroids[i].x2 &&
				y >= data.asteroids[i].y1 && y <= data.asteroids[i].y2) {
				found = false;
				goto seek_fail;
			}
		}
		for (i = 0; i < data.enemies_count; ++i) {
			if (x == data.enemies[i].x && y == data.enemies[i].y) {
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

static void data_init_enemies(void)
{
	int i;
	int new_count = XENO_rand_range(ENEMIES_MIN, ENEMIES_MAX);
	for (i = 0; i < new_count; ++i) {
		if (!data_find_empty_field(
				MAX_RANDOM_SEEKS,
				&(data.enemies[i].x),
				&(data.enemies[i].y))) {
			fprintf(stderr, "ERROR: Failed finding random free field too many times.\n");
			exit(1);
		}
		++(data.enemies_count);
	}
}

static void data_init_player(void)
{
	if (!data_find_empty_field(
			MAX_RANDOM_SEEKS,
			&(data.player.x),
			&(data.player.y))) {
		fprintf(stderr, "ERROR: Failed finding random free field too many times.\n");
		exit(1);
	}
	printf("Generating player at (%d, %d).\n", data.player.x, data.player.y);
}

static void data_init(void)
{
	data.asteroids_count = 0;
	data.enemies_count = 0;

	data_init_map();
	data_init_asteroids();
	data_init_enemies();
	data_init_player();
}

/*
 * Scanning related operation.
 * ===========================
 */

static int generic_scan(int x1, int y1, int x2, int y2, int(*func)(int, int))
{
	int dx, dy;
	int i, max_i;
	double x, y, fdx, fdy;
	int scan_result;

	dx = x2 - x1;
	dy = y2 - y1;

	if (dx == 0 && dy == 0) {
		return func(x1, y1);
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
		if ((scan_result = func(x, y)) != 0) {
			return scan_result;
		}
		x += fdx;
		y += fdy;
	}

	/* 
	 * If for the precission reasons the target is
	 * not reached let's enforce its scan here.
	 */
	return func(x2, y2);
}

static int tactical_scan(int x, int y)
{
	int i;

	for (i = 0; i < data.asteroids_count; ++i) {
		if (x >= data.asteroids[i].x1 && x <= data.asteroids[i].x2 &&
		    y >= data.asteroids[i].y1 && y <= data.asteroids[i].y2) {
			data.map_buffer[y * MAP_WIDTH + x] = SF_ASTEROID;
			return 1;
		}
	}

	for (i = 0; i < data.enemies_count; ++i) {
		if (x == data.enemies[i].x && y == data.enemies[i].y) {
			data.map_buffer[y * MAP_WIDTH + x] = '0' + i;
			return 1;
		}
	}

	data.map_buffer[y * MAP_WIDTH + x] = SF_SPACE;
	return 0;
}

static int attack_scan(int x, int y)
{
	int i;

	for (i = 0; i < data.asteroids_count; ++i) {
		if (x >= data.asteroids[i].x1 && x <= data.asteroids[i].x2 &&
		    y >= data.asteroids[i].y1 && y <= data.asteroids[i].y2) {
			data.map_buffer[y * MAP_WIDTH + x] = SF_ASTEROID;
			return -1;
		}
	}

	for (i = 0; i < data.enemies_count; ++i) {
		if (x == data.enemies[i].x && y == data.enemies[i].y) {
			data.map_buffer[y * MAP_WIDTH + x] = '0' + i;
			return i;
		}
	}

	return 0;
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

	plot_fog();
	plot_asteroids();
	plot_map();

	for (i = 0; i < MAP_HEIGHT; ++i) {
		printf("%.*s\n", MAP_WIDTH, data.map_buffer + (i * MAP_WIDTH));
	}
}

/*
 * Game engine.
 * ============
 */

static bool game_move_player(int dx, int dy)
{
	int new_x = data.player.x + dx;
	int new_y = data.player.y + dy;

	int new_field = data.map_buffer[new_y * MAP_WIDTH + new_x];

	bool obstacle = new_field == SF_ASTEROID ||
					(new_field >= '0' && new_field <= '9');
	bool outside = new_x < 0 || new_x >= MAP_WIDTH ||
				   new_y < 0 || new_y >= MAP_HEIGHT;

	if (obstacle || outside) {
		return false;
	} else {
		data.player.x = new_x;
		data.player.y = new_y;
		return true;
	}
}

static bool game_fire_laser(void)
{
	int scan_result = 0;
	int target = - 1, hit = -1;
	int x, y;

	while (scan_result != 1) {
		printf("Fire laser, select target [0-%d] (-1 to cancel): ", data.enemies_count - 1);
		scan_result = scanf("%d", &target);
		printf("\n");
		if (target == -1) {
			printf("Aborting attack.\n");
			return false;
		}
		if (target < 0 || target >= data.enemies_count) {
			scan_result = 0;
		}
	} 

	x = data.enemies[target].x;
	y = data.enemies[target].y;

	hit = generic_scan(
			data.player.x,
			data.player.y,
			x,
			y,
			attack_scan);

	if (hit == -1) {
		printf("Obstacle hit.\n");
		return false;
	} else if (hit == target) {
		printf("Target hit.\n");
		return true;
	} else {
		printf("Another one hit.\n");
		return true;
	}
}

static void game_loop(void)
{
	int c = 0;

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
			game_fire_laser();
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
