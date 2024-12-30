#pragma once

#include <cuda.h>
#include <cuda_runtime.h>

#ifdef _WIN64
#include <windows.h>
#endif

#ifndef __ANDROID__
#define VK_NO_PROTOTYPES // for volk
// max has been made a macro. This happens at some point inside windows.h.
// Define NOMINMAX prior to including to stop windows.h from doing that.
// volk.h when set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR) will include windows.h
#define NOMINMAX 
#include "volk.h"
#endif

namespace cudaEngine
{
    void selectDevice();

#ifdef _WIN64  // For windows
  HANDLE getVkImageMemoryHandle(VkDevice logicalDevice, VkExternalMemoryHandleTypeFlagsKHR externalMemoryHandleType);
  //HANDLE getVkSemaphoreHandle(VkDevice logicalDevice, VkExternalSemaphoreHandleTypeFlagBitsKHR externalSemaphoreHandleType, VkSemaphore& semVkCuda);
#else
  int getVkImageMemHandle(VkDevice logicalDevice, VkExternalMemoryHandleTypeFlagsKHR externalMemoryHandleType);
  int getVkSemaphoreHandle(VkDevice logicalDevice, VkExternalSemaphoreHandleTypeFlagBitsKHR externalSemaphoreHandleType, VkSemaphore& semVkCuda);
#endif
}