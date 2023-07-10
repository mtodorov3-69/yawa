WIFI_SCAN = wifi_scan.o
EXAMPLES = wifi-scan-station wifi-scan-all
CC = gcc
CXX = g++
DEBUG =
CFLAGS = -O2 -g -Wall -c $(DEBUG)
CXX_FLAGS = -O2 -std=c++11 -Wall -c $(DEBUG)
LDLIBS = -lmnl -lncurses -lm

wifi_scan.o : wifi_scan.h wifi_scan.c
	$(CC) $(CFLAGS) wifi_scan.c

all : $(WIFI_SCAN) $(EXAMPLES)

examples: $(EXAMPLES)

wifi-scan-station : wifi_scan.o wifi_scan_station.o
	$(CC) wifi_scan.o wifi_scan_station.o $(LDLIBS) -o wifi-scan-station

wifi-scan-all : wifi_scan.o wifi_scan_all.o get_mac_table.o mvwnprintw.o
	$(CC) wifi_scan.o wifi_scan_all.o get_mac_table.o mvwnprintw.o -lstdc++ -o wifi-scan-all $(LDLIBS)

wifi_scan_station.o : wifi_scan.h examples/wifi_scan_station.c
	$(CC) $(CFLAGS) examples/wifi_scan_station.c

wifi_scan_all.o : wifi_scan.h wifi_chan.h my_ncurses.h examples/wifi_scan_all.cpp
	$(CC) $(CFLAGS) examples/wifi_scan_all.cpp

clean:
	\rm -f *.o examples/*.o $(WIFI_SCAN) $(EXAMPLES)
