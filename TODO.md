# TODO

- Add links to external resources for more in depth information
- **[Feature] GPU-built BVH (LBVH via Morton codes)**: After B3's CPU SAH-BVH, add a challenge variant that constructs the BVH entirely on the GPU using Morton-code sorting + radix sort → parallel hierarchy build. Enables per-frame rebuild for dynamic scenes without CPU round-trip. Natural follow-on to B3 Challenge (Phase 4).
- Consider ROS2 related examples to integrate with ROS world better(launch files, service to restart static publisher, parameters handling, loaned messages runtime setup and verification etc.)
- Work on setup with docker
- Code quality improvements
