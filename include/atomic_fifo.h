/*/////////////////////////////////////////////////////////////////////////////
/// @summary Defines and implements several types of FIFO containers supporting
/// concurrent access with different attributes, along with some frequently 
/// required functions for working with dynamic arrays.
///////////////////////////////////////////////////////////////////////////80*/

#ifndef ATOMIC_FIFO_H
#define ATOMIC_FIFO_H

/*////////////////
//   Includes   //
////////////////*/
#ifndef PLATFORM_INTRINSICS_DEFINED
#include "intrinsics.h"
#endif
#include <atomic>
#include <thread>

/*////////////////////
//   Preprocessor   //
////////////////////*/

/*/////////////////
//   Constants   //
/////////////////*/

/*////////////////////////////
//   Forward Declarations   //
////////////////////////////*/

/*//////////////////
//   Data Types   //
//////////////////*/
/// @summary Define the data associated with a single pool-managed node used in
/// the unbounded queue implementations (spsc_fifo_u_t and mpsc_fifo_u_t.)
template <typename T>
struct fifo_node_t
{   typedef typename std::atomic<fifo_node_t*> next_node_t;
    next_node_t      Next;    /// The next node in the queue. Used by FIFO only.
    T                Item;    /// The data stored at the node. Used by FIFO only.
    fifo_node_t     *Foot;    /// Footer data for the node. Used by allocator only.
};

/// @summary Define the data associated with a 'lock-free' list of FIFO nodes.
/// The allocator is specifically designed for FIFO allocation patterns where
/// nodes are freed in the order they are allocated. The allocator is safe for
/// concurrent access by a single producer and single consumer only. For use
/// with a multi-producer queue, each producer thread must maintain its own
/// allocator, but be aware that the nodes managed by the allocator can only
/// safely be freed after the target queue has been destroyed.
template <typename T>
struct fifo_allocator_t
{
    fifo_node_t<T>  *Head;    /// The least recently used node in the allocator.
    cacheline_t      Pad0;    /// Padding separating producer-only from consumer-only data.
    fifo_node_t<T>  *Tail;    /// The most recently used node in the allocator.
    size_t           Count;   /// The number of nodes managed by the allocator.
    std::thread::id  Owner;   /// The identifier of the thread that owns the allocator.
    cacheline_t      Pad1;    /// Padding separating allocator data from other fields.
};

/// @summary Define the data associated with a table mapping target queue to 
/// its corresponding FIFO node allocator. The target queue is identified by
/// address only and is stored internally as an opaque pointer-size value. 
/// This structure is not safe for concurrent access by multiple threads.
template <typename T>
struct fifo_allocator_table_t
{   typedef uintptr_t             queue_t;
    typedef fifo_allocator_t<T>   alloc_t;
    size_t           Count;   /// The number of items in the table.
    size_t           Capacity;/// The maximum number of items in the table.
    queue_t         *KList;   /// The list of keys.
    alloc_t         *VList;   /// The list of allocator instances.
};

/// @summary Define the data associated with a FIFO in which the producer and
/// consumer are the same thread. There are no locks and no waiting. This type
/// of queue is good for internal buffering. The FIFO maintains an internal
/// free list of nodes to reduce memory allocations.
template <typename T>
struct lplc_fifo_u_t
{
    /// @summary Define the node structure. Very basic, just a next pointer and data.
    /// The node is reused between the queue and the free list.
    struct lplc_node_t
    {
        lplc_node_t *Next;    /// The next node in the queue.
        T            Item;    /// The data stored at the node.
    };
    lplc_node_t     *Head;    /// The head of the queue (oldest item).
    lplc_node_t     *FreeTail;/// The tail of the free list (most recently freed).
    lplc_node_t     *Tail;    /// The tail of the queue (newest item).
    lplc_node_t     *FreeHead;/// The head of the free list (oldest item).
};

