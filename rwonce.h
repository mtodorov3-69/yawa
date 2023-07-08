
/*
 * mtodorov 2023-07-08 16:08 UTC+02
 *
 * rwonce.h - atomic operations for multithreaded environment
 *
 */

#ifndef __MY_RW_ONCE_H__
#define __MY_RW_ONCE_H__

#define READ_ONCE(X) __atomic_load_n(&(X), __ATOMIC_SEQ_CST)
#define WRITE_ONCE(X, VAL) __atomic_store_n(&(X), (VAL), __ATOMIC_SEQ_CST)
#define SET_ONCE(X) __atomic_test_and_set(&(X), __ATOMIC_SEQ_CST)
#define CLEAR_ONCE(X) __atomic_clear(&(X), __ATOMIC_SEQ_CST)
#define INCR_ONCE(X) __atomic_add_fetch(&(X), 1, __ATOMIC_SEQ_CST)
#define DECR_ONCE(X) __atomic_sub_fetch(&(X), 1, __ATOMIC_SEQ_CST)

#endif /* __MY_RW_ONCE_H__ */

