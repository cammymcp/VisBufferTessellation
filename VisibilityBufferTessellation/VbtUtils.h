#ifndef HELPERFUNCTIONS_H
#define HELPERFUNCTIONS_H

#include "PhysicalDevice.h"

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
}

#endif // !HELPERFUNCTIONS_H
