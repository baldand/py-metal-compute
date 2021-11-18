import metalcompute as mc
mc.init()

code = """
#include <metal_stdlib>
using namespace metal;

kernel void add_one(
    const device float* in [[ buffer(0) ]],
    device float* out [[ buffer(1) ]],
    uint id [[ thread_position_in_grid ]]) {
    out[id] = in[id] + 1.0;
}
"""
mc.compile(code, "add_one")
import numpy as np
from time import time as now
c = 250000000
in_buf = np.arange(c,dtype='f')
out_buf = np.empty(c,dtype='f')
start = now()
mc.run(in_buf, out_buf, c)
end = now()
print(end-start)
print(c/(end-start))
#print("in",in_buf)
#print("out",out_buf, out_buf.dtype)
