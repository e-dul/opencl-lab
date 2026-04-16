# Capability: io-standards

Source: `00_master_specs.md` §3

## Overview

All modules produce visual output artifacts. Asset paths come from CLI args. Image I/O uses stb_image.

---

#### Scenario: Visual output required

WHEN a module contains GPU kernels
THEN the binary produces at least one visual artifact (e.g. `output.bmp`)
AND console-only output does not satisfy this requirement

#### Scenario: Numeric-only exception

WHEN a module is a benchmark, pipeline timing tool, or multi-GPU comparison
THEN it produces a structured console timing table instead of a BMP
AND this exception is documented in the module's README

#### Scenario: Image format

WHEN a module writes image output
THEN the format is BMP or PNG
AND JPEG is not used (compression artifacts corrupt debugging)

#### Scenario: Image channels

WHEN a module reads or writes image data
THEN the pixel layout is RGBA (4 channels) or Grayscale (1 channel)

#### Scenario: Image I/O library

WHEN a module performs image I/O
THEN `stb_image.h` and `stb_image_write.h` are used
AND utilities from `common/image_utils.hpp` are preferred over re-implementing from scratch

#### Scenario: Asset path via CLI

WHEN a module reads an input file (image, audio, model)
THEN the path is received as a CLI argument (not hardcoded)
AND the asset physically lives under `assets/` at the repository root

#### Scenario: Hardcoded paths forbidden

WHEN module source code references an input file
THEN no string literal containing an absolute or relative path to `assets/` appears in the code
