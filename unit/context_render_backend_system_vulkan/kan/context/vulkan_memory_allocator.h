#pragma once

#include <kan/api_common/mute_warnings.h>
#include <kan/error/critical.h>
#include <kan/log/logging.h>

// Order of includes matters here.
// clang-format off
#define VK_NO_PROTOTYPES __CUSHION_PRESERVE__
#include <vulkan/vulkan.h>
#include <volk.h>
// clang-format on

#define VMA_HEAVY_ASSERT(expr) __CUSHION_PRESERVE__ KAN_ASSERT (expr)
#define VMA_DEDICATED_ALLOCATION __CUSHION_PRESERVE__ 0
#define VMA_DEBUG_MARGIN __CUSHION_PRESERVE__ 16
#define VMA_DEBUG_DETECT_CORRUPTION __CUSHION_PRESERVE__ 0
#define VMA_DEBUG_MIN_BUFFER_IMAGE_GRANULARITY __CUSHION_PRESERVE__ 256
#define VMA_USE_STL_SHARED_MUTEX __CUSHION_PRESERVE__ 0
#define VMA_MEMORY_BUDGET __CUSHION_PRESERVE__ 0
#define VMA_STATS_STRING_ENABLED __CUSHION_PRESERVE__ 0
#define VMA_MAPPING_HYSTERESIS_ENABLED __CUSHION_PRESERVE__ 0
#define VMA_KHR_MAINTENANCE5 __CUSHION_PRESERVE__ 0

#define VMA_VULKAN_VERSION __CUSHION_PRESERVE__ 1001000 // Vulkan 1.1

#define VMA_DEBUG_LOG(...) __CUSHION_PRESERVE__ KAN_LOG (vulkan_memory_allocator, KAN_LOG_DEBUG, __VA_ARGS__)

KAN_MUTE_THIRD_PARTY_WARNINGS_BEGIN
#include <vk_mem_alloc.h>
KAN_MUTE_THIRD_PARTY_WARNINGS_END
