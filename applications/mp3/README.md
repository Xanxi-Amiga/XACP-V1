# XACP MP3 Playback

This directory contains the first practical multimedia application developed on top of the XACP architecture.

The current implementation includes :

* ARM-side MP3 decoding
* Shared DDR streaming
* AHI playback integration
* Streaming refill/backpressure handling
* Practical ARM compute offloading from the Amiga 68060
* Dedicated GUI player ZZPlayGUI
* mpega.library for XACP, new XACP-enabled implementation developed from scratch using standard GCC/libnix development tool
* XACP zz9000.engine for AmigaAMP developped with the knowledge and interface information kindly provided by Tom Wenzel 

The XX16c firmware branch significantly reduced 68060 CPU usage compared to early implementations while improving playback stability.

Current work focuses on:

* mpega.library compatibility improvements
* Future DSP/equalizer support
