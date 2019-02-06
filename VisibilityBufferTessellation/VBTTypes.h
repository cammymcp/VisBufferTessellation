#ifndef VBT_TYPES_H
#define VBT_TYPES_H

#include <vulkan\vulkan.h>
#include <vector>
#include <optional>

#pragma region SwapChain
struct SwapChainSupportDetails
{
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};
#pragma endregion

#pragma region Device Queues
struct DeviceQueues
{
	VkQueue graphics;
	VkQueue present;
};

struct QueueFamilyIndices
{
	std::optional<uint32_t> graphicsFamily;
	std::optional<uint32_t> presentationFamily;

	bool isSuitable()
	{
		return graphicsFamily.has_value() && presentationFamily.has_value();
	}
};
#pragma endregion

#endif // !HELPERTYPES_H
