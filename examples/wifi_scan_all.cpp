/*
 * wifi-scan-all example for wifi-scan library
 *
 * Copyright (C) 2016 Bartosz Meglicki <meglickib@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/*
 *  This example makes passive scan of all the networks on interface.
 *  Passive scanning may take a while and your link may be unusable for other programs during the scan.
 *  For the above reason you should not call it too often (e.g. once a second) unless you don't mind side effects.
 * 
 *  Triggering a scan needs persmission. The program will fail with operation not supported error message if you don't have them.
 *  You can use sudo for testing.
 * 
 *  Program expects wireless interface as argument, e.g:
 *  wifi-scan-all wlan0
 * 
 */

/* 
 * Copyright (C) 2023 Mirsad Todorovac <mtodorov3_69@yahoo.com>
 *
 * Added ncurses interface.
 *
 * Added pthreads to avoid blocking and delays in processing SIGWINCH

 * 2023-07-08 0.07.00 moved to GitHub
 *	              moved getncols(), getnrows() to my_ncurses.h

 * 2023-06-17
 *	      0.05.00 convert windows to c++ objects
 *	      0.04.02 decrease startline in wifiarea when enlarging window
 *	      0.04.01 sort only when changed wifi data or the sorting order
 * 	      0.04.00 added scrolling of the wifi area window

 * DISCLAIMER
 *
 * Uses portions of code (aio function) from top package procps.
 *
 */

#include <stdio.h>  //printf
#include <stdlib.h>  //printf
#include <string.h>  //printf
#include <unistd.h> //sleep
#include <ncurses.h>
#include <signal.h>
#include <math.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include "../wifi_scan.h"
#include "../wifi_chan.h"
#include "../get_mac_table.h"
#include "../my_ncurses.h"
#include "../rwonce.h"

#define swapxy(X,Y) ({ typeof(X) tmp = (X); (X) = (Y); (Y) = tmp; })
#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))

#define INIT_BSS_INFOS 1024

#define WHITE_ON_BLACK 10

#define SCREEN_REFRESH_HZ 60
#define DOT_TICK_HZ 5
#define WIFI_BAR_LENGTH 70

WINDOW *wintext = NULL, *wingraph = NULL, *winwifiarea = NULL, *winrfbar = NULL;
static bool rotating_bar = true;
int stdscr_lines, stdscr_columns, graph_lines, graph_columns, text_lines, text_columns;
int  winstart = 0;
volatile int status = 0, i;
volatile int global_status = 0;

pthread_mutex_t bss_mutex = PTHREAD_MUTEX_INITIALIZER;

class smart_window {
	friend class screen_window;
protected:
	WINDOW *window;
	bool dirty;
	class smart_window *parent;
public:
	smart_window() { window = NULL; parent = NULL; }
	smart_window(WINDOW *w) { window = w; dirty = true; }
	virtual ~smart_window() { delwin(window);  window = NULL; }
	virtual void repaint(void) = 0;
	virtual void touch(void) { dirty = true; }
	void resize(void);
	WINDOW *getwindow(void) { return window; }
};

class screen_window : public smart_window {
	friend class text_window;
	friend class graph_window;
	friend class rfbar_window;
	class smart_window *child[3];
	int nchildren;
public:
	screen_window(WINDOW *w) { window = w; nchildren = 0; }
	virtual void repaint(void) {
		for (int i = 0; i < nchildren; i++)
			child[i]->repaint();
	}
	void add_child(class smart_window *w) { child[nchildren++] = w; w->parent = this; }
};

class text_window : public smart_window {
	friend class screen_window;
public:
	text_window(WINDOW *w) { window = w; dirty = true; }
	virtual void repaint(void);
	void wifiarea_update (WINDOW *winwifiarea);
	void wifiarea_repaint (WINDOW *winwifiarea);
};

class graph_window : public smart_window {
	friend class screen_window;
public:
	graph_window(WINDOW *w) { window = w; dirty = true; }
	virtual void repaint(void);
};

class rfbar_window : public smart_window {
	friend class screen_window;
public:
	rfbar_window(WINDOW *w) { window = w; dirty = true; }
	virtual void repaint(void);
};

int startline = 0;

class screen_window *wscreen = NULL;
class text_window  *wtext = NULL;
class graph_window *wgraph = NULL;
class rfbar_window *wrfbar = NULL;

int BSS_INFOS=INIT_BSS_INFOS; //the maximum amounts of APs (Access Points) we want to store
static sigset_t sigwinch_set;

