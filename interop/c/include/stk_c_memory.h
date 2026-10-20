/*
 * SuperTinyKernel(TM) RTOS: Lightweight High-Performance Deterministic C++ RTOS for Embedded Systems.
 *
 * Source: https://github.com/SuperTinyKernel-RTOS
 *
 * Copyright (c) 2022-2026 Neutron Code Limited <stk@neutroncode.com>. All Rights Reserved.
 * License: MIT License, see LICENSE for a full text.
 */

#ifndef STK_C_MEMORY_H_
#define STK_C_MEMORY_H_

#include "stk_c.h"

/*! \file     stk_c_memory.h
    \brief    C language bindings for stk::memory::BlockMemoryPool and stk::memory::BytePool.

    \c stk_blockpool_t instances are allocated from a static pool of
    \c STK_C_BLOCKPOOL_MAX slots.  Obtain a handle with \c stk_blockpool_create()
    or \c stk_blockpool_create_static() and release it with \c stk_blockpool_destroy().

    Two storage modes are exposed, mirroring the C++ class:

    | Mode             | Function                       | Who owns storage?       |
    |------------------|--------------------------------|-------------------------|
    | Heap storage     | stk_blockpool_create()         | Pool (freed on destroy) |
    | External storage | stk_blockpool_create_static()  | Caller                  |

    For zero-heap deployments, declare a storage buffer with the helper macro
    \c STK_BLOCKPOOL_STORAGE_SIZE() and pass it to \c stk_blockpool_create_static():

    \code
    #define PKT_COUNT  8U
    #define PKT_SIZE   sizeof(Packet)

    STK_BLOCKPOOL_STORAGE_DECL(g_PktStorage, PKT_COUNT, PKT_SIZE);

    stk_blockpool_t *pool = stk_blockpool_create_static(
        PKT_COUNT, PKT_SIZE,
        g_PktStorage, sizeof(g_PktStorage), "pkt_pool");

    void ISR_Receiver(void) {
        void *blk = stk_blockpool_try_alloc(pool);
        if (blk) { FillPacket((Packet *)blk); Queue_Write(blk); }
    }

    void Task_Parser(void) {
        void *blk = NULL;
        if (Queue_Read(&blk)) {
            Parse((Packet *)blk);
            stk_blockpool_free(pool, blk);
        }
    }
    \endcode

    \c stk_bytepool_t (variable-size, first-fit allocator with coalescing) follows the
    same pattern with \c STK_C_BYTEPOOL_MAX slots, \c stk_bytepool_create(),
    \c stk_bytepool_create_static() and \c stk_bytepool_destroy():

    \code
    STK_BYTEPOOL_STORAGE_DECL(g_Heap, STK_BYTEPOOL_REQUIRED_SIZE(128) * 4 + STK_BYTEPOOL_REQUIRED_SIZE(512));

    stk_bytepool_t *bp = stk_bytepool_create_static(g_Heap, sizeof(g_Heap), "msg_pool");

    void Task_Producer(void) {
        char *msg = (char *)stk_bytepool_timed_alloc(bp, 64, stk_ticks_from_ms(100));
        if (msg) { Format(msg, 64); Queue_Write(msg); }
    }

    void Task_Consumer(void) {
        void *msg = NULL;
        if (Queue_Read(&msg)) { Send((const char *)msg); stk_bytepool_free(bp, msg); }
    }
    \endcode

    \defgroup c_api_memory STK C Memory API
    \brief    Pure C interface for stk::memory::BlockMemoryPool and stk::memory::BytePool.
    @{
*/

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Configuration macros
// =============================================================================

/*! \def     STK_C_BLOCKPOOL_MAX
    \brief   Maximum number of concurrent \c stk_blockpool_t instances (default: 8).
    \note    Increase if your application needs more simultaneous pools.
*/
#ifndef STK_C_BLOCKPOOL_MAX
    #define STK_C_BLOCKPOOL_MAX (8U)
#endif

/*! \def     STK_C_BYTEPOOL_MAX
    \brief   Maximum number of concurrent \c stk_bytepool_t instances (default: 8).
    \note    Increase if your application needs more simultaneous pools.
*/
#ifndef STK_C_BYTEPOOL_MAX
    #define STK_C_BYTEPOOL_MAX (8U)
#endif

// =============================================================================
// Storage helper macros
// =============================================================================

