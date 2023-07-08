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
	
	/* versions of mvwprintw() and wprintw() that print at nost size characters
	   and truncate output */

	extern int mvwnprintw(WINDOW *win, int y, int x, size_t size, const char *fmt, ...);
	extern int wnprintw(WINDOW *win, size_t size, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

inline int getnrows(WINDOW *win)
{
        int rows, cols;
        getmaxyx(win, rows, cols);
        (void)cols;
        return rows;
}

inline int getncols(WINDOW *win)
{
        int rows, cols;
        getmaxyx(win, rows, cols);
        (void)rows;
        return cols;
}

#endif /* __MY_NCURSES_H */

