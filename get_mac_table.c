/*
 *
 * mtodorov - 2023-06-14 Get MAC vendor table
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <values.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>

#define NO_BUCKETS (2 << 12)

struct mac_vendor {
	const char *mac;
	unsigned long long ulmac;
	const char *vendor;
} __aligned;

struct mac_vendor_listitem {
	struct mac_vendor_listitem *next;
	struct mac_vendor_listitem *prev;
	const char *mac;
	const char *vendor;
};

struct mac_vendor_list {
	struct mac_vendor_listitem *head, *tail;
	unsigned int n;
};

#define HW_MAC_STR_LEN 17
#define MAX_VENDORS_INIT 24576

#define EVENDORFORMAT  1024

#define SWAPXY(X,Y) ({ __typeof(X) tmp = (X); (X) = (Y); (Y) = tmp; })

#define ROTL32(X,D) ((X) << (D) | (X) >> (32 - (D)) & 0xffffffff)

size_t max_vendors = 0;
size_t n_vendors = 0;
size_t vend_increment = 1024;

static struct mac_vendor *vendorTable = NULL;
struct mac_vendor_list *hash_bucket = NULL;

static inline int strmatchlen(const char *s1, const char *s2)
{
	int n = 0;

	// printf("strmatchlen('%s','%s')\n", s1, s2);

	while (*s1 && *s2 && *s1++ == *s2++)
		n++;

	// printf("returning %d\n", n);

	return n;
}

__attribute((pure))
static inline char * strtoupper (const char *s1)
{
	char *rets = strdup(s1);
	char *p = rets;

	if (rets)
		while ((*(p++) = toupper(*(s1++))));

	return rets;
			
}

__attribute((pure))
static inline unsigned int nindex(const char *p, char c)
{
	return index(p, c) - p;
}

unsigned long long time_nanoseconds()
{
	struct timespec ts;

	if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
		perror("clock_gettime");
		exit(1);
	}

	return ts.tv_sec * 1000000000UL + ts.tv_nsec;
}

__attribute((pure))
unsigned int mac_crc12(const char *mac) {
	// unsigned int crc = 0;
	const char *hexdigits = "0123456789ABCDEF";
/*
	for (char *p = mac; *p && p - mac < HW_MAC_STR_LEN; ) {
		crc ^= ROTL32(crc, 2) ^ index("0123456789ABCDEF", *p++) << 4 + index("0123456789ABCDEF", *p++);
		if (*p == ':')
			p++;
	}

	return (crc >> 16 ^ crc) & 0xffff;
*/
	return (nindex (hexdigits, mac[0]) << 8 | nindex (hexdigits, mac[3]) << 4 | nindex (hexdigits, mac[6])) ^
	       (nindex (hexdigits, mac[4]) << 8 | nindex (hexdigits, mac[1]) << 4 | nindex (hexdigits, mac[7]));
}

__attribute((pure))
unsigned long long ulmac(const char *mac)
{
	const char *hexdigits = "0123456789ABCDEF";

	return
		nindex(hexdigits, mac[0]) << 20 |
		nindex(hexdigits, mac[1]) << 16 |
		nindex(hexdigits, mac[3]) << 12 |
		nindex(hexdigits, mac[4]) <<  8 |
		nindex(hexdigits, mac[6]) <<  4 |
		nindex(hexdigits, mac[7]);

}

__attribute((pure))
int mid_sq(const char *mac)
{
	unsigned crc12 = mac_crc12(mac);
	unsigned long sq = crc12 * crc12;

	return (sq >> 6) & 0x00000fff;
}

void print_list (struct mac_vendor_list *bucket_slot)
{
	struct mac_vendor_listitem *p;

	printf("<list>\n");
	for (p = bucket_slot->head; p; p = p->next) {
		printf("%s\n", p->mac);
	}
	printf("</list>\n");
}

