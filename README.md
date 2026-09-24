# catia-sketch-cpp

C++20 starter library for reading CATIA V5 `.CATPart` sketch data.

## Status

This repository is a safe, conservative foundation. It validates the outer
`V5_CFV2\0` signature, exposes a `std::list<Sketch>`, preserves native bytes,
and provides typed containers for points, lines, circles, arcs, splines,
profiles, and constraints.

The current parser does **not** claim to decode sketch ownership from arbitrary
CATIA files. The CATIA layout specification identifies sketches in the outer
`7C08`/`7C09` object graph. A sketch is emitted as typed data only after its
object, owner record, fields, entity records, and references are all proven to
belong to the same graph. Until that decoder is implemented, the input is
returned as a native document payload instead of guessed geometry.

The reference format is:

- [`docs/layouts/catia.toml`](https://github.com/cadmpeg/cadmpeg/blob/main/docs/layouts/catia.toml)
- [`docs/formats/catia.md`](https://github.com/cadmpeg/cadmpeg/blob/main/docs/formats/catia.md)
- [`crates/cadmpeg-codec-catia/src/sketch.rs`](https://github.com/cadmpeg/cadmpeg/blob/main/crates/cadmpeg-codec-catia/src/sketch.rs)

## Build

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Run

```sh
./build/catia-sketch-read part.CATPart
```

The API uses `std::list` for sketches, entities, constraints, profiles, and
native members. Unknown records remain available as byte payloads.
