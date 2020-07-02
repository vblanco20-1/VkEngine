#include "vulkan_init.h"
#include "vulkan_types.h"
#include "vulkan_render.h"
#include "sdl_render.h"
 
#include "termcolor.hpp"
 
static VKAPI_ATTR VkBool32 VKAPI_CALL vulkanValidationErrorCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData) {

	if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT || messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT || messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {

		ZoneScopedNC("Vulkan Debug Callback", tracy::Color::Red);

		auto printtype = [](auto messageType) {
			if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) {
				std::cerr << "[VALIDATION]  ";
			}
			if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) {
				std::cerr << "[GENERAL]  ";
			}
			if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) {
				std::cerr << "[PERFORMANCE]  ";
			}
		};


		if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
			std::cerr << termcolor::red << "[ERROR]  " << termcolor::reset;
			printtype(messageType);
			std::cerr << pCallbackData->pMessage << std::endl;
		}
		else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
			std::cerr << termcolor::yellow << "[WARNING]  " << termcolor::reset;
			printtype(messageType);
			std::cerr << pCallbackData->pMessage << std::endl;
		}
		else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
			std::cerr << termcolor::reset << "[INFO]  ";
			printtype(messageType);
			std::cerr << pCallbackData->pMessage << std::endl;
		}
		std::cerr << termcolor::reset << std::endl;
	}
	return VK_FALSE;
}
bool VulkanEngine::check_layer_support() {
	ZoneScoped;
	auto availibleLayers = vk::enumerateInstanceLayerProperties();

	for (auto&& layerName : validationLayers) {
		bool bFound = false;
		for (auto&& layerProperties : availibleLayers) {
			if (strcmp(layerName, layerProperties.layerName) == 0) {
				bFound = true;
				break;
			}
		}

		if (!bFound) {
			//return false;
		}
	}
	return true;
}