/*! \def     STK_BLOCKPOOL_ALIGN
    \brief   Required storage alignment in bytes (equals sizeof(void*)).
*/
#define STK_BLOCKPOOL_ALIGN (sizeof(stk_word_t))

/*! \def     STK_BLOCKPOOL_ALIGN_BLOCK_SIZE(raw_size)
    \brief   Compute the internally aligned block size for a given raw byte size.
    \details Rounds \a raw_size up to the nearest multiple of \c STK_BLOCKPOOL_ALIGN,
             with a minimum of \c STK_BLOCKPOOL_ALIGN.  Use this at compile time to
             size external storage buffers correctly.
*/
#define STK_BLOCKPOOL_ALIGN_BLOCK_SIZE(raw_size) \
    (((raw_size) < STK_BLOCKPOOL_ALIGN) \
        ? STK_BLOCKPOOL_ALIGN \
        : (((raw_size) + (STK_BLOCKPOOL_ALIGN - 1U)) & ~(STK_BLOCKPOOL_ALIGN - 1U)))

/*! \def     STK_BLOCKPOOL_STORAGE_SIZE(capacity, raw_block_size)
    \brief   Compute the minimum external storage buffer size in bytes.
    \param   capacity: Number of blocks the pool will hold.
    \param   raw_block_size: Raw per-block size in bytes.
*/
#define STK_BLOCKPOOL_STORAGE_SIZE(capacity, raw_block_size) \
    ((capacity) * STK_BLOCKPOOL_ALIGN_BLOCK_SIZE(raw_block_size))

/*! \def     STK_BLOCKPOOL_STORAGE_DECL(name, capacity, raw_block_size)
    \brief   Declare a correctly sized and aligned external storage array.
    \details Expands to a \c static \c stk_word_t array with the required size and
             pointer-sized alignment. Intended for file-scope or function-scope use.
             \c STK_BLOCKPOOL_ALIGN_BLOCK_SIZE() guarantees the total byte count is
             always an exact multiple of \c sizeof(stk_word_t), so the integer
             division in the array dimension is lossless by construction.
    \param   name: C identifier for the array variable.
    \param   capacity: Number of blocks the pool will hold.
    \param   raw_block_size: Raw per-block size in bytes.

    \code
    STK_BLOCKPOOL_STORAGE_DECL(g_BufStorage, 16, sizeof(MyBuf));
    stk_blockpool_t *pool = stk_blockpool_create_static(
        16, sizeof(MyBuf), g_BufStorage, sizeof(g_BufStorage), NULL);
    \endcode
*/
#define STK_BLOCKPOOL_STORAGE_DECL(name, capacity, raw_block_size) \
    static stk_word_t name[STK_BLOCKPOOL_STORAGE_SIZE(capacity, raw_block_size) / sizeof(stk_word_t)]

// =============================================================================
// BytePool storage helper macros
// =============================================================================

/*! \def     STK_BYTEPOOL_ALIGN
    \brief   Payload alignment and block size granularity of a byte pool, in bytes (always 8).
*/
#define STK_BYTEPOOL_ALIGN (8U)

/*! \def     STK_BYTEPOOL_HEADER_SIZE
    \brief   Per-block header size in bytes (always 8). Every allocation consumes this
             many bytes of the pool in addition to its aligned payload.
*/
#define STK_BYTEPOOL_HEADER_SIZE (8U)

/*! \def     STK_BYTEPOOL_ALIGN_SIZE(raw_size)
    \brief   Round \a raw_size up to the nearest multiple of \c STK_BYTEPOOL_ALIGN.
*/
#define STK_BYTEPOOL_ALIGN_SIZE(raw_size) \
    (((raw_size) + (STK_BYTEPOOL_ALIGN - 1U)) & ~(STK_BYTEPOOL_ALIGN - 1U))

/*! \def     STK_BYTEPOOL_REQUIRED_SIZE(payload_size)
    \brief   Pool space in bytes consumed by one allocation of \a payload_size bytes
             (aligned payload plus block header). Sum the value for every block that
             must be alive simultaneously to size external storage.
*/
#define STK_BYTEPOOL_REQUIRED_SIZE(payload_size) \
    (STK_BYTEPOOL_ALIGN_SIZE(payload_size) + STK_BYTEPOOL_HEADER_SIZE)

