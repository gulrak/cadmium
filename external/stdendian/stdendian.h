#pragma once

/*
 * stdendian.h
 *
 * Declared public domain at https://gist.github.com/michaeljclark/3b4fd912f6fa8bb598b3
 * new home at: https://github.com/michaeljclark/stdendian
 *
 * This header defines the following endian macros as defined here:
 *
 *   http://austingroupbugs.net/view.php?id=162
 *
 *   BYTE_ORDER         this macro shall have a value equal to one
 *                      of the *_ENDIAN macros in this header.
 *   LITTLE_ENDIAN      if BYTE_ORDER == LITTLE_ENDIAN, the host
 *                      byte order is from least significant to
 *                      most significant.
 *   BIG_ENDIAN         if BYTE_ORDER == BIG_ENDIAN, the host byte
 *                      order is from most significant to least
 *                      significant.
 *
 * This header also defines several byte-swap interfaces, some that
 * map directly to the host byte swap intrinsics and some sensitive
 * to the host endian representation, performing a swap only if the
 * host representation differs from the chosen representation.
 *
 * Direct byte swapping interfaces:
 *
 *   uint16_t bswap16(uint16_t x); (* swap bytes 16-bit word *)
 *   uint32_t bswap32(uint32_t x); (* swap bytes 32-bit word *)
 *   uint64_t bswap64(uint64_t x); (* swap bytes 64-bit word *)
 *
 * Simplified host endian interfaces:
 *
 *   uint16_t be16(uint16_t x); (* big-endian representation 16-bit word *)
 *   uint32_t be32(uint32_t x); (* big-endian representation 32-bit word *)
 *   uint64_t be64(uint64_t x); (* big-endian representation 64-bit word *)
 *
 *   uint16_t le16(uint16_t x); (* little-endian representation 16-bit word *)
 *   uint32_t le32(uint32_t x); (* little-endian representation 32-bit word *)
 *   uint64_t le64(uint64_t x); (* little-endian representation 64-bit word *)
 *
 * BSD host endian interfaces:
 *
 *   uint16_t htobe16(uint16_t x) { return be16(x); }
 *   uint16_t htole16(uint16_t x) { return le16(x); }
 *   uint16_t be16toh(uint16_t x) { return be16(x); }
 *   uint16_t le16toh(uint16_t x) { return le16(x); }
 *
 *   uint32_t htobe32(uint32_t x) { return be32(x); }
 *   uint32_t htole32(uint32_t x) { return le32(x); }
 *   uint32_t be32toh(uint32_t x) { return be32(x); }
 *   uint32_t le32toh(uint32_t x) { return le32(x); }
 *
 *   uint64_t htobe64(uint64_t x) { return be64(x); }
 *   uint64_t htole64(uint64_t x) { return le64(x); }
 *   uint64_t be64toh(uint64_t x) { return be64(x); }
 *   uint64_t le64toh(uint64_t x) { return le64(x); }
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Linux */
#if defined(__linux__) || defined(__GLIBC__)
#include <endian.h>
#include <byteswap.h>
#define __ENDIAN_DEFINED
#define __BSWAP_DEFINED
#define __HOSTSWAP_DEFINED
#define _BYTE_ORDER             __BYTE_ORDER
#define _LITTLE_ENDIAN          __LITTLE_ENDIAN
#define _BIG_ENDIAN             __BIG_ENDIAN
#define bswap16(x)              bswap_16(x)
#define bswap32(x)              bswap_32(x)
#define bswap64(x)              bswap_64(x)
#endif /* __linux__ || __GLIBC__ */

/* BSD */
#if defined(__FreeBSD__) || defined(__NetBSD__) || \
    defined(__DragonFly__) || defined(__OpenBSD__)
#include <sys/endian.h>
#define __ENDIAN_DEFINED
#define __BSWAP_DEFINED
#define __HOSTSWAP_DEFINED
#endif /* BSD */

