/*
 * hash.h - generic hash template, akin to queue.h & tree.h
 *
 * Copyright (c) 2005 Marius Eriksen <marius@monkey.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _HASH_H_		/* Possibly this is too generic. */
#define _HASH_H_

#include <sys/tree.h>

#define HASH_ENTRY SPLAY_ENTRY

#define HASH_HEAD(name, type, nbuckets) 			\
	SPLAY_HEAD(name##_HASH_TREE, type);			\
	struct name {						\
		struct name##_HASH_TREE buckets[nbuckets];	\
		unsigned int (*hashfn)(struct type *elm);	\
	};

#define HASH_NBUCKETS(head)					\
	(sizeof((head)->buckets)/sizeof((head)->buckets[0]))

#define HASH_INIT(head, fn) do {			\
	int i;						\
	for (i = 0; i < HASH_NBUCKETS(head); i++) {	\
		SPLAY_INIT(&(head)->buckets[i]);	\
	}						\
	(head)->hashfn = fn;				\
} while (0)

#define HASH_PROTOTYPE(name, type, field, cmp)				\
SPLAY_PROTOTYPE(name##_HASH_TREE, type, field, cmp)			\
struct type *name##_HASH_TREE_FIND(struct name *head, struct type *find); \
void name##_HASH_TREE_INSERT(struct name *head, struct type *insert);

#define HASH_GENERATE(name, type, field, cmp)				\
SPLAY_GENERATE(name##_HASH_TREE, type, field, cmp)			\
struct type *name##_HASH_TREE_FIND(struct name *head, struct type *find) \
{									\
	struct name##_HASH_TREE *bucket =				\
	    &head->buckets[(*head->hashfn)(find) % HASH_NBUCKETS(head)]; \
	return SPLAY_FIND(name##_HASH_TREE, bucket, find);		\
}									\
void name##_HASH_TREE_INSERT(struct name *head, struct type *insert)	\
{									\
	struct name##_HASH_TREE *bucket =				\
	    &head->buckets[(*head->hashfn)(insert) % HASH_NBUCKETS(head)]; \
									\
	SPLAY_INSERT(name##_HASH_TREE, bucket, insert);			\
}

#define HASH_FIND(name, head, find) name##_HASH_TREE_FIND((head), (find))
#define HASH_INSERT(name, head, insert) name##_HASH_TREE_INSERT((head), (insert))

#endif /* _HASH_H_ */
