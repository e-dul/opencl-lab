## ADDED Requirements

### Requirement: 4-pass LSD radix sort correctness
The radix sort implementation SHALL sort an array of N `uint` keys in ascending order using 4 passes, each processing 8 bits (digit range 0–255). After all 4 passes the output buffer SHALL contain keys in non-decreasing order.

#### Scenario: Already sorted input
- **WHEN** radix sort runs on an input array that is already sorted
- **THEN** the output is identical to the input

#### Scenario: Reverse sorted input
- **WHEN** radix sort runs on a reverse-sorted input
- **THEN** the output is correctly sorted in ascending order

#### Scenario: Duplicate keys
- **WHEN** the input contains repeated key values
- **THEN** all occurrences appear contiguously in the output, and the sort does not hang or corrupt memory

---

### Requirement: Per-pass histogram kernel
Each radix sort pass SHALL begin with a histogram kernel that counts the frequency of each 8-bit digit (0–255) for the active byte of every key. The histogram SHALL be written to a device buffer of size 256 × num_work_groups.

#### Scenario: Histogram sum equals N
- **WHEN** the histogram kernel runs on N elements
- **THEN** the sum of all 256 bucket counts equals N

---

### Requirement: Prefix scan (Blelloch) over histogram
After the histogram kernel, a prefix-scan kernel SHALL produce the exclusive prefix sum of each bucket's total count across all work-groups. The scan output is used as the base scatter offset for each digit bucket.

#### Scenario: Exclusive prefix sum correctness
- **WHEN** prefix scan runs on array [3, 1, 4, 1, 5]
- **THEN** output is [0, 3, 4, 8, 9]

#### Scenario: Last element
- **WHEN** prefix scan completes
- **THEN** `scan[255] + count[255] == N`

---

### Requirement: Scatter kernel preserves key-value association
The scatter kernel SHALL write each (key, payload) pair to its sorted destination address computed from the prefix-scan offsets. If a companion index array is sorted alongside keys (to track original triangle indices), the scatter SHALL update both arrays atomically as a pair.

#### Scenario: Triangle indices follow their keys
- **WHEN** scatter completes after sorting Morton codes
- **THEN** `sorted_indices[i]` holds the original triangle index whose Morton code is `sorted_keys[i]`

---

### Requirement: CPU reference self-test before first GPU dispatch
Before dispatching the GPU sort in the first frame, the host SHALL verify the GPU sort result against `std::sort` on the same input. If they differ, the binary SHALL throw `std::runtime_error` with a descriptive message.

#### Scenario: Self-test passes on valid hardware
- **WHEN** the binary is launched and the GPU sort produces a correctly sorted result
- **THEN** the self-test completes silently and rendering proceeds

#### Scenario: Self-test detects driver bug
- **WHEN** the GPU sort produces an incorrect result (simulated by injecting a known-bad input)
- **THEN** a `std::runtime_error` is thrown before any BVH build kernel is enqueued
