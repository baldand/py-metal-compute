import metalcompute as mc
import numpy as np
from time import time as now

dev = mc.Device(0)
print(dev)

kern = dev.kernel("""
#include <metal_stdlib>
using namespace metal;

kernel void test(const device uchar *in [[ buffer(0) ]],
                device uchar  *out [[ buffer(1) ]],
                uint id [[ thread_position_in_grid ]]) {
    out[id] = in[id] + 2;
}
""")
test = kern.function("test")

dim = 40000
reps = 100

buf_in = dev.buffer(dim*dim)

np_buf_in = np.frombuffer(buf_in, dtype=np.uint8).reshape(dim,dim)

np_buf_in[:] = 42

buf_out = dev.buffer(dim*dim)

del dev # Should be safe due to ref counting

np_buf_out = np.frombuffer(buf_out, dtype=np.uint8).reshape(dim,dim)

start = now()
# Calls to "test" will not block until the returned handles are released
handles = [test(np_buf_out.size, buf_in, buf_out) for i in range(reps)]
# Now that all calls are queued, release the handles to block until all completed
del handles
end = now()

assert(np_buf_out[-1,-1] == 44)

print(f"Data transfer rate: {(dim*dim*reps*2)/(1E9*(end-start)):3.6} GB/s")