/*
void smart_window::resize(void) {
	struct winsize ws;

	// get terminal size the safe way
	if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == -1)
		perror("ioctl");

	resizeterm(ws.ws_row, ws.ws_col);
	getmaxyx(stdscr, stdscr_lines, stdscr_columns);
	// if (winwifiarea)
	// 	delwin(winwifiarea);
	// delete wrfbar;
	// delete wtext;
	winstart = stdscr_lines - WIFI_NCHAN - 4;
	if (winstart < 0)
		winstart = 0;
	if (winstart > 5) {
		wresize(wintext, winstart, stdscr_columns);
		wresize(winwifiarea, winstart > 4 ? winstart - 4 : 0, stdscr_columns - 4);
	} else {
		if (wintext)
			delwin(wintext);
		if (winwifiarea)
			delwin(winwifiarea);
		wintext     = newwin(winstart, stdscr_columns, 0, 0);
		winwifiarea = derwin(wintext, winstart - 4 ? winstart - 4 : 0, stdscr_columns - 4, 3, 2);
		// mvwin(winwifiarea, 3, 2);
	}
	if (winrfbar)
		delwin(winrfbar);
	winrfbar = derwin(wintext, 1, (stdscr_columns > WIFI_BAR_LENGTH ? WIFI_BAR_LENGTH : stdscr_columns) - 4, winstart - 1, 3);
	// wresize(winrfbar, 1, (stdscr_columns > WIFI_BAR_LENGTH ? WIFI_BAR_LENGTH : stdscr_columns) - 4);
	int nrwifi = getnrows(winwifiarea);
	if (status < nrwifi || nrwifi == 0)
		startline = 0;
	else if (startline > status - nrwifi)
		startline = status - nrwifi;
 	// wborder(wintext, 0, 0, 0, 0, 0, 0, 0, 0);
	delete wgraph;
	wingraph = newwin(stdscr_lines - winstart > 0 ? stdscr_lines - winstart : 0, stdscr_columns, winstart, 0);
	wborder(wingraph, 0, 0, 0, 0, 0, 0, 0, 0);
	wborder(wintext, 0, 0, 0, 0, 0, 0, 0, 0);

	wscreen = new screen_window(stdscr);
	wtext   = new text_window (wintext);
	wgraph  = new graph_window(wingraph);
	wscreen->add_child(wtext);
	wscreen->add_child(wgraph);
	wrfbar  = new rfbar_window(winrfbar);
}
*/

/*
void smart_window::resize(void)
{
	struct winsize ws;

	// get terminal size the safe way
	if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == -1)
		perror("ioctl");

	resizeterm(ws.ws_row, ws.ws_col);
	getmaxyx(stdscr, stdscr_lines, stdscr_columns);
	if (winwifiarea)
		delwin(winwifiarea);
	if (winrfbar)
		delwin(winrfbar);
	if (wintext)
		delwin(wintext);
	winstart = stdscr_lines - WIFI_NCHAN - 4;
	if (winstart < 0)
		winstart = 0;
	wintext = newwin(winstart, stdscr_columns, 0, 0);
	winwifiarea = derwin(wintext, winstart - 4, stdscr_columns - 4, 3, 2);
	winrfbar = derwin(wintext, 1, (stdscr_columns > WIFI_BAR_LENGTH ? WIFI_BAR_LENGTH : stdscr_columns) - 4, winstart - 1, 3);
	int nrwifi = getnrows(winwifiarea);
	if (READ_ONCE(global_status) < nrwifi)
		startline = 0;
	else if (startline > status - nrwifi)
		startline = status - nrwifi;
 	wborder(wintext, 0, 0, 0, 0, 0, 0, 0, 0);
	if (wingraph)
		delwin(wingraph);
	wingraph = newwin(stdscr_lines - winstart > 0 ? stdscr_lines - winstart : 0, stdscr_columns, winstart, 0);
	wborder(wingraph, 0, 0, 0, 0, 0, 0, 0, 0);
	wborder(wintext, 0, 0, 0, 0, 0, 0, 0, 0);

	wscreen = new screen_window(stdscr);
	wtext   = new text_window (wintext);
	wgraph  = new graph_window(wingraph);
	wscreen->add_child(wtext);
	wscreen->add_child(wgraph);
	wrfbar  = new rfbar_window(winrfbar);
}
*/

