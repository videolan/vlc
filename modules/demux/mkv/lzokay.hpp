/*
 * Copyright (c) 2018 Jack Andersen
 * SPDX-License-Identifier: MIT
 * https://github.com/AxioDL/lzokay
 */

#pragma once
#include <cstddef>
#include <cstdint>

namespace lzokay {

enum class EResult {
  LookbehindOverrun = -4,
  OutputOverrun = -3,
  InputOverrun = -2,
  Error = -1,
  Success = 0,
  InputNotConsumed = 1,
};


EResult decompress(const uint8_t* src, std::size_t src_size,
                   uint8_t* dst, std::size_t dst_size,
                   std::size_t& out_size);

}
