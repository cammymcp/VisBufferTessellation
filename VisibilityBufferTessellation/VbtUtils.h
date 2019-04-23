#ifndef HELPERFUNCTIONS_H
#define HELPERFUNCTIONS_H

#include "PhysicalDevice.h"
#include <fstream>

namespace vbt
{
	#define SCAST_U32(val) static_cast<uint32_t>(val)

	static VkCommandBuffer BeginSingleTimeCommands(VkDevice& device, VkCommandPool& cmdPool)
	{
		// Set up a command buffer to perform the data transfer
		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = cmdPool;
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer commandBuffer;
		vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

		// Begin recording
		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkBeginCommandBuffer(commandBuffer, &beginInfo);

		return commandBuffer;
	}

	static void EndSingleTimeCommands(VkCommandBuffer commandBuffer, VkDevice& device, PhysicalDevice& physDevice, VkCommandPool& cmdPool)
	{
		vkEndCommandBuffer(commandBuffer);

		// Now execute the command buffer
		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		vkQueueSubmit(physDevice.Queues()->graphics, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(physDevice.Queues()->graphics); // Maybe use a fence here if performing multiple transfers, allowing driver to optimise

		vkFreeCommandBuffers(device, cmdPool, 1, &commandBuffer);
	}

	static void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, VkDevice& device, PhysicalDevice& physDevice, VkCommandPool& cmdPool)
	{
		VkCommandBuffer commandBuffer = BeginSingleTimeCommands(device, cmdPool);

		// Copy the data
		VkBufferCopy copyRegion = {};
		copyRegion.size = size;
		vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

		EndSingleTimeCommands(commandBuffer, device, physDevice, cmdPool);
	}

	static std::vector<char> ReadFile(const std::string& filename)
	{
		std::ifstream file(filename, std::ios::ate | std::ios::binary);

		if (!file.is_open())
		{
			throw std::runtime_error("Failed to open file: " + filename);
		}

		// Get size of the file and create a buffer
		size_t fileSize = (size_t)file.tellg();
		std::vector<char> buffer(fileSize);

		// Read all bytes
		file.seekg(0);
		file.read(buffer.data(), fileSize);
		file.close();

		return buffer;
	}

	static void ImGuiCheckVKResult(VkResult err)
	{
		if (err == 0) return;
		printf("VkResult %d\n", err);
		if (err < 0)
			abort();
	}

	// Wraps given angle to the range of [0, 360)
	static float WrapAngle(float x) 
	{
		x = fmod(x, 360);
		if (x < 0)
			x += 360;
		return x;
	}

	// Calculates number of triangles produced by barycentric subdivision (i.e. tessellation)
	// of a single trianlge where outer and inner lod is always equal
	static int CalculateTriangleSubdivision(int lod) 
	{
		if (lod < 0)   return 1;                      
		if (lod == 0)  return 0;
		return ((2 * lod - 2) * 3) + CalculateTriangleSubdivision(lod - 2);
	}
}

#endif // !HELPERFUNCTIONS_H
