import math, time

from metalize import metalize

import numpy as np
np.seterr(all='ignore') # Hide float warnings

from PIL import Image

@metalize # Comment this out to run same with pure NumPy
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