void list_add_item (struct mac_vendor_list *const bucket_slot, unsigned index, const struct mac_vendor *mv)
{
	struct mac_vendor_listitem *p, *q, *pitem = (struct mac_vendor_listitem *) malloc (sizeof (struct mac_vendor_listitem));
	// int newlen = strlen(mv->mac);
	// printf("slot %06x: %d items: newlen=%d '%s' -> '%s'.\n", index, bucket_slot->n, newlen, mv->mac, mv->vendor);

	if (!pitem)
		printf("slot %06x: %d items: not allocated: '%s' -> '%s'.\n", index, bucket_slot->n, mv->mac, mv->vendor);

	pitem->mac    = mv->mac;
	pitem->vendor = mv->vendor;

	// printf("slot %06x: %d items: newlen=%d '%s' -> '%s'.\n", index, bucket_slot->n, newlen, mv->mac, mv->vendor);
	// print_list(bucket_slot);
	if (bucket_slot->head == NULL) {
		/* the empty hash slot */
		// printf("slot %06x: adding to empty head '%s' -> '%s'.\n", index, mv->mac, mv->vendor);
		pitem->next       = NULL;
		pitem->prev       = NULL;
		bucket_slot->head = pitem;
	} else {
		/* bucket_slot->head should be non-NULL ?! */
		// printf("bucket_slot->head=%0p\n", bucket_slot->head);
		// printf("bucket_slot->head->mac=%0p\n", bucket_slot->head->mac);
		// printf("strlen(bucket_slot->head->mac)=%d\n", strlen(bucket_slot->head->mac));
		// printf("slot %06x: checkpoint: if strlen=%d '%s': '%s' -> '%s'.\n", index, strlen(bucket_slot->head->mac), bucket_slot->head->mac, mv->mac, mv->vendor);
		// if (strlen(bucket_slot->head->mac) < newlen) {
		if (strcmp(bucket_slot->head->mac, mv->mac) > 0) {
			/* this new item should come first */
			// printf("slot %06x: adding before first.len=%d '%s' -> '%s'.\n", index, strlen(bucket_slot->head->mac), mv->mac, mv->vendor);
			pitem->next       = bucket_slot->head;
			pitem->prev       = NULL;
			bucket_slot->head->prev = pitem;
			bucket_slot->head = pitem;
		} else {
			// printf("slot %06x: checkpoint: start walk: '%s' -> '%s'.\n", index, mv->mac, mv->vendor);
			for (q = p = bucket_slot->head; p && strcmp(p->mac, mv->mac) < 0; q = p, p = p->next);
			// printf("slot %06x: before choice p mac '%s': '%s' -> '%s'.\n", index, p ? p->mac : "nulll", mv->mac, mv->vendor);
			if (p) { /* found the place to insert new item */
				// printf("slot %06x: inserting before p mac '%s': '%s' -> '%s'.\n", index, p->mac, mv->mac, mv->vendor);
				if (p->prev)
					p->prev->next = pitem;
				pitem->next = p;
				pitem->prev = p->prev;
				p->prev = pitem;
			} else { /* last item */
				// printf("slot %06x: inserting after mac '%s': '%s' -> '%s'.\n", index, q->mac, mv->mac, mv->vendor);
				q->next = pitem;
				pitem->prev = q;
				pitem->next = NULL;
			}
		}
	}
	// print_list(bucket_slot);
	// printf("slot %06x: %d items: end: '%s' -> '%s'.\n", bucket_slot->n, index, mv->mac, mv->vendor);

	++ (bucket_slot->n);
}

const char *list_get_item (struct mac_vendor_list *bucket_slot, const char *mac)
{
	struct mac_vendor_listitem *p, *pbest = NULL;
	int best_len = 0, new_len;

	if (!bucket_slot->head)
		return "Unknown-1";

	/* find the first match */

	for (p = bucket_slot->head; p; p = p->next) {
		// fprintf(stderr, "list_get_item: mac=%s p->mac=%p p->mac=%s p>vendor=%s\n", mac, p->mac, p->mac, p->vendor);
		// if (strncmp(p->mac, mac, strmatchlen(p->mac, mac) >= 8) == 0) {
		if (strncmp(p->mac, mac, 8) == 0) {
			// fprintf(stderr, "list_get_item: MATCH mac=%s p->mac==%s p->mac=%s p->vendor=%s\n", mac, p->mac, p->mac, p->vendor);
			pbest = p;
			best_len = strlen(p->mac);
			break;
		}
	}

	if (!pbest)
		return "Unknown";

	/* Adjust for longer length vendor prefixes */
	for (p = pbest; p; p = p->next) {
		// fprintf(stderr, "list_get_item: mac=%s p->mac=%p p->mac=%s p>vendor=%s\n", mac, p->mac, p->mac, p->vendor);
		if ((new_len = strmatchlen(p->mac, mac)) > best_len /* && strncmp(p->mac, mac, new_len) == 0 */) {
			// fprintf(stderr, "list_get_item: BETTER mac=%s p->mac=%s p->mac=%p p->vendor=%s\n", mac, p->mac, p->mac, p->vendor);
			pbest = p;
			best_len = new_len;
		} else if (new_len < best_len)
			break;
	}

	// fprintf(stderr, "list_get_item: BEST mac=%s p->mac=%p p->mac=%s p->vendor=%s\n", mac, pbest->mac, pbest->mac, pbest->vendor);
	return pbest->vendor;
}

