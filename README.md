# catia-sketch-cpp

C++20 parser prototype for CATIA V5 sketch entities and constraints.

## What is implemented

This repository now targets the core sketch extraction path required by the
CATIA layout specification:

- `Sketch` objects are emitted from native object-graph markers,
- `2DPoint` candidates are exposed as `SketchEntity` objects,
- `ConstraintDYS` candidates are exposed as native constraints,
- geometry is preserved as either typed `Point2D` or `UnknownGeometry`,
- the original CATIA payload bytes are retained for later exact decoding.

This is intentionally conservative. The code does not guess unsupported geometry
or solver semantics when the object graph is incomplete.

## Source references

- CATIA layout table: https://github.com/cadmpeg/cadmpeg/blob/main/docs/layouts/catia.toml
- Format document: https://github.com/cadmpeg/cadmpeg/blob/main/docs/formats/catia.md
- Sketch transfer logic: https://github.com/cadmpeg/cadmpeg/blob/main/crates/cadmpeg-codec-catia/src/sketch.rs

## Build

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Usage

```bash
./build/catia-sketch-read part.CATPart
```

The public API uses `std::list<Sketch>` for sketches, `std::list<SketchEntity>`
for entities, and `std::list<SketchConstraint>` for constraints. The parser keeps
raw payloads and records unresolved relations instead of fabricating values.
