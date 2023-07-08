#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
// #include <ncurses.h>
#include "my_ncurses.h"

#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))

int
mvwnprintw(WINDOW *win, int y, int x, size_t size, const char *fmt, ...)
{
	int n = 0;
	char *p = NULL;
	va_list ap;
	int ret;

	/* Determine required size. */

	va_start(ap, fmt);
	n = vsnprintf(p, 0, fmt, ap);
	va_end(ap);

	if (n < 0)
		return n;

	size = (size_t) MIN(size, n) + 1;      /* One extra byte for '\0' */
	p = malloc(size);
	if (p == NULL)
		return -1;

	va_start(ap, fmt);
	n = vsnprintf(p, size, fmt, ap);
	va_end(ap);

	if (n < 0) {
	       free(p);
	       return n;
	}

	// ret = printf("%s", p);
	ret = mvwprintw(win, y, x, "%s", p);

	free(p);

	return ret;
}

int
wnprintw(WINDOW *win, size_t size, const char *fmt, ...)
{
	int n = 0;
	char *p = NULL;
	va_list ap;
	int ret;

	/* Determine required size. */

	va_start(ap, fmt);
	n = vsnprintf(p, 0, fmt, ap);
	va_end(ap);

	if (n < 0)
		return n;

	size = (size_t) MIN(size, n) + 1;      /* One extra byte for '\0' */
	p = malloc(size);
	if (p == NULL)
		return -1;

	va_start(ap, fmt);
	n = vsnprintf(p, size, fmt, ap);
	va_end(ap);

	if (n < 0) {
	       free(p);
	       return n;
	}

	ret = wprintw(win, "%s", p);
	// ret = printf("%s", p);

	free(p);

	return ret;
}

#ifdef DEVELOP

int main (int argc, char *argv[])
{
	initscr();

	mvwnprintw(stdscr, 10, 10, 15, "123456789012345678901234567890");

	wmove(stdscr, 20, 50);
	wnprintw(stdscr, 10, "ABCDEFGHIJKLMNOPQRSTUVWXYZ");

	getch();
	endwin();
}

#endif /* DEVELOP */