/// @summary Define the data associated with an unbounded FIFO that is safe for
/// concurrent access by a single reader and a single writer. Nodes are managed
/// by a fifo_allocator_t<T> owned by the producer thread.
template <typename T>
struct spsc_fifo_u_t
{   typedef fifo_node_t<T>        spsc_node_t;
    spsc_node_t     *Head;    /// The head of the queue (oldest item).
    cacheline_t      Pad0;    /// Padding separating producer-only from consumer-only data.
    spsc_node_t     *Tail;    /// The tail of the queue (newest item).
    fifo_node_t<T>  *Stub;    /// A dummy node used internally.
    cacheline_t      Pad1;    /// Padding separating queue data from other fields.
};

/// @summary Define the data associated with a bounded FIFO that is safe for
/// concurrent access by a single reader and a single writer. This implementation
/// requires that the platform support atomic reads and writes of aligned 32-bit
/// integer values (all modern platforms meet this requirement).
template <typename T>
struct spsc_fifo_b_t
{   typedef std::atomic<size_t>   spsc_index_t;
    size_t           Size;    /// The capacity of the queue, a power-of-two.
    size_t           Mask;    /// Mask off all bits greater than capacity.
    T               *Store;   /// Storage for items in the queue.
    cacheline_t      Pad0;    /// Separate producer-consumer data from producer- or consumer-only.
    spsc_index_t     Head;    /// The index of the oldest item in the queue.
    cacheline_t      Pad1;    /// Separate producer-only from consumer-only data.
    spsc_index_t     Tail;    /// The index of the newest item in the queue.
    cacheline_t      Pad2;    /// Padding separating queue data from other fields.
};

/// @summary Define the data associated with an unbounded FIFO that is safe for
/// concurrent access by a single reader and multple writers. Nodes are managed
/// by a fifo_allocator_t<T> owned by each producer thread.
template <typename T>
struct mpsc_fifo_u_t
{   typedef typename std::atomic<fifo_node_t<T>*> mpsc_node_t;
    mpsc_node_t      Head;    /// The head of the queue (oldest item).
    cacheline_t      Pad0;    /// Padding separating producer-only from consumer-only data.
    mpsc_node_t      Tail;    /// The tail of the queue (newest item).
    fifo_node_t<T>  *Stub;    /// A dummy node used internally.
    cacheline_t      Pad1;    /// Padding separating queue data from other fields.
};

/*/////////////////
//   Functions   //
/////////////////*/
/// @summary Calculate the new allocation capacity for a dynamic array.
/// @param current The current array capacity, in elements.
/// @param required The minimum required array capacity, in elements.
/// @param threshold The element count at which the capacity begins growing by a fixed amount.
/// @param fixed The number of elements to add if the required capacity exceeds the threshold.
public_function size_t calculate_capacity(size_t current, size_t required, size_t threshold, size_t fixed)
{
    size_t newc = current > 0 ? current : 1;
    while (newc < required && newc <= threshold)
    {   // grow capacity by doubling until the threshold is reached.
        newc *= 2;
    }
    while (newc < required)
    {   // grow capacity by adding a fixed amount until the required capacity is reached.
        newc += fixed;
    }
    return newc;
}

/// @summary Mark a node managed by a fifo_allocator_t as being available for reuse.
/// @param node The node to mark as available for reuse.
template <typename T>
inline void fifo_allocator_return(fifo_node_t<T> *node)
{   // set the LSB of pointer node->Foot to indicate the node is not used.
    uintptr_t addr = reinterpret_cast<uintptr_t> (node->Foot) | 0x1;
    std::atomic_thread_fence(std::memory_order_release);
    node->Foot = reinterpret_cast<fifo_node_t<T>*>(addr);
}

/// @summary Check a node managed by a fifo_allocator_t to determine whether it is
/// available for reuse. This is typically not needed by client code.
/// @param node The node to check.
/// @param addr On return, this value is set to the address of the next node.
/// @return true if node is available for reuse.
template <typename T>
inline bool fifo_allocator_isfree(fifo_node_t<T> *node, uintptr_t &addr)
{   // TODO: can we make this branch-free?
    if (node != NULL)
    {   // if the LSB is set, the node is available for reuse.
        addr  = reinterpret_cast<uintptr_t>(node->Foot);
        return (addr & 0x1);
    }
    else return false;
}

