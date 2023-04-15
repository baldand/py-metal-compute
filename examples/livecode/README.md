# Livecode visual metal kernels in VSCode

The script `livemetal.py` can be used on macOS to livecode with visual metal shader files in VSCode.

## Dependencies

The following python libraries must be installed, e.g. using pip install into a virtualenv.

1. metalcompute
2. aiohttp

## Setup

The script is intended to be used from a VSCode task defined in the supplied file `.vscode/tasks.json`

Open the source directory in VSCode.

Select the menu `Terminal | Run Task...`

Select `1. Run LiveMetal server`

This will run the rendering server in the Terminal pane.

Select again `Terminal | Run Task...``

Select `2. View LiveMetal page`

This will open a simple browser connected to the local livemetal server.

Now edit `shader.metal`

## Usage

The shader will be rendered in real time, e.g. up to 120 FPS on a recent MacBook Pro.

Whenever the shader file is saved, the view will update
to the new version.

If there are syntax errors, they will be highlighted in the editor.

Close the simple browser view when not in use to stop rendering and reduce power consumption.