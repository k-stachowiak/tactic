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

#define FAKE_PLAYER_INDEX 999
#define FAKE_ASTEROID_INDEX -1

#if MAP_WIDTH < 2 || MAP_HEIGHT < 2
#	error Map side too small!
#endif

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define PRINT_HR(MACRO_width)\
	do {\
		int MACRO_i;\
		for (MACRO_i = 0; MACRO_i < (MACRO_width); ++MACRO_i) {\
			printf("=");\
		}\
		printf("\n");\
	} while(0)

enum scan_field {
	SF_UNSCANNED = '~',
	SF_SPACE = ' ',
	SF_FOG = '.',
	SF_ASTEROID = '#',
	SF_PLAYER = '*'
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

static struct {

	struct { int x1, y1, x2, y2; } asteroids[ASTEROIDS_MAX];
	int asteroids_count;

	struct {
		int x, y;
		enum enemy_behavior behavior;
		struct { int x, y; } *hunt_path;
		int hunt_path_length;
	} enemies[ENEMIES_MAX];
	int enemies_count;

	struct {
		int x, y;
		double health;
	} player;

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

static int XENO_rand_range(unsigned min, unsigned max)
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

/** @brief Finds a random field that is not occupied.
  * @param[in] max_seeks The maximum number of acceptable fails.
  * @param[out] out_x The x coordinate of the found point.
  * @param[out] out_y The y coordinate of the found point.
  * @return True if seek was successfull, false otherwise.
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
	data.enemies_count = 0;
	int new_count = XENO_rand_range(ENEMIES_MIN, ENEMIES_MAX);
	for (i = 0; i < new_count; ++i) {
		if (!data_find_empty_field(
				MAX_RANDOM_SEEKS,
				&(data.enemies[i].x),
				&(data.enemies[i].y))) {
			fprintf(stderr, "ERROR: Failed finding random free field too many times.\n");
			exit(1);
		}
		data.enemies[i].behavior = EB_IDLE;
		data.enemies[i].hunt_path = NULL;
		data.enemies[i].hunt_path_length = 0;
		++(data.enemies_count);
	}
}

static void data_init_player(void)
{
	data.player.health = 100.0;
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
	data_init_map();
	data_init_asteroids();
	data_init_enemies();
	data_init_player();
}

/*
 * Scanning related operation.
 * ===========================
 */

/** @brief Scans a line between two points, calling a callback function
  *        for each point along the scan line. If the callback returns
  *        a non-zero value, the scan is stopped and the value is returned.
  * @param x1 The x coordinate of the start point.
  * @param y1 The y coordinate of the start point.
  * @param x2 The x coordinate of the end point.
  * @param y2 The y coordinate of the end point.
  * @param func The callback function.
  * @return 0 if scan was performed without interruption, the hit value
  *         if the scan was interrupted.
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
		if ((scan_result = func(x, y)) != 0) {
			return scan_result;
		}
		x += fdx;
		y += fdy;
	}

	return 0;
}

/** @brief Callback function for a map plotting scan.
  * @param x The x coordinate of the scanned point.
  * @param y The y coordinate of the scanned point.
  * @return 0 in nothing was detected, 1 if an obstacle was hit.
  */
static int plot_scan(int x, int y)
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

/** @brief Callback for the visibility scan.
  * @param x The x coordinate of the scanned point.
  * @param y The y coordinate of the scanned point.
  * @return 0 if nothing was hit,
  *         -1 if asteroid was hit,
  *         index of enemy + 1 if an enemy was hit.
  */
static int visibility_scan(int x, int y)
{
	int i;

	for (i = 0; i < data.asteroids_count; ++i) {
		if (x >= data.asteroids[i].x1 && x <= data.asteroids[i].x2 &&
		    y >= data.asteroids[i].y1 && y <= data.asteroids[i].y2) {
			return FAKE_ASTEROID_INDEX;
		}
	}

	for (i = 0; i < data.enemies_count; ++i) {
		if (x == data.enemies[i].x && y == data.enemies[i].y) {
			return i + 1; // Solve case when enemy 0 hit
		}
	}

	if (x == data.player.x && y == data.player.y) {
		return FAKE_PLAYER_INDEX;
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
			generic_scan(data.player.x, data.player.y, x, y, plot_scan);
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

static enum move_result try_move(int new_x, int new_y)
{
	int new_field = data.map_buffer[new_y * MAP_WIDTH + new_x];

	bool obstacle = new_field == SF_ASTEROID;
	bool enemy = (new_field >= '0' && new_field <= '9');
	bool player = (new_x == data.player.x && new_y == data.player.y);
	bool outside = new_x < 0 || new_x >= MAP_WIDTH ||
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
	int new_x = data.player.x + dx;
	int new_y = data.player.y + dy;

	switch (try_move(new_x, new_y)) {
	case MR_CLEAR:
		data.player.x = new_x;
		data.player.y = new_y;
	case MR_BLOCK:
		break;
	case MR_SHIP:
		data.player.health = 0.0;
		break;
	}
}

static void game_move_enemy(int index, int dx, int dy)
{
	int new_x = data.enemies[index].x + dx;
	int new_y = data.enemies[index].y + dy;

	switch (try_move(new_x, new_y)) {
	case MR_CLEAR:
		data.enemies[index].x = new_x;
		data.enemies[index].y = new_y;
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
	scan_result = generic_scan(data.player.x, data.player.y, x, y, visibility_scan);
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
		return false;
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
	int px = data.player.x;
	int py = data.player.y;
	int ex = data.enemies[index].x;
	int ey = data.enemies[index].y;
	
	scan_result = generic_scan(ex, ey, px, py, visibility_scan);

	if (scan_result == FAKE_PLAYER_INDEX) {
		// Atack and begin hunt.
		data.enemies[index].behavior = EB_HUNT;
		/* TODO: implement and uncomment: game_enemy_set_hunt_path(index); */
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