/*! \def     STK_BYTEPOOL_STORAGE_DECL(name, size_bytes)
    \brief   Declare an 8-byte aligned external storage array for a byte pool.
    \details Expands to a \c static \c uint64_t array so that the start address is
             aligned to \c STK_BYTEPOOL_ALIGN on every target. Use
             \c sizeof(name) as the \a storage_size argument.
    \param   name: C identifier for the array variable.
    \param   size_bytes: Required pool size in bytes (headers included), e.g. a sum of
             \c STK_BYTEPOOL_REQUIRED_SIZE() values.
*/
#define STK_BYTEPOOL_STORAGE_DECL(name, size_bytes) \
    static uint64_t name[((size_bytes) + (sizeof(uint64_t) - 1U)) / sizeof(uint64_t)]

// =============================================================================
// Types
// =============================================================================

/*! \brief Opaque handle to a \c stk::memory::BlockMemoryPool instance.
*/
typedef struct stk_blockpool_t stk_blockpool_t;

/*! \brief Opaque handle to a \c stk::memory::BytePool instance.
*/
typedef struct stk_bytepool_t stk_bytepool_t;

// =============================================================================
// Lifecycle - heap storage
// =============================================================================

/*! \brief     Create a block pool backed by \b heap-allocated storage.
    \details   Allocates a flat byte buffer of
               \c capacity * AlignBlockSize(raw_block_size) bytes from the heap.
               Call \c stk_blockpool_is_storage_valid() immediately after creation
               when operating without exceptions (typical embedded configuration).
    \param[in] capacity: Total number of blocks.
    \param[in] raw_block_size: Requested per-block size in bytes.
    \param[in] name: Optional human-readable name (may be \c NULL).
               Forwarded to \c ITraceable::SetTraceName().
    \return    Pool handle, or \c NULL if the static slot pool is exhausted
               (\c STK_C_BLOCKPOOL_MAX reached).
               A non-NULL handle does \b not guarantee the backing storage was
               successfully heap-allocated; always call \c stk_blockpool_is_storage_valid()
               immediately after creation when operating without exceptions.
    \note      Not ISR-safe.
    \see       stk_blockpool_is_storage_valid(), stk_blockpool_destroy()
*/
stk_blockpool_t *stk_blockpool_create(size_t      capacity,
                                      size_t      raw_block_size,
                                      const char *name);

/*! \brief     Create a block pool backed by \b caller-supplied (external) storage.
    \details   The pool references \a storage directly without taking ownership.
               The caller must keep the buffer alive for the entire lifetime of
               the pool. The storage is \b not freed on destruction.
    \param[in] capacity: Total number of blocks the pool can hold.
    \param[in] raw_block_size: Requested per-block size in bytes.
    \param[in] storage: Pointer to a caller-owned byte buffer. Must be aligned to
               at least \c sizeof(void*) and large enough to hold
               at least \c STK_BLOCKPOOL_STORAGE_SIZE(capacity, raw_block_size)
               bytes. Asserted at construction time.
    \param[in] storage_size: Size of \a storage in bytes (used for the size assertion).
    \param[in] name: Optional human-readable name (may be \c NULL).
    \return    Pool handle, or \c NULL if the static slot pool is exhausted
               (\c STK_C_BLOCKPOOL_MAX reached).
    \note      Not ISR-safe.
    \see       stk_blockpool_destroy()
*/
stk_blockpool_t *stk_blockpool_create_static(size_t      capacity,
                                             size_t      raw_block_size,
                                             uint8_t    *storage,
                                             size_t      storage_size,
                                             const char *name);

/*! \brief     Destroy a pool and return its slot to the static pool.
    \details   If the pool owns heap storage, it is freed. External storage is
               never touched.
    \param[in] pool: Pool handle obtained via \c stk_blockpool_create() or
               \c stk_blockpool_create_static().
    \warning   Destroying a pool while tasks are blocked in \c stk_blockpool_alloc()
               or \c stk_blockpool_timed_alloc() is a logic error and triggers an
               assertion in debug builds.
    \warning   Set the pool pointer to \c NULL after this call to prevent accidental
               use-after-destroy (the handle slot may be reused by a subsequent
               \c stk_blockpool_create() call).
    \note      Not ISR-safe.
*/
void stk_blockpool_destroy(stk_blockpool_t *pool);

// =============================================================================
// Allocation
// =============================================================================

/*! \brief     Allocate one block, blocking indefinitely until one is available.
    \param[in] pool: Pool handle.
    \return    Pointer to an uninitialized block of at least \a raw_block_size bytes.
               Never returns \c NULL.
    \warning   Not ISR-safe.
*/
void *stk_blockpool_alloc(stk_blockpool_t *pool);