/* Apple */
#if defined(__APPLE__)
#include <machine/endian.h>
#include <libkern/OSByteOrder.h>
#define __ENDIAN_DEFINED
#define __BSWAP_DEFINED
#define _BYTE_ORDER             BYTE_ORDER
#define _LITTLE_ENDIAN          LITTLE_ENDIAN
#define _BIG_ENDIAN             BIG_ENDIAN
#define bswap16(x) OSSwapInt16(x)
#define bswap32(x) OSSwapInt32(x)
#define bswap64(x) OSSwapInt64(x)
#endif /* Apple */

/* Windows */
#if defined(_WIN32) || defined(_MSC_VER)
/* assumes all Microsoft targets are little endian */
#include <stdlib.h>
#define _LITTLE_ENDIAN          1234
#define _BIG_ENDIAN             4321
#define _BYTE_ORDER             _LITTLE_ENDIAN
#define __ENDIAN_DEFINED
#define __BSWAP_DEFINED
static inline uint16_t bswap16(uint16_t x) { return _byteswap_ushort(x); }
static inline uint32_t bswap32(uint32_t x) { return _byteswap_ulong(x); }
static inline uint64_t bswap64(uint64_t x) { return _byteswap_uint64(x); }
#endif /* Windows */

/* OpenCL */
#if defined (__OPENCL_VERSION__)
#define _LITTLE_ENDIAN          1234
#define _BIG_ENDIAN             4321
#if defined (__ENDIAN_LITTLE__)
#define _BYTE_ORDER             _LITTLE_ENDIAN
#else
#define _BYTE_ORDER             _BIG_ENDIAN
#endif
#define bswap16(x)              as_ushort(as_uchar2(ushort(x)).s1s0)
#define bswap32(x)              as_uint(as_uchar4(uint(x)).s3s2s1s0)
#define bswap64(x)              as_ulong(as_uchar8(ulong(x)).s7s6s5s4s3s2s1s0)
#define __ENDIAN_DEFINED
#define __BSWAP_DEFINED
#endif

/* For everything else, use the compiler's predefined endian macros */
#if !defined (__ENDIAN_DEFINED) && defined (__BYTE_ORDER__) && \
    defined (__ORDER_LITTLE_ENDIAN__) && defined (__ORDER_BIG_ENDIAN__)
#define __ENDIAN_DEFINED
#define _BYTE_ORDER             __BYTE_ORDER__
#define _LITTLE_ENDIAN          __ORDER_LITTLE_ENDIAN__
#define _BIG_ENDIAN             __ORDER_BIG_ENDIAN__
#endif

/* No endian macros found */
#ifndef __ENDIAN_DEFINED
#error Could not determine CPU byte order
#endif

/* POSIX - http://austingroupbugs.net/view.php?id=162 */
#ifndef BYTE_ORDER
#define BYTE_ORDER              _BYTE_ORDER
#endif
#ifndef LITTLE_ENDIAN
#define LITTLE_ENDIAN           _LITTLE_ENDIAN
#endif
#ifndef BIG_ENDIAN
#define BIG_ENDIAN              _BIG_ENDIAN
#endif

/*
 * Natural to foreign endian helpers defined using bswap
 *
 * MSC can't lift byte swap expressions efficiently so we
 * define host integer swaps using explicit byte swapping.
 */

/* helps to have these function for symmetry */
static inline uint8_t le8(uint8_t x) { return x; }
static inline uint8_t be8(uint8_t x) { return x; }

#if defined(__BSWAP_DEFINED)
#if _BYTE_ORDER == _BIG_ENDIAN
static inline uint16_t be16(uint16_t x) { return x; }
static inline uint32_t be32(uint32_t x) { return x; }
static inline uint64_t be64(uint64_t x) { return x; }
static inline uint16_t le16(uint16_t x) { return bswap16(x); }
static inline uint32_t le32(uint32_t x) { return bswap32(x); }
static inline uint64_t le64(uint64_t x) { return bswap64(x); }
#endif
#if _BYTE_ORDER == _LITTLE_ENDIAN
static inline uint16_t be16(uint16_t x) { return bswap16(x); }
static inline uint32_t be32(uint32_t x) { return bswap32(x); }
static inline uint64_t be64(uint64_t x) { return bswap64(x); }
static inline uint16_t le16(uint16_t x) { return x; }
static inline uint32_t le32(uint32_t x) { return x; }
static inline uint64_t le64(uint64_t x) { return x; }
#endif

