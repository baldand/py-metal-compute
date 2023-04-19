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
        function scale(event) {
            frame_scale = frame_scale * 2
            if (frame_scale == 16) frame_scale = 1;
        }
        function toggle(event) {
            if (event.type != "pointerdown") return;
            running = !running;
            if (running) {
                started = true;
                queueRaf();
            }
            update();
        }
        function queueRaf() {
            update();
            if (running && raf_handle==0) {
                raf_handle = window.requestAnimationFrame(raf);
            }
        }
        function sleep(ms) {
            return new Promise( resolve => setTimeout (resolve, ms));
        }
        async function raf(timestamp) {
            let timediff = timestamp - last_timestamp;
            if ((timediff>0)&&(!started)) {
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
                response = await fetch('video?t='+now+'&w='+width+'&h='+height);
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
            //console.log(response);
            const buf = await response.arrayBuffer();
            const view = new Uint8ClampedArray(buf);
            if ((width != canvas.width) || (height != canvas.height)) {
                canvas.width = width;
                canvas.height = height;
                data = ctx.getImageData(0,0,width,height);
            }
            data.data.set(view);
            ctx.putImageData(data,0,0);
            //console.log(response, data, buf, view);
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
        window.onload = async function (){
            let state = document.getElementById("state");
            canvas = document.getElementById("canvas");
            ctx = canvas.getContext("2d");
            state.addEventListener("pointerdown", toggle);
            queueRaf();
        };
        </script>
        </head>
        <body style="margin:0;overflow:clip;width:100%;height:100%;background:black;">
        <div style="color:white;position:absolute;">
            <button style="margin:3px;" id="scale" onclick="scale();"></button>
            <button style="margin:3px;" id="state" onclick="toggle();">Pause</button>
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

    def render(self, height, width, timestamp):
        uniforms = array('f',[height,width,timestamp*0.001])
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
        buf = bytes()
        try:
            self.update_shader()
            buf = self.render(height, width, timestamp)
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