/*! \brief     Allocate one block, blocking until one becomes available or the timeout expires.
    \param[in] pool: Pool handle.
    \param[in] timeout: Maximum time to wait in OS ticks.
               Pass \c STK_WAIT_INFINITE to block indefinitely (same as \c stk_blockpool_alloc()),
               or \c STK_NO_WAIT for a non-blocking attempt identical to \c stk_blockpool_try_alloc().
    \return    Pointer to an uninitialized block, or \c NULL if the timeout expired
               before a block became available.
    \note      If OS tick rate is 1 kHz then 1 tick = 1 ms. Use \c stk_ticks_from_ms()
               to convert a millisecond value to ticks for best portability across
               different tick rates.
    \warning   ISR-safe \b only when \a timeout = \c STK_NO_WAIT; not ISR-safe otherwise.
*/
void *stk_blockpool_timed_alloc(stk_blockpool_t *pool, stk_timeout_t timeout);

/*! \brief     Non-blocking allocation attempt.
    \details   Returns a block immediately if one is available, or \c NULL if the
               pool is empty. Never suspends the calling task.
    \param[in] pool: Pool handle.
    \return    Pointer to an uninitialized block, or \c NULL if the pool is empty.
    \note      ISR-safe.
*/
void *stk_blockpool_try_alloc(stk_blockpool_t *pool);

// =============================================================================
// Deallocation
// =============================================================================

/*! \brief     Return a previously allocated block to the pool.
    \details   Pushes the block back onto the free-list head in O(1) and wakes
               exactly one task blocked inside \c stk_blockpool_alloc() or
               \c stk_blockpool_timed_alloc(), if any.
    \param[in] pool: Pool handle.
    \param[in] ptr: Pointer previously returned by \c stk_blockpool_alloc(),
               \c stk_blockpool_timed_alloc(), or \c stk_blockpool_try_alloc().
               Must belong to \a pool. Bounds and alignment are validated;
               failures trigger an assertion in debug builds.
    \return    \c true  on success.
               \c false if \a ptr is \c NULL, out of range, or misaligned -
               each case indicates a caller logic error.
    \note      ISR-safe.
    \warning   Null the pointer after \c stk_blockpool_free() to prevent double-free.
*/
bool stk_blockpool_free(stk_blockpool_t *pool, void *ptr);

// =============================================================================
// Query
// =============================================================================

/*! \brief     Verify that the backing storage is valid and the pool is ready for use.
    \details   Always \c true for pools created with \c stk_blockpool_create_static().
               For heap-constructed pools (\c stk_blockpool_create()), \c false if
               \c operator new failed. Must be checked after heap construction when
               operating without exceptions.
    \param[in] pool: Pool handle.
    \return    \c true if the pool is ready for use.
    \note      ISR-safe.
*/
bool stk_blockpool_is_storage_valid(const stk_blockpool_t *pool);

/*! \brief     Get the total block capacity of the pool.
    \param[in] pool: Pool handle.
    \return    Maximum number of blocks that can be simultaneously allocated.
    \note      ISR-safe.
*/
size_t stk_blockpool_get_capacity(const stk_blockpool_t *pool);

/*! \brief     Get the aligned block size used internally by the allocator.
    \details   Equal to \c STK_BLOCKPOOL_ALIGN_BLOCK_SIZE(raw_block_size) as passed
               at construction. Always >= \c STK_BLOCKPOOL_ALIGN.
    \param[in] pool: Pool handle.
    \return    Aligned block size in bytes.
    \note      ISR-safe.
*/
size_t stk_blockpool_get_block_size(const stk_blockpool_t *pool);

/*! \brief     Get the number of currently allocated (outstanding) blocks.
    \param[in] pool: Pool handle.
    \return    Point-in-time snapshot. May be stale immediately after return in a
               multi-task environment.
    \note      ISR-safety depends on target ABI: the counter is a 16-bit value, so a
               single-instruction atomic read is guaranteed on 32-bit Cortex-M (aligned
               halfword load) but not on 8-bit targets where two bus cycles may be needed.
               Treat the result as advisory in all multi-core or 8-bit contexts.
*/
size_t stk_blockpool_get_used_count(const stk_blockpool_t *pool);

/*! \brief     Get the number of free (available) blocks.
    \param[in] pool: Pool handle.
    \return    Point-in-time snapshot of capacity - used_count.
    \note      ISR-safe.
*/
size_t stk_blockpool_get_free_count(const stk_blockpool_t *pool);