/// @summary Initializes a fifo_allocator_t to empty and assigns the owner to
/// the calling thread. This function is not safe for concurrent access.
/// @param alloc The allocator to initialize.
template <typename T>
inline void fifo_allocator_init(fifo_allocator_t<T> *alloc)
{
    alloc->Head  = NULL;
    alloc->Tail  = NULL;
    alloc->Count = 0;
    alloc->Owner = std::this_thread::get_id();
}

/// @summary Assigns the owner of a fifo_allocator_t to the calling thread.
/// @param alloc The allocator being assigned to the calling thread.
template <typename T>
inline void fifo_allocator_owner(fifo_allocator_t<T> *alloc)
{
    alloc->Owner = std::this_thread::get_id();
}

/// @summary Frees all resources managed by a fifo_allocator_t and re-initializes
/// the allocator so its initial, empty state. The owning thread is not changed.
/// This function is not safe for concurrent access.
/// @param alloc The allocator being deleted and re-initialized.
template <typename T>
inline void fifo_allocator_reinit(fifo_allocator_t<T> *alloc)
{
    uintptr_t const mask = uintptr_t(0x1);
    fifo_node_t<T> *iter = alloc->Head;
    while (iter != NULL)
    {
        fifo_node_t<T> *node = iter;
        uintptr_t       addr = reinterpret_cast<uintptr_t>(node->Foot) & ~mask;
        iter                 = reinterpret_cast<fifo_node_t<T>*>(addr);
        delete node;
    }
    alloc->Head  = NULL;
    alloc->Tail  = NULL;
    alloc->Count = 0;
}

/// @summary Retrieve a node from the allocator. This function must be called
/// only by the producer thread that owns the allocator.
/// @param alloc The FIFO node allocator to query.
/// @return A pointer to the node.
template <typename T>
inline fifo_node_t<T>* fifo_allocator_get(fifo_allocator_t<T> *alloc)
{
    uintptr_t const mask = uintptr_t(0x1);
    uintptr_t       addr = 0;
    fifo_node_t<T> *node = NULL;
    fifo_node_t<T> *head = alloc->Head;
    if (fifo_allocator_isfree(head, addr))
    {   // reuse the existing node at the head of the list.
        node = head;
        alloc->Head = reinterpret_cast<fifo_node_t<T>*>(addr & ~mask);
    }
    else
    {   // no existing node is available; allocate a new node.
        node = new fifo_node_t<T>();
        alloc->Count++;
    }
    // move the node being returned to the tail of the list.
    node->Foot = NULL;                                 // node is the end of the list
    if (alloc->Tail != NULL) alloc->Tail->Foot = node; // existing tail points to node
    if (alloc->Head == NULL) alloc->Head = node;       // list was empty; Head = Tail
    alloc->Tail = node;                                // node is the new tail
    return node;
}

/// @summary Initializes a new node allocator table.
/// @param table The FIFO node allocator table to initialize.
/// @param capacity The number of node allocator instances to pre-allocate.
template <typename T>
inline void fifo_allocator_table_create(fifo_allocator_table_t<T> *table, size_t capacity)
{   typedef typename fifo_allocator_table_t<T>::queue_t k_t;
    typedef typename fifo_allocator_table_t<T>::alloc_t v_t;
    table->Count    = 0;
    table->Capacity = 0;
    table->KList    = NULL;
    table->VList    = NULL;
    if (capacity > 0)
    {
        table->KList    = (k_t*) malloc(capacity * sizeof(k_t));
        table->VList    = (v_t*) malloc(capacity * sizeof(v_t));
        table->Capacity = capacity;
    }
    for (size_t i = 0; i < capacity; ++i)
    {   // initialize the fields of each allocator.
        fifo_allocator_init(&table->VList[i]);
    }
}

/// @summary Frees resources associated with an allocator table.
/// @param table The allocator table to delete.
template <typename T>
inline void fifo_allocator_table_delete(fifo_allocator_table_t<T> *table)
{
    for (size_t i = 0, n = table->Count; i < n; ++i)
    {   // free all nodes in each allocator pool.
        fifo_allocator_reinit(&table->VList[i]);
    }
    free(table->VList); table->VList = NULL;
    free(table->KList); table->KList = NULL;
    table->Count   = 0;
    table->Capacity= 0;
}

