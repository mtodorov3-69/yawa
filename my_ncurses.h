/*
 * my_ncurses.h
 *
 * supplemenary functions to extend ncurses library
 *
 * mtodorov - 2023-06-17
 *
 */

#ifndef __MY_NCURSES_H
#define __MY_NCURSES_H

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ncurses.h>

#ifdef __cplusplus
extern "C" {
#endif

	extern int mvwnprintw(WINDOW *win, int y, int x, size_t size, const char *fmt, ...);
	extern int wnprintw(WINDOW *win, size_t size, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* __MY_NCURSES_H */

