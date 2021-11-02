from distutils.command import build_ext as build_module
from distutils.core import setup, Extension
import os

def build_swift():
    print("Building swift part")
    os.system("swiftc metalcompute.swift -emit-library -emit-module -target arm64-apple-macos12 -o libmetalcomputearm.dylib")
    os.system("swiftc metalcompute.swift -emit-library -emit-module -target x86_64-apple-macos12 -o libmetalcomputex64.dylib")
    os.system("lipo -create libmetalcomputearm.dylib libmetalcomputex64.dylib -o libmetalcompute.dylib")
    os.system("rm -f metalcomputearm.* metalcomputex64.*")


class build(build_module.build_ext):
    def run(self):
        build_swift()
        build_module.build_ext.run(self)

setup(name="MetalCompute",
      version="1.0",
      cmdclass = {'build_ext': build,},
      ext_modules=[Extension(
          'metalcompute', 
          ['metalcompute.c'], 
          extra_compile_args=["-mmacosx-version-min=12.0"],
          extra_link_args=["-mmacosx-version-min=12.0"],
          library_dirs=["."],
          libraries=["metalcompute"])],
    )