/// @summary Retrieves the allocator associated with a given key. If the key is not known, a new allocator is initialized.
/// @param table The allocator table to query or update.
/// @param key The address (usually of the target FIFO) used as the key.
/// @return The FIFO node allocator associated with the key, or NULL if memory allocation failed.
template <typename T>
inline fifo_allocator_t<T>* fifo_allocator_table_get(fifo_allocator_table_t<T> *table, void const *key)
{   typedef typename fifo_allocator_table_t<T>::queue_t k_t;
    typedef typename fifo_allocator_table_t<T>::alloc_t v_t;
    k_t const k = (k_t const) key;
    for (size_t i = 0, n = table->Count; i < n; ++i)
    {
        if (table->KList[i] == k)
        {   // this allocator already exists in the table.
            return &table->VList[i];
        }
    }
    if (table->Count == table->Capacity)
    {   // need to allocate additional storage.
        size_t newc   = calculate_capacity(table->Capacity, table->Capacity + 1, 64, 16);
        k_t   *newk   =(k_t*) realloc(table->KList, newc * sizeof(k_t));
        v_t   *newv   =(v_t*) realloc(table->VList, newc * sizeof(v_t));
        if (newk != NULL) table->KList = newk;
        if (newv != NULL) table->VList = newv;
        if (newk != NULL && newv != NULL) table->Capacity = newc;
        else return NULL;
    }
    size_t index = table->Count++;
    table->KList[index] = k; fifo_allocator_init(&table->VList[index]);
    return &table->VList[index];
}

/// @summary Initialize the queue to empty. No existing resources are freed,
/// so lplc_fifo_u_delete() should be called prior to calling this function
/// when reusing a queue instance.
/// @param fifo The queue to initialize.
template <typename T>
inline void lplc_fifo_u_init(lplc_fifo_u_t<T> *fifo)
{   typedef typename lplc_fifo_u_t<T>::lplc_node_t lplc_node_t;
    lplc_node_t *fifo_dummy = new lplc_node_t();
    lplc_node_t *free_dummy = new lplc_node_t();
    fifo_dummy->Next = NULL;
    free_dummy->Next = NULL;
    fifo->Head       = fifo_dummy;
    fifo->FreeTail   = free_dummy;
    fifo->Tail       = fifo_dummy;
    fifo->FreeHead   = free_dummy;
}

/// @summary Frees all resources associated with the queue. All nodes are
/// deleted from both the queue and the internal free list.
/// @param fifo The queue to delete.
template <typename T>
inline void lplc_fifo_u_delete(lplc_fifo_u_t<T> *fifo)
{   typedef typename lplc_fifo_u_t<T>::lplc_node_t lplc_node_t;
    {   // move all nodes sitting in the queue to the free list.
        lplc_node_t *iter = fifo->Head->Next;
        while (iter != NULL)
        {
            fifo->FreeTail->Next = iter;
            fifo->FreeTail = iter;
            iter = iter->Next;
        }
    }
    {   // delete all nodes from the free list.
        lplc_node_t *iter = fifo->FreeHead;
        while (iter != NULL)
        {
            lplc_node_t *node = iter;
            iter = iter->Next;
            delete node;
        }
    }
    fifo->Head     = NULL;
    fifo->FreeTail = NULL;
    fifo->Tail     = NULL;
    fifo->FreeHead = NULL;
}

/// @summary Determine whether the queue is empty.
/// @param fifo The queue to query.
/// @return true if the queue is empty.
template <typename T>
inline bool lplc_fifo_u_empty(lplc_fifo_u_t<T> *fifo)
{
    return (fifo->Head->Next != NULL) ? false : true;
}

