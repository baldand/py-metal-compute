# metalcompute for Python

![Build status](https://github.com/baldand/py-metal-compute/actions/workflows/test.yml/badge.svg?branch=main)

A python library to run metal compute kernels on MacOS

# Usage

Example execution from M1-based Mac running MacOS 12.0:

```
> ./build.sh
> python3 test_basic.py
Calculating sin of 1234567 values
Expected value: 0.9805107116699219 Received value: 0.9807852506637573
Metal compute took: 0.01446986198425293 s
Reference compute took: 0.1068720817565918 s
```

# Interface

```
import metalcompute as mc

mc.init() # Call before using
mc.compile(program, function_name) # Will raise exception with details if metal kernel has errors
mc.run(input_1d_np_float_array, output_1d_np_float_array)
mc.release()
```

# Status

This is a very early version. 

Not widely tested yet.
