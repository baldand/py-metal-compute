# metalcompute for Python

![Build status](https://github.com/baldand/py-metal-compute/actions/workflows/test.yml/badge.svg?branch=main)

A python library to run metal compute kernels on macOS >= 11

## Installations

Install latest stable release from PyPI:

```
> python3 -m pip install metalcompute
```

Install latest unstable version from Github:

```
> python3 -m pip install git+https://github.com/baldand/py-metal-compute.git
```

Install locally from source:

```
> python3 -m pip install .
```

## Basic test

Example execution from M1-based Mac running macOS 12:

```
> python3 tests/basic.py
Calculating sin of 1234567 values
Expected value: 0.9805107116699219 Received value: 0.9807852506637573
Metal compute took: 0.0040209293365478516 s
Reference compute took: 0.1068720817565918 s
```

## Interface

```
import metalcompute as mc

devices = mc.get_devices()
# Get list of available Metal devices

mc.init() 
# Call before use. Will open default Metal device
# or to pick a specific device:
# mc.init(device_index)

mc.compile(program, function_name)
# Will raise exception with details if metal kernel has errors

mc.run(input_f32_or_u8_array, output_f32_or_u8_array, kernel_call_count)
# Run the kernel once with supplied input data, 
# filling supplied output data
# Specify number of kernel calls

mc.release()
# Call after use

```

## Examples

### Measure TFLOPS of GPU

```
> metalcompute-measure-flops
Using device: Apple M1 (unified memory=True)
Running compute intensive Metal kernel to measure TFLOPS...
Estimated GPU TFLOPS: 2.55613
```

### Render a 3D image with raymarching

```
# Usage: metalcompute-raymarch [<width> <height> [<output image file: PNG, JPG>]]

> metalcompute-raymarch.py 1024 1024 raymarch.jpg
Render took 0.0119569s
```

![Raymarched spheres scene](images/raymarch.jpg)

### Mandelbrot set

```
# Usage: metalcompute-mandelbrot [<width> <height> [<output image file: PNG, JPG>]]

> metalcompute-mandelbrot
Rendering mandelbrot set using Metal compute, res:4096x4096, iters:8192
Render took 0.401446s
Writing image to mandelbrot.png
Image encoding took 1.35182s
```

![Mandelbrot set](images/mandelbrot.jpg)

## Status

This is an early preview version. 
