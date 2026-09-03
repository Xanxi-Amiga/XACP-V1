
#ifndef ENGINE_H__INCLUDED
#define ENGINE_H__INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

enum {
	ENGINECAP_FORMATS
};

enum {
	ENGINEOP_START = 0,
	ENGINEOP_STOP,
	ENGINEOP_GET_MEMORY,

	ENGINEOP_INIT_FIFO = 10,
	ENGINEOP_EXIT_FIFO,
	ENGINEOP_CLEAR_FIFO,
	ENGINEOP_FILL_FIFO,

	ENGINEOP_INIT_EQUALIZER = 20,
	ENGINEOP_EXIT_EQUALIZER,
	ENGINEOP_CONFIG_EQUALIZER,
	ENGINEOP_SET_EQUALIZER,
	ENGINEOP_RUN_EQUALIZER,


	ENGINEOP_INIT_AMP = 30,
	ENGINEOP_EXIT_AMP,
	ENGINEOP_DECODE_AMP
};

/*
** Used to query pointers to shared memory.
** If the corresponding pointer is non-zero
** then the Engine provides the shared memory
** and you have to use it.
** If the corresponding pointer is zero then
** you have to allocate the shared memory yourself.
*/
struct SharedMemoryParameters {
	unsigned long   NumFrames;  // out: number of frames in DstBuffer (1152 samples), SpecRawL and SpecRawR (576 values)
	unsigned char*  DstBuffer;  // out: pointer to buffer where decoded data will be written to
	unsigned short* SpecRawL;   // out: pointer to buffer where left raw spectral data will be written to
	unsigned short* SpecRawR;   // out: pointer to buffer where right raw spectral data will be written to
	float*          EqualizerL; // out: pointer to buffer where left equalizer parameters are stored
	float*          EqualizerR; // out: pointer to buffer where right equalizer parameters are stored
};

struct InitAmpParameters {
	void*           itsFifo;    // in: private fifo pointer in arbitrary byte order
	unsigned long   Delay;
	unsigned long   Padding;
};

struct ExitAmpParameters {
	void*           itsAmp;     // in: private amp pointer in arbitrary byte order
};

struct InitFifoParameters {
 	unsigned long   Size;       // out: FIFO size
};

struct ExitFifoParameters {
	void*           itsFifo;    // in: private fifo pointer in arbitrary byte order
};

struct ClearFifoParameters {
	void*           itsFifo;    // in: private fifo pointer in arbitrary byte order
 	unsigned long   Size;       // out: FIFO size
};

struct FillFifoParameters {
	void*           itsFifo;    // in: private fifo pointer in arbitrary byte order
	unsigned char*  SrcBuffer;  // in: pointer to buffer to fill into FIFO
	unsigned long   NumBytes;   // in: number of bytes to fill into FIFO
};


struct InitEqualizerParameters {
	unsigned long   Dummy;
};

struct ExitEqualizerParameters {
	void*           itsEqualizer;  // in: private equalizer pointer in arbitrary byte order
};

struct ConfigEqualizerParameters {
	void*           itsEqualizer;  // in: private equalizer pointer in arbitrary byte order
	unsigned long   new_channels;
	unsigned long   new_rate;
};

struct SetEqualizerParameters {
	void*           itsEqualizer;  // in: private equalizer pointer in arbitrary byte order
	float*          bands;
};

struct RunEqualizerParameters {
	void*           itsEqualizer;  // in: private equalizer pointer in arbitrary byte order
	short*          data;
	unsigned long   samples;
};

struct DecodeParameters {
	void*           itsAmp;        // in: private amp pointer in arbitrary byte order
	unsigned long   NumFrames;     // in: number of mpeg frames per visualisation frame
	unsigned long   VisFrames;     // in: number of visualisation frames
	unsigned short  Layer;         // in: mpeg layer (2 or 3)
	unsigned short  Channels;      // in: channels (1 or 2)
	unsigned long   DecodeEOF;     // out: end of file while decoding
	unsigned char*  DstBuffer;     // in: pointer to buffer where decoded data will be written to
	float*          EqualizerL;    // in: pointer to buffer where left equalizer parameters are stored
	float*          EqualizerR;    // in: pointer to buffer where right equalizer parameters are stored
	unsigned short* SpecRawL;      // in: pointer to buffer where left raw spectral data will be written to
	unsigned short* SpecRawR;      // in: pointer to buffer where right raw spectral data will be written to
};




#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
