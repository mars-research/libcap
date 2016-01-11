/* 
 * libcap primary header file and interface.
 *
 * Author: Charles Jacobsen <charlesj@cs.utah.edu>
 * Copyright: University of Utah
 *
 * This is the non-isolated code interface to the microkernel. The
 * implementation is in virt/lcd-domains/kliblcd.c.
 *
 * An LCD that runs in non-isolated code is called a klcd.
 */
#ifndef __LIBCAP_H__
#define __LIBCAP_H__

#include "libcap_types.h"

struct cnode;
struct cspace;

struct cap_type_ops {
	char *name;
	int (*delete)(struct cspace *cspace, struct cnode *cnode, void *object);
	int (*revoke)(struct cspace *cspace, struct cnode *cnode, void *object);
};

/*
 * Add some macros to generate built-in capability object type.  Not
 * ideal to put this here, but don't want to expose internal headers,
 * and have to give per-platform a chance to change them.
 */
#define CAP_BUILD_CORE_TYPES(PT)				\
	typedef enum cap_type {					\
		CAP_TYPE_ERR = -1,				\
		CAP_TYPE_NONE = 0,				\
		CAP_TYPE_INVALID,				\
		CAP_TYPE_FREE,					\
		CAP_TYPE_CNODE,					\
		PT,						\
		CAP_TYPE_FIRST_NONBUILTIN			\
	} cap_type_t
#define CAP_BUILD_CORE_TYPES_NOBUILTIN()			\
	typedef enum cap_type {					\
		CAP_TYPE_NONE = 0,				\
		CAP_TYPE_INVALID,				\
		CAP_TYPE_FREE,					\
		CAP_TYPE_CNODE,					\
		CAP_TYPE_FIRST_NONBUILTIN			\
	} cap_type_t

#ifdef __KERNEL__
#include "libcap_kernel.h"
#else
#include "libcap_user.h"
#endif

#ifndef CAP_TYPE_MAX
#define CAP_TYPE_MAX 256
#endif

#define CAP_BUG() __cap_bug()

/**
 * For now, put debug macros in the user-accessible part; convenient.
 */
extern int cap_debug_level;

#define CAP_ERR __cap_err
#define CAP_WARN __cap_warn
#define CAP_MSG __cap_msg

#define CAP_DEBUG_ERR  1
#define CAP_DEBUG_WARN 2
#define CAP_DEBUG_MSG  3

#define CAP_DEBUG(lvl, msg, ...) {					\
	if (lvl <= cap_debug_level)					\
	    __cap_debug(msg,## __VA_ARGS__);				\
	}

/* CPTRs -------------------------------------------------- */

#include "libcap_internal.h" /* temporary hack */


/**
 * __cptr -- Construct a cptr from an unsigned long
 * @cptr: the unsigned long to use
 *
 * This is a low-level function. You need to know how to pack
 * the bits into the unsigned long.
 */
static inline cptr_t __cptr(unsigned long cptr)
{
	return (cptr_t) {cptr};
}
/**
 * cptr_val -- Extract the unsigned long (bits) in the cptr
 * @c: the ctpr to extract
 *
 * This can be useful if you want to pass a cptr in a register,
 * as a scalar.
 */
static inline unsigned long cptr_val(cptr_t c)
{
	return c.cptr;
}
/**
 * cap_cptr_slot -- Returns the slot index into the final cnode table
 * @c: the cptr
 *
 * Once you have arrived at the correct cnode table in the cspace
 * radix tree, this is the index into that table to get the
 * capability that @c refers to.
 */
static inline unsigned long cap_cptr_slot(cptr_t c)
{
	/*
	 * Mask off low bits
	 */
	return cptr_val(c) & ((1 << (CAP_CSPACE_CNODE_TABLE_BITS - 1)) - 1);
}
/**
 * cap_cptr_fanout -- Gives fanout index for going *from* @lvl to @lvl + 1
 * @c: the cptr
 * @lvl: the level in the cspace radix tree, where 0 <= lvl < CAP_CSPACE_DEPTH
 *
 * Each node in the cspace radix tree is a cnode table. Each cnode
 * table is split in half: the first half are capability slots, and
 * the other half are pointers to further nodes in the tree. If a
 * cptr refers to a slot in a deeper level in the tree, you need to
 * follow these pointers. The fanout index tells you which pointers
 * to follow at each level.
 */
static inline unsigned long cap_cptr_fanout(cptr_t c, int lvl)
{
	unsigned long i;

	if (lvl >= CAP_CSPACE_DEPTH - 1)
		CAP_BUG();

	i = cptr_val(c);
	/*
	 * Shift and mask off bits at correct section
	 */
	i >>= ((lvl + 1) * (CAP_CSPACE_CNODE_TABLE_BITS - 1));
	i &= ((1 << (CAP_CSPACE_CNODE_TABLE_BITS - 1)) - 1);

	return i;
}
/**
 * cap_cptr_level -- The zero-indexed level in the cspace radix tree
 * @c: the cptr
 *
 * Returns the level of the slot which @c refers to. 0 means the root 
 * cnode table.
 */
