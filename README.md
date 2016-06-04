# Digest-based procedural generation

The intention of this code base is to explore procedural generation
based on message digest, through a number of possible approaches,
sharing a common basis: the hashing of some salient (reproducible)
aspect to obtain a string of seemingly random bits.

## The basics

Most notably, one can hash the coordinate(s) of the cell or point or
whatever as a starting point. The resulting hash can the be used as:

* seed for a traditional PRNG;
* starting point of a PRNG-like number generator, based on consecutive
  hashing.

An interesting characteristic of this approach is that a single
hashing function produces a "large" number of bits: MD5 produces
128-bit hashes, i.e. 16 bytes or 4 32-bit integers, SHA-256 produces
two times as much. These can be used for additional variability.

And of course, for each hashable element, multiple hashes can be
generated, for extra bitness.

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
