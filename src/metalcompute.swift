/*
metalcompute.swift

Swift backend for Python extension to use Metal for compute

(c) Andrew Baldwin 2021
*/

import MetalKit

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

// Integers so we can understand these from C/Python sidea
let Success = 0
let CannotCreateDevice = -1
let CannotCreateCommandQueue = -2
let NotReadyToCompile = -3
let FailedToCompile = -4
let FailedToFindFunction = -5
let NotReadyToCompute = -6
let FailedToMakeInputBuffer = -7
let FailedToMakeOutputBuffer = -8
let NotReadyToRun = -9
let CannotCreateCommandBuffer = -10
let CannotCreateCommandEncoder = -11
let CannotCreatePipelineState = -12
let IncorrectOutputCount = -13
let NotReadyToRetrieve = -14
let UnsupportedInputFormat = -15
let UnsupportedOutputFormat = -16

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

@_cdecl("mc_sw_init") public func mc_sw_init() -> Int {
    guard let newDevice = MTLCreateSystemDefaultDevice() else {
        return CannotCreateDevice 
    }
    device = newDevice
    guard let newCommandQueue = newDevice.makeCommandQueue() else {
        return CannotCreateCommandQueue 
    } 
    commandQueue = newCommandQueue
    readyToCompile = true
    readyToCompute = false

    return Success
}

@_cdecl("mc_sw_release") public func mc_sw_release() -> Int {
    return Success
}

@_cdecl("mc_sw_compile") public func mc_sw_compile(programRaw: UnsafePointer<CChar>, functionNameRaw: UnsafePointer<CChar>) -> Int {
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

@_cdecl("mc_sw_alloc") public func mc_sw_alloc(icount: Int, input: UnsafeRawPointer, iformat: Int, ocount: Int, oformat: Int) -> Int {
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

@_cdecl("mc_sw_run") public func mc_sw_run(kcount:Int) -> Int {
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

@_cdecl("mc_sw_retrieve") public func mc_sw_retrieve(ocount:Int, output: UnsafeMutableRawPointer) -> Int {
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