/*! \brief     Check whether all blocks are currently allocated (pool exhausted).
    \param[in] pool: Pool handle.
    \return    \c true if no blocks are available for allocation.
    \note      ISR-safe.
*/
bool stk_blockpool_is_full(const stk_blockpool_t *pool);

/*! \brief     Check whether all blocks are free (no outstanding allocations).
    \param[in] pool: Pool handle.
    \return    \c true if no blocks are currently allocated.
    \note      ISR-safe.
*/
bool stk_blockpool_is_empty(const stk_blockpool_t *pool);

// =============================================================================
// BytePool - lifecycle
// =============================================================================

/*! \brief     Create a byte pool backed by \b heap-allocated storage.
    \details   Allocates \a storage_size bytes from the heap. The size includes block headers
               (see \c STK_BYTEPOOL_REQUIRED_SIZE()). Call \c stk_bytepool_is_storage_valid()
               immediately after creation when operating without exceptions.
    \param[in] storage_size: Size of the pool area in bytes (headers included).
    \param[in] name: Optional human-readable name (may be \c NULL).
    \return    Pool handle, or \c NULL if the static slot pool is exhausted
               (\c STK_C_BYTEPOOL_MAX reached). A non-NULL handle does \b not guarantee
               that the heap allocation succeeded; check \c stk_bytepool_is_storage_valid().
    \note      Not ISR-safe.
    \see       stk_bytepool_is_storage_valid(), stk_bytepool_destroy()
*/
stk_bytepool_t *stk_bytepool_create(size_t storage_size, const char *name);

/*! \brief     Create a byte pool backed by \b caller-supplied (external) storage.
    \details   The pool references \a storage directly without taking ownership. The caller
               must keep the buffer alive for the entire lifetime of the pool. A misaligned
               start address is corrected internally (unaligned head/tail bytes are unused).
    \param[in] storage: Pointer to a caller-owned byte buffer, preferably aligned to
               \c STK_BYTEPOOL_ALIGN (see \c STK_BYTEPOOL_STORAGE_DECL()).
    \param[in] storage_size: Size of \a storage in bytes. Must be at least
               \c 2 * STK_BYTEPOOL_HEADER_SIZE (asserted).
    \param[in] name: Optional human-readable name (may be \c NULL).
    \return    Pool handle, or \c NULL if the static slot pool is exhausted
               (\c STK_C_BYTEPOOL_MAX reached).
    \note      Not ISR-safe.
    \see       stk_bytepool_destroy()
*/
stk_bytepool_t *stk_bytepool_create_static(uint8_t    *storage,
                                           size_t      storage_size,
                                           const char *name);

/*! \brief     Destroy a byte pool and return its slot to the static pool.
    \details   Heap storage is freed if owned. External storage is never touched.
    \param[in] pool: Handle obtained via \c stk_bytepool_create() or
               \c stk_bytepool_create_static().
    \warning   Destroying a pool while tasks are blocked in \c stk_bytepool_alloc() or
               \c stk_bytepool_timed_alloc() is a logic error.
    \warning   Set the pool pointer to \c NULL after this call; the slot may be reused.
    \note      Not ISR-safe.
*/
void stk_bytepool_destroy(stk_bytepool_t *pool);

// =============================================================================
// BytePool - allocation
// =============================================================================

/*! \brief     Allocate a block, blocking indefinitely until enough contiguous memory is available.
    \param[in] pool: Pool handle.
    \param[in] size: Requested payload size in bytes (> 0 and <= \c stk_bytepool_get_max_request()).
    \return    Pointer to an uninitialized block aligned to \c STK_BYTEPOOL_ALIGN, or \c NULL
               if the request is invalid (zero, larger than the pool) or the wait was cancelled.
               Never returns \c NULL because of a timeout.
    \warning   Not ISR-safe.
*/
void *stk_bytepool_alloc(stk_bytepool_t *pool, size_t size);

/*! \brief     Allocate a block, blocking until memory becomes available or the timeout expires.
    \details   The timeout is a total budget, not per wake-up.
    \param[in] pool: Pool handle.
    \param[in] size: Requested payload size in bytes.
    \param[in] timeout: Maximum time to wait in OS ticks. \c STK_WAIT_INFINITE blocks
               indefinitely; \c STK_NO_WAIT is identical to \c stk_bytepool_try_alloc().
    \return    Pointer to an uninitialized block, or \c NULL on timeout, cancellation or
               invalid arguments.
    \warning   ISR-safe \b only when \a timeout = \c STK_NO_WAIT.
*/
void *stk_bytepool_timed_alloc(stk_bytepool_t *pool, size_t size, stk_timeout_t timeout);

