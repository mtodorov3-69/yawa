/*
 *
 * mtodorov 2023-06-15
 *
 */

#ifndef __GET_MAC_TABLE_H
#define __GET_MAC_TABLE_H

#define NO_BUCKETS (2 << 12)
#define SWAPXY(X,Y) ({ __typeof(X) tmp = (X); (X) = (Y); (Y) = tmp; })
#define ROTL32(X,D) ((X) << (D) | (X) >> (32 - (D)) & 0xffffffff)

#ifdef __cplusplus
extern "C" {
#endif

extern char *get_vendor_by_mac (const char *mac);
extern char *get_vendor_by_mac_hashtable (const char *mac);
extern char *get_vendor_by_mac_binary (const char *mac);
extern int vendor_initialise(const char *mac_vendor_list);

#ifdef __cplusplus
}
#endif

#endif /* __GET_MAC_TABLE_H */

