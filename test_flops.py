from time import time as now
from array import array
import math

import metalcompute as mc

kernel_start = """
#include <metal_stdlib>
using namespace metal;

kernel void test(const device float *in [[ buffer(0) ]],
                device float  *out [[ buffer(1) ]],
                uint id [[ thread_position_in_grid ]]) {
    float v1 = in[0] * .0001 + id * .00001;
    float v2 = v1 + .0001 + id * .00002;

    for (int i=0;i<2048;i++) {
"""

kernel_step = """
        v1 = v1 * v1 + .00001;
        v2 = v2 * v2 - .00001;
"""

kernel_end = """
    }
    float v = v1 + v2;
    out[id] = v;
}
"""

count = 1024*1024
i = array('f', [0])
o = array('f', [0 for i in range(count)])

print("Running compute intensive Metal kernel to measure TFLOPS...")
mc.init()
reps = 400
mc.compile(kernel_start+kernel_step*reps+kernel_end, "test")
i[0] = 0.42
mc.run(i, o) # Run once to warm up
mc.rerun() # Rerun once with same data
start = now()
mc.rerun() # Profile this time
end = now()
mc.release()

ops_per_kernel = reps*4*2048 + 7 # 24 * 10000 + 1 # 33
ops_per_run = 1 * ops_per_kernel * count
time_per_run = end - start
ops_per_sec = ops_per_run / time_per_run
print(f"Estimated GPU TFLOPS: {ops_per_sec/1e12:1.6}")
