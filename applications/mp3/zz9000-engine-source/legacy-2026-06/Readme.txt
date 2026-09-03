Structure
---------
See also Structure.txt

The basis on the 68k side is a shared library with a minimum set of functions.
The library accesses a module via a standardised interface (defined in iEngine.h) 
which forwards the function calls to the coprocessor side.
An interface iEngine_xxx.c must therefore be implemented for each variant.

The basis on the coprocessor side is a handler that receives the function calls from the 
68k side and, in turn, forwards them as necessary to functions in fifo.c, 
amp.c or future decoders.

Data exchange preferably takes place via shared memory 
(shared memory).

The shared memory can be provided either by the decoder or by the application 
. If the decoder provides the shared memory, 
it takes priority and must be used by the application.

If the coprocessor provides the shared memory, the 
application is responsible for enclosing the code block {set equaliser, call decoder, 
process returned data} within an Engine_obtain() /
Enginerelease() pair.

If the coprocessor cannot share its memory with the 68k side, routines must be 
incorporated into the interface implementation that transfer the memory contents 
back and forth between the 68k and coprocessor sides 
by other means (e.g. register accesses).

How it works
------------
A decoder for compressed audio typically operates with a variable 
input bitrate and a fixed output bitrate.

The function call therefore usually produces a predictable length of 
output, but requires an input length that is only partially (within certain limits) 
predictable.

If the decoder now decodes from buffer to buffer, we need 
a FIFO for the input. This FIFO must be filled with sufficient 
data before the decoding call and contains a remainder of 
data not (yet) processed at the end of the decoding call.

In order to be able to decode multiple streams in parallel if necessary, a FIFO is required for each decoder 
instance.

Examples
--------
 * m68k-amigaos.engine
   Everything in a single shared library; fifo and amp can be called directly.

 * ppc-warpos.engine
   68k side in a shared library, which passes operations via AmigaOS 
   messaging to a running WarpOS task
