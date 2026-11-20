/*
Copyright 2026 miyabi-no1-fan

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <ostream>
#include <vector>

#include "slp_png.hpp"

namespace slp {

auto write_png_tag(std::ostream& io) -> std::expected<void, Error>;
auto write_ihdr(const Image& image, std::ostream& io) -> std::expected<void, Error>;
auto write_plte(const Image& image, std::ostream& io) -> std::expected<void, Error>;
auto write_trns(const Image& image, std::ostream& io) -> std::expected<void, Error>;
auto write_idat(const Image& image, std::ostream& io) -> std::expected<void, Error>;
auto write_iend(std::ostream& io) -> std::expected<void, Error>;

void filter(const Image& image, std::array<std::vector<std::uint8_t>, 5>& filter_buffers,
            std::array<std::uint64_t, 5>& filter_scores, std::size_t i);

}  // namespace slp
