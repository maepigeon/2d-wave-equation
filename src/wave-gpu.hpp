#ifndef WAVE_FD_GPU
#define WAVE_FD_GPU
#include <VkBootstrap.h>
#include <vk_mem_alloc.h>
#include <vector>
#include <iostream>
#include <vulkan/vulkan.hpp>
#include <SDL3/SDL.h> 

namespace WaveFdNS {
    struct ComputePipeline {
        vk::Pipeline pipeline;
        vk::PipelineLayout pipelineLayout;
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
    struct GpuImage {
        VkImage image;
        VmaAllocation allocation;
        VkImageView view; // VkImageView is a wrapper for an image in a descriptor set
    };
    struct GpuBuffer {
        VkBuffer buffer;
        VmaAllocation allocation;
        VmaAllocationInfo info; // info.pMappedData
        VkDeviceSize size; // used to give the descriptor the buffer's byte range
    };
    struct WavePushConstants {
        uint32_t width;
        uint32_t height;
        float dt2v2_dx2;
    };
}

class WaveGpu {
    public:
        void run_compute(uint32_t gridWidth, uint32_t gridHeight, float dx, float velocity,
                SDL_Surface* surface, SDL_Texture* texture, SDL_Renderer* sdlRenderer);
    private:
        WaveFdNS::GpuContext setupShaderContext();
        void teardownShaderContext(WaveFdNS::GpuContext& context);
        VmaAllocator createVmaAllocator(WaveFdNS::GpuContext& context, vkb::PhysicalDevice& physicalDevice);     
        vk::DescriptorPool createStorageBufferPool(vk::Device device, int32_t bufferCount, uint32_t maxSets);
        vk::DescriptorSet allocateStorageBufferSet(vk::Device device, vk::DescriptorPool pool,
        vk::DescriptorSetLayout layout, const std::vector<vk::DescriptorBufferInfo>& buffers);
        vk::DescriptorSetLayout createStorageBufferLayout(vk::Device device, uint32_t bufferCount);
        std::vector<vk::QueueFamilyProperties> createComputeCommandQ();
        WaveFdNS::ComputePipeline createComputePipeline(std::vector<char>& shaderContents, vk::Device device, vk::DescriptorSetLayout descriptorSetLayout);
        void recordComputeDispatch(vk::CommandBuffer commandBuffer, vk::Pipeline vkPipeline, vk::PipelineLayout layout,
            const std::array<vk::DescriptorSet, 3>& descriptorSets, WaveFdNS::WavePushConstants& pushConstants, uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ,
            uint32_t& rotation, int substeps);
        vk::DescriptorPool createPool_wave(vk::Device device);
        vk::DescriptorSetLayout createLayout_wave(vk::Device device);
        vk::DescriptorSet allocateSet_wave(vk::Device device, vk::DescriptorPool pool, vk::DescriptorSetLayout layout, 
            vk::DescriptorImageInfo& G0Info, vk::DescriptorImageInfo& G1Info, vk::DescriptorImageInfo& G2Info);
        std::array<vk::DescriptorSet, 3> createDescriptorSets(vk::Device device, vk::DescriptorPool pool, vk::DescriptorSetLayout layout, std::array<WaveFdNS::GpuImage, 3>& images);
        WaveFdNS::GpuImage createImage(VmaAllocator allocator, vk::Device device, uint32_t width, uint32_t height);
        WaveFdNS::GpuBuffer createBuffer(VmaAllocator allocator, VkDeviceSize size, VkBufferUsageFlags usage, VmaAllocationCreateFlags vmaFlags);
        void transitionImageLayout(vk::CommandBuffer commandBuffer, vk::Image image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
            vk::AccessFlags srcAccess, vk::AccessFlags dstAccess, vk::PipelineStageFlags srcStage, vk::PipelineStageFlags dstStage);
        void readShader(std::string fName, std::vector<char>& shaderContents);
        void initializeImageData(vk::Device device, VmaAllocator allocator, vk::Queue queue, vk::CommandPool commandPool,
            std::vector<float>& data, std::vector<vk::Image>& targetImages, uint32_t width, uint32_t height);
    };
#endif