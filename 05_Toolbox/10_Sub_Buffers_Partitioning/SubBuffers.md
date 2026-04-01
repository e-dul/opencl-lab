# Sub-Buffers & Partitioning

Partition a single large `cl::Buffer` into aligned sub-regions and dispatch a separate kernel to each region — zero device memory copies, zero extra allocations.

**Symptom**: Kernel processes a large buffer in logically independent segments (strips, tiles, channels) but the host manually copies each segment into a fresh `cl::Buffer`. Profiling shows significant time in `enqueueWriteBuffer` calls between dispatches. **Root cause:** The data is already on the device — `cl::Buffer::createSubBuffer` creates an aliased view at zero cost; no device memory is moved.

## Prerequisites

No delta from [Toolbox prerequisites](../Toolbox.md) — standard Module 1 host API skills (`cl::Buffer`, `enqueueNDRangeKernel`, `CL_CHECK`) are sufficient. Sub-buffers are part of the OpenCL 1.2 core spec; no extension required.

## Build & Run

```bash
cd 05_Toolbox/10_Sub_Buffers_Partitioning
cmake -B build && cmake --build build

# Default: 512×512 image, 4 horizontal strips
./build/sub_buffers

# Custom strip count
./build/sub_buffers --strips 8 --width 1024 --height 1024

# Pin to a specific GPU vendor
GPU=AMD    ./build/sub_buffers
GPU=INTEL  ./build/sub_buffers
GPU=NVIDIA ./build/sub_buffers

# Print all flags (--width, --height, --strips, --output)
./build/sub_buffers --help
```

## Verify

Expected console output (values depend on hardware):

```text
Platform : Intel(R) OpenCL Graphics
Device   : Intel(R) Iris(R) Xe Graphics
Alignment : 128 bytes  (CL_DEVICE_MEM_BASE_ADDR_ALIGN = 1024 bits)
Image     : 512 x 512 (4 strips)
Strip     : 128 rows, 262144 bytes
Buffer    : 1048576 bytes total
Output    : output.bmp
```

**Key checks:**

- `Alignment` is printed (non-zero, hardware-queried value).
- `output.bmp` exists and contains N distinct horizontal color bands — one per strip — with no corruption or solid-black regions.
- No errors or exceptions on exit.

Open `output.bmp` and confirm N horizontal color bands, each covering an equal image strip. The file is written to the directory from which you invoked the binary (i.e., `05_Toolbox/10_Sub_Buffers_Partitioning/output.bmp` when following the Build & Run commands above).

## Concept

### Parent buffer and sub-buffer aliasing

```text
Parent buffer (device memory):
┌──────────────────────────────────────────────────────┐
│ Strip 0 │ Strip 1 │ Strip 2 │ Strip 3 │ (padding)    │
└──────────────────────────────────────────────────────┘
    ↑           ↑           ↑           ↑
 sub_buf[0]  sub_buf[1]  sub_buf[2]  sub_buf[3]
```

(`padding` exists because `strip_bytes` is rounded up to the alignment boundary — see Alignment-safe strip partitioning below.)

Each `sub_buf[i]` is created with:

```cpp
// strip_bytes must already be rounded up to align_bytes (see Alignment-safe strip partitioning)
cl_buffer_region region{ strip_bytes * i, strip_bytes };
cl::Buffer sub_buf = parent_buf.createSubBuffer(
    CL_MEM_READ_WRITE,
    CL_BUFFER_CREATE_TYPE_REGION,
    &region, &err);
```

The kernel receives `sub_buf` as its `pixels` argument. Because the driver adjusted the base pointer, `pixels[0]` inside the kernel maps to `parent[strip_bytes * i]` — zero overhead, no copy. `createSubBuffer` does not allocate new device memory: it returns a `cl::Buffer` whose internal base pointer points into the parent's allocation at `region.origin` bytes. Writes through the sub-buffer land in the parent's storage — `enqueueReadBuffer` on the **parent** after all kernel dispatches assembles the result without any additional copy.