static inline unsigned long cap_cptr_level(cptr_t c)
{
	unsigned long i;

	i = cptr_val(c);
	/*
	 * Shift and mask
	 */
	i >>= (CAP_CSPACE_DEPTH * (CAP_CSPACE_CNODE_TABLE_BITS - 1));
	i &= ((1 << CAP_CSPACE_DEPTH_BITS) - 1);

	return i;
}
/**
 * cap_cptr_set_level -- Sets the level for @c
 * @c: the cptr
 * @lvl: the level in the cspace radix tree, 0 <= lvl < CAP_CSPACE_DEPTH
 */
static inline void cap_cptr_set_level(cptr_t *c, int lvl)
{
	/* Shift and OR to store lvl */
	c->cptr |= (lvl << 
		(CAP_CSPACE_DEPTH * (CAP_CSPACE_CNODE_TABLE_BITS - 1)));
}
/**
 * cptr_is_null -- Returns non-zero if cptr is the special null cptr
 * @c: cptr to test
 */
static inline int cptr_is_null(cptr_t c)
{
	return cptr_val(c) == cptr_val(CAP_CPTR_NULL);
}

/* CPTR CACHEs -------------------------------------------------- */

#if (CAP_CSPACE_DEPTH == 4)

struct cptr_cache {
	/* lock */
	cap_mutex_t lock;
	/* level 0 bitmap */
	unsigned long bmap0[CAP_BITS_TO_LONGS(CAP_CSPACE_SLOTS_IN_LEVEL(0))];
	/* level 1 bitmap */
	unsigned long bmap1[CAP_BITS_TO_LONGS(CAP_CSPACE_SLOTS_IN_LEVEL(1))];
	/* level 2 bitmap */
	unsigned long bmap2[CAP_BITS_TO_LONGS(CAP_CSPACE_SLOTS_IN_LEVEL(2))];
	/* level 3 bitmap */
	unsigned long bmap3[CAP_BITS_TO_LONGS(CAP_CSPACE_SLOTS_IN_LEVEL(3))];
};

#else
#error "You need to adjust the cptr cache def."
#endif

/**
 * cptr_init -- Initalize the cptr cache subsystem
 */
int cptr_init(void);
/**
 * cptr_fini -- Tear down the cptr cache subsystem.
 */
void cptr_fini(void);
/**
 * cptr_cache_alloc -- Allocate a cptr cache data structure (not initialized)
 * @out: out param, pointer to newly alloc'd cache
 *
 * You should call cptr_cache_free when done with the cache. Returns
 * non-zero on error.
 */
int cptr_cache_alloc(struct cptr_cache **out);
/**
 * cptr_cache_free -- Free a cptr cache alloc'd via cptr_cache_alloc
 * @cache: the cptr cache to free
 */
void cptr_cache_free(struct cptr_cache *cache);
/**
 * cptr_cache_init -- Initialize the data in a cptr cache
 * @cache: the cptr cache to initialize
 *
 * Zeros out things, and sets some initial values. You *must* call this
 * function before using the cptr cache.
 */
int cptr_cache_init(struct cptr_cache *cache);
/**
 * cptr_cache_destroy -- Destroys internals of cptr cache
 * @cache: cache to destroy
 *
 * For now, this is a no-op. You *must* call this before freeing the
 * cache. (Yes, for now it is a no-op, but perhaps it won't be in the
 * future.)
 */
void cptr_cache_destroy(struct cptr_cache *cache);
/**
 * cptr_alloc -- Allocate a new cptr from the cache
 * @cache: the cptr cache to allocate from
 * @free_cptr: out param, points to the allocated cptr
 *
 * Returns non-zero if there are no more slots left.
 */
int cptr_alloc(struct cptr_cache *cptr_cache, cptr_t *free_cptr);
/**
 * cptr_free -- Return a cptr to the cache
 * @cache: the cptr cache to return the cptr to
 * @c: the cptr to return
 *
 * Fails silently if the cptr is free already.
 */
void cptr_free(struct cptr_cache *cptr_cache, cptr_t c);


/* CSPACES -------------------------------------------------- */

/**
 * Initializes caches, etc. in capability subsystem. Called when microkernel
 * intializes.
 */
int cap_init(void);
/**
 * Tears down caches, etc. in capability subsystem. Called when microkernel
 * is exiting.
 */
void cap_fini(void);

/**
 * cap_cspace_slots_in_level -- Return total number of slots in cspace at lvl
 * @lvl: the level to query
 *
 * Returns the total number of *capability* slots in all of the
 * cnode tables at a given @lvl of the cspace radix tree.
 */
