/*/////////////////////////////////////////////////////////////////////////////
/// @summary Defines the types and functions for working with a list of 
/// commands to be send to a presentation driver. The command types are not 
/// driver specific. Each command list is safe for access from a single thread
/// only, but multiple threads can each generate command lists and submit them
/// to the same presentation driver instance concurrently.
///////////////////////////////////////////////////////////////////////////80*/

/*////////////////
//   Includes   //
////////////////*/
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

#include "intrinsics.h"
#include "atomic_fifo.h"

/*/////////////////
//   Constants   //
/////////////////*/
/// @summary Define the maximum number of command lists per command queue.
static size_t const PR_COMMAND_LIST_QUEUE_MAX              = 16;

/// @summary Define the allocation granularity for command list command data.
static size_t const PR_COMMAND_LIST_ALLOCATION_GRANULARITY = 64 * 1024;

/*///////////////////
//   Local Types   //
///////////////////*/
/// @summary Forward declae the structure representing a command list.
struct  pr_command_list_t;

/// @summary Define aliases for types used internally by a command list queue.
typedef fifo_allocator_t<pr_command_list_t*> cmdq_alloc_t;
typedef mpsc_fifo_u_t<pr_command_list_t*>    cmdq_queue_t;

/// @summary Define the set of recognized command identifiers.
enum pr_command_type_e : uint16_t
{
    /// @summary A no-op command with no data.
    PR_COMMAND_NO_OP                = 0,
    /// @summary Clear the color buffer to a specified color.
    /// @param bgra_color A 32-bit unsigned integer value specifying the BGRA clear color.
    PR_COMMAND_CLEAR_COLOR_BUFFER   = 1,
};

/// @summary Defines the structure of a single presentation command. Each command consists
/// of a 32-bit header specifying the command identifier and data size, followed by data 
/// specific to that type of command.
struct pr_command_t
{
    uint16_t           CommandId;             /// The command identifier, one of pr_command_type_e.
    uint16_t           DataSize;              /// The size of the command data, up to 64KB.
    uint8_t            Data[1];               /// The command data, up to 64KB in length.
};

/// @summary Define the data associated with the command list.
struct pr_command_list_t
{
    size_t             BytesTotal;            /// The number of bytes allocated for this command list.
    size_t             BytesUsed;             /// The number of bytes actually used for this command list.
    size_t             CommandCount;          /// The number of buffered commands.
    uint8_t           *CommandData;           /// The allocated memory block.
};

/// @summary Define the data associated with a MPSC FIFO queue of command lists. 
/// Each command list can be populated by a thread and then submitted to the queue.
struct pr_command_queue_t
{
    #define N  PR_COMMAND_LIST_QUEUE_MAX
    cmdq_alloc_t       CommandListAlloc;      /// The FIFO node allocator for the queue.
    cmdq_queue_t       CommandListQueue;      /// The MPSC queue of submitted command lists.
    size_t             ListFreeCount;         /// The number of valid items in the free list.
    pr_command_list_t *CommandListFree [N];   /// Pointers into CommandListStore representing available lists.
    pr_command_list_t  CommandListStore[N];   /// Actual storage for all of the command lists.
    #undef  N
};

/*///////////////
//   Globals   //
///////////////*/

/*///////////////////////
//   Local Functions   //
///////////////////////*/

/*///////////////////////
//  Public Functions   //
///////////////////////*/
/// @summary Initialize a command list to empty, with no memory allocated.
/// @param cl The command list to initialize.
public_function void pr_command_list_init(pr_command_list_t *cl)
{
    cl->BytesTotal   = 0;
    cl->BytesUsed    = 0;
    cl->CommandCount = 0;
    cl->CommandData  = NULL;
}

/// @summary Clear a command list without allocating or freeing any memory.
/// @param cl The command list to clear.
public_function void pr_command_list_clear(pr_command_list_t *cl)
{
    cl->BytesUsed    = 0;
    cl->CommandCount = 0;
}

/// @summary Frees all memory allocated to a command list and re-initializes it to empty.
/// @param cl The command list to delete.
public_function void pr_command_list_free(pr_command_list_t *cl)
{
    if (cl->BytesTotal > 0 && cl->CommandData != NULL)
    {
        free(cl->CommandData);
    }
    cl->BytesTotal   = 0;
    cl->BytesUsed    = 0;
    cl->CommandCount = 0;
    cl->CommandData  = NULL;
}

