#ifndef HTON_H
#define HTON_H

#include <stdint.h>

#if defined(BYTE_ORDER_IS_LITTLE_ENDIAN)
	#ifndef HTONS
		#define HTONS(x) ((uint16_t) (((uint16_t) (x) << 8) | ((uint16_t) (x) >> 8)))
	#endif
	#ifndef HTONL
		#define HTONL(x) ((uint32_t) (((uint32_t) HTONS(x) << 16) | HTONS((uint32_t) (x) >> 16)))
	#endif
	#ifndef HTONF
		#define HTONF(x) ((float) (((uint32_t) HTONS(x) << 16) | HTONS((uint32_t) (x) >> 16)))
	#endif
	#ifndef NTOHS
		#define NTOHS(x) ((uint16_t) (((uint16_t) (x) << 8) | ((uint16_t) (x) >> 8)))
	#endif
	#ifndef NTOHL
		#define NTOHL(x) ((uint32_t) (((uint32_t) NTOHS(x) << 16) | NTOHS((uint32_t) (x) >> 16)))
	#endif
	#ifndef NTOHF
		#define NTOHF(x) ((float) (((uint32_t) NTOHS(x) << 16) | NTOHS((uint32_t) (x) >> 16)))
	#endif
	#ifndef NTOHP
		#define NTOHP(x) ((void*) (((uint32_t) NTOHS(x) << 16) | NTOHS((uint32_t) (x) >> 16)))
	#endif
#elif defined(BYTE_ORDER_IS_BIG_ENDIAN)
	#ifndef HTONS
		#define HTONS(x) ((uint16_t) (x))
	#endif
	#ifndef HTONL
		#define HTONL(x) ((uint32_t) (x))
	#endif
	#ifndef HTONF
		#define HTONF(x) ((float) (x))
	#endif
	#ifndef NTOHS
		#define NTOHS(x) ((uint16_t) (x))
	#endif
	#ifndef NTOHL
		#define NTOHL(x) ((uint32_t) (x))
	#endif
	#ifndef NTOHF
		#define NTOHF(x) ((float) (x))
	#endif
	#ifndef NTOHP
		#define NTOHP(x) ((void*) (x))
	#endif
#else
	#error No byte order defined!
#endif


#endif /* HTON_H */

