#include <metal_stdlib>;
using namespace metal;

kernel void render(const device float *uniform [[ buffer(0) ]],
                   device uchar4 *out [[ buffer(1) ]],
                   uint id [[ thread_position_in_grid ]]) {
    float width = uniform[1];
    float height = uniform[0];
    float time = uniform[2];
    // Calculate pixel coordinate
    int cx = id%int(width);
    int cy = int(id/int(width));
    // x -1 on left, +1 on right
    float x = 2.0*(cx-(0.5*width))/height;
    // y -1 on bottom, +1 on right
    float y = -2.0*(cy-(0.5*height))/height;

    // Render an interesting image using x,y,time

    // Make rate of time non-linear
    float speed = 0.01;
    time = speed*time + 1.1*sin(speed*time);

    float amp = 0.7+0.1*sin(100.0*time);
    float2 c = float2(
        amp*sin(time),
        amp*cos(time));

    // Calculate julia sets
    float2 z = float2(x,y);
    float i = 0.0;
    for (int iter=0;iter<100;iter++) {
        float2 z2 = float2(z.x*z.x-z.y*z.y+c.x,
                           2.0*z.x*z.y+c.y);   
        z = z2;
        if ((z.x*z.x+z.y*z.y)>1.0) break;
        i += 1.0;
    }

    float3 rgb = float3(0.5+0.5*sin(0.3*i),0.5+0.5*sin(0.5*i),0.5+0.5*sin(0.7*i));

    // Output RGBA in range 0..255
    float a = 1.0;
    out[id] = uchar4(255*rgb.r,255*rgb.g,255*rgb.b,255);
}