/// @summary Attempt to retrieve the item at the front of the queue without consuming it.
/// @param fifo The queue to update.
/// @param item On return, if the function returns true, this location is updated
/// with a copy of the data stored at the oldest item in the queue.
/// @return true if an item was copied from the queue.
template <typename T>
inline bool lplc_fifo_u_front(lplc_fifo_u_t<T> *fifo, T &item)
{   typedef typename lplc_fifo_u_t<T>::lplc_node_t lplc_node_t;
    lplc_node_t *old_head = fifo->Head;
    lplc_node_t *new_head = old_head->Next;
    if (new_head != NULL)
    {
        item = new_head->Item;
        return true;
    }
    else return false;
}

/// @summary Attempt to consume a single item from the queue.
/// @param fifo The queue to update.
/// @param item On return, if the function returns true, this location is updated
/// with a copy of the data stored at the oldest item in the queue.
/// @return true if an item was removed from the queue.
template <typename T>
inline bool lplc_fifo_u_consume(lplc_fifo_u_t<T> *fifo, T &item)
{   typedef typename lplc_fifo_u_t<T>::lplc_node_t lplc_node_t;
    lplc_node_t *old_head = fifo->Head;
    lplc_node_t *new_head = old_head->Next;
    if (new_head != NULL)
    {
        item = new_head->Item;
        fifo->Head = new_head;
        fifo->FreeTail->Next = old_head;
        fifo->FreeTail = old_head;
        return true;
    }
    else return false;
}

/// @summary Push a single item onto the back of the queue.
/// @param fifo The queue to update.
/// @param item The item to insert into the queue.
template <typename T>
inline void lplc_fifo_u_produce(lplc_fifo_u_t<T> *fifo, T const &item)
{   typedef typename lplc_fifo_u_t<T>::lplc_node_t lplc_node_t;
    lplc_node_t *old_free = fifo->FreeHead;
    lplc_node_t *new_free = old_free->Next;
    lplc_node_t *new_node = NULL;
    if (new_free != NULL)
    {   // reuse a node from the free list.
        fifo->FreeHead = new_free;
        new_node = old_free;
    }
    else
    {   // no nodes availble on the free list; allocate a new node.
        new_node = new lplc_node_t();
    }
    new_node->Next   = NULL;
    new_node->Item   = item;
    fifo->Tail->Next = new_node;
    fifo->Tail       = new_node;
}

/// @summary Initialize an unbounded SPSC queue to empty. This function is not
/// safe for concurrent access by multiple threads.
/// @param fifo The queue to initialize.
template <typename T>
inline void spsc_fifo_u_init(spsc_fifo_u_t<T> *fifo)
{
    fifo_node_t<T>  *stub = new fifo_node_t<T>();
    stub->Next.store(NULL,  std::memory_order_relaxed);
    stub->Foot     = NULL;
    fifo->Head     = stub;
    fifo->Tail     = stub;
    fifo->Stub     = stub;
}

/// @summary Removes all items from the queue. It is not necessary to clear the
/// queue prior to delete. This function is not safe for concurrent access.
/// @param fifo The queue to flush.
template <typename T>
inline void spsc_fifo_u_flush(spsc_fifo_u_t<T> *fifo)
{
    fifo_node_t<T> *node = NULL;
    fifo_node_t<T> *iter = fifo->Head;
    while (iter !=  NULL)
    {
        node = iter;
        iter = iter->Next;
        fifo_allocator_return(node);
    }
    fifo->Head = fifo->Stub;
    fifo->Tail = fifo->Stub;
}

/// @summary Free internal resources allocated by an unbounded SPSC queue. It
/// is necessary to call spsc_fifo_u_init(fifo) to re-initialize the queue.
/// This function is not safe for concurrent access by multiple threads.
template <typename T>
inline void spsc_fifo_u_delete(spsc_fifo_u_t<T> *fifo)
{
    fifo->Head = NULL;
    fifo->Tail = NULL;
    if (fifo->Stub != NULL)
    {
        delete fifo->Stub;
        fifo->Stub = NULL;
    }
}

