#include <iostream>

#include <cuDevice.h>
#include <cuPredefines.h>

namespace core::cuda
{
    // from cuda-samples repo
    inline int _ConvertSMVer2Cores(int major, int minor)
    {
       // Defines for GPU Architecture types (using the SM version to determine
       // the # of cores per SM
       typedef struct
       {
           int SM; // 0xMm (hexidecimal notation), M = SM Major version,
           // and m = SM minor version
           int Cores;
       } sSMtoCores;

       sSMtoCores nGpuArchCoresPerSM[] = {
           {0x30, 192},
           {0x32, 192},
           {0x35, 192},
           {0x37, 192},
           {0x50, 128},
           {0x52, 128},
           {0x53, 128},
           {0x60, 64},
           {0x61, 128},
           {0x62, 128},
           {0x70, 64},
           {0x72, 64},
           {0x75, 64},
           {0x80, 64},
           {0x86, 128},
           {0x87, 128},
           {0x89, 128},
           {0x90, 128},
           {-1, -1}};

       int index = 0;

       while (nGpuArchCoresPerSM[index].SM != -1)
       {
           if (nGpuArchCoresPerSM[index].SM == ((major << 4) + minor))
           {
               return nGpuArchCoresPerSM[index].Cores;
           }

           index++;
       }

       // If we don't find the values, we default use the previous one
       // to run properly
       printf(
           "MapSMtoCores for SM %d.%d is undefined."
           "  Default to use %d Cores/SM\n",
           major, minor, nGpuArchCoresPerSM[index - 1].Cores);
       return nGpuArchCoresPerSM[index - 1].Cores;
    }

    void selectDevice()
    {
        int nDevices;
        cudaGetDeviceCount(&nDevices);
        printf("Number of devices: %d\n", nDevices);
        size_t maxGFLOPS = 0;
        int selectedDeviceId = 0;
        for (int i = 0; i < nDevices; i++)
        {
           cudaDeviceProp prop;
           cudaGetDeviceProperties(&prop, i);
           printf("Device Number: %d\n", i);
           printf("  Device name: %s\n", prop.name);
           printf("  Memory Clock Rate (MHz): %d\n",
                  prop.memoryClockRate / 1024);
           printf("  Memory Bus Width (bits): %d\n",
                  prop.memoryBusWidth);
           printf("  Peak Memory Bandwidth (GB/s): %.1f\n",
                  2.0 * prop.memoryClockRate * (prop.memoryBusWidth / 8) / 1.0e6);

           printf("  Total global memory (Gbytes) %.1f\n", (float)(prop.totalGlobalMem) / 1024.0 / 1024.0 / 1024.0);
           printf("  Total constant memory (Kbytes) %.1f\n", (float)(prop.totalConstMem) / 1024.0);
           printf("  Shared memory per block (Kbytes) %.1f\n", (float)(prop.sharedMemPerBlock) / 1024.0);
           printf("  Max Threads per block %.1f\n", (float)(prop.maxThreadsPerBlock));
           printf("  Max Threads per SM %.1f\n", (float)(prop.maxThreadsPerMultiProcessor));
           printf("  Max Registers per block %.1f\n", (float)(prop.regsPerBlock));
           printf("  Max Registers per SM %.1f\n", (float)(prop.regsPerMultiprocessor));
           printf("  SM count: %d\n", prop.multiProcessorCount);
           printf("  minor-major: %d-%d\n", prop.minor, prop.major);
           printf("  Warp-size: %d\n", prop.warpSize);
           printf("  device overlap (perform cudaMemcpy and kernel exec simultaneously): %s\n", prop.deviceOverlap ? "yes" : "no");
           printf("  canMapHostMemoryTo Device Address space: %s\n", prop.canMapHostMemory ? "yes" : "no");
           printf("  Concurrent kernels: %s\n", prop.concurrentKernels ? "yes" : "no");
           printf("  Concurrent computation/communication: %s\n\n", prop.deviceOverlap ? "yes" : "no");

           // cudaError_t
           int computeMode = -1, multiProcessorCount = 0, clockRate = 0;
           CUDA_CHECK(cudaDeviceGetAttribute(&computeMode, cudaDevAttrComputeMode, i));
           CUDA_CHECK(cudaDeviceGetAttribute(&clockRate, cudaDevAttrClockRate, i));
           CUDA_CHECK(cudaDeviceGetAttribute(&multiProcessorCount, cudaDevAttrMultiProcessorCount, i));
           auto numSmPerMultiProcessor = _ConvertSMVer2Cores(prop.major, prop.minor);
           auto GFLOPS = multiProcessorCount * numSmPerMultiProcessor * clockRate;
           // GFLOPS stands for "Giga Floating Point Operations Per Second." It is a measure of a co
           std::cout << "GFLOPS: " << GFLOPS << std::endl;

           if (GFLOPS > maxGFLOPS)
           {
               maxGFLOPS = GFLOPS;
               selectedDeviceId = i;
           }
        }
        // since cuda12.0, cudaSetDevice will now explicitly initialize the runtime. check for initialize errors.
        CUDA_CHECK(cudaSetDevice(selectedDeviceId));
    }
}