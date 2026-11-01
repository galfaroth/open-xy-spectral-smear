#pragma once
#include <opxy/effect.hpp>
namespace spectralsmear {
const opxy::u32 transform_size = 128, ring_size = 256, latency = 128;
const opxy::u32 buffer_words = ring_size * 4 + transform_size * 2 + 4 + (transform_size / 2 + 1) * 4;
struct State { opxy::u32 write, initialized; opxy::i32 level; };
void initialize(State&);
void process(State&, opxy::Buffer, opxy::Channel, opxy::Channel, opxy::Parameters);
}