const char *get_vendor_by_mac_hashtable (const char *mac_parm)
{
	// struct mac_vendor_listitem *p;
	char *mac = strtoupper(mac_parm);
	const char *rets;

	if (!mac)
		return NULL;

	rets = list_get_item(&hash_bucket[mac_crc12(mac)], mac);
	// rets = list_get_item(&hash_bucket[1], mac);
	free(mac);

	return rets;
}
	
void hash_populate(struct mac_vendor_list *hash_bucket, const struct mac_vendor *vendorTable)
{
	printf("entering hash_populate()\n");
	for (int i = 0; i < n_vendors; i++) {
		// printf("adding item mac='%s' vendor='%s'\n", vendorTable[i].mac, vendorTable[i].vendor);
		list_add_item(&hash_bucket[mac_crc12(vendorTable[i].mac)], mac_crc12(vendorTable[i].mac), &vendorTable[i]);
		// list_add_item(&hash_bucket[1], 1, &vendorTable[i]);
	}
	printf("leaving hash_populate()\n");
}
		// list_add_item(&hash_bucket[mac_crc12(vendorTable[i].mac)], mac_crc12(vendorTable[i].mac), &vendorTable[i]);

int vendor_entry_compare (const void *e1, const void *e2)
{
	printf( "Comparing '%s' and '%s'.\n", ((struct mac_vendor *)e1)->mac, ((struct mac_vendor *)e2)->mac);
	return strcmp(((struct mac_vendor *)e1)->mac, ((struct mac_vendor *)e2)->mac);
}

static inline int partition(struct mac_vendor * const table, const int low, const int high)
{
	int pivot = high;
	int i = low - 1;

	for (int j = low; j <= high - 1; j++) {
		if (strcmp(table[j].mac, table[pivot].mac) < 0) {
			i++;
			SWAPXY(table[i], table[j]);
		}
	}
	SWAPXY(table[i + 1], table[high]);
	return i + 1;
}

void quickSort(struct mac_vendor *const table, const int low, const int high)
{
	if (low < high) {
		int pi = partition(table, low, high);

		quickSort(table, low, pi - 1);
		quickSort(table, pi + 1, high);
	}
}

static inline int partition_ulmac(struct mac_vendor *table, int low, int high)
{
	int pivot = high;
	int i = low - 1;

	for (int j = low; j <= high - 1; j++) {
		if (table[j].ulmac < table[pivot].ulmac) {
			i++;
			SWAPXY(table[i], table[j]);
		}
	}
	SWAPXY(table[i + 1], table[high]);
	return i + 1;
}

void quickSort_ulmac(struct mac_vendor *table, int low, int high)
{
	if (low < high) {
		int pi = partition_ulmac(table, low, high);

		quickSort_ulmac(table, low, pi - 1);
		quickSort_ulmac(table, pi + 1, high);
	}
}

const char *get_vendor_by_mac (const char *mac_parm)
{
	int best_match = -1;
	int best_match_len;
	int current, match_len = 0;
	char *mac = strtoupper(mac_parm);

	if (!mac)
		return NULL;

	if (vendorTable == NULL) {
		fprintf(stderr, "Must call vendor_initialise() first!\n");
		exit(1);
	}

	for (int i = 0; i < n_vendors; i++) {
		// printf("%d of %d\r", i, n_vendors);
		// fflush(stdout);
		if (strncmp(vendorTable[i].mac, mac, strnlen(vendorTable[i].mac, HW_MAC_STR_LEN)) == 0) {
			best_match = i;
			break;
		}
	}

	// printf("1: mac = '%p' best match = %d vendor='%p'\n", mac, best_match, vendorTable[best_match].vendor);
	// printf("1: mac = '%s' best match = %d\n", mac, best_match);

	if (best_match == -1) {
		free(mac);
		return "Unknown";
	} else if (best_match != n_vendors - 1) {
		// printf("1: mac = '%s' best match = %d vendor='%s'\n", mac, best_match, vendorTable[best_match].vendor);

		// printf("1: mac = '%s' best match = %d vendor='%s'\n", mac, best_match, vendorTable[best_match].vendor);
		current = best_match;
		// printf("entering strmatchlen()\n");
		best_match_len = strmatchlen(vendorTable[current].mac, mac);
		// printf("exited strmatchlen()\n");
		do {
			// printf("1: current=%d match_len=%d best_match_len=%d\n", current, match_len, best_match_len);
			current ++;
			// printf("1: current=%d match_len=%d best_match_len=%d mac='%s' vendor='%s'\n", current, match_len, best_match_len, vendorTable[current].mac, vendorTable[current].vendor);
			match_len = strmatchlen(vendorTable[current].mac, mac);
			if (match_len > best_match_len) {
				best_match = current;
				best_match_len = match_len;
			}
			// printf("2: current=%d match_len=%d best_match_len=%d mac='%s' vendor='%s'\n", current, match_len, best_match_len, vendorTable[current].mac, vendorTable[current].vendor);
		} while (match_len >= best_match_len && current < n_vendors);

		// printf("returning 1 mac='%s' vndr='%s'\n", vendorTable[best_match].mac, vendorTable[best_match].vendor);
	}

	free(mac);
	return vendorTable[best_match].vendor;

}

