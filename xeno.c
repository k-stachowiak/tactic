#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

/*
 * Xeno code (copied from the internetz).
 * ======================================
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

int XENO_rand_range(unsigned min, unsigned max)
{
	const int base_random = rand();
	const int range = max - min;
	const int remainder = RAND_MAX % range;
	const int bucket = RAND_MAX / range;

	if (RAND_MAX == base_random) {
		return XENO_rand_range(min, max);
	}

	if (base_random < RAND_MAX - remainder) {
		return min + base_random/bucket;
	} else {
		return XENO_rand_range(min, max);
	}
}