/// @summary Attempt to consume a single item from the queue (a read operation).
/// This function is safe with respect to a single concurrent produce operation.
/// @param fifo The queue to read from.
/// @param item If the consume operation returns true, the dequeued item data
/// is copied to this location and the owning node is returned to the allocator.
/// @return true if an item was consumed from the queue.
template <typename T>
inline bool spsc_fifo_u_consume(spsc_fifo_u_t<T> *fifo, T &item)
{
    fifo_node_t<T> *head = fifo->Head; // consumer-only
    fifo_node_t<T> *next = head->Next.load(std::memory_order_consume);
    if (next != NULL)
    {
        item = next->Item;             // copy node data for caller
        fifo->Head = next;             // pop the head of the queue
        fifo_allocator_return(head);   // mark previous head for reuse
        return true;
    }
    else return false;
}

/// @summary Attempt to produce a single item in the queue (a write operation).
/// This function is safe with respect to a single concurrent consume operation.
/// @param fifo The queue to write to.
/// @param node The initialize node to write to the back of the queue. This node
/// is allocated from and managed by a fifo_allocator_t<T>, and the Item field
/// of the node is expected to be fully initialized by the caller.
template <typename T>
inline void spsc_fifo_u_produce(spsc_fifo_u_t<T> *fifo, fifo_node_t<T> *node)
{
    node->Next.store(NULL, std::memory_order_relaxed);
    fifo->Tail->Next.store(node, std::memory_order_release);
    fifo->Tail = node;
}

/// @summary Initialize an unbounded MPSC queue to empty. This function is not
/// safe for concurrent access by multiple threads.
/// @param fifo The queue to initialize.
template <typename T>
inline void mpsc_fifo_u_init(mpsc_fifo_u_t<T> *fifo)
{
    fifo_node_t<T>  *stub = new fifo_node_t<T>();
    stub->Next.store(NULL,  std::memory_order_relaxed);
    stub->Foot     = NULL;
    fifo->Head.store(stub,  std::memory_order_relaxed);
    fifo->Tail.store(stub,  std::memory_order_relaxed);
    fifo->Stub     = stub;
}

/// @summary Removes all items from the queue. It is not necessary to clear the
/// queue prior to delete. This function is not safe for concurrent access.
/// @param fifo The queue to flush.
template <typename T>
inline void mpsc_fifo_u_flush(mpsc_fifo_u_t<T> *fifo)
{
    fifo_node_t<T> *node = NULL;
    fifo_node_t<T> *iter = fifo->Head.load(std::memory_order_relaxed);
    while (iter !=  NULL)
    {
        node = iter;
        iter = iter->Next;
        fifo_allocator_return(node);
    }
    fifo->Head.store(fifo->Stub, std::memory_order_relaxed);
    fifo->Tail.store(fifo->Stub, std::memory_order_relaxed);
}

/// @summary Free internal resources allocated by an unbounded MPSC queue. It
/// is necessary to call mpsc_fifo_u_init(fifo) to re-initialize the queue.
/// This function is not safe for concurrent access by multiple threads.
/// @param fifo The queue to delete.
template <typename T>
inline void mpsc_fifo_u_delete(mpsc_fifo_u_t<T> *fifo)
{
    fifo->Head.store(NULL, std::memory_order_relaxed);
    fifo->Tail.store(NULL, std::memory_order_relaxed);
    if (fifo->Stub != NULL)
    {
        delete fifo->Stub;
        fifo->Stub = NULL;
    }
}

/// @summary Attempt to consume a single item from the queue (a read operation).
/// This function is safe with respect to multiple concurrent produce operations.
/// @param fifo The queue to read from.
/// @param item If the consume operation returns true, the dequeued item data
/// is copied to this location and the owning node is returned to the allocator.
/// @return true if an item was consumed from the queue.
template <typename T>
inline bool mpsc_fifo_u_consume(mpsc_fifo_u_t<T> *fifo, T &item)
{
    fifo_node_t<T> *head = fifo->Head.load(std::memory_order_relaxed);
    fifo_node_t<T> *next = head->Next.load(std::memory_order_acquire);
    if (next != NULL)
    {
        item  = next->Item;
        fifo->Head.store(next, std::memory_order_relaxed);
        fifo_allocator_return(head); // release barrier
        return true;
    }
    else return false;
}

