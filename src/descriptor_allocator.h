#include <stdint.h>
#include <vulkan/vulkan_core.h>

namespace vke {

	enum class DescriptorAllocatorLifetime {
		Static,
		PerFrame
	};

	struct DescriptorAllocatorPool;

	struct DescriptorAllocatorHandle {		

		DescriptorAllocatorHandle() = default;

		~DescriptorAllocatorHandle();

		DescriptorAllocatorHandle(DescriptorAllocatorHandle&& other);

		DescriptorAllocatorHandle& operator=(const DescriptorAllocatorHandle&) = delete;

		DescriptorAllocatorHandle& operator=(DescriptorAllocatorHandle&& other);

		void Return();
		bool Allocate(const VkDescriptorSetLayout& layout, VkDescriptorSet& builtSet);

		DescriptorAllocatorPool* ownerPool{nullptr};
		VkDescriptorPool vkPool;
		int8_t poolIdx;
	};

	struct DescriptorAllocatorPool {

		static DescriptorAllocatorPool* Create(const VkDevice& device, int nFrames = 3);

		virtual void Flip() = 0;

		virtual void SetPoolSizeMultiplier(VkDescriptorType type, float multiplier) = 0;

		virtual DescriptorAllocatorHandle GetAllocator(DescriptorAllocatorLifetime lifetime) = 0;
	};
}