static inline int cap_cspace_slots_in_level(int lvl)
{
	int out = CAP_CSPACE_CNODE_TABLE_SIZE/2;
	if (lvl < 0 || lvl >= CAP_CSPACE_DEPTH)
		CAP_BUG();
	for ( ; lvl > 0; lvl-- )
		out *= CAP_CSPACE_CNODE_TABLE_SIZE/2;
	return out;
}

/**
 * Register a new capability object type.  If you pass type == 0, the
 * system will select the next available identifier and return it.  You
 * should use the returned value as your object identifier.  If you
 * attempt to use a type that is already in use, this returns
 * -EADDRINUSE.  If there are no types remaining or you exceed
 * CAP_TYPE_MAX, this returns -ENOMEM .
 */
cap_type_t cap_register_type(cap_type_t type, const struct cap_type_ops *ops);
/**
 * Revoke all derived capabilities.
 *
 * Does not delete the caller's capability.
 *
 * This may change the state of the lcd's whose capabilities are revoked (see
 * comment lcd_cap_delete).
 */
int cap_revoke(struct cspace *cspace, cptr_t c);
/**
 * Delete the capability in slot from this cspace.
 *
 * This may change the state of the caller. (For example, if the caller is
 * a regular lcd, and if the capability is to a page, the page will be unmapped
 * from the caller's address space.)
 *
 * If this is the last capability to the object, the object will be destroyed,
 * unless it is a kernel page. See klcd_add_page and klcd_rm_page.
 */
void cap_delete(struct cspace *cspace, cptr_t c);

/**
 * Allocates a new cspace. If no memory could be allocated, returns NULL.
 */
struct cspace * cap_alloc_cspace(void);

/**
 * Frees a cspace allocated with `cap_alloc_cspace`.
 */
void cap_free_cspace(struct cspace *cspace);

/**
 * Sets up cspace - initializes lock, root cnode table, etc.
 */
int cap_init_cspace(struct cspace *cspace);
/**
 * Set the "owner" field of the given cspace
 */
void cap_cspace_setowner(struct cspace *cspace, void * owner);
/**
 * Get the "owner" field of the given cspace
 */
void* cap_cspace_getowner(struct cspace *cspace);
/**
 * Inserts object data into cspace at cnode pointed at by c.
 */
int cap_insert(struct cspace *cspace, cptr_t c, void *object, cap_type_t type);
/**
 * Deletes object data from cspace at cnode pointed at by c.
 *
 * Updates the state of the microkernel to reflect rights change (e.g., if
 * a cnode for a page is deleted, and the page is mapped, the page will be
 * unmapped).
 *
 * If this is the last cnode that refers to the object, the object is
 * destroyed.
 */
void cap_delete(struct cspace *cspace, cptr_t c);
/**
 * Copies cnode data in src cnode at c_src to dest cnode at c_dst. The dest
 * cnode will be a child of the src cnode in the cdt containing src cnode.
 */
int cap_grant(struct cspace *cspacesrc, cptr_t c_src,
	      struct cspace *cspacedst, cptr_t c_dst);
/**
 * Equivalent to calling lcd_cap_delete on all of the cnode's children. 
 *
 * ** Does not delete the cnode itself. **
 */
int cap_revoke(struct cspace *cspace, cptr_t c);
/**
 * Equivalent to calling lcd_cap_delete on all cnodes in cspace. Frees up
 * all cnode tables, etc.
 */
void cap_destroy_cspace(struct cspace *cspace);
/**
 * Looks up cnode at cap in cspace.
 *
 * ** Frees the cnode lock itself without relying on user.
 *
 * ** Interrupts and preemption *are not* disabled. **
 *    (so we can easily get out of deadlocks while debugging)
 */
int cap_cnode_verify(struct cspace *cspace, cptr_t cap);
/**
 * Return the cptr that points to this cnode.
 */
cptr_t cap_cnode_cptr(struct cnode *cnode);

/**
 * Return the cnode that this cptr points to in the given cspace. Acquires
 * a lock to the cnode. Returns zero on success. Make sure to call
 * cap_cnode_put after every cap_cnode_get.
 */
int cap_cnode_get(struct cspace *cspace, cptr_t cptr, struct cnode **cnode);

/**
 * Unlock the cnode. Call this on every cnode you've called
 * cap_cnode_get on.
 */
void cap_cnode_put(struct cnode *cnode);

/**
 * Get the object stored at this cnode.
 */
void* cap_cnode_object(struct cnode *cnode);

/**
 * Get the type of this cnode
 */
cap_type_t cap_cnode_type(struct cnode *cnode);

/**
 * Get the cspace this cnode is in.
 */
struct cspace * cap_cnode_cspace(struct cnode *cnode);


#endif /* __LIBCAP_H__ */
