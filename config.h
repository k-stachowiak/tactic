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


