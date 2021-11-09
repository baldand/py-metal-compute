# metalcompute for Python

![Build status](https://github.com/baldand/py-metal-compute/actions/workflows/test.yml/badge.svg?branch=main)

A python library to run metal compute kernels on MacOS

## Usage

Example execution from M1-based Mac running MacOS 12.0:

```
> ./build.sh
> python3 test_basic.py
Calculating sin of 1234567 values
Expected value: 0.9805107116699219 Received value: 0.9807852506637573
Metal compute took: 0.0040209293365478516 s
Reference compute took: 0.1068720817565918 s
```

## Interface

```
import metalcompute as mc

mc.init() 
# Call before use

mc.compile(program, function_name)
# Will raise exception with details if metal kernel has errors

mc.run(input_f32_or_u8_array, output_f32_or_u8_array, kernel_call_count)
# Run the kernel once with supplied input data, 
# filling supplied output data
# Specify number of kernel calls

mc.release()
# Call after use

```

## Example

### Measure TFLOPS of GPU

```
> python3 test_flops.py
Running compute intensive Metal kernel to measure TFLOPS...
Estimated GPU TFLOPS: 2.50825
```

### Render a 3D image with raymarching

```
# Usage: python3 test_raymarch.py <width> <height> <output image file: PNG, BMP>

> python3 test_raymarch.py 1024 1024 raymarch.png
Render took 0.0119569s
```

# Mandelbrot set

```
> python3 examples/mandelbrot.py
Rendering mandelbrot set using Metal compute, res:4096x4096, iters:8192
Render took 0.401446s
Writing image to mandelbrot.png
Image encoding took 1.35182s
```

## Status

This is an early version. 

Not widely tested yet.