/// @summary Reserves memory for a single command.
/// @param cl The command list to allocate from.
/// @param total_size The number of bytes to allocate for the command, including the 32-bit header.
/// @return A pointer to the newly allocated command, or NULL.
public_function pr_command_t* pr_command_list_allocate(pr_command_list_t *cl, size_t total_size)
{
    if (cl->BytesUsed + total_size > cl->BytesTotal)
    {   // allocate additional memory for command data.
        size_t new_size = align_up(cl->BytesTotal + total_size, PR_COMMAND_LIST_ALLOCATION_GRANULARITY);
        void  *new_mem  = realloc (cl->CommandData, new_size);
        if (new_mem == NULL) return NULL;
        cl->BytesTotal  = new_size;
        cl->CommandData = (uint8_t*) new_mem;
    }
    // save a pointer to the newly allocated command:
    pr_command_t *cmd = (pr_command_t*) &cl->CommandData[cl->BytesUsed];
    // bump up the number of bytes used and command count:
    cl->BytesUsed    += total_size;
    cl->CommandCount++;
    // initialize the new command to a no-op:
    cmd->CommandId = PR_COMMAND_NO_OP;
    cmd->DataSize  = 0;
    return cmd;
}

/// @summary Initialize a command list submission queue to empty.
/// @param fifo The command list submission queue to initialize.
public_function void pr_command_queue_init(pr_command_queue_t *fifo)
{   // initialize and add all command lists to the free set.
    fifo->ListFreeCount  = 0;
    for (size_t i = 0, n = PR_COMMAND_LIST_QUEUE_MAX; i < n; ++i)
    {
        pr_command_list_init(&fifo->CommandListStore[i]);
        fifo->CommandListFree[fifo->ListFreeCount++] = &fifo->CommandListStore[i];
    }
    // initialize the allocator and FIFO used for command list submission.
    mpsc_fifo_u_init(&fifo->CommandListQueue);
    fifo_allocator_init(&fifo->CommandListAlloc);
}

/// @summary Clear a command list submission queue. The caller is responsible for 
/// ensuring that no command lists remain outstanding and owned by the client.
/// @param fifo The command list submission queue to clear.
public_function void pr_command_queue_clear(pr_command_queue_t *fifo)
{
    // return all command lists to free status. no storage is freed.
    fifo->ListFreeCount  = 0;
    for (size_t i = 0, n = PR_COMMAND_LIST_QUEUE_MAX; i < n; ++i)
    {
        pr_command_list_clear(&fifo->CommandListStore[i]);
        fifo->CommandListFree[ fifo->ListFreeCount++] = &fifo->CommandListStore[i];
    }
    // flush the queue of submitted command lists.
    mpsc_fifo_u_flush(&fifo->CommandListQueue);
}

/// @summary Submits a command list for later processing.
/// @param fifo The command list submission queue.
/// @param list The populated command list to submit. 
public_function void pr_command_queue_submit(pr_command_queue_t *fifo, pr_command_list_t *list)
{
    fifo_node_t<pr_command_list_t*> *node = fifo_allocator_get(&fifo->CommandListAlloc);
    node->Item = list;
    mpsc_fifo_u_produce(&fifo->CommandListQueue, node);
}

/// @summary Return a command list to the free list, making it available for use.
/// @param fifo The command list submission queue that owns the command list.
/// @param list The command list being freed.
public_function void pr_command_queue_return(pr_command_queue_t *fifo, pr_command_list_t *list)
{
    fifo->CommandListFree[fifo->ListFreeCount++] = list;
}

/// @summary Retrieve the next unused command list from the free list.
/// @param fifo The command list submission queue to allocate from.
/// @return An available command list, or NULL.
public_function pr_command_list_t* pr_command_queue_next_available(pr_command_queue_t *fifo)
{
    if (fifo->ListFreeCount > 0)
    {   // retrieve the command list from the free list.
        pr_command_list_t *list = fifo->CommandListFree[--fifo->ListFreeCount];
        pr_command_list_clear(list);
        return list;
    }
    else return NULL;
}

/// @summary Retrieve the next waiting populated command list.
/// @param fifo The command list submission queue to query.
/// @return The next waiting command list, or NULL.
public_function pr_command_list_t* pr_command_queue_next_submitted(pr_command_queue_t *fifo)
{
    pr_command_list_t *list = NULL;
    mpsc_fifo_u_consume(&fifo->CommandListQueue, list);
    return list;
}

/// @summary Frees all resources associated with a command list submission queue.
/// @param fifo The command list submission queue to delete.
public_function void pr_command_queue_delete(pr_command_queue_t *fifo)
{   // free storage associated with the individual command lists.
    for (size_t i = 0, n = PR_COMMAND_LIST_QUEUE_MAX; i < n; ++i)
    {
        pr_command_list_free(&fifo->CommandListStore[i]);
        fifo->CommandListFree[i]  = NULL;
    }
    fifo->ListFreeCount = 0;
    // flush the queue of submitted command lists and free allocated nodes.
    mpsc_fifo_u_delete(&fifo->CommandListQueue);
    fifo_allocator_reinit(&fifo->CommandListAlloc);
}
