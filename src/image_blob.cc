/*/////////////////////////////////////////////////////////////////////////////
/// @summary Defines constants, types and functions for managing blobs of 
/// image data. Image data is stored in buckets, one for each DXGI format.
/// When the first image is added to a bucket, it reserves a block of virtual 
/// address space, which is committed on demand. There is one image blob for 
/// each supported image encoding, and the blob maintains image usage 
/// information. The image blob can function as a cache for image data.
///////////////////////////////////////////////////////////////////////////80*/

/*////////////////
//   Includes   //
////////////////*/

/*////////////////////
//   Preprocessor   //
////////////////////*/

/*/////////////////
//   Constants   //
/////////////////*/

/*///////////////////
//   Local Types   //
///////////////////*/
// What do we want to do here?
// We can load the image data from disk/network and parse the file format.
// Now we need to do something with the pixel data.
// The pixel data is being received on a parser thread. The ultimate goal 
// is to get it to the screen, but what's the process to get there? We have
// some pixel data, which is possibly compressed. We may want to decompress
// it, or we may want to transcode it to some other compressed format. Either
// way, we want to store it in system memory so it doesn't have to be re-loaded
// from disk, which is slow. If necessary, the image can then be uploaded to 
// a device memory for further processing. So I think that, at this stage, we 
// are concerned with system memory storage only. Really, this is somewhat 
// necessary, because pixel data may be provided in small chunks, and we don't
// want to be trying to write to device memory like that.
//
// Each image has a known storage format, so we basically have one blob of 
// address space for each storage format.
//
// Need to handle the case where not enough system memory is available, so 
// one level of the system can function like a LRU cache. Since it may not
// be possible to load individual frames (think MPEG data source), this 
// operates at whole-image granularity. 
// 
// Because there needs to be some way to re-load an evicted image on-demand, 
// something needs to maintain a table mapping application-defined identifier 
// to file load parameters. This would seem to be the ideal way to load an 
// image - in one step, the application specifies an ID and load parameters 
// to this table, which does not load the image, but rather makes it "known".
// Then, as a second step, the application requests the image by ID from a 
// particular cache, which triggers the actual load if necessary. These two 
// steps can be performed on separate threads.
//
// Need to be able to look up an image by its application-defined identifier.
// - Check whether it is loaded.
// - Lock its position in memory and return a pointer to the data, or a status:
//   - The image might not be 'available' right now, because maybe it has to be
//     re-loaded into the cache.
// - Unlock it in memory, signalling that you're done with the image data, and it
//   can be relocated or evicted.
// - Maybe want these operations to be performed as a request/response queue style.
//   That is, I want to lock an image for display or transfer to device memory, so 
//   I submit a lock request to the cache command queue, and tell it what queue to 
//   notify when the request has completed (that is, the data is available.) If 
//   necessary, the cache will submit the load request to the VFS, wait for decode
//   and parse to complete, and then complete the parent request. This allows anyone
//   to request some image data (it was evicted from display memory and needs to be
//   re-loaded, it was already loaded and the CPU needs access to it, etc.)
// - This queue-based architecture also implies that the image loading functionality
//   is driven by the cache layer as part of its update tick! That is, during the 
//   update tick, it is the cache layer that creates and manages the parser state, 
//   consumes the pixel data and performs the encoding. This solves a large number of
//   threading issues (or does it?) as all writes to the image cache table are 
//   guaranteed to be performed on the same thread as decode->parse->encode. If there 
//   are multiple supported image encodings, it's possible to for example run raw on 
//   one thread/core, while running JPEG/RLE on another core. 
// 
// image_registry_t: The table mapping image ID to load params. SRWLock protecting table.
//                   - Also maintain what cache the image is loaded into. Shouldn't just
//                     identify by image encoding, though that would be easiest.
// image_cache_t   : Maintains blobs, table mapping image ID to blob, LRU state, create encoder(s). One per-encoding.
// image_blob_t    : Manages block of address space, maintains entry list. One per-format.
// image_entry_t   : Stores metadata for a single logical image.

// TODO(rlk):
// Need to figure out how to expose encoding to the parser.
// The parser doesn't need to know about image_cache_t etc. it only needs to know about the image_encoder_t.
// Define the image_encoder_t in image_defs.cc, move image_blob.cc to after the parser includes.
// The image_cache_t needs to know about all of the parser implementations.
// Move the parser list stuff from image_load.cc to this file.
// Delete image_load.cc as it is no longer needed.
// Have a function to switch on image_encoding_e and instantiate the correct image_encoder_t.
// Need to be able to support multi-frame images, which have a uintptr_t ImageId and a FrameIndex+FrameCount.
// That covers the loading of images. Then need to add support for the cache functionality.
// The image_blob_t should be possible to implement without image_cache_t or any of the request stuff
//   as it is only responsible for reserving and committing memory (basically it's just a stack allocator).

/*///////////////
//   Globals   //
///////////////*/

/*///////////////////////
//   Local Functions   //
///////////////////////*/

/*////////////////////////
//   Public Functions   //
////////////////////////*/