/*! \brief     Non-blocking allocation attempt.
    \param[in] pool: Pool handle.
    \param[in] size: Requested payload size in bytes.
    \return    Pointer to an uninitialized block, or \c NULL if no suitable free block exists.
    \note      ISR-safe, but execution time is proportional to the number of blocks scanned.
*/
void *stk_bytepool_try_alloc(stk_bytepool_t *pool, size_t size);

// =============================================================================
// BytePool - deallocation
// =============================================================================

/*! \brief     Return a previously allocated block to the pool.
    \details   Marks the block free, merges it with following free blocks and wakes all tasks
               blocked in \c stk_bytepool_alloc() / \c stk_bytepool_timed_alloc().
    \param[in] pool: Pool handle.
    \param[in] ptr: Pointer previously returned by an allocation function of this pool.
    \return    \c true on success; \c false if \a ptr is \c NULL, outside the pool, misaligned,
               or not in use (double-free).
    \note      ISR-safe, but execution time is proportional to the number of blocks scanned.
    \warning   Null the pointer after \c stk_bytepool_free() to prevent double-free.
*/
bool stk_bytepool_free(stk_bytepool_t *pool, void *ptr);

// =============================================================================
// BytePool - query
// =============================================================================

/*! \brief     Collect pool statistics (mutually consistent snapshot).
    \param[in]  pool: Pool handle.
    \param[out] available: Total allocatable payload bytes over all free fragments
                (may be \c NULL).
    \param[out] largest: Payload size of the largest free fragment, i.e. the biggest request
                that would succeed right now (may be \c NULL).
    \param[out] fragments: Number of free fragments; 1 means no external fragmentation
                (may be \c NULL).
    \note      ISR-safe, but O(n) in the number of blocks.
*/
void stk_bytepool_get_info(const stk_bytepool_t *pool,
                           size_t               *available,
                           size_t               *largest,
                           size_t               *fragments);

/*! \brief     Verify that the backing storage is valid and the pool is ready for use.
    \param[in] pool: Pool handle.
    \return    \c true if the pool is ready; \c false if heap allocation failed or the supplied
               area is too small for even one block.
    \note      ISR-safe.
*/
bool stk_bytepool_is_storage_valid(const stk_bytepool_t *pool);

/*! \brief     Get the total managed (aligned) area size in bytes, headers included.
    \param[in] pool: Pool handle.
    \return    Managed area size, or 0 if the storage is invalid.
    \note      ISR-safe.
*/
size_t stk_bytepool_get_size(const stk_bytepool_t *pool);

/*! \brief     Get the largest request that can ever succeed on this pool.
    \param[in] pool: Pool handle.
    \return    Payload size of a single block spanning the whole pool, or 0 if the storage
               is invalid. Larger requests fail immediately instead of blocking forever.
    \note      ISR-safe.
*/
size_t stk_bytepool_get_max_request(const stk_bytepool_t *pool);

#ifdef __cplusplus
namespace stk { namespace memory { class BlockMemoryPool; class BytePool; } }

/*! \brief     Get the underlying C++ \c stk::memory::BlockMemoryPool object wrapped by a \c stk_blockpool_t handle.
    \param[in] pool: Pool handle obtained via \c stk_blockpool_create() or
               \c stk_blockpool_create_static().
    \return    Pointer to the wrapped \c stk::memory::BlockMemoryPool instance. Never \c NULL for a valid handle.
    \note      C++ callers only (guarded by \c __cplusplus); not part of the C ABI.
*/
extern "C" stk::memory::BlockMemoryPool *stk_blockpool_get_instance(stk_blockpool_t *pool);

/*! \brief     Get the underlying C++ \c stk::memory::BytePool object wrapped by a \c stk_bytepool_t handle.
    \param[in] pool: Pool handle obtained via \c stk_bytepool_create() or
               \c stk_bytepool_create_static().
    \return    Pointer to the wrapped \c stk::memory::BytePool instance. Never \c NULL for a valid handle.
    \note      C++ callers only (guarded by \c __cplusplus); not part of the C ABI.
*/
extern "C" stk::memory::BytePool *stk_bytepool_get_instance(stk_bytepool_t *pool);
#endif // __cplusplus

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* STK_C_MEMORY_H_ */
