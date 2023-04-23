#include <metal_stdlib>;
using namespace metal;

kernel void render(const device float *uniform [[ buffer(0) ]],
                   device uchar4 *out [[ buffer(1) ]],
                   uint id [[ thread_position_in_grid ]]) {
    float width = uniform[1];
    float height = uniform[0];
    float time = uniform[2];
    float zoom = uniform[3];
    float2 center = float2(uniform[4],uniform[5]);

    // Calculate pixel coordinate
    int cx = id%int(width);
    int cy = int(id/int(width));
    // x -1 on left, +1 on right
    float scale = 2.7*pow(2.0,zoom);
    float x = scale*(cx-(0.5*width))/height;
    // y -1 on bottom, +1 on right
    float y = -scale*(cy-(0.5*height))/height;

    x += center.x;
    y += center.y;

    // Render an interesting image using x,y,time

    float speed = 0.01;
    time = speed*time + 1.2*sin(speed*time*0.3828);

    float amp = 0.8+0.2*sin(time*9.119281);
    float2 c = float2(
        amp*sin(time),
        amp*cos(time));

    // Calculate julia sets
    float R = 100.0;
    float2 z = float2(x,y);
    float i = 0.0;
    float m_old = 0.0;
    float m_new = 0.0;
    for (int iter=0;iter<1000;iter++) {
        float2 z2 = float2(z.x*z.x-z.y*z.y+c.x,
                           2.0*z.x*z.y+c.y);   
        z = z2;
        m_new = (z.x*z.x)+(z.y*z.y);
        if (m_new>(R*R)) {
            break;
        }
        m_old = m_new;
        i += 1.0;
    }

    float add = log2(log(sqrt(m_new))/log(R))-1.;
    i -= add;
    float huespeed = 8.;
    i = log2(i);
    float3 rgb = float3(0.5+0.4*sin(huespeed*0.31*i),0.5+0.4*sin(huespeed*0.53*i),0.5+0.4*sin(huespeed*0.71*i));
    // Output RGBA in range 0..255

    float a = 1.0;
    out[id] = uchar4(255*rgb.r,255*rgb.g,255*rgb.b,255);
}