const char *get_mac_by_vendor (char *vendor) {
	for (int i = 0; i < n_vendors; i++) {
		// printf("%d of %d\r", i, n_vendors);
		// fflush(stdout);
		if (strcmp(vendorTable[i].vendor, vendor) == 0) {
			return vendorTable[i].mac;
		}
	}
	return "Vendor not in the table";
}

const char *get_vendor_by_mac_binary (const char *mac)
{
	int low = 0;
	int high = n_vendors - 1;
	int i = 0;
	int best_match = -1;
	int best_match_len;
	int current, match_len = 0;
	int last_i = 0;

	do {
		last_i = i;
		i = (low + high) / 2;
		if (last_i == i && i < high)
			i++;
		else if (last_i == i) {
			best_match = i;
			break;
		}
		// printf("%d of %ld low=%d high=%d\n", i, n_vendors, low, high);
		// printf("BINARY: mac='%s' vT[%d].mac='%s' vendor='%s'\n", mac, i, vendorTable[i].mac, vendorTable[i].vendor);
		if (strncmp(mac, vendorTable[i].mac, strnlen(vendorTable[i].mac, 8)) < 0)
			high = i;
		else if (strncmp(mac, vendorTable[i].mac, strnlen(vendorTable[i].mac, 8)) > 0)
			low = i;
		else if (strncmp(mac, vendorTable[i].mac, strnlen(vendorTable[i].mac, 8)) == 0) {
			// return vendorTable[i].vendor;
			best_match = i;
			break;
		}
	} while (1);

	// printf("2: best match = %d\n", i);

	if (best_match == -1)
		return "Unknown";

	while (strncmp(vendorTable[i].mac, mac, 8) == 0 && i > 0) {
		// printf("BT: mac='%s' vT[%d].mac='%s' vendor='%s'\n", mac, i, vendorTable[i].mac, vendorTable[i].vendor);
		i--;
	}

	best_match = i;

	current = best_match;
	match_len = strmatchlen(vendorTable[current].mac, mac);
	best_match_len = strmatchlen(vendorTable[current].mac, mac);

	while (match_len >= best_match_len && current < n_vendors - 1) {
		current ++;
		match_len = strmatchlen(vendorTable[current].mac, mac);
		// printf("SEEK: current=%d match_len=%d best_match_len=%d mac='%s' vendor='%s'\n", current, match_len, best_match_len, vendorTable[current].mac, vendorTable[current].vendor);
		if (match_len > best_match_len) {
			best_match = current;
			best_match_len = match_len;
		}
	}

	// printf("returning 2 %s\n", vendorTable[best_match].vendor);

	return vendorTable[best_match].vendor;
}

/* This version has disappointingly the same speed as get_vendor_by_mac_binary()
   with strcmp() and backtracing, but it does no backtracing, which would be unfriendly
   for some media i.e. magnetic tapes.

   Binary search on magnetic tapes seems awkward, but I didn't like backtracing anyway,
   because it was not elegant binary search. :-)

   Another blunder: it is slower (unsigned long longs than strcmp chars) and it
   didn't eliminate the need for backtracing.

*/

