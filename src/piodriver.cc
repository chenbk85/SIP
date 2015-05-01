/*/////////////////////////////////////////////////////////////////////////////
/// @summary Defines the constants, types and functions that implement the 
/// prioritized I/O driver. The prioritized I/O driver is responsible for 
/// the high-level coordination logic for different I/O types, and for pushing
/// a prioritized stream of I/O commands to the asynchronous I/O driver.
///////////////////////////////////////////////////////////////////////////80*/

/*////////////////////
//   Preprocessor   //
////////////////////*/

/*////////////////
//   Includes   //
////////////////*/

/*/////////////////
//   Constants   //
/////////////////*/

/*///////////////////
//   Local Types   //
///////////////////*/
/// @summary Maintains all of the state for the prioritized I/O driver.
struct pio_driver_t
{
    aio_driver_t *aio_d;   /// The asynchronous I/O driver interface.
};

/*///////////////
//   Globals   //
///////////////*/

/*///////////////////////
//   Local Functions   //
///////////////////////*/
/// @summary Implements the per-tick entry point for the prioritized I/O driver.
/// Ultimately, this builds a priority queue of pending I/O operations and pushes
/// them down to the asynchronous I/O driver for processing.
/// @param driver The prioritized I/O driver state.
/// @return Zero to continue running, or non-zero if a shutdown signal was received.
internal_function int pio_driver_main(pio_driver_t *driver)
{
    return 0;
}

/*////////////////////////
//   Public Functions   //
////////////////////////*/
/// @summary Allocates resources for a prioritized I/O driver.
/// @param driver The prioritized I/O driver state to initialize.
/// @param aio The asynchronous I/O driver to use for asynchronous operations.
/// @return ERROR_SUCCESS on success, or an error code returned by GetLastError() on failure.
public_function DWORD pio_driver_open(pio_driver_t *driver, aio_driver_t *aio)
{
    driver->aio_d = aio;
    return ERROR_SUCCESS;
}

/// @summary Closes a prioritized I/O driver and frees associated resources.
/// @param driver The prioritized I/O driver state to clean up.
public_function void pio_driver_close(pio_driver_t *driver)
{
    driver->aio_d = NULL;
}

/// @summary Executes a single AIO driver update in polling mode. The kernel
/// AIO system will be polled for completed events without blocking. This
/// dispatches completed command results to their associated target queues and
/// launches as many not started commands as possible.
/// @param driver The AIO driver state to poll.
public_function void pio_driver_poll(pio_driver_t *driver)
{
    pio_driver_main(driver);
}
