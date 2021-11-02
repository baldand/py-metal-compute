# metalcompute for Python

A python library to run metal compute kernels on MacOS 12

# Usage

Example execution from M1-based Mac running MacOS 12.0:

```
> ./build.sh
> python3 test_basic.py
Calculating sin of 1234567 values
Expected value: 0.9805108 Received value: 0.98078525
Metal compute took: 0.008514881134033203 s
Reference (numpy) compute took: 0.011723995208740234 s
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

This is the first version. 

Not widely tested yet.

Not tested on Intel devices.