std::vector<const char*> VulkanEngine::get_extensions()
{
	ZoneScoped;
	uint32_t extensionCount;
	SDL_Vulkan_GetInstanceExtensions(sdl_get_window(), &extensionCount, nullptr);
	std::vector<const char*> extensionNames(extensionCount);
	SDL_Vulkan_GetInstanceExtensions(sdl_get_window(), &extensionCount, extensionNames.data());

	if (enableValidationLayers) {
		extensionNames.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	return extensionNames;
}



void VulkanEngine::pick_physical_device()
{
	ZoneScoped;
	std::vector<vk::PhysicalDevice> devices = instance.enumeratePhysicalDevices();
	if (devices.size() == 0) {
		throw std::runtime_error("failed to find GPUs with Vulkan support!");
	}

	for (const auto& device : devices) {
		if (isDeviceSuitable(device)) {
			physicalDevice = device;
			break;
		}
	}

	deviceProperties = physicalDevice.getProperties();
	size_t minUboAlignment = deviceProperties.limits.minUniformBufferOffsetAlignment;
	size_t dynamicAlignment = sizeof(UniformBufferObject);
	if (minUboAlignment > 0) {
		dynamicAlignment = (dynamicAlignment + minUboAlignment - 1) & ~(minUboAlignment - 1);
	}

	StagingCPUUBOArray = AlignedBuffer<UniformBufferObject>(dynamicAlignment);
	StagingCPUUBOArray.resize(MAX_UNIFORM_BUFFER);
}


void findQueueFamilies(vk::PhysicalDevice& physicalDevice, vk::SurfaceKHR& surface, int& graphicsFamilyIndex, int& presentFamilyIndex) {
	//--- QUEUE FAMILY
	std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();
	graphicsFamilyIndex = 0;

	int i = 0;
	for (const auto& queueFamily : queueFamilyProperties) {
		if (queueFamily.queueCount > 0 && queueFamily.queueFlags & vk::QueueFlagBits::eGraphics) {
			graphicsFamilyIndex = i;
			break;
		}
		i++;
	}
	presentFamilyIndex = 0;
	i = 0;
	for (const auto& queueFamily : queueFamilyProperties) {
		if (queueFamily.queueCount > 0 && queueFamily.queueFlags & vk::QueueFlagBits::eGraphics) {
			VkBool32 presentSupport = false;
			if (physicalDevice.getSurfaceSupportKHR(i, surface))
			{
				presentFamilyIndex = i;
				break;
			}
		}
		i++;
	}
}



void VulkanEngine::create_device()
{
	ZoneScoped;
	//--- QUEUE FAMILY
	int graphicsFamilyIndex = 0;
	int presentFamilyIndex = 0;
	findQueueFamilies(physicalDevice, surface, graphicsFamilyIndex, presentFamilyIndex);

	//--- QUEUE CREATE INFO

	vk::DeviceQueueCreateInfo queueCreateInfo;
	queueCreateInfo.queueFamilyIndex = graphicsFamilyIndex;
	queueCreateInfo.queueCount = 1;
	float fprio = 1.0f;
	queueCreateInfo.pQueuePriorities = &fprio;

	//auto& diagnosticsConfig = vk::DeviceDiagnosticsConfigCreateInfoNV{};
	//
	//
	//vk::DeviceDiagnosticsConfigFlagsNV aftermathFlags =
	//	vk::DeviceDiagnosticsConfigFlagBitsNV::eEnableResourceTracking |
	//	vk::DeviceDiagnosticsConfigFlagBitsNV::eEnableAutomaticCheckpoints |
	//	vk::DeviceDiagnosticsConfigFlagBitsNV::eEnableShaderDebugInfo;
	//
	//diagnosticsConfig.setFlags(aftermathFlags);

	vk::DeviceQueueCreateInfo presentQueueCreateInfo;
	presentQueueCreateInfo.queueFamilyIndex = presentFamilyIndex;
	presentQueueCreateInfo.queueCount = 1;
	presentQueueCreateInfo.pQueuePriorities = &fprio;

	//--- DEVICE FEATURES
	vk::PhysicalDeviceFeatures deviceFeatures;
	deviceFeatures.samplerAnisotropy = true;
	deviceFeatures.shaderSampledImageArrayDynamicIndexing = true;
	deviceFeatures.fragmentStoresAndAtomics = true;
	deviceFeatures.vertexPipelineStoresAndAtomics = true;
	deviceFeatures.shaderInt64 = true;
	deviceFeatures.multiDrawIndirect = true;
	deviceFeatures.drawIndirectFirstInstance = true;
	//deviceFeatures.

	VkPhysicalDeviceDescriptorIndexingFeatures descriptor_indexing_features{};
	descriptor_indexing_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
	descriptor_indexing_features.pNext = nullptr;//&diagnosticsConfig;
	// Enable non-uniform indexing
	descriptor_indexing_features.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
	descriptor_indexing_features.runtimeDescriptorArray = VK_TRUE;
	descriptor_indexing_features.descriptorBindingVariableDescriptorCount = VK_TRUE;
	descriptor_indexing_features.descriptorBindingPartiallyBound = VK_TRUE;

	//vk::PhysicalDeviceTimelineSemaphoreFeaturesKHR timelineFeatures;
	//timelineFeatures.timelineSemaphore = true;

	vk::PhysicalDeviceRayTracingFeaturesKHR rayFeatures;
	rayFeatures.rayTracing = true;
	//rayFeatures.rayTracingShaderGroupHandleCaptureReplay;
	//rayFeatures.rayTracingShaderGroupHandleCaptureReplayMixed;
	//rayFeatures.rayTracingAccelerationStructureCaptureReplay;
	//rayFeatures.rayTracingIndirectTraceRays = true;
	//rayFeatures.rayTracingIndirectAccelerationStructureBuild = true;
	//rayFeatures.rayTracingHostAccelerationStructureCommands = true;
	rayFeatures.rayQuery = true;
	//rayFeatures.rayTracingPrimitiveCulling = true;

	
	vk::PhysicalDeviceVulkan12Features vk12features;
	vk12features.timelineSemaphore = true;
	vk12features.bufferDeviceAddress =true;
	vk12features.descriptorIndexing = true;
	vk12features.shaderSampledImageArrayNonUniformIndexing = true;
	vk12features.runtimeDescriptorArray = true;
	vk12features.imagelessFramebuffer = true;
	vk12features.descriptorBindingVariableDescriptorCount = true;
	vk12features.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
	vk12features.runtimeDescriptorArray = VK_TRUE;
	vk12features.descriptorBindingVariableDescriptorCount = VK_TRUE;
	vk12features.descriptorBindingPartiallyBound = VK_TRUE;
	
#ifdef RTX_ON
	vk12features.pNext = &rayFeatures;
#else
	vk12features.pNext = &descriptor_indexing_features;
#endif

	vk::PhysicalDeviceFeatures2 features2;
	features2.features = deviceFeatures;
	features2.pNext =  &vk12features;
	

	//deviceFeatures.
	//--- DEVICE CREATE
	vk::DeviceCreateInfo createInfo;

	//auto extensions = get_extensions();

	if (queueCreateInfo.queueFamilyIndex == presentQueueCreateInfo.queueFamilyIndex) {
		std::array<vk::DeviceQueueCreateInfo, 2> queues{ queueCreateInfo,presentQueueCreateInfo };
		createInfo.pQueueCreateInfos = queues.data();
		createInfo.queueCreateInfoCount = 1;
		createInfo.pEnabledFeatures = &deviceFeatures;
		createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
		createInfo.ppEnabledExtensionNames = deviceExtensions.data();
		createInfo.pNext = &features2;

		device = physicalDevice.createDevice(createInfo);
	}
	else {
		std::array<vk::DeviceQueueCreateInfo, 2> queues{ queueCreateInfo,presentQueueCreateInfo };
		createInfo.pQueueCreateInfos = queues.data();
		createInfo.queueCreateInfoCount = 2;
		createInfo.pEnabledFeatures = &deviceFeatures;
		createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
		createInfo.ppEnabledExtensionNames = deviceExtensions.data();

		device = physicalDevice.createDevice(createInfo);
	}

	std::cout << "device created1 " << std::endl;
	char d;
	//std::cin >> d;

	graphicsQueue = device.getQueue(graphicsFamilyIndex, 0);
	presentQueue = device.getQueue(presentFamilyIndex, 0);

	// This dispatch class will fetch function pointers for the passed device if possible, else for the passed instance
	extensionDispatcher.init(instance, device);// = vk::DispatchLoaderDynamic{ instance, device };
}

struct SwapChainSupportDetails {
	vk::SurfaceCapabilitiesKHR capabilities;
	std::vector<vk::SurfaceFormatKHR> formats;
	std::vector<vk::PresentModeKHR> presentModes;
};

SwapChainSupportDetails querySwapChainSupport(vk::PhysicalDevice device, vk::SurfaceKHR surface) {
	SwapChainSupportDetails details;

	details.capabilities = device.getSurfaceCapabilitiesKHR(surface);
	details.formats = device.getSurfaceFormatsKHR(surface);
	details.presentModes = device.getSurfacePresentModesKHR(surface);

	return details;
}

vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats) {
	if (availableFormats.size() == 1 && availableFormats[0].format == vk::Format::eUndefined) {
		return { vk::Format::eB8G8R8A8Unorm, vk::ColorSpaceKHR::eVkColorspaceSrgbNonlinear };
	}

	for (const auto& availableFormat : availableFormats) {
		if (availableFormat.format == vk::Format::eB8G8R8A8Unorm && availableFormat.colorSpace == vk::ColorSpaceKHR::eVkColorspaceSrgbNonlinear) {
			return availableFormat;
		}
	}
	return availableFormats[0];
}
vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes) {
	for (const auto& availablePresentMode : availablePresentModes) {
		if (availablePresentMode == vk::PresentModeKHR::eMailbox) {
			return availablePresentMode;
		}
	}
	return vk::PresentModeKHR::eFifo;
}

vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities) {
	ZoneScoped;
	int w, h;
	SDL_GetWindowSize(sdl_get_window(), &w, &h);

	if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
		return capabilities.currentExtent;
	}
	else {
		VkExtent2D actualExtent = { w, h };

		actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
		actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

		return actualExtent;
	}
}



void VulkanEngine::createSwapChain()
{
	ZoneScoped;
	SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice, surface);

	vk::SurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
	vk::PresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
	vk::Extent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

	uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
	if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
		imageCount = swapChainSupport.capabilities.maxImageCount;
	}
	//imageCount = 3;
	vk::SwapchainCreateInfoKHR createInfo;
	createInfo.surface = surface;

	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;

	//QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

	int graphicsFamilyIndex = 0;
	int presentFamilyIndex = 0;
	findQueueFamilies(physicalDevice, surface, graphicsFamilyIndex, presentFamilyIndex);

	uint32_t queueFamilyIndices[] = { graphicsFamilyIndex, presentFamilyIndex };

	if (graphicsFamilyIndex != presentFamilyIndex) {
		createInfo.imageSharingMode = vk::SharingMode::eConcurrent;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else {
		createInfo.imageSharingMode = vk::SharingMode::eExclusive;
		createInfo.queueFamilyIndexCount = 0; // Optional
		createInfo.pQueueFamilyIndices = nullptr; // Optional
	}

	createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
	createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;
	createInfo.oldSwapchain = nullptr;

	swapChain = device.createSwapchainKHR(createInfo, nullptr);

	swapChainImages = device.getSwapchainImagesKHR(swapChain);

	swapChainImageFormat = surfaceFormat.format;
	swapChainExtent = extent;
}

