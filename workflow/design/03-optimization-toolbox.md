# Optimization Toolbox

A centralized resource for optimization techniques.

##  Memory Management (Zero-Copy, Coalescing)

##  Kernel Execution (Thread Divergence, Local Memory)

##  Debugging (Oclgrind, Profiling)

##  GenericKernelTemplates Content

### Structure

```
GenericKernelTemplates/
├── README.md              # Tool overview, build/run
├── 01_Basic_MAD/          # V1: Single-type MAD kernel
├── 02_Generic_MAD/        # V2: Macro-based generic (uchar/float)
├── 03_AutoTune/           # V3: Runtime type selection + profiling
├── kernels/
│   └── mad_kernel.cl      # Shared source w/ #ifdef TYPE
└── CMakeLists.txt         # Standalone build [file:11]
```


### Key Concepts

Single kernel source compiles multiple variants for reuse (emulates templates); profile to pick best type per workload.[^3]

Example in `mad_kernel.cl`:

```cl
#ifdef TYPE
  #if TYPE == uchar
    typedef uchar scalar_t;
  #elif TYPE == float
    typedef float scalar_t;
  #endif
#endif
__kernel void mad(global scalar_t* out, const global scalar_t* in, float contrast, float brightness) {
  int gid = get_global_id(0);
  out[gid] = in[gid] * contrast + brightness;
}
```

Build: `clBuildProgram(program, 1, &device, "-D TYPE=float", NULL, NULL)`.

Performance Gate: Generic float MAD <1ms on 1080p vs uchar baseline.

##  MultiGPU_Strategy

**Organizer**: `99_Toolbox/MultiGPU_Strategy/`

**Description**
Detect GPUs (`clGetDeviceIDs`), create shared context + per-GPU queues; dispatch workloads (e.g., image slices) from multi-threads for throughput (single-thread ok w/ OOO queues).
Strategy: Round-robin/proximity, event profiling for balancing.
Verify: Dual-GPU halves 4K process time vs single.

##  AsyncMultiThread

**Organizer**: `99_Toolbox/AsyncMultiThread/`

**Description**
Progression:

- 01_BasicSync: Blocking calls baseline.
- 02_AsyncSingle: OOO queue (`CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE`), non-block enqueues + events (overlap compute/transfer).
- 03_MultiThreadAsync: Shared ctx thread-safe, per-thread queues; mutex kernel args, event barriers for consumers.
Verify: 2x overlap via timeline (Nsight-like events).