/// @summary Attempt to produce a single item in the queue (a write operation).
/// This function is safe with respect to a single concurrent consume operation
/// and multiple concurrent produce operations.
/// @param fifo The queue to write to.
/// @param node The initialize node to write to the back of the queue. This node
/// is allocated from and managed by a fifo_allocator_t<T>, and the Item field
/// of the node is expected to be fully initialized by the caller.
template <typename T>
inline void mpsc_fifo_u_produce(mpsc_fifo_u_t<T> *fifo, fifo_node_t<T> *node)
{
    node->Next.store(NULL, std::memory_order_relaxed);
    fifo_node_t<T> *prev = fifo->Tail.exchange(node, std::memory_order_acq_rel);
    prev->Next.store(node, std::memory_order_release);
}

/// @summary Initialize a fixed-size SPSC queue to empty. This function is not
/// safe for concurrent access by multiple threads.
/// @param fifo The queue to initialize.
/// @param capacity The queue capacity, in items. This value must be a non-zero power of two.
template <typename T>
inline void spsc_fifo_b_init(spsc_fifo_b_t<T> *fifo, size_t capacity)
{
    fifo->Size  = capacity;
    fifo->Mask  = capacity-1;
    fifo->Store = new T[capacity+1];
    fifo->Head.store(0, std::memory_order_relaxed);
    fifo->Tail.store(0, std::memory_order_relaxed);
}

/// @summary Reset a bounded SPSC queue to empty. This function is not safe for
/// concurrent access by multiple threads.
/// @param fifo The queue to flush.
template <typename T>
inline void spsc_fifo_b_flush(spsc_fifo_b_t<T> *fifo)
{
    fifo->Head.store(0, std::memory_order_relaxed);
    fifo->Tail.store(0, std::memory_order_relaxed);
}

/// @summary Free internal resources allocated by a bounded SPSC queue. It
/// is necessary to call spsc_fifo_u_init(fifo) to re-initialize the queue.
/// This function is not safe for concurrent access by multiple threads.
/// @param fifo The queue to delete.
template <typename T>
inline void spsc_fifo_b_delete(spsc_fifo_b_t<T> *fifo)
{
    if (fifo->Store != NULL)
    {
        delete[] fifo->Store;
        fifo->Store = NULL;
    }
    fifo->Size = 0;
    fifo->Mask = 0;
    fifo->Head.store(0, std::memory_order_relaxed);
    fifo->Tail.store(0, std::memory_order_relaxed);
}

/// @summary Attempt to consume a single item from the queue (a read operation).
/// This function is safe with respect to a single concurrent produce operation.
/// @param fifo The queue to read from.
/// @param item If the consume operation returns true, the dequeued item data is copied to this location.
/// @return true if an item was consumed from the queue, or false if the queue is empty.
template <typename T>
inline bool spsc_fifo_b_consume(spsc_fifo_b_t<T> *fifo, T &item)
{
    size_t const mask = fifo->Mask;
    size_t const head = fifo->Head.load(std::memory_order_relaxed);
    if (((fifo->Tail.load(std::memory_order_acquire) - head) & mask) >= 1)
    {
        item = fifo->Store[head & mask];
        fifo->Head.store(head + 1, std::memory_order_release);
        return true;
    }
    else return false;
}

/// @summary Attempt to produce a single item in the queue (a write operation).
/// This function is safe with respect to a single concurrent consume operation.
/// @param fifo The queue to write to.
/// @param item The item to write to the back of the queue.
/// @return true if the item was written to the queue, or false if the queue is full.
template <typename T>
inline bool spsc_fifo_b_produce(spsc_fifo_b_t<T> *fifo, T const &item)
{
    size_t const mask = fifo->Mask;
    size_t const tail = fifo->Tail.load(std::memory_order_relaxed);
    if (((fifo->Head.load(std::memory_order_acquire) - (tail + 1)) & mask) >= 1)
    {
        fifo->Store[tail & mask] = item;
        fifo->Tail.store(tail + 1, std::memory_order_release);
        return true;
    }
    else return false;
}

#undef  PLATFORM_ATOMIC_FIFO_DEFINED
#define PLATFORM_ATOMIC_FIFO_DEFINED
#endif /* !defined(ATOMIC_FIFO_H) */