void smart_window::resize(void) {
	struct winsize ws;

	// get terminal size the safe way
	if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == -1)
		perror("ioctl");

	resizeterm(ws.ws_row, ws.ws_col);
	getmaxyx(stdscr, stdscr_lines, stdscr_columns);
	if (winwifiarea)
		delwin(winwifiarea);
	delete wrfbar;
	delete wtext;
	winstart = stdscr_lines - WIFI_NCHAN - 4;
	if (winstart < 0)
		winstart = 0;
	wintext = newwin(winstart, stdscr_columns, 0, 0);
	winwifiarea = derwin(wintext, winstart - 4, stdscr_columns - 4, 3, 2);
	winrfbar = subpad(wintext, 1, (stdscr_columns > WIFI_BAR_LENGTH ? WIFI_BAR_LENGTH : stdscr_columns) - 4, winstart - 1, 3);
	int nrwifi = getnrows(winwifiarea);
	if (status < nrwifi)
		startline = 0;
	else if (startline > status - nrwifi)
		startline = status - nrwifi;
 	wborder(wintext, 0, 0, 0, 0, 0, 0, 0, 0);
	delete wgraph;
	wingraph = newwin(stdscr_lines - winstart > 0 ? stdscr_lines - winstart : 0, stdscr_columns, winstart, 0);
	wborder(wingraph, 0, 0, 0, 0, 0, 0, 0, 0);
	wborder(wintext, 0, 0, 0, 0, 0, 0, 0, 0);

	wscreen = new screen_window(stdscr);
	wtext   = new text_window (wintext);
	wgraph  = new graph_window(wingraph);
	wscreen->add_child(wtext);
	wscreen->add_child(wgraph);
	wrfbar  = new rfbar_window(winrfbar);
}

int sort_key = 'c';
bool ascending = true;

void Usage(char **argv);

struct wifi_scan *wifi=NULL;    //this stores all the library information
struct bss_info  *bss = NULL; //this is where we are going to keep informatoin about APs (Access Points)
struct bss_info  *bss_new = NULL;
char mac[BSSID_STRING_LENGTH];  //a placeholder where we convert BSSID to printable hardware mac address
char mac2[BSSID_STRING_LENGTH];  //a placeholder where we convert BSSID to printable hardware mac address

volatile bool RF_scanning = false;
volatile bool RF_scan_progress = false;
volatile int scanner_dots = 0;
bool initial_screen = true;
bool color_mode = false;
bool resized = false;


void reinitialise_windows()
{
	struct winsize ws;

	/* get terminal size the safe way */
	if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == -1)
		perror("ioctl");

	resizeterm(ws.ws_row, ws.ws_col);
	getmaxyx(stdscr, stdscr_lines, stdscr_columns);
	if (winwifiarea)
		delwin(winwifiarea);
	if (winrfbar)
		delwin(winrfbar);
	if (wintext)
		delwin(wintext);
	winstart = stdscr_lines - WIFI_NCHAN - 4;
	if (winstart < 0)
		winstart = 0;
	wintext = newwin(winstart, stdscr_columns, 0, 0);
	winwifiarea = derwin(wintext, winstart - 4, stdscr_columns - 4, 3, 2);
	winrfbar = derwin(wintext, 1, (stdscr_columns > WIFI_BAR_LENGTH ? WIFI_BAR_LENGTH : stdscr_columns) - 4, winstart - 1, 3);
	int nrwifi = getnrows(winwifiarea);
	if (READ_ONCE(global_status) < nrwifi)
		startline = 0;
	else if (startline > status - nrwifi)
		startline = status - nrwifi;
 	wborder(wintext, 0, 0, 0, 0, 0, 0, 0, 0);
	if (wingraph)
		delwin(wingraph);
	wingraph = newwin(stdscr_lines - winstart > 0 ? stdscr_lines - winstart : 0, stdscr_columns, winstart, 0);
	wborder(wingraph, 0, 0, 0, 0, 0, 0, 0, 0);
	wborder(wintext, 0, 0, 0, 0, 0, 0, 0, 0);
}

void resizeHandler(int sig)
{
	SET_ONCE(resized);
	signal(SIGWINCH, resizeHandler);
}

#define MAX_PER_CHAN 16

int wifis_per_chan[WIFI_NCHAN + 1];
int power_per_chan[WIFI_NCHAN + 1][MAX_PER_CHAN];
int index_per_chan[WIFI_NCHAN + 1][MAX_PER_CHAN];
double congestion_per_chan[WIFI_NCHAN + 1];

