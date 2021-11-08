from distutils.command import build_ext as build_module
from distutils.core import setup, Extension
import os

def build_swift():
    print("Building swift part")
    os.system("swiftc metalcompute.swift -static -emit-library -target arm64-apple-macos12 -o metalcomputeswiftarm.a")
    os.system("swiftc metalcompute.swift -static -emit-library -target x86_64-apple-macos12 -o metalcomputeswiftx64.a")
    os.system("lipo -create metalcomputeswiftarm.a metalcomputeswiftx64.a -o metalcomputeswift.a")
    os.system("rm -f metalcomputeswiftarm.* metalcomputeswiftx64.*")


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
          library_dirs=[".","/usr/lib","/usr/lib/swift"],
          libraries=["swiftFoundation","swiftMetal"],
          extra_objects=["metalcomputeswift.a"])],
    )
