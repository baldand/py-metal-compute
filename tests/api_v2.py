import metalcompute as mc
import numpy as np
from time import time as now

dev = mc.Device(0)
print(dev)

kern = mc.Kernel(dev, """
#include <metal_stdlib>
using namespace metal;

kernel void test(const device uchar *in [[ buffer(0) ]],
                device uchar  *out [[ buffer(1) ]],
                uint id [[ thread_position_in_grid ]]) {
    out[id] = in[id] + 2;
}
""")

test = mc.Function(kern, "test")

buf_in = mc.Buffer(dev, 40000*40000)

np_buf_in = np.frombuffer(buf_in, dtype=np.uint8).reshape(40000,40000)

np_buf_in[:] = 42

buf_out = mc.Buffer(dev, 40000*40000)

np_buf_out = np.frombuffer(buf_out, dtype=np.uint8).reshape(40000,40000)

start = now()
test(buf_in, buf_out, np_buf_out.size)
end = now()

print(np_buf_out[-1,-1], end-start)