const char *get_vendor_by_mac_binary_ulmac (char *mac_parm)
{
	int low = 0;
	int high = n_vendors - 1;
	int i = 0;
	int best_match = -1;
	int best_match_len;
	int current, match_len = 0;
	char *mac = strtoupper(mac_parm);
	unsigned long long my_ulmac = ulmac(mac);

	do {
		i = (low + high) / 2;
		if (i == low) {
			if (strmatchlen(mac, vendorTable[i].mac) > strmatchlen(mac, vendorTable[i+1].mac))
				best_match = i;
			else
				best_match = i + 1;
			break;
		}
		// printf("%d of %ld low=%d high=%d\n", i, n_vendors, low, high);
		// printf("BINARY: mac='%s' vT[%d].mac='%s' vendor='%s'\n", mac, i, vendorTable[i].mac, vendorTable[i].vendor);
		if	(my_ulmac <  vendorTable[i].ulmac)
			high = i;
		else if (my_ulmac >  vendorTable[i].ulmac)
			low = i;
		else if (my_ulmac == vendorTable[i].ulmac) {
			// return vendorTable[i].vendor;
			best_match = i;
			break;
		}
	} while (1);

	// printf("2: best match = %d\n", i);

	if (best_match == -1)
		return "Unknown";

	// while ((my_ulmac & 0x00fff000) == (vendorTable[i].ulmac & 0x00fff000) && i > 0) {
	while (strncmp(vendorTable[i].mac, mac, 8) == 0 && i > 0) {
		// printf("BT: mac='%s' vT[%d].mac='%s' vendor='%s'\n", mac, i, vendorTable[i].mac, vendorTable[i].vendor);
		i--;
	}
	best_match = i;


	current = best_match;
	match_len = strmatchlen(vendorTable[current].mac, mac);
	best_match_len = strmatchlen(vendorTable[current].mac, mac);

	while (match_len >= best_match_len && current < n_vendors - 1) {
		current ++;
		match_len = strmatchlen(vendorTable[current].mac, mac);
		// printf("SEEK: current=%d match_len=%d best_match_len=%d mac='%s' vendor='%s'\n", current, match_len, best_match_len, vendorTable[current].mac, vendorTable[current].vendor);
		if (match_len > best_match_len) {
			best_match = current;
			best_match_len = match_len;
		}
	}

	// printf("returning 2 %s\n", vendorTable[best_match].vendor);

	return vendorTable[best_match].vendor;
}

void sort_vendor_table(void)
{
	for (int i = 0; i < n_vendors; i++)
		for (int j = i + 1; j < n_vendors; j ++)
			if (strcmp(vendorTable[i].mac, vendorTable[j].mac) < 0)
				SWAPXY(vendorTable[i], vendorTable[j]);
}

int vendor_initialise(const char *mac_vendor_list)
{
	FILE  *fvendor;
	size_t n = 200;
	char *line = (char *) malloc(n);
	int ret = 0;

	if ((fvendor = fopen(mac_vendor_list, "r")) == NULL) {
		perror("fopen");
		return -1;
	}

	max_vendors = MAX_VENDORS_INIT;

	vendorTable = (struct mac_vendor *) calloc (max_vendors, sizeof(struct mac_vendor));
	if (!vendorTable)
		return -ENOMEM;
	
	ret = getline(&line, &n, fvendor);

	while ((ret = getline(&line, &n, fvendor) != -1)) {
		char *pdelim1 = NULL, *pdelim2 = NULL;
		char *mac, *vendor;
		
		// The vendor file format is ^mac,vendor,true|false,format,date$
		//			  or ^mac,"vendor, Ltd.",true|false,format,date$

		// fprintf(stderr, "Line %5d: %s\n", n_vendors, line);

		if ((pdelim1 = strchr(line, ','))) {
			*pdelim1++ = '\0';
			mac = line;
			// fprintf(stderr, "mac='%s'\n", mac);
			if (*(pdelim1) == '"') {
				pdelim1++;
				if (!(pdelim2 = strchr(pdelim1, '"')) || !(*(pdelim2 + 1) == ','))
					goto error;
			} else if (!(pdelim2 = strchr(pdelim1, ',')))
				goto error;
			*pdelim2 = '\0';
			vendor = pdelim1;
			// fprintf(stderr, "vendor='%s'\n", vendor);
			if ((vendorTable[n_vendors].mac    = strdup(mac   )) == NULL ||
			    (vendorTable[n_vendors].vendor = strdup(vendor)) == NULL)
				return -ENOMEM;
			else {
				vendorTable[n_vendors].ulmac = ulmac(mac);
				if (n_vendors >= max_vendors) {
					max_vendors += vend_increment;
					vendorTable = (struct mac_vendor *) realloc (vendorTable, max_vendors * sizeof(struct mac_vendor));
				}
			}
		} else
			goto error;
// 		printf("vT[%ld].mac='%s', vT[%ld].vendor='%s'\n",
// 			n_vendors, vendorTable[n_vendors].mac,
// 			n_vendors, vendorTable[n_vendors].vendor);
		n_vendors ++;
	}

	if (errno)
		perror("getline");

	fclose(fvendor);

	printf("Sorting vendor table ... ");
	fflush(stdout);
	// sort_vendor_table();
	// qsort (vendorTable, sizeof(struct mac_vendor), n_vendors, vendor_entry_compare);
	quickSort(vendorTable, 0, n_vendors - 1);
	printf("done.\n");
	printf("Populating hash table ... ");
	fflush(stdout);
	hash_bucket = (struct mac_vendor_list *) calloc (NO_BUCKETS, sizeof(struct mac_vendor_list));
	printf("Allocated hash table ... ");
	fflush(stdout);
	hash_populate(hash_bucket, vendorTable);
	printf("done.\n");
	int min_n = MAXINT, max_n = 0, sum = 0;
	for (int i = 0; i < NO_BUCKETS; i++) {
		if (hash_bucket[i].n < min_n)
			min_n = hash_bucket[i].n;
		if (hash_bucket[i].n > max_n)
			max_n = hash_bucket[i].n;
		sum += hash_bucket[i].n;
	}
	printf("hash_table: min/bucket=%d max/bucket=%d avg/bucket=%d\n", min_n, max_n, sum/NO_BUCKETS);
//	exit(1);

	return n_vendors;

error:
	free (vendorTable);
	fclose(fvendor);
	fprintf(stderr, "MAC: format error in line %ld\n", n_vendors + 1);
	return -EVENDORFORMAT;

}

