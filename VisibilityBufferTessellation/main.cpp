#include <iostream>
#include <stdexcept>
#include "VulkanApplication.h"

int main()
{
	vbt::VulkanApplication application;

	try
	{
		application.Run();
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}
	std::getchar();
	return EXIT_SUCCESS;
}