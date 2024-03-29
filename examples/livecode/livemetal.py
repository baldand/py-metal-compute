import argparse
import sys
import traceback
import time
from pathlib import Path
from array import array
import asyncio 

try: 
    import metalcompute as mc
except:
    raise Exception("metalcompute not installed\nPlease do 'pip install metalcompute'\n")

try:
    import aiohttp
    from aiohttp import web
except:
    raise Exception("aiohttp not installed\nPlease do 'pip install aiohttp'\n")

class MetalViewHTTPServer:
    def __init__(self):
        self.parseargs()
        self.shader_file = Path(self.args.shader_file)
        self.start = time.time()
        self.running = 1.0
        self.dev = mc.Device()
        self.image = None
        self.width = None
        self.height = None
        self.ui_running = False
        self.last_modified = None
        self.count = 0
        self.update_shader()

    def parseargs(self):
        parser = argparse.ArgumentParser()
        parser.add_argument("shader_file",help="Metal shader file to watch")
        self.args = parser.parse_args()

    def update_shader(self):
        last_modified = self.shader_file.stat().st_mtime
        if self.last_modified is None or last_modified > self.last_modified:
            self.shader = self.shader_file.read_text()
            self.last_modified = last_modified

            sys.stderr.write(f"Compiling[{self.count}]:\n")
            try:
                self.shader_kernel = self.dev.kernel(self.shader).function("render")
            except:
                exc = traceback.format_exc()
                exc = exc.replace("program_source","shader.metal")
                sys.stderr.write(exc)
            sys.stderr.write(f"Complete[{self.count}].\n")
            self.count += 1

    async def page(self, request):
        text = """
        <html style="margin:0;overflow:clip;width:100%;height:100%;background:black;">
        <head>
        <script>
        let now = 0;
        let last_timestamp = 0;
        let running = true;
        let started = false;
        let raf_handle = 0;
        let frame_scale = 2;
        let data = 0;
        let canvas = 0;
        let ctx = 0;
        let zoom = 0.0;
        let x = 0.0;
        let y = 0.0;
        let basex = 0.0;
        let basey = 0.0;
        let panstartx = 0.0;
        let panstarty = 0.0;
        function update() {
            let b = document.getElementById("state");
            let fps = document.getElementById("fps");
            let s = document.getElementById("scale");
            if (running) {
                b.innerText = "Pause";
                s.innerText = frame_scale+"x";
            } else {
                b.innerText = "Play";
                fps.innerText = "0.00";
                s.innerText = frame_scale+"x";
            }
        }
        function cyclescale(event) {
            event.preventDefault();
            if (event.type != "pointerdown") return;
            frame_scale = frame_scale * 2
            if (frame_scale == 16) frame_scale = 1;
            queueRaf(true);
        }
        function toggle(event) {
            event.preventDefault();
            if (event.type != "pointerdown") return;
            running = !running;
            if (running) {
                started = true;
                queueRaf();
            } else {
                update();
            }
        }
        function queueRaf(force=false) {
            update();
            if ((running || force) && raf_handle==0) {
                raf_handle = window.requestAnimationFrame(raf);
            }
        }
        function sleep(ms) {
            return new Promise( resolve => setTimeout (resolve, ms));
        }
        async function raf(timestamp) {
            let timediff = timestamp - last_timestamp;
            if ((timediff>0)&&(!started)&&running) {
                now += timediff;
            }
            last_timestamp = timestamp;
            started = false;
            raf_handle = 0;
            let response;
            // Get current real size of canvas element
            let width = (canvas.clientWidth * window.devicePixelRatio) / frame_scale;
            let height = (canvas.clientHeight * window.devicePixelRatio) / frame_scale;
            try {
                response = await fetch('video?t='+now+'&w='
                                        +width+'&h='+height
                                        +'&z='+zoom+'&x='+x+'&y='+y);
                if ((response.status < 200)||(response.status >= 300)) {
                    // Delay to balance responsiveness & power consumption
                    await sleep(100);
                    queueRaf();
                    return;
                } else {
                    queueRaf();
                }
            } catch (e) {
                await sleep(100);
                queueRaf();
                return;
            }
            const buf = await response.arrayBuffer();
            const view = new Uint8ClampedArray(buf);
            if ((width != canvas.width) || (height != canvas.height)) {
                canvas.width = width;
                canvas.height = height;
                data = ctx.getImageData(0,0,width,height);
            }
            data.data.set(view);
            ctx.putImageData(data,0,0);
            let fps = document.getElementById("fps");
            let tims = document.getElementById("time");
            let s = document.getElementById("scale");
            if (running) {
                fps.innerText = (1000.0/(timediff)).toFixed(2);
                time.innerText = (now*0.001).toFixed(3);
                s.innerText = frame_scale+"x";
            } else {
                fps.innerText = "0.00";
                time.innerText = (now*0.001).toFixed(3);
                s.innerText = frame_scale+"x";
            }
        }
        function resize(event) {
            // Trigger new frame
            queueRaf(true);
        }
        function zoomevent(event) {
            event.preventDefault();
            zoom += 0.01*event.deltaY; 
            queueRaf(true);
        }
        function screen2norm(x, y) {
            // Convert a screen x,y value into normalised -1<->+1 coordinates
            // Need to check current CSS width/height of canvas
            // and calculate aspect ratio from that
            let canvas = document.getElementById("canvas");
            let cw = canvas.clientWidth;
            let ch = canvas.clientHeight;
            let shortest = cw*0.5;
            if (ch < cw) shortest = ch*0.5;
            let cx = (x - cw*0.5)/shortest;
            let cy = (y - ch*0.5)/shortest;
            return [cx,-cy];
        }
        function panstart(event) {
            let canvas = document.getElementById("canvas");
            event.preventDefault();
            canvas.setPointerCapture(event.pointerId);
            canvas.addEventListener("pointermove", panmove)
            let start = screen2norm(event.clientX, event.clientY);
            panstartx = start[0];
            panstarty = start[1];
            basex = x;
            basey = y;
        }
        function panend(event) {
            let canvas = document.getElementById("canvas");
            event.preventDefault();
            canvas.releasePointerCapture(event.pointerId);
            canvas.removeEventListener("pointermove", panmove)
            let current = screen2norm(event.clientX, event.clientY);
            x = basex + (panstartx-current[0])*Math.pow(2.0,zoom);
            y = basey + (panstarty-current[1])*Math.pow(2.0,zoom);
            queueRaf(true);
        }
        function panmove(event) {
            event.preventDefault();
            let current = screen2norm(event.clientX, event.clientY);
            x = basex + (panstartx-current[0])*Math.pow(2.0,zoom);
            y = basey + (panstarty-current[1])*Math.pow(2.0,zoom);
            queueRaf(true);
        }
        window.onload = async function (){
            canvas = document.getElementById("canvas");
            canvas.addEventListener("wheel", zoomevent);
            canvas.addEventListener("pointerdown", panstart)
            canvas.addEventListener("pointerup", panend)
            ctx = canvas.getContext("2d");
            let state = document.getElementById("state");
            state.addEventListener("pointerdown", toggle);
            let scale = document.getElementById("scale");
            scale.addEventListener("pointerdown", cyclescale);
            window.addEventListener("resize", resize)
            queueRaf(true);
        };
        </script>
        </head>
        <body style="margin:0;overflow:clip;width:100%;height:100%;background:black;">
        <div style="color:white;position:absolute;">
            <button style="margin:3px;" id="scale"></button>
            <button style="margin:3px;" id="state">Pause</button>
            <div style="margin:3px;">FPS:<span id="fps"></span></div>
            <div style="margin:3px;">TIME:<span id="time"></span></div>
        </div>
        <div style="
            width:100%;
            height:100%;
            display:flex;
            flex-direction:column;
            align-items:stretch;
            justify-content:stretch;
            margin:0;
            background:black;
            overflow:clip;">
        <canvas id="canvas" width="1280" height="720" style="
            max-width: 100%;
            max-height: 100%;
            flex-grow: 2;
            object-fit: contain;
            ">
        </canvas>
        </div>
        </body>
        </html>
        """
        return web.Response(text=text, content_type="text/html")

    def create_image(self, height, width):
        if self.height != height or self.width != width:
            del self.image
            self.image = None
        if self.image is None:
            self.height = height
            self.width = width
            self.image = self.dev.buffer((self.height * self.width * 4))
            #print("new image", height, width)

    def render(self, height, width, timestamp, zoom, x, y):
        uniforms = array('f',[height,width,timestamp*0.001, zoom, x, y])
        self.create_image(height, width)
        start = time.time()
        self.shader_kernel(height*width, uniforms, self.image)
        end = time.time()
        #print(end-start, width, height)
        return bytearray(self.image)

    async def video(self, request):
        timestamp = float(request.query["t"])
        width = int(float(request.query["w"]))
        height = int(float(request.query["h"]))
        zoom = float(request.query["z"])
        x = float(request.query["x"])
        y = float(request.query["y"])
        buf = bytes()
        try:
            self.update_shader()
            buf = self.render(height, width, timestamp, zoom, x, y)
        except:
            pass
        return web.Response(body=buf, status=200, content_type="application/octet-stream")

    async def init(self):
        self.app = web.Application()
        self.app.add_routes([web.get('/', self.page),
                    web.get('/video', self.video)])
        self.runner = web.AppRunner(self.app)
        await self.runner.setup()
        self.site = web.TCPSite(self.runner, 'localhost', 1173)
        await self.site.start()

async def amain():
    v = MetalViewHTTPServer()
    await v.init()
    while True:
        await asyncio.sleep(60)

if __name__ == '__main__':
    loop = asyncio.get_event_loop()
    loop.run_until_complete(amain())
    loop.close()
