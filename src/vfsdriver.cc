/*/////////////////////////////////////////////////////////////////////////////
/// @summary Defines the constants, types and functions that implement the 
/// virtual file system driver. The virtual file system driver abstracts access
/// to the underlying file system, allowing files to be loaded from a local 
/// disk, remote disk or archive file (zip file, tar file, etc.) The VFS driver
/// provides a consistent and friendly interface to the application. All I/O, 
/// with a few exceptions for utility functions, is performed asynchronously. 
/// The virtual file system driver generates prioritized I/O driver commands 
/// and is responsible for creating stream decoders based on the underlying 
/// file system or archive type. These stream decoders will receive I/O results
/// from the asynchronous I/O driver and are returned directly to the caller, 
/// who may poll the decoder for data on a separate thread.
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

/*///////////////
//   Globals   //
///////////////*/

/*///////////////////////
//   Local Functions   //
///////////////////////*/

/*////////////////////////
//   Public Functions   //
////////////////////////*/