#ifdef DEVELOP_MAC_TABLE

int main (int argc, char *argv[])
{
	char *mac_vendors_file = "mac-vendors-export.csv";
	size_t n = 0;
	int    err = 0;

	if ((n = vendor_initialise(mac_vendors_file)) <= 0)
		fprintf(stderr, "%s: Problem processing mac vendors list.\n", mac_vendors_file);

	printf ("Having read %ld entries.\n", n);
/*
	for (int i = 0; i < n; i++)
		printf("%-20s %s\n", vendorTable[i].mac, vendorTable[i].vendor);
*/

	unsigned long long ts1, te1, ts2, te2;

	char long_mac[18] = "FF:FF:FF:FF:FF:FF";
	char *mac = (char *) calloc(1, 18);

#define COMMENTED_CODE
#ifdef COMMENTED_CODE
	printf("Starting get_vendor_by_mac() test ... ");
	fflush(stdout);

	ts1 = time_nanoseconds();

	for (int i = 0; i < n; i++) {
		char *mac = vendorTable[i].mac;
		char *vendor = get_vendor_by_mac(mac);

		// `printf("%d of %ld: mac='%s' vendor='%s'\r", i, n, mac, vendor = hashtable_get_vendor_by_mac(mac));
		// fflush(stdout);
		if (strcmp(vendor, vendorTable[i].vendor) != 0) {
			fprintf(stderr, "%d: '%s' -> '%s' != '%s' -> '%s'!\n", i,
								get_mac_by_vendor(vendor),
								vendor,
								vendorTable[i].mac,
								vendorTable[i].vendor);
			err ++;
		}
	}

	printf ("done.\nHaving found %d errors.\n", err);
	printf ("Duration = %lld\n", time_nanoseconds() - ts1);

	printf("Starting get_vendor_by_mac() full mac test ... ");
	fflush(stdout);

	err = 0;
	ts2 = time_nanoseconds();

	for (int i = 0; i < n; i++) {
		
		strcpy(mac, long_mac);
		strcpy(mac, vendorTable[i].mac);

		char *vendor = get_vendor_by_mac(mac);
		// `printf("%d of %ld: mac='%s' vendor='%s'\r", i, n, mac, vendor = hashtable_get_vendor_by_mac(mac));
		// fflush(stdout);
		if (strcmp(vendor, vendorTable[i].vendor) != 0) {
			fprintf(stderr, "%d: '%s' -> '%s' != '%s' -> '%s'!\n", i,
								get_mac_by_vendor(vendor),
								vendor,
								vendorTable[i].mac,
								vendorTable[i].vendor);
			err ++;
		}
/*
		if (strcmp (get_vendor_by_mac(mac), get_vendor_by_mac_binary(mac)) != 0) {
			err ++;
			printf("Error: ind=%d mac='%s' vendor1='%s' vendor2='%s'\n", i, mac,
					get_vendor_by_mac(mac), get_vendor_by_mac_binary(mac));
		}
*/
	}

	printf ("Having found %d errors.\n", err);
	printf ("Duration = %lld\n", time_nanoseconds() - ts2);

#endif /* COMMENTED CODE */

	printf("Starting get_vendor_by_mac_hashtable() test ... ");
	fflush(stdout);

	err = 0;
	ts2 = time_nanoseconds();

	for (int i = 0; i < n; i++) {
		char *mac = vendorTable[i].mac;
		char *vendor = get_vendor_by_mac_hashtable(mac);
		

		// `printf("%d of %ld: mac='%s' vendor='%s'\r", i, n, mac, vendor = hashtable_get_vendor_by_mac(mac));
		// fflush(stdout);
		if (strcmp(vendor, vendorTable[i].vendor) != 0) {
			fprintf(stderr, "%d: '%s' -> '%s' != '%s' -> '%s'!\n", i,
								get_mac_by_vendor(vendor),
								vendor,
								vendorTable[i].mac,
								vendorTable[i].vendor);
			err ++;
		}
/*
		if (strcmp (get_vendor_by_mac(mac), get_vendor_by_mac_binary(mac)) != 0) {
			err ++;
			printf("Error: ind=%d mac='%s' vendor1='%s' vendor2='%s'\n", i, mac,
					get_vendor_by_mac(mac), get_vendor_by_mac_binary(mac));
		}
*/
	}

	printf ("done.\nHaving found %d errors.\n", err);
	printf ("Duration = %lld\n", time_nanoseconds() - ts2);

	printf("Starting get_vendor_by_mac_hashtable() full mac test ... ");
	fflush(stdout);

	err = 0;
	ts2 = time_nanoseconds();

	for (int i = 0; i < n; i++) {
		
		strcpy(mac, long_mac);
		strcpy(mac, vendorTable[i].mac);

		char *vendor = get_vendor_by_mac_hashtable(mac);
		// `printf("%d of %ld: mac='%s' vendor='%s'\r", i, n, mac, vendor = hashtable_get_vendor_by_mac(mac));
		// fflush(stdout);
		if (strcmp(vendor, vendorTable[i].vendor) != 0) {
			fprintf(stderr, "%d: '%s' -> '%s' != '%s' -> '%s'!\n", i,
								get_mac_by_vendor(vendor),
								vendor,
								vendorTable[i].mac,
								vendorTable[i].vendor);
			err ++;
		}
/*
		if (strcmp (get_vendor_by_mac(mac), get_vendor_by_mac_binary(mac)) != 0) {
			err ++;
			printf("Error: ind=%d mac='%s' vendor1='%s' vendor2='%s'\n", i, mac,
					get_vendor_by_mac(mac), get_vendor_by_mac_binary(mac));
		}
*/
	}

	printf ("done.\nHaving found %d errors.\n", err);
	printf ("Duration = %lld\n", time_nanoseconds() - ts2);

	printf("Starting get_vendor_by_mac_binary() test ... ");
	fflush(stdout);

	err = 0;
	ts2 = time_nanoseconds();

	for (int i = 0; i < n; i++) {
		char *mac = vendorTable[i].mac;
		char *vendor = get_vendor_by_mac_binary(mac);
		

		// `printf("%d of %ld: mac='%s' vendor='%s'\r", i, n, mac, vendor = hashtable_get_vendor_by_mac(mac));
		// fflush(stdout);
		if (strcmp(vendor, vendorTable[i].vendor) != 0) {
			fprintf(stderr, "%d: '%s' -> '%s' != '%s' -> '%s'!\n", i,
								get_mac_by_vendor(vendor),
								vendor,
								vendorTable[i].mac,
								vendorTable[i].vendor);
			err ++;
		}
/*
		if (strcmp (get_vendor_by_mac(mac), get_vendor_by_mac_binary(mac)) != 0) {
			err ++;
			printf("Error: ind=%d mac='%s' vendor1='%s' vendor2='%s'\n", i, mac,
					get_vendor_by_mac(mac), get_vendor_by_mac_binary(mac));
		}
*/
	}

	printf ("done.\nHaving found %d errors.\n", err);
	printf ("Duration = %lld\n", time_nanoseconds() - ts2);

	printf("Starting get_vendor_by_mac_binary() full mac test ... ");
	fflush(stdout);

	err = 0;
	ts2 = time_nanoseconds();

	for (int i = 0; i < n; i++) {
		
		strcpy(mac, long_mac);
		strcpy(mac, vendorTable[i].mac);

		char *vendor = get_vendor_by_mac_binary(mac);
		// `printf("%d of %ld: mac='%s' vendor='%s'\r", i, n, mac, vendor = hashtable_get_vendor_by_mac(mac));
		// fflush(stdout);
		if (strcmp(vendor, vendorTable[i].vendor) != 0) {
			fprintf(stderr, "%d: '%s' -> '%s' != '%s' -> '%s'!\n", i,
								get_mac_by_vendor(vendor),
								vendor,
								vendorTable[i].mac,
								vendorTable[i].vendor);
			err ++;
		}
/*
		if (strcmp (get_vendor_by_mac(mac), get_vendor_by_mac_binary(mac)) != 0) {
			err ++;
			printf("Error: ind=%d mac='%s' vendor1='%s' vendor2='%s'\n", i, mac,
					get_vendor_by_mac(mac), get_vendor_by_mac_binary(mac));
		}
*/
	}

	printf ("done.\nHaving found %d errors.\n", err);
	printf ("Duration = %lld\n", time_nanoseconds() - ts2);

/*
	printf("Sorting vendor table ... ");
	fflush(stdout);
	// sort_vendor_table();
	// qsort (vendorTable, sizeof(struct mac_vendor), n_vendors, vendor_entry_compare);
	quickSort_ulmac(vendorTable, 0, n_vendors - 1);
	printf("done.\n");
*/

	printf("Starting get_vendor_by_mac_binary_ulmac() test ... ");
	fflush(stdout);

	err = 0;
	ts2 = time_nanoseconds();

	for (int i = 0; i < n; i++) {
		char *mac = vendorTable[i].mac;
		char *vendor = get_vendor_by_mac_binary_ulmac(mac);

		// `printf("%d of %ld: mac='%s' vendor='%s'\r", i, n, mac, vendor = hashtable_get_vendor_by_mac(mac));
		// fflush(stdout);
		if (strcmp(vendor, vendorTable[i].vendor) != 0) {
			fprintf(stderr, "%d: '%s' -> '%s' != '%s' -> '%s'!\n", i,
								get_mac_by_vendor(vendor),
								vendor,
								vendorTable[i].mac,
								vendorTable[i].vendor);
			err ++;
		}
/*
		if (strcmp (get_vendor_by_mac(mac), get_vendor_by_mac_binary(mac)) != 0) {
			err ++;
			printf("Error: ind=%d mac='%s' vendor1='%s' vendor2='%s'\n", i, mac,
					get_vendor_by_mac(mac), get_vendor_by_mac_binary(mac));
		}
*/
	}

	printf ("done.\nHaving found %d errors.\n", err);
	printf ("Duration = %lld\n", time_nanoseconds() - ts2);

	printf("Starting get_vendor_by_mac_binary_ulmac() full mac test ... ");
	fflush(stdout);

	err = 0;
	ts2 = time_nanoseconds();

	for (int i = 0; i < n; i++) {
		
		strcpy(mac, long_mac);
		strcpy(mac, vendorTable[i].mac);

		char *vendor = get_vendor_by_mac_binary_ulmac(mac);
		// `printf("%d of %ld: mac='%s' vendor='%s'\r", i, n, mac, vendor = hashtable_get_vendor_by_mac(mac));
		// fflush(stdout);
		if (strcmp(vendor, vendorTable[i].vendor) != 0) {
			fprintf(stderr, "%d: '%s' -> '%s' != '%s' -> '%s'!\n", i,
								get_mac_by_vendor(vendor),
								vendor,
								vendorTable[i].mac,
								vendorTable[i].vendor);
			err ++;
		}
/*
		if (strcmp (get_vendor_by_mac(mac), get_vendor_by_mac_binary(mac)) != 0) {
			err ++;
			printf("Error: ind=%d mac='%s' vendor1='%s' vendor2='%s'\n", i, mac,
					get_vendor_by_mac(mac), get_vendor_by_mac_binary(mac));
		}
*/
	}

	printf ("done.\nHaving found %d errors.\n", err);
	printf ("Duration = %lld\n", time_nanoseconds() - ts2);

}

#endif /* DEVELOP_MAC_TABLE */

