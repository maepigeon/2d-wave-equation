#ifndef DOUBLE
#define DOUBLE

#include <VkBootstrap.h>
#include <vk_mem_alloc.h>
#include <vector>
#include <iostream>
#include <vulkan/vulkan.hpp>

struct ComputePipeline {
    vk::Pipeline            pipeline;
    vk::PipelineLayout      pipelineLayout;
    vk::DescriptorSetLayout descriptorSetLayout;
};
struct GpuContext {
    vkb::Instance instance;
    vkb::Device device;
    VkQueue computeQueue;
    uint32_t computeQueueFamily;
    VmaAllocator allocator;
    vk::PhysicalDevice physicalDevice;
};
struct GpuBuffer {
    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo info;
    VkDeviceSize size;
};

struct ParamsUBO {
    float offset;
};

class Double {
    public:
        void run_compute();
    private:
        GpuContext setupShaderContext();
        void teardownShaderContext(GpuContext& context);
        VmaAllocator createVmaAllocator(GpuContext& context, vkb::PhysicalDevice& physicalDevice);     
        vk::DescriptorPool createStorageBufferPool(vk::Device device, int32_t bufferCount, uint32_t maxSets);
        vk::DescriptorSet allocateStorageBufferSet(vk::Device device, vk::DescriptorPool pool,
        vk::DescriptorSetLayout layout, const std::vector<vk::DescriptorBufferInfo>& buffers);
        vk::DescriptorSetLayout createStorageBufferLayout(vk::Device device, uint32_t bufferCount);
        std::vector<vk::QueueFamilyProperties> createComputeCommandQ();
        ComputePipeline createComputePipeline(std::vector<char>& shaderContents, vk::Device device, vk::DescriptorSetLayout descriptorSetLayout);
        void recordComputeDispatch(vk::CommandBuffer commandBuffer, vk::Pipeline vkPipeline, vk::PipelineLayout layout,
            uint32_t numElements, vk::DescriptorSet descriptorSet, uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ);
        void createUniform(GpuContext& context);
        vk::DescriptorPool createDoublePool(vk::Device device);
        vk::DescriptorSetLayout createDoubleLayout(vk::Device device);
        vk::DescriptorSet allocateDoubleSet(vk::Device device, vk::DescriptorPool pool, vk::DescriptorSetLayout layout, 
                vk::DescriptorBufferInfo& inInfo, vk::DescriptorBufferInfo& outInfo, vk::DescriptorBufferInfo& uboInfo);
        GpuBuffer createBuffer(VmaAllocator allocator, VkDeviceSize size, VkBufferUsageFlags usage, VmaAllocationCreateFlags vmaFlags);
        void destroyBuffer(VmaAllocator allocator, GpuBuffer& buf);
        void printResults(int numElements, const GpuBuffer& inBuffer, const GpuBuffer& outBuffer);
        void readShader(std::string fName, std::vector<char>& shaderContents);
    };
#endif