/*/////////////////////////////////////////////////////////////////////////////
/// @summary Defines the interface to and identifiers of all available compute
/// pipelines, which perform transformations on input image data using OpenCL.
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
// TODO(rlk): Each compute pipeline has a known ID.
// Items in the presentation command list reference the pipeline using this ID.
// As part of the presentation command indicating that the pipeline should be executed, 
// any data is specified. The presentation layer receives the submitted command list 
// and reads all commands. Each pipeline command gets submitted to the appropriate 
// pipeline execution queue (SPSC(/LPLC?) maintained by each pipeline) and a pending 
// completion token is saved. The presentation layer then saves a reference to the 
// presentation command list along with all of the tokens in a list somewhere.
// During each present call, it polls the pipeline output queues and updates the 
// status of the corresponding completion tokens. When all completion tokens have 
// been signaled, the presentation command list is dequeued and executed.

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