bool VulkanEngine::isDeviceSuitable(vk::PhysicalDevice device) {

	vk::PhysicalDeviceProperties deviceProperties = device.getProperties();
	vk::PhysicalDeviceFeatures deviceFeatures = device.getFeatures();
	auto details = querySwapChainSupport(device, surface);

	return deviceProperties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu &&
		deviceFeatures.geometryShader && !details.formats.empty() && !details.presentModes.empty();
}
void VulkanEngine::create_command_pool()
{
	ZoneScoped;
	int graphicsFamilyIndex = 0;
	int presentFamilyIndex = 0;
	findQueueFamilies(physicalDevice, surface, graphicsFamilyIndex, presentFamilyIndex);

	vk::CommandPoolCreateInfo poolInfo;
	poolInfo.queueFamilyIndex = graphicsFamilyIndex;
	poolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
	commandPool = device.createCommandPool(poolInfo);

	vk::CommandPoolCreateInfo transferPoolInfo;
	transferPoolInfo.queueFamilyIndex = graphicsFamilyIndex;
	transferPoolInfo.flags = vk::CommandPoolCreateFlagBits::eTransient;

	transferCommandPool = device.createCommandPool(transferPoolInfo);	
}

void VulkanEngine::init_vulkan_debug()
{
	ZoneScoped;
	vk::DebugUtilsMessengerCreateInfoEXT createInfo;

	createInfo.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eError | vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo;
	createInfo.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;

	createInfo.pfnUserCallback = vulkanValidationErrorCallback;
	createInfo.pUserData = nullptr; // Optional

	VkDebugUtilsMessengerEXT msg;// = debugMessenger;

	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");

	func(instance, &VkDebugUtilsMessengerCreateInfoEXT(createInfo), nullptr, &msg);

	debugMessenger = msg;

}