#else
#define __BSWAP_DEFINED

/*
 * Natural to foreign endian helpers using type punning
 *
 * Recent Clang and GCC lift these expressions to bswap
 * instructions. This makes baremetal code easier.
 */

static inline uint16_t be16(uint16_t x)
{
    union { uint8_t a[2]; uint16_t b; } y = {
        .a = { (uint8_t)(x >> 8), (uint8_t)(x) }
    };
    return y.b;
}

static inline uint16_t le16(uint16_t x)
{
    union { uint8_t a[2]; uint16_t b; } y = {
        .a = { (uint8_t)(x), (uint8_t)(x >> 8) }
    };
    return y.b;
}

static inline uint32_t be32(uint32_t x)
{
    union { uint8_t a[4]; uint32_t b; } y = {
        .a = { (uint8_t)(x >> 24), (uint8_t)(x >> 16),
              (uint8_t)(x >> 8), (uint8_t)(x) }
    };
    return y.b;
}

static inline uint32_t le32(uint32_t x)
{
    union { uint8_t a[4]; uint32_t b; } y = {
        .a = { (uint8_t)(x), (uint8_t)(x >> 8),
              (uint8_t)(x >> 16), (uint8_t)(x >> 24) }
    };
    return y.b;
}

static inline uint64_t be64(uint64_t x)
{
    union { uint8_t a[8]; uint64_t b; } y = {
        .a = { (uint8_t)(x >> 56), (uint8_t)(x >> 48),
              (uint8_t)(x >> 40), (uint8_t)(x >> 32),
              (uint8_t)(x >> 24), (uint8_t)(x >> 16),
              (uint8_t)(x >> 8), (uint8_t)(x) }
    };
    return y.b;
}

static inline uint64_t le64(uint64_t x)
{
    union { uint8_t a[8]; uint64_t b; } y = {
        .a = { (uint8_t)(x), (uint8_t)(x >> 8),
              (uint8_t)(x >> 16), (uint8_t)(x >> 24),
              (uint8_t)(x >> 32), (uint8_t)(x >> 40),
              (uint8_t)(x >> 48), (uint8_t)(x >> 56) }
    };
    return y.b;
}

/*
 * Define byte swaps using the natural endian helpers
 *
 * This method relies on the compiler lifting byte swaps.
 */
#if _BYTE_ORDER == _BIG_ENDIAN
static inline uint16_t bswap16(uint16_t x) { return le16(x); }
static inline uint32_t bswap32(uint32_t x) { return le32(x); }
static inline uint64_t bswap64(uint64_t x) { return le64(x); }
#endif

#if _BYTE_ORDER == _LITTLE_ENDIAN
static inline uint16_t bswap16(uint16_t x) { return be16(x); }
static inline uint32_t bswap32(uint32_t x) { return be32(x); }
static inline uint64_t bswap64(uint64_t x) { return be64(x); }
#endif
#endif

/*
 * BSD host integer interfaces
 */

#if !defined(__HOSTSWAP_DEFINED) && !defined(EMSCRIPTEN)
static inline uint16_t htobe16(uint16_t x) { return be16(x); }
static inline uint16_t htole16(uint16_t x) { return le16(x); }
static inline uint16_t be16toh(uint16_t x) { return be16(x); }
static inline uint16_t le16toh(uint16_t x) { return le16(x); }

static inline uint32_t htobe32(uint32_t x) { return be32(x); }
static inline uint32_t htole32(uint32_t x) { return le32(x); }
static inline uint32_t be32toh(uint32_t x) { return be32(x); }
static inline uint32_t le32toh(uint32_t x) { return le32(x); }

static inline uint64_t htobe64(uint32_t x) { return be64(x); }
static inline uint64_t htole64(uint32_t x) { return le64(x); }
static inline uint64_t be64toh(uint32_t x) { return be64(x); }
static inline uint64_t le64toh(uint32_t x) { return le64(x); }
#endif

#ifdef __cplusplus
}
#endif
