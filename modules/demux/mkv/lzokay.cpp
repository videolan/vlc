/*
 * Copyright (c) 2018 Jack Andersen
 * SPDX-License-Identifier: MIT
 * https://github.com/AxioDL/lzokay
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "lzokay.hpp"
#include <cstring>
#include <limits>

/*
 * Based on documentation from the Linux sources: Documentation/lzo.txt
 * https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/Documentation/lzo.txt
 */

namespace lzokay {

static inline uint16_t get_le16(const void *p)
{
  uint16_t val;

  memcpy (&val, p, sizeof (val));
#ifdef WORDS_BIGENDIAN
  val = (val << 8) | (val >> 8);
#endif
  return val;
}

constexpr std::size_t Max255Count = std::numeric_limits<size_t>::max() / 255 - 2;

#define NEEDS_IN(count) \
  if (inp + (count) > inp_end) { \
    dst_size = outp - dst; \
    return EResult::InputOverrun; \
  }

#define NEEDS_OUT(count) \
  if (outp + (count) > outp_end) { \
    dst_size = outp - dst; \
    return EResult::OutputOverrun; \
  }

#define CONSUME_ZERO_BYTE_LENGTH \
  std::size_t offset; \
  { \
    const uint8_t *old_inp = inp; \
    while (*inp == 0) ++inp; \
    offset = inp - old_inp; \
    if (offset > Max255Count) { \
      dst_size = outp - dst; \
      return EResult::Error; \
    } \
  }

// constexpr uint32_t M1Marker = 0x0;
// constexpr uint32_t M2Marker = 0x40;
constexpr uint32_t M3Marker = 0x20;
constexpr uint32_t M4Marker = 0x10;

EResult decompress(const uint8_t* src, std::size_t src_size,
                   uint8_t* dst, std::size_t init_dst_size,
                   std::size_t& dst_size) {
  dst_size = init_dst_size;

  if (src_size < 3) {
    dst_size = 0;
    return EResult::InputOverrun;
  }

  const uint8_t* inp = src;
  const uint8_t* inp_end = src + src_size;
  uint8_t* outp = dst;
  uint8_t* outp_end = dst + dst_size;
  uint8_t* lbcur;
  std::size_t lblen;
  std::size_t state = 0;
  std::size_t nstate = 0;

  /* First byte encoding */
  if (*inp >= 22) {
    /* 22..255 : copy literal string
     *           length = (byte - 17) = 4..238
     *           state = 4 [ don't copy extra literals ]
     *           skip byte
     */
    std::size_t len = *inp++ - uint8_t(17);
    NEEDS_IN(len)
    NEEDS_OUT(len)
    for (std::size_t i = 0; i < len; ++i)
      *outp++ = *inp++;
    state = 4;
  } else if (*inp >= 18) {
    /* 18..21 : copy 0..3 literals
     *          state = (byte - 17) = 0..3  [ copy <state> literals ]
     *          skip byte
     */
    nstate = *inp++ - uint8_t(17);
    state = nstate;
    NEEDS_IN(nstate)
    NEEDS_OUT(nstate)
    for (std::size_t i = 0; i < nstate; ++i)
      *outp++ = *inp++;
  }
  /* 0..17 : follow regular instruction encoding, see below. It is worth
   *         noting that codes 16 and 17 will represent a block copy from
   *         the dictionary which is empty, and that they will always be
   *         invalid at this place.
   */

  while (true) {
    NEEDS_IN(1)
    uint8_t inst = *inp++;
    if (inst & 0xC0) {
      /* [M2]
       * 1 L L D D D S S  (128..255)
       *   Copy 5-8 bytes from block within 2kB distance
       *   state = S (copy S literals after this block)
       *   length = 5 + L
       * Always followed by exactly one byte : H H H H H H H H
       *   distance = (H << 3) + D + 1
       *
       * 0 1 L D D D S S  (64..127)
       *   Copy 3-4 bytes from block within 2kB distance
       *   state = S (copy S literals after this block)
       *   length = 3 + L
       * Always followed by exactly one byte : H H H H H H H H
       *   distance = (H << 3) + D + 1
       */
      NEEDS_IN(1)
      lbcur = outp - ((*inp++ << 3) + ((inst >> 2) & 0x7) + 1);
      lblen = std::size_t(inst >> 5) + 1;
      nstate = inst & uint8_t(0x3);
    } else if (inst & M3Marker) {
      /* [M3]
       * 0 0 1 L L L L L  (32..63)
       *   Copy of small block within 16kB distance (preferably less than 34B)
       *   length = 2 + (L ?: 31 + (zero_bytes * 255) + non_zero_byte)
       * Always followed by exactly one LE16 :  D D D D D D D D : D D D D D D S S
       *   distance = D + 1
       *   state = S (copy S literals after this block)
       */
      lblen = std::size_t(inst & uint8_t(0x1f)) + 2;
      if (lblen == 2) {
        CONSUME_ZERO_BYTE_LENGTH
        NEEDS_IN(1)
        lblen += offset * 255 + 31 + *inp++;
      }
      NEEDS_IN(2)
      nstate = get_le16(inp);
      inp += 2;
      lbcur = outp - ((nstate >> 2) + 1);
      nstate &= 0x3;
    } else if (inst & M4Marker) {
      /* [M4]
       * 0 0 0 1 H L L L  (16..31)
       *   Copy of a block within 16..48kB distance (preferably less than 10B)
       *   length = 2 + (L ?: 7 + (zero_bytes * 255) + non_zero_byte)
       * Always followed by exactly one LE16 :  D D D D D D D D : D D D D D D S S
       *   distance = 16384 + (H << 14) + D
       *   state = S (copy S literals after this block)
       *   End of stream is reached if distance == 16384
       */
      lblen = std::size_t(inst & uint8_t(0x7)) + 2;
      if (lblen == 2) {
        CONSUME_ZERO_BYTE_LENGTH
        NEEDS_IN(1)
        lblen += offset * 255 + 7 + *inp++;
      }
      NEEDS_IN(2)
      nstate = get_le16(inp);
      inp += 2;
      lbcur = outp - (((inst & 0x8) << 11) + (nstate >> 2));
      nstate &= 0x3;
      if (lbcur == outp)
        break; /* Stream finished */
      lbcur -= 16384;
    } else {
      /* [M1] Depends on the number of literals copied by the last instruction. */
      if (state == 0) {
        /* If last instruction did not copy any literal (state == 0), this
         * encoding will be a copy of 4 or more literal, and must be interpreted
         * like this :
         *
         *    0 0 0 0 L L L L  (0..15)  : copy long literal string
         *    length = 3 + (L ?: 15 + (zero_bytes * 255) + non_zero_byte)
         *    state = 4  (no extra literals are copied)
         */
        std::size_t len = inst + 3;
        if (len == 3) {
          CONSUME_ZERO_BYTE_LENGTH
          NEEDS_IN(1)
          len += offset * 255 + 15 + *inp++;
        }
        /* copy_literal_run */
        NEEDS_IN(len)
        NEEDS_OUT(len)
        for (std::size_t i = 0; i < len; ++i)
          *outp++ = *inp++;
        state = 4;
        continue;
      } else if (state != 4) {
        /* If last instruction used to copy between 1 to 3 literals (encoded in
         * the instruction's opcode or distance), the instruction is a copy of a
         * 2-byte block from the dictionary within a 1kB distance. It is worth
         * noting that this instruction provides little savings since it uses 2
         * bytes to encode a copy of 2 other bytes but it encodes the number of
         * following literals for free. It must be interpreted like this :
         *
         *    0 0 0 0 D D S S  (0..15)  : copy 2 bytes from <= 1kB distance
         *    length = 2
         *    state = S (copy S literals after this block)
         *  Always followed by exactly one byte : H H H H H H H H
         *    distance = (H << 2) + D + 1
         */
        NEEDS_IN(1)
        nstate = inst & uint8_t(0x3);
        lbcur = outp - ((inst >> 2) + (*inp++ << 2) + 1);
        lblen = 2;
      } else {
        /* If last instruction used to copy 4 or more literals (as detected by
         * state == 4), the instruction becomes a copy of a 3-byte block from the
         * dictionary from a 2..3kB distance, and must be interpreted like this :
         *
         *    0 0 0 0 D D S S  (0..15)  : copy 3 bytes from 2..3 kB distance
         *    length = 3
         *    state = S (copy S literals after this block)
         *  Always followed by exactly one byte : H H H H H H H H
         *    distance = (H << 2) + D + 2049
         */
        NEEDS_IN(1)
        nstate = inst & uint8_t(0x3);
        lbcur = outp - ((inst >> 2) + (*inp++ << 2) + 2049);
        lblen = 3;
      }
    }
    if (lbcur < dst) {
      dst_size = outp - dst;
      return EResult::LookbehindOverrun;
    }
    NEEDS_IN(nstate)
    NEEDS_OUT(lblen + nstate)
    /* Copy lookbehind */
    for (std::size_t i = 0; i < lblen; ++i)
      *outp++ = *lbcur++;
    state = nstate;
    /* Copy literal */
    for (std::size_t i = 0; i < nstate; ++i)
      *outp++ = *inp++;
  }

  dst_size = outp - dst;
  if (lblen != 3) /* Ensure terminating M4 was encountered */
    return EResult::Error;
  if (inp == inp_end)
    return EResult::Success;
  else if (inp < inp_end)
    return EResult::InputNotConsumed;
  else
    return EResult::InputOverrun;
}

}