### `get_global_id(0)` is sub-buffer-relative

`gid = 0` inside the kernel maps to the **sub-buffer origin**, not to element 0 of the parent buffer. The OpenCL driver adjusts the base pointer below the kernel ABI boundary — the kernel needs no knowledge of where it sits in the parent. This is why the same kernel source can be dispatched once per strip without any offset argument.

### Alignment-safe strip partitioning

```text
nominal_strip_bytes = ceil(height / strips) * width * 4
strip_bytes         = round_up(nominal_strip_bytes, align_bytes)
strip_i origin      = strip_bytes * i            // always aligned
```

The last strip may include padding rows. The host crops `host_pixels` to `width * height * 4` bytes before writing the BMP, so padding rows never appear in the output.

> **Stop and Read: `CL_MISALIGNED_SUB_BUFFER_OFFSET`**
>
> `createSubBuffer` throws `CL_MISALIGNED_SUB_BUFFER_OFFSET` if `region.origin` is not a multiple of `CL_DEVICE_MEM_BASE_ADDR_ALIGN`. This value is hardware-specific:
>
> | Hardware          | Typical alignment |
> |:------------------|:------------------|
> | Intel Xe (iGPU)   | 128 bytes         |
> | AMD RDNA          | 256 bytes         |
> | NVIDIA (via pocl) | varies            |
>
> **Never hardcode** an alignment value. Always query at runtime:
>
> ```cpp
> cl_uint align_bits = 0;
> CL_CHECK(device.getInfo(CL_DEVICE_MEM_BASE_ADDR_ALIGN, &align_bits));
> const size_t align_bytes = static_cast<size_t>(align_bits) / 8u;
> ```
>
> Then round each strip's byte size up to the nearest multiple of `align_bytes` before computing sub-buffer origins. The demo in `main.cpp` implements exactly this pattern.

### Zero data movement invariant

```text
(parent buffer starts uninitialized — populated entirely by kernel dispatches below)
createSubBuffer × N             ← aliased views, no copy
enqueueNDRangeKernel × N        ← each kernel writes through its sub-buffer alias
enqueueReadBuffer(parent_buf)   ← single read of the assembled result
```

No `enqueueWriteBuffer` ever targets a sub-region after initial setup.

## Mini-Challenge

1. Run `--strips 1`. What does `output.bmp` contain? Why is there only one sub-buffer? What does that tell you about the relationship between a sub-buffer and the parent when the sub-buffer spans the entire allocation?
2. **Stretch:** Add a second kernel that reads back through a sub-buffer and inverts the colors. Confirm that the parent buffer reflects the inversion after the second dispatch — without any `enqueueWriteBuffer` call.

**Natural next steps:** [08_Multi_GPU_Strategy](../08_Multi_GPU_Strategy/MultiGPUStrategy.md) shows how to partition work across multiple devices using the same slice-partitioning concept. [16_Async_Multi_Thread](../16_Async_Multi_Thread/AsyncMultiThread.md) shows how to pipeline overlapping transfers and kernel dispatches across sub-regions.

## Troubleshooting

- **Confirm zero data movement:** `grep -n enqueueWriteBuffer ./main.cpp` should return zero matches after initial parent buffer setup — all writes go through sub-buffer kernel dispatches.
- **`CL_MISALIGNED_SUB_BUFFER_OFFSET` on `createSubBuffer`:** The strip byte size was computed before alignment rounding. Check that `strip_bytes % align_bytes == 0` before calling `createSubBuffer`.
- **Solid black output:** `enqueueReadBuffer` finished before all kernel dispatches. If adapting this pattern, ensure `queue.finish()` is called after the last `enqueueNDRangeKernel` and before `enqueueReadBuffer` (the demo already does this).
- **Last strip is wrong color:** The strip count is larger than `height`, causing zero-pixel strips. Validate `strips <= height` on input.
- **Build error — `cl_buffer_region` not found:** Your OpenCL headers predate 1.2. Run `sudo apt install opencl-headers`.

---

[Back to Toolbox](../Toolbox.md)
