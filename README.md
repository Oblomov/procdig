# Digest-based procedural generation

The intention of this code base is to explore procedural generation
based on message digest, through a number of possible approaches,
sharing a common basis: the hashing of some salient (reproducible)
aspect to obtain a string of seemingly random bits.

## The basics

Most notably, one can hash the coordinate(s) of the cell or point or
whatever as a starting point. The resulting hash can then be used as:

* seed for a traditional PRNG;
* starting point of a PRNG-like number generator, based on consecutive
  hashing.

An interesting characteristic of this approach is that a single
hashing function produces a "large" number of bits: MD5 produces
128-bit hashes, i.e. 16 bytes or 4 32-bit integers, SHA-256 produces
two times as much. These can be used for additional variability (e.g.
using the first integer to seed the PRNG for heights, the second
integer to seed the PRNG for biome, etc).

And of course, for each hashable element, multiple hashes (e.g. both
MD5 _and_ SHA-256) can be generated, to get even more variability.

## Multiple layers

Many procedural generation mechanisms rely on world subdivision for
performance and efficiency. The hash approach allows extra data to be
generated at a higher level and passed to the lower subdivisions.

Consider for example a one-dimensional world. An SHA-256 of the
top-level world descriptor (even just the null string) would generate
32 bytes, each of which could be assigned to any of 32 equal
subdivisions of the world, and combined with the subdivision
coordinates to generate the next level of hashes, which again would
pass the information over to 1024 subdivisions of the next level, and
so on.

# Credits and licensing

All documentation, unless otherwise specified, is licensed under the
<a rel="license"
href="http://creativecommons.org/licenses/by-sa/4.0/"><img
alt="Creative Commons License"
src="https://i.creativecommons.org/l/by-sa/4.0/88x31.png"/>
Creative Commons Attribution-ShareAlike 4.0 International License</a>.

All source code, unless otherwise specified, is licensed under the
Mozilla Public License, version 2.0 (see LICENSING).
