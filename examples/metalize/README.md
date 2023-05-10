# Metalize

Metalize is a library for python on macOS that lets you run simple python functions very quickly on your GPU

## Simple example

The ```add``` function in this example will add two arrays together on your GPU.

```
from metalize import metalize
import numpy as np

@metalize # 'add' will run on GPU
def add(a, b):
    return a + b

count = 100000000
a = np.arange(count)
b = np.ones(count)
c = add(a,b)
print(c[-1]) 
# Should print 100000000
```

The same function can be run on the CPU just by commenting out the ```@metalize``` decorator on the line before the add function.

The speed difference is small, because the function does very little work.

## A more complex example

In the next example, the ```julia``` function runs on the GPU. Depending on your GPU this can take 0.1-0.5s.

```
import math, time

from metalize import metalize

import numpy as np
np.seterr(all='ignore') # Hide float warnings

from PIL import Image

@metalize 
def julia(x, y):
    R = 100
    zi = x
    zr = y
    cx = -1.2
    cy = 0.05
    i = 0
    m = 0
    for _ in range(1000):
        zi2 = (zi * zi) - (zr * zr) + cx
        zr2 = (2 * zi * zr) + cy
        mag = (zi2 * zi2) + (zr2 * zr2)
        zi = zi2
        zr = zr2
        inside = mag < (R*R)
        i = i + (inside*1.0)
    return i

w = 4096
h = 4096
x = np.linspace(-2, 2, w)
y = np.linspace(-2, 2, h)
xv, yv = np.meshgrid(x, y)
s = time.time()
j = julia(xv, yv)
e = time.time()
print(e-s)
m = np.array((j,j,j)).transpose(1,2,0)
m[:,:,0] = 0.5+0.5*(np.sin(m[:,:,0]*.7))
m[:,:,1] = 0.5+0.5*(np.sin(m[:,:,1]*1.1))
m[:,:,2] = 0.5+0.5*(np.sin(m[:,:,2]*1.3))
i = (255.0*m).astype(np.uint8)

Image.fromarray(i).save("julia.png")
```

If this function is run on the CPU (by commenting out ```@metalize```), it can take as long as *100s* - ***1000*** times slower than with metalize.

## Features

Metalize understands a very limited set of operations

- +, -, *, /
- arguments can be arrays or constants
- greater than and less than comparisons
- sin, cos, log, log2 - available with dot syntax, e.g. ```a.sin``` is equivalent to ```np.sin(a)```
- ```if/elif/else``` statements cannot be used
- for loops can be used
- all supplied arguments (arrays) must have same shape and returned values will have the same shape