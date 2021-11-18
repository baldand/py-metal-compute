from time import time as now
from array import array
import math

import metalcompute as mc

kernel = """
#include <metal_stdlib>
using namespace metal;

kernel void test(const device float *in [[ buffer(0) ]],
                device float  *out [[ buffer(1) ]],
                uint id [[ thread_position_in_grid ]]) {
    float r = 0.0;
    float ii = in[id];
    out[id] = sin(ii);
}
"""

kernel_with_error = """
invalid_program
"""

dev = mc.Device()

count = 1234567
in_buf = dev.buffer(count*4)
in_buf_mv = memoryview(in_buf).cast('f')
in_buf_mv[:] = array('f',range(count))
out_buf = dev.buffer(count*4)
out_buf_mv = memoryview(out_buf).cast('f')

# Compile errors are returned in exception when compiling invalid programs
try:
    fn_error = dev.kernel(kernel_with_error).function(function_name)
    assert(false) # Should not reach here
except mc.error as err:
    # To see error message from compiler do: print(err)
    pass # Expected exception here

# When compiling must give name of needed function
try:
    fn_bad_name = dev.kernel(kernel).function("unknown")
    assert(false) # Should not reach here
except mc.error:
    pass # Expected exception here

function_name = "test"

# This should work
fn_good = dev.kernel(kernel).function(function_name)

print("Calculating sin of",count,"values")
s1 = now()

# This should work. Arrays must be 1D float at the moment
fn_good(count, in_buf, out_buf)
e1 = now()

s2 = now()
oref = array('f',[math.sin(value) for value in in_buf_mv])
e2 = now()

print("Expected value:",oref[-1], "Received value:",out_buf_mv[-1])
print("Metal compute took:",e1-s1,"s")
print("Reference compute took:",e2-s2,"s")