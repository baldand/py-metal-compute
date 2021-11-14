/*
metalcompute.swift

Swift backend for Python extension to use Metal for compute

(c) Andrew Baldwin 2021
*/

import MetalKit

import mc_c2sw // Bridging C to Swift

let Success:RetCode = 0
let CannotCreateDevice:RetCode = -1
let CannotCreateCommandQueue:RetCode = -2
let NotReadyToCompile:RetCode = -3
let FailedToCompile:RetCode = -4
let FailedToFindFunction:RetCode = -5
let NotReadyToCompute:RetCode = -6
let FailedToMakeInputBuffer:RetCode = -7
let FailedToMakeOutputBuffer:RetCode = -8
let NotReadyToRun:RetCode = -8
let CannotCreateCommandBuffer:RetCode = -9
let CannotCreateCommandEncoder:RetCode = -10
let CannotCreatePipelineState:RetCode = -11
let IncorrectOutputCount:RetCode = -12
let NotReadyToRetrieve:RetCode = -13
let UnsupportedInputFormat:RetCode = -14
let UnsupportedOutputFormat:RetCode = -15
// Reserved block for Swift level errors

// Buffer formats
let FormatUnknown = -1
let FormatI8 = 0
let FormatU8 = 1
let FormatI16 = 2
let FormatU16 = 3
let FormatI32 = 4
let FormatU32 = 5
let FormatI64 = 6
let FormatU64 = 7
let FormatF16 = 8
let FormatF32 = 9
let FormatF64 = 10


// -------------------------------------------------
// v0.1 of API - simple functions and retained state
//
// Can only be used for single kernel 
// sequential synchronous (non-pipelined) execution
// Data must be copied in/out (copy overhead)

var device:MTLDevice?
var commandQueue:MTLCommandQueue?
var library:MTLLibrary?
var function:MTLFunction?
var inputBuffer:MTLBuffer?
var inputCount:Int = 0;
var inputStride:Int = 0
var outputBuffer:MTLBuffer?
var outputCount:Int = 0
var outputStride:Int = 0
var readyToCompile = false
var readyToCompute = false
var readyToRun = false
var readyToRetrieve = false
var compileError:String = ""

@_cdecl("mc_sw_init") public func mc_sw_init(device_index_i64:Int64) -> RetCode {
    let device_index = Int(device_index_i64)
    let devices = MTLCopyAllDevices()
    guard let defaultDevice = MTLCreateSystemDefaultDevice() else {
        return CannotCreateDevice
    }
    if devices.count == 0 {
        return CannotCreateDevice
    }
    if device_index >= devices.count {
        return CannotCreateDevice
    }
    let newDevice = device_index < 0 ? defaultDevice : devices[device_index] 
    device = newDevice
    guard let newCommandQueue = newDevice.makeCommandQueue() else {
        return CannotCreateCommandQueue 
    } 
    commandQueue = newCommandQueue
    readyToCompile = true
    readyToCompute = false

    return Success
}


@_cdecl("mc_sw_release") public func mc_sw_release() -> RetCode {
    inputBuffer = nil
    outputBuffer = nil
    function = nil
    library = nil
    device = nil
    readyToCompile = false
    readyToCompute = false
    readyToRun = false
    readyToRetrieve = false

    return Success
}

@_cdecl("mc_sw_compile") public func mc_sw_compile(programRaw: UnsafePointer<CChar>, functionNameRaw: UnsafePointer<CChar>) -> RetCode {
    guard readyToCompile else { return NotReadyToCompile }
    guard let lDevice = device else { return NotReadyToCompile }

    // Convert c strings to Swift String
    let program = String(cString:programRaw)
    let functionName = String(cString:functionNameRaw)

    let options = MTLCompileOptions();
    options.fastMathEnabled = true
    options.languageVersion = .version2_3
    do {
        let newLibrary = try lDevice.makeLibrary(source: program, options:options) 
        library = newLibrary
        guard let newFunction = newLibrary.makeFunction(name: functionName) else { return FailedToFindFunction }
        function = newFunction
    } catch {
        compileError = error.localizedDescription
        return FailedToCompile
    }

    readyToCompute = true
    readyToRun = false

    return Success; 
}

func get_stride(_ format: Int) -> Int {
    if (format == FormatF32) {
        return MemoryLayout<Float>.stride;
    } else if (format == FormatU8) {
        return MemoryLayout<UInt8>.stride;
    } else {
        return 0;
    }
}

@_cdecl("mc_sw_alloc") public func mc_sw_alloc(icount: Int, input: UnsafeRawPointer, iformat: Int, ocount: Int, oformat: Int) -> RetCode {
    // Allocate input/output buffers for run
    // Separating this step allows python global lock to be released for the actual run which does not need any python objects
    guard readyToCompute else { return NotReadyToCompute }
    guard let lDevice = device else { return NotReadyToCompute }

    inputStride = get_stride(iformat);
    guard inputStride != 0 else { return UnsupportedInputFormat }

    outputStride = get_stride(oformat);
    guard outputStride != 0 else { return UnsupportedOutputFormat }

    guard let newInputBuffer = lDevice.makeBuffer(bytes: input, length: inputStride * icount, options: .storageModeShared) else { return FailedToMakeInputBuffer }
    guard let newOutputBuffer = lDevice.makeBuffer(length: outputStride * ocount, options: .storageModeShared) else { return FailedToMakeOutputBuffer }

    inputBuffer = newInputBuffer
    outputBuffer = newOutputBuffer
    inputCount = icount
    outputCount = ocount
    readyToRun = true
    readyToRetrieve = false

    return Success
}

@_cdecl("mc_sw_run") public func mc_sw_run(kcount:Int) -> RetCode {
    // Execute the configured compute task, waiting for completion
    guard readyToRun else { return NotReadyToRun }
    guard let lDevice = device else { return NotReadyToRun }
    guard let lFunction = function else { return NotReadyToRun }
    guard let lCommandQueue = commandQueue else { return NotReadyToRun }
    guard let lInputBuffer = inputBuffer else { return NotReadyToRun }
    guard let lOutputBuffer = outputBuffer else { return NotReadyToRun }
    guard let commandBuffer = lCommandQueue.makeCommandBuffer() else { return CannotCreateCommandBuffer }
    guard let encoder = commandBuffer.makeComputeCommandEncoder() else { return CannotCreateCommandEncoder }

    do {
        let pipelineState = try lDevice.makeComputePipelineState(function:lFunction)
        encoder.setComputePipelineState(pipelineState);
        encoder.setBuffer(lInputBuffer, offset: 0, index: 0)
        encoder.setBuffer(lOutputBuffer, offset: 0, index: 1)
        let w = pipelineState.threadExecutionWidth
        let h = pipelineState.maxTotalThreadsPerThreadgroup / w
        let numThreadgroups = MTLSize(width: (kcount+(w*h-1))/(w*h), height: 1, depth: 1)
        let threadsPerThreadgroup = MTLSize(width: w*h, height: 1, depth: 1)
        //print(numThreadgroups, threadsPerThreadgroup)
        encoder.dispatchThreadgroups(numThreadgroups, threadsPerThreadgroup: threadsPerThreadgroup)
        encoder.endEncoding()
        commandBuffer.commit()
        commandBuffer.waitUntilCompleted()

    } catch {
        return CannotCreatePipelineState
    }

    readyToRetrieve = true

    return Success
}

@_cdecl("mc_sw_retrieve") public func mc_sw_retrieve(ocount:Int, output: UnsafeMutableRawPointer) -> RetCode {
    // Return result of compute task
    guard readyToRetrieve else { return NotReadyToRetrieve }
    guard ocount == outputCount else { return IncorrectOutputCount }
    guard let lOutputBuffer = outputBuffer else { return NotReadyToRetrieve }

    output.copyMemory(from: lOutputBuffer.contents(), byteCount: outputCount * outputStride)

    return Success
}

@_cdecl("mc_sw_get_compile_error") public func mc_sw_get_compile_error() -> UnsafeMutablePointer<CChar> {
    return strdup(compileError)
}

// ------------------------------
// v0.2 of the API - object based
//
// - Multiple devices can be opened
// - Buffers are allocated and filled/emptied externally
// - Multiple kernel objects are possible
// - Kernels can be pipelined to devices
// - Callbacks when kernels are completed and data is available

struct mc_sw_fn {
    var fn:MTLFunction;
}

struct mc_sw_kern {
    var mc_sw_fns:[mc_sw_fn]
}

struct mc_sw_dev {
    var dev:MTLDevice;
    var mc_sw_kerns:[mc_sw_kern];
};

// Index of next object
var mc_next_index:Int = 4242; 
var mc_devs:[mc_sw_dev] = [];

@_cdecl("mc_sw_count_devs") public func mc_sw_get_devices(shared: UnsafeMutablePointer<mc_shared>) -> RetCode {
    let metal_devices = MTLCopyAllDevices();

    shared[0].dev_count = Int64(metal_devices.count);
    let dev_array = UnsafeMutablePointer<mc_dev>.allocate(capacity: metal_devices.count) // Must be freed by python  
    shared[0].devs = dev_array 
    for dev_index in 0..<metal_devices.count {
        let dev = metal_devices[dev_index]
        dev_array[dev_index].recommendedMaxWorkingSetSize = Int64(dev.recommendedMaxWorkingSetSize);
        dev_array[dev_index].maxTransferRate = Int64(dev.maxTransferRate);
        dev_array[dev_index].hasUnifiedMemory = Bool(dev.hasUnifiedMemory);
        dev_array[dev_index].name = strdup(dev.name) // Copy - must be released by python side
    }

    return Success
}

@_cdecl("mc_sw_open_dev") public func mc_sw_open_dev() -> RetCode {
    // Allocate a HW buffer of the requested size
    return Success
}