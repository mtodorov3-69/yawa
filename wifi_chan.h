/*
 * mtodorov 2023-06-11 14:20
 */

struct wifi_chan {
	int chan;
	int freq_mhz;
};

static struct wifi_chan wifi_channel [] = {
	{   1, 2412 },
	{   2, 2417 },
	{   3, 2422 },
	{   4, 2427 },
	{   5, 2432 },
	{   6, 2437 },
	{   7, 2442 },
	{   8, 2447 },
	{   9, 2452 },
	{  10, 2457 },
	{  11, 2462 },
	{  12, 2467 },
	{  13, 2472 },
	{  36, 5180 },
	{  40, 5200 },
	{  44, 5220 },
	{  48, 5240 },
	{  52, 5260 },
	{  56, 5280 },
	{  60, 5300 },
        {  64, 5320 },
        { 100, 5500 },
        { 104, 5520 },
        { 108, 5540 },
        { 112, 5560 },
        { 116, 5580 },
        { 120, 5600 },
        { 124, 5620 },
        { 128, 5640 },
        { 132, 5660 },
        { 136, 5680 },
        { 140, 5700 }
};

#define WIFI_NCHAN (sizeof(wifi_channel) / sizeof(struct wifi_chan))

static inline int channel_from_freq_mhz(int freq_mhz) {
        for (unsigned int i = 0; i < WIFI_NCHAN; i++)
	    if (freq_mhz == wifi_channel[i].freq_mhz)
		return wifi_channel[i].chan;
	return 0;
}

static inline int index_from_freq_mhz(int freq_mhz) {
        for (unsigned int i = 0; i < WIFI_NCHAN; i++)
	    if (freq_mhz == wifi_channel[i].freq_mhz)
		return i;

	return -1;
}