//convert bssid to printable hardware mac address
char *bssid_to_string(const uint8_t bssid[BSSID_LENGTH], char bssid_string[BSSID_STRING_LENGTH])
{
	snprintf(bssid_string, BSSID_STRING_LENGTH, "%02x:%02x:%02x:%02x:%02x:%02x",
         bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
	return bssid_string;
}


void initialise()
{
	//this is where we are going to keep informatoin about APs (Access Points)
	bss     = (struct bss_info*) calloc (sizeof (struct bss_info), 2 * BSS_INFOS);
	bss_new = (struct bss_info*) calloc (sizeof (struct bss_info), BSS_INFOS);

	sigemptyset(&sigwinch_set);
	sigaddset(&sigwinch_set, SIGWINCH);
}

// NOTE: This portion of code was included from top.c in the procps package
        /*
         * An 'I/O available' routine which will detect raw single byte |
         * unsolicited keyboard input which was susceptible to SIGWINCH |
         * interrupts (or any other signal).  He'll also support timout |
         * in the absence of any user keystrokes or a signal interrupt. | */
static inline int ioa (struct timespec *ts) {
   fd_set fs;
   int rc;

   FD_ZERO(&fs);
   FD_SET(STDIN_FILENO, &fs);

#ifdef SIGNALS_LESS // conditional comments are silly, but help in documenting
   // hold here until we've got keyboard input, any signal except SIGWINCH
   // or (optionally) we timeout with nanosecond granularity
#else
   // hold here until we've got keyboard input, any signal (including SIGWINCH)
   // or (optionally) we timeout with nanosecond granularity
#endif
   rc = pselect(STDIN_FILENO + 1, &fs, NULL, NULL, ts, &sigwinch_set);

   if (rc < 0) rc = 0;
   return rc;
} // end: ioa

static inline int kbhit(void) {
	struct timespec ts = { 0, 1000000 };

	return ioa(&ts);
}

// NOTE END OF INCLUDED SECTION

void rfbar_window::repaint (void)
{
	const char *progress = "\\|/-";
	if (RF_scan_progress && READ_ONCE(RF_scanning)) {
		int prog = READ_ONCE(scanner_dots);
		if (!prog)
			return;
		int dots = prog / (SCREEN_REFRESH_HZ / DOT_TICK_HZ);
		wattron(window, COLOR_PAIR(WHITE_ON_BLACK));
		mvwprintw(window, 0, 0, " RF scanning ");
		mvwhline(window, 0, sizeof(" RF scanning ") - 1, '.', dots);
		if (rotating_bar)
			mvwaddch(window, 0, sizeof(" RF scanning ") - 1 + dots, progress[prog % 4]);
		mvwaddch(window, 0, sizeof(" RF scanning ") - 1 + dots + (rotating_bar ? 1 : 0), ' ');
		wrefresh(window);
	}
}

int oldstartline = startline;

void text_window::wifiarea_update (WINDOW *winwifiarea)
{
	int colourpair, wifipc, chan;
	// int nrwifi = getnrows(winwifiarea), ncwifi = getncols(winwifiarea);
	int nrwifi = getnrows(wtext->window) - 4, ncwifi = getncols(wtext->window);
	int repaint_end = MIN(READ_ONCE(global_status), BSS_INFOS);

	// getmaxyx(winwifiarea, nrwifi, ncwifi);

	for (int i = 0; i < repaint_end; i++) {
		bool flip = i - startline > nrwifi;

		if (flip)
			break;

		switch (wifipc = wifis_per_chan[chan = index_from_freq_mhz(bss[i].frequency) + 1]) {
		case 1:
			colourpair = 1; break;
		case 2:
			colourpair = 2; break;
		default:
			colourpair = 3; break;
		}
		wattron(winwifiarea, ( color_mode ? COLOR_PAIR(colourpair + 3) : 0 ) | A_BOLD);
		mvwnprintw(winwifiarea, i - (flip ? nrwifi - 4: 0) - startline, flip ? 100 : 0, ncwifi, "%2d %s %20.20s   %3d dBm   %u MHz      %3d   %5d ms ago %3d %2d %d  %s ",
		   i,
		   bssid_to_string(bss[i].bssid, mac), 
		   bss[i].ssid,  
		   bss[i].signal_mbm/100, 
		   bss[i].frequency,
		   channel_from_freq_mhz(bss[i].frequency),
		   bss[i].seen_ms_ago,
		   chan, wifipc, colourpair,
		   get_vendor_by_mac_hashtable(bssid_to_string(bss[i].bssid, mac))
		);
		if (bss[i].status == BSS_ASSOCIATED)
			waddch(winwifiarea, ACS_DIAMOND);
		waddch(winwifiarea, '\n');
		wattroff(winwifiarea, ( color_mode ? COLOR_PAIR(colourpair + 3) : 0 ) | A_BOLD);
	}
}

void text_window::wifiarea_repaint (WINDOW *winwifiarea)
{
	// int nrwifi = getnrows(winwifiarea), ncwifi = getncols(winwifiarea);
	int nrwifi = getnrows(wtext->window) - 4, ncwifi = getncols(wtext->window);

	if (nrwifi > 1)
		prefresh(winwifiarea, startline, 0, 0, 0, nrwifi - 1, ncwifi - 1);
}

void text_window::repaint(void)
{
	// int colourpair, wifipc, chan;
	int nr = getnrows(window), nc = getncols(window);
	// int nrwifi = getnrows(winwifiarea), ncwifi = getncols(winwifiarea);
	// int nrwifi, ncwifi;
	int nrwifi = getnrows(wtext->window) - 4, ncwifi = getncols(wtext->window);

	// getmaxyx(winwifiarea, nrwifi, ncwifi);


	// if (!dirty)
		// return;
	// fprintf(stderr, "window=%8p\n", window);

	//wifi_scan_all returns the number of found stations, it may be greater than BSS_INFOS that's why we test for both in the loop
	wclear(window);
	wnprintw(window, nc - 2, "\n  n APs=%d SK=%c.%c %dx%d (%dx%d)\n", READ_ONCE(global_status), (char)sort_key, ascending ? 'a' : 'd', nr, nc, nrwifi, ncwifi);
	wnprintw(window, nc - 2, "  %2s %17s %20.20s    %s  frequency  channel    seen ms ago   status  vendor\n",
				"N", "MAC", "SSID", "signal");
	wifiarea_update(winwifiarea);
	wifiarea_repaint(winwifiarea);
	wborder(window, 0, 0, 0, 0, 0, 0, 0, 0);
	wrefresh(window);
	wrfbar->repaint();

	dirty = false;
}

void graph_window::repaint(void)
{
	int colourpair;

	if (!dirty)
		return;

	wclear(window);
	wborder(window, 0, 0, 0, 0, 0, 0, 0, 0);
	mvwaddch(window, 2, 0, ACS_LTEE);
	mvwhline(window, 2, 1, ACS_HLINE, getncols(window) - 2);
	mvwaddch(window, 2, stdscr_columns - 1, ACS_RTEE);

	for (int j = 0; j <= 100; j += 10) {
		mvwvline(window, 3, 30 + j, ACS_VLINE, getnrows(window) - 4);
		mvwaddch(window, 2, 30 + j, ACS_TTEE);
		mvwaddch(window, getnrows(window) - 1, 30 + j, ACS_BTEE);
		mvwprintw(window, 1, 30 + j - 1 + (j == 100), "%d", -100 + j);
	}
	mvwprintw(window, 1, 1, "%4s=%2d,%2d %10.10s  %s %3s  %s", "CH", (int)WIFI_NCHAN, getnrows(window), "SSID", "N", "dBm", "Signal strength");
	mvwprintw(window, 1, 132, "%s", "Congestion");

	for (unsigned int line = 1; line <= WIFI_NCHAN; ++line) {
		int wline = line + 2; // offset for the header
		if (wline > getnrows(window) - 2)
			continue;
		if (wifis_per_chan[line] == 0) {
			wmove (window, wline, 1);
			wprintw(window, "%4d ", wifi_channel[line - 1].chan);
			// wrefresh(window);
			continue;
		}

		for (int j = 0; j < wifis_per_chan[line]; j ++) {
			for (int k = j + 1; k < wifis_per_chan[line]; k ++) {
				if (power_per_chan[line][j] < power_per_chan[line][k]) {
					swapxy(power_per_chan[line][j], power_per_chan[line][k]);
					swapxy(index_per_chan[line][j], index_per_chan[line][k]);
				}
			}
		}

		i = index_per_chan[line][0];
		wmove (window, wline, 1);
		wprintw(window, "%4d %16.16s %2d %3d", wifi_channel[line - 1].chan, bss[i].ssid,
				  wifis_per_chan[line], bss[i].signal_mbm/100);

		switch (wifis_per_chan[line]) {
		case 1:
			colourpair = 1; break;
		case 2:
			colourpair = 2; break;
		default:
			colourpair = 3; break;
		}

		for (int j = 1; j <= 100 + bss[i].signal_mbm/100; j ++) {
			mvwaddch(window, wline, j + 30, '*' | (color_mode ? COLOR_PAIR(colourpair) : 0 ) | A_BOLD);
		}

		if (bss[i].status == BSS_ASSOCIATED)
			mvwaddch(window, wline, 100 + 2 + bss[i].signal_mbm/100 + 30,
					   ACS_DIAMOND | (color_mode ? COLOR_PAIR(colourpair + 3) : 0 ) | A_BOLD);

		for (int j = 1; j < wifis_per_chan[line]; j++) {
			mvwaddch(window, wline, 30 + 100 + power_per_chan[line][j],
					   ACS_VLINE | (color_mode ? COLOR_PAIR(colourpair + 6) : 0));
		}

	}
	// color_mode = false;
	for (unsigned int line = 1; line <= WIFI_NCHAN; ++line) {
		int wline = line + 2; // offset for the header
		switch (wifis_per_chan[line]) {
		case 1:
			colourpair = 1; break;
		case 2:
			colourpair = 2; break;
		default:
			colourpair = 3; break;
		}

		for (int j = 1; j <= 100 + (100 + congestion_per_chan[line])/3 - 100; j ++) {
			mvwaddch(window, wline, j + 130, '!' | (color_mode ? COLOR_PAIR(colourpair) : 0 ) | A_BOLD);
		}
	}
	// color_mode = true;
	wrefresh(window);

	dirty = false;
}

volatile bool sorted = false;

void init_stats(void) {
	int status = READ_ONCE(global_status);

	for (unsigned i = 0; i <= WIFI_NCHAN; i ++) {
		wifis_per_chan[i] = 0;
		congestion_per_chan[i] = 0;
		for (int j = 0; j < MAX_PER_CHAN; j++) {
			power_per_chan[i][j] = 0;
			index_per_chan[i][j] = 0;
		}
	}

	for (i = 0; i < status && i < BSS_INFOS; ++i) {
		int line = 1 + index_from_freq_mhz(bss[i].frequency);
		index_per_chan[line][wifis_per_chan[line]] = i;
		power_per_chan[line][wifis_per_chan[line]] = bss[i].signal_mbm/100;
		++ wifis_per_chan[line];
		// index_per_chan[line][0] = i;
	}

	for (unsigned int line = 1; line <= WIFI_NCHAN; line ++) {
		for (i = 0; i < wifis_per_chan[line]; i ++)
			congestion_per_chan[line] += exp10(power_per_chan[line][i] / 10);
		congestion_per_chan[line] = 10 * log10(congestion_per_chan[line]);
	}

}

int perform_sorting() {
	int i, j;
	int status = READ_ONCE(global_status);

	if (READ_ONCE(sorted))
		return sort_key; // nothing to do

	for (i = 0; i < status; i++)
		for (j = i + 1; j < status; j ++)
			if      (sort_key == 'c' &&  ascending && (bss[i].frequency >  bss[j].frequency ||
								  (bss[i].frequency == bss[j].frequency && bss[i].signal_mbm < bss[j].signal_mbm)))
				swapxy(bss[i], bss[j]);
			else if (sort_key == 'c' && !ascending && (bss[i].frequency <  bss[j].frequency ||
								  (bss[i].frequency == bss[j].frequency && bss[i].signal_mbm < bss[j].signal_mbm)))
				swapxy(bss[i], bss[j]);
			else if (sort_key == 'm' &&  ascending && strncmp(bssid_to_string(bss[i].bssid, mac), bssid_to_string(bss[j].bssid, mac2), BSSID_STRING_LENGTH - 1) > 0)
				swapxy(bss[i], bss[j]);
			else if (sort_key == 'm' && !ascending && strncmp(bssid_to_string(bss[i].bssid, mac), bssid_to_string(bss[j].bssid, mac2), BSSID_STRING_LENGTH - 1) < 0)
				swapxy(bss[i], bss[j]);
			else if (sort_key == 'v' &&  ascending && strcmp(get_vendor_by_mac_hashtable(bssid_to_string(bss[i].bssid, mac)), get_vendor_by_mac_hashtable(bssid_to_string(bss[j].bssid, mac2))) > 0)
				swapxy(bss[i], bss[j]);
			else if (sort_key == 'v' && !ascending && strcmp(get_vendor_by_mac_hashtable(bssid_to_string(bss[i].bssid, mac)), get_vendor_by_mac_hashtable(bssid_to_string(bss[j].bssid, mac2))) < 0)
				swapxy(bss[i], bss[j]);
			else if (sort_key == 'i' &&  ascending && strcmp(bss[i].ssid, bss[j].ssid) > 0)
				swapxy(bss[i], bss[j]);
			else if (sort_key == 'i' && !ascending && strcmp(bss[i].ssid, bss[j].ssid) < 0)
				swapxy(bss[i], bss[j]);
			else if (sort_key == 's' &&  ascending && bss[i].signal_mbm < bss[j].signal_mbm)
				swapxy(bss[i], bss[j]);
			else if (sort_key == 's' && !ascending && bss[i].signal_mbm > bss[j].signal_mbm)
				swapxy(bss[i], bss[j]);

	init_stats();

	wtext->touch();
	wgraph->touch();
	// wifiarea_update(winwifiarea);
	SET_ONCE(sorted);

	return sort_key;
}

int merge_arrays (struct bss_info *bss_old, int n_old, struct bss_info *bss_new, int n_new)
{
	int last_old = n_old;
	bool already_present = false;

	for (int i = 0; i < n_new; i++) {
		already_present = false;
		for (int j = 0; j < n_old; j++) {
			if (bss_new[i].frequency == bss_old[j].frequency &&
			    memcmp (bss_new[i].bssid, bss_old[j].bssid, BSSID_LENGTH) == 0 &&
			    strncmp(bss_new[i].ssid,  bss_old[j].ssid,  SSID_MAX_LENGTH_WITH_NULL) == 0) {
				already_present = true;
				bss_old[j] = bss_new[i];
				break;
			}
		}
		if (!already_present)
			bss_old[last_old ++] = bss_new[i];
	}

	return last_old;
}

int process_keypress_event() {
	int c;
	int status = READ_ONCE(global_status);

	if ((c = getch()) != ERR) {
		if (c == sort_key && strchr("csmiv", c)) {
			ascending = !ascending;
			CLEAR_ONCE(sorted);
		} else if (c != sort_key && strchr("csmiv", c)) {
			ascending = true;
			sort_key = c;
			CLEAR_ONCE(sorted);
		} else {
			int nrwifi = getnrows(winwifiarea);

			switch (c) {
			case KEY_UP:   if (startline > 0) startline --; break;
			case KEY_DOWN: if (startline < status - nrwifi && status > nrwifi) startline ++; break;
			case KEY_HOME: startline = 0; break;
			case KEY_END:  startline = status >= nrwifi? status - nrwifi : 0; break;
			case KEY_PPAGE: startline -= nrwifi; if (startline < 0) startline = 0; break;
			case KEY_NPAGE: if (status >= nrwifi) {
						if (startline < status - 2 * nrwifi) 
							startline += nrwifi;
						else
							startline = status - nrwifi;
					}
				        break;
			case '+': ascending = true;  CLEAR_ONCE(sorted); break;
			case '-': ascending = false; CLEAR_ONCE(sorted); break;
			case 'R': rotating_bar = !rotating_bar; break;
			case 'r': RF_scan_progress = !RF_scan_progress; break;
			case 'q':
				wifi_scan_close(wifi);
				endwin();
				exit(0);
				break;
			case '0': sort_key = '0'; break;
			default:
				break;
			}
			wtext->touch();
			wgraph->touch();
		}
	}
	return c;
}

pthread_mutex_t first_scan_mutex;
volatile bool first_scan_passed = false;

void *wifi_scan_thread(void *arg)
{
	int last_status = 0;

	while (1)
	{
		int prev_status = READ_ONCE(global_status);

		SET_ONCE(RF_scanning);
		last_status = wifi_scan_all(wifi, bss_new, BSS_INFOS);
		if (!first_scan_passed)	{
			SET_ONCE(first_scan_passed);
			pthread_mutex_unlock(&first_scan_mutex);
		}
		if (last_status == -1) {
			// failed scan, nothing useful to do ATM
			goto out;
		} else if (last_status >= BSS_INFOS) {
			// extend the buffer as needed: we miss at most one scan's extra data
			int new_status = BSS_INFOS;
			BSS_INFOS = last_status + 32;
			bss_new = (struct bss_info*) realloc (bss_new, sizeof (struct bss_info) * BSS_INFOS);
			last_status = new_status; // read old data as still valid
		}

		pthread_mutex_lock(&bss_mutex);
		last_status = merge_arrays(bss, prev_status, bss_new, last_status);
		pthread_mutex_unlock(&bss_mutex);

		CLEAR_ONCE(sorted);
		WRITE_ONCE(global_status, last_status);
out:
		WRITE_ONCE(scanner_dots, 0);
		CLEAR_ONCE(RF_scanning);
		usleep(500000);
	}
}

pthread_t scan_threadID;

int main(int argc, char **argv)
{
	char *wifi_if = NULL;

	for (int i = 1; i < argc; i++) {
		if (strncmp(argv[i], "--dbm-descend", 11) == 0)
			sort_key = 's', ascending = false;
		else if (strncmp(argv[i], "--dbm", 5) == 0)
			sort_key = 's', ascending = true;
		else if (strncmp(argv[i], "--chan-descend", 13) == 0)
			sort_key = 'c', ascending = false;
		else if (strncmp(argv[i], "--chan", 6) == 0)
			sort_key = 'c', ascending = true;
		else if (strncmp(argv[i], "--mac-descend", 12) == 0)
			sort_key = 'm', ascending = false;
		else if (strncmp(argv[i], "--mac", 5) == 0)
			sort_key = 'm', ascending = true;
		else if (strncmp(argv[i], "--ssid-descend", 13) == 0)
			sort_key = 'i', ascending = false;
		else if (strncmp(argv[i], "--ssid", 6) == 0)
			sort_key = 'i', ascending = true;
		else if (strncmp(argv[i], "--rf-progress", 4) == 0)
			RF_scan_progress = true;
		else if (strncmp(argv[i], "--", 2) == 0) {
			Usage(argv);
			exit (1);
		} else
			wifi_if = argv[i];
	}

	if (!wifi_if) {
		Usage(argv);
		exit(1);
	}

	if (vendor_initialise("mac-vendors-export.csv") < 0)
		exit(1);
	initialise();
	initscr();
	cbreak();
	noecho();
	keypad(stdscr, TRUE);
	// getch();
	nodelay(stdscr, TRUE);
	nodelay(wintext, TRUE);
	nodelay(wingraph, TRUE);

	signal(SIGWINCH, resizeHandler);

	getmaxyx(stdscr, stdscr_lines, stdscr_columns);
	winstart = getnrows(stdscr) - WIFI_NCHAN - 4;
	wintext     = newwin(winstart, stdscr_columns, 0, 0);
	winwifiarea = derwin(wintext, winstart - 4, stdscr_columns - 4, 3, 2);
	// winwifiarea = derwin(wintext, winstart - 4, stdscr_columns - 4, 3, 2);
	winrfbar    = derwin(wintext, 1, (stdscr_columns > WIFI_BAR_LENGTH ? WIFI_BAR_LENGTH : stdscr_columns) - 4, winstart - 1, 3);
	wingraph    = newwin(stdscr_lines - winstart, stdscr_columns, winstart, 0);
	// wborder(wintext, 0, 0, 0, 0, 0, 0, 0, 0);
	// wrefresh(wintext);
	wborder(wingraph, 0, 0, 0, 0, 0, 0, 0, 0);
	wrefresh(wingraph);

	wscreen = new screen_window(stdscr);
	wtext   = new text_window (wintext);
	wgraph  = new graph_window(wingraph);
	wscreen->add_child(wtext);
	wscreen->add_child(wgraph);
	wrfbar  = new rfbar_window(winrfbar);

	if (has_colors() && start_color() == OK) {
		init_pair(1, COLOR_GREEN,  COLOR_GREEN);
		init_pair(2, COLOR_YELLOW, COLOR_YELLOW);
		init_pair(3, COLOR_RED,    COLOR_RED);
		init_pair(4, COLOR_GREEN,  COLOR_BLACK);
		init_pair(5, COLOR_YELLOW, COLOR_BLACK);
		init_pair(6, COLOR_RED,    COLOR_BLACK);
		init_pair(7, COLOR_BLACK,  COLOR_GREEN);
		init_pair(8, COLOR_BLACK,  COLOR_YELLOW);
		init_pair(9, COLOR_BLACK,  COLOR_RED);
		init_pair(WHITE_ON_BLACK, COLOR_WHITE, COLOR_BLACK);
		color_mode = true;
	}
	
	wprintw(wintext, "This is just example, this is library - not utility!\n\n");

	wprintw(wintext, "Triggering scan needs permissions.\n");
	wprintw(wintext, "The program will fail if you don't have them with message:\n");
	wprintw(wintext, "\"Operation not permitted\". The simplest way is to use sudo. \n\n");

	wprintw(wintext, "windows screen=%8p text=%8p graph=%8p rfbar=%8p\n", wscreen, wtext, wgraph, wrfbar); 

	wprintw(wintext, "### Close the program with ctrl+c when you're done ###\n\n");
	wprintw(wintext, "RF scanning .");

	wrefresh(wintext);

	// initialize the library with network interface argv[1] (e.g. wlan0)
	wifi = wifi_scan_init(wifi_if);

	pthread_mutex_lock(&first_scan_mutex);

	if (pthread_create(&scan_threadID, NULL, &wifi_scan_thread, NULL) == -1) {
		perror("pthread");
		exit(1);
	}

	while (!READ_ONCE(first_scan_passed)) {
		// pthread_mutex_lock(&first_scan_mutex);
		wprintw(wintext, ".");
		wrefresh(wintext);
		usleep(200000);
	}

	wprintw(wintext, " done.\n\nPress any key ... ");
	wrefresh(wintext);
	wmove(wintext, 0, 0);
	wclear(wintext);
	initial_screen = false;
	perform_sorting();
	// wifiarea_update(winwifiarea);
	wscreen->repaint();
	// wgraph->repaint();

	while(1)
	{

		if (READ_ONCE(resized)) {
			wscreen->resize();
			wscreen->repaint();
			CLEAR_ONCE(resized);
		}

		if (kbhit()) {
			process_keypress_event();
			perform_sorting();
			// wifiarea_update(winwifiarea);
			wscreen->repaint();
			// wgraph->repaint();
		}

		if (READ_ONCE(RF_scanning)) {
			do {
				if (RF_scan_progress) {
					INCR_ONCE(scanner_dots);
					wrfbar->repaint();
				}
				if (READ_ONCE(resized)) {
					wscreen->resize();
					wscreen->repaint();
					CLEAR_ONCE(resized);
				} else if (kbhit()) {
					process_keypress_event();
					perform_sorting();
					// wifiarea_update(winwifiarea);
					wscreen->repaint();
				}
				usleep(1000000UL/SCREEN_REFRESH_HZ);
			} while (READ_ONCE(RF_scanning));
			//it may happen that device is unreachable (e.g. the device works in such way that it doesn't respond while scanning)
			//you may test for errno==EBUSY here and make a retry after a while, this is how my hardware works for example
			// if (status < 0)
				// perror("Unable to get scan data");
			// else {
				perform_sorting();
				// wifiarea_update(winwifiarea);
				wscreen->repaint();
			// }
		} else
			usleep(200000);

	}
	
	//free the library resources
	wifi_scan_close(wifi);

	endwin();

	return 0;
}

void Usage(char **argv)
{
	printf("Usage:\n");
	printf("%s wireless_interface\n\n", argv[0]);
	printf("examples:\n");
	printf("%s wlan0\n", argv[0]);
	
}
