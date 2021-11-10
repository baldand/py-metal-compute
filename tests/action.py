from time import time as now
from array import array
import math

import metalcompute as mc

# This test is intended to run in github action runner which does not support metal
# We can at least import the build library and check API calls fail with valid errors

try:
    print("Testing mc.init callable")
    mc.init()
    # This pass exeddcuted in HW
except mc.error:
    # This pass executed in CI
    pass
print("OK")

# Check some error handling. Cannot run before compiling
try:
    print("Testing mc.run callable")
    i = array('f',[0,1])
    o = array('f',[0,1])
    mc.run(i, o, 1)
    assert(false) # Should not reach here
except mc.error:
    print("OK")
    pass # Expected exception here

# When compiling must give name of needed function
try:
    print("Testing mc.compile callable")
    mc.compile("invalid_kernel", "unknown_function")
    assert(false) # Should not reach here
except mc.error:
    print("OK")
    pass # Expected exception here

print("Testing mc.release callable")
mc.release() # Should not fail
print("OK")
