/*/////////////////////////////////////////////////////////////////////////////
/// @summary Defines the interface to and identifiers of all available
/// graphics pipelines, which perform image processing operations using a 
/// standard graphics API (GDI, OpenGL, Direct3D, etc.) and actually perform 
/// image display operations. Each graphics API provides its own implementation
/// in a separate file (gpipeline_gdi.cc, gpipeline_gl2.cc, etc.)
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
// TODO(rlk): Each graphics pipeline has a known ID.
// Items in the presentation command list reference the pipeline using this ID.
// As part of the presentation command indicating that the pipeline should be executed, 
// any data is specified. The graphics pipeline receives a resolved presentation 
// command list (that is, the command list after all compute pipeline operations have 
// been completed and results have been specified.) It then sets up all pipeline state
// (any constant data, etc. is specified as part of the draw command in the presentation
// command list) and shaders and generates API draw calls.

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
