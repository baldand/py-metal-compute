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


count = 1234567
i = array('f', range(count))
o = array('f', [0 for i in range(count)])

mc.init()

# Check some error handling. Cannot run before compiling
try:
    mc.run(i, o, count)
    assert(false) # Should not reach here
except mc.error:
    pass # Expected exception here

# When compiling must give name of needed function
try:
    mc.compile(kernel, "unknown")
    assert(false) # Should not reach here
except mc.error:
    pass # Expected exception here

function_name = "test"

# Compile errors are returned in exception when compiling invalid programs
try:
    mc.compile(kernel_with_error, function_name)
    assert(false) # Should not reach here
except mc.error as err:
    # To see error message from compiler do: print(err)
    pass # Expected exception here

# This should work
mc.compile(kernel, function_name)

print("Calculating sin of",count,"values")
s1 = now()

# This should work. Arrays must be 1D float at the moment
mc.run(i, o, count)
e1 = now()

s2 = now()
oref = array('f',[math.sin(value) for value in i])
e2 = now()

print("Expected value:",oref[-1], "Received value:",o[-1])
print("Metal compute took:",e1-s1,"s")
print("Reference compute took:",e2-s2,"s")

mc.release()