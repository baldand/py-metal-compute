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
        self.image = self.dev.buffer((self.args.height * self.args.width * 4))
        self.ui_running = False
        self.last_modified = None
        self.count = 0
        self.update_shader()

    def parseargs(self):
        parser = argparse.ArgumentParser()
        parser.add_argument("shader_file",help="Metal shader file to watch")
        parser.add_argument("-width",default=1280,help="Width of images")
        parser.add_argument("-height",default=720,help="Height of images")
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
        function queueRaf() {
            window.requestAnimationFrame(raf);
        }
        function sleep(ms) {
            return new Promise( resolve => setTimeout (resolve, ms));
        }
        async function raf(timestamp) {
            let response;
            try {
                response = await fetch('video');
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
            const canvas = document.getElementById("canvas");
            const ctx = canvas.getContext("2d");
            let data = ctx.getImageData(0,0,1280,720);
            data.data.set(view);
            ctx.putImageData(data,0,0);
            //console.log(response, data, buf, view);
        }
        window.onload = async function (){
            queueRaf();
        };
        </script>
        </head>
        <body style="margin:0;overflow:clip;width:100%;height:100%;background:black;">
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

    def render(self):
        uniforms = array('f',[self.args.height,self.args.width,time.time()-self.start])
        self.shader_kernel(self.args.height*self.args.width, uniforms, self.image)
        return bytearray(self.image)

    async def video(self, request):
        buf = bytes()
        try:
            self.update_shader()
            buf = self.render()
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
