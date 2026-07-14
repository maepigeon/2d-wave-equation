#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-completeness"
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
#pragma clang diagnostic pop
#include "double.hpp"
#include <fstream>
#include <iostream>

VmaAllocator Double::createVmaAllocator(GpuContext& context, vkb::PhysicalDevice& physicalDevice) {
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = physicalDevice;  
    allocatorInfo.device = context.device;
    allocatorInfo.instance = context.instance;
    vmaCreateAllocator(&allocatorInfo, &context.allocator);
    return context.allocator;
}

GpuContext Double::setupShaderContext() {
    GpuContext context;
    std::vector<const char*> extensions;
    #ifdef __APPLE__
        extensions.push_back("VK_KHR_portability_enumeration");
    #endif
    vkb::InstanceBuilder ib;
    vkb::Result<vkb::Instance> instance = 
        ib.set_app_name("Compute Shader Test")
            .request_validation_layers(true)
            .use_default_debug_messenger()
            .enable_extensions(extensions)
            .build();
    if (!instance) {
        std::cout << "Failed to connect to the Vulkan library..." << std::endl;
        throw std::runtime_error(instance.error().message());
    }
    context.instance = instance.value();

    vkb::PhysicalDeviceSelector selector{ context.instance };
    vkb::Result<vkb::PhysicalDevice> physicalDevice = selector.defer_surface_initialization().select();
    if (!physicalDevice) {
        std::cout << "Failed to select a gpu..." << std::endl;
        throw std::runtime_error(physicalDevice.error().message());
    }
    context.physicalDevice = physicalDevice.value();
    vkb::DeviceBuilder db = vkb::DeviceBuilder(physicalDevice.value());
    vkb::Result<vkb::Device> device = db.build();
    if (!device) {
        std::cout << "Selected gpu device failed to connect..." << std::endl;
        throw std::runtime_error(device.error().message());
    }
    context.device = device.value();   

    vkb::Result<VkQueue> q = context.device.get_queue(vkb::QueueType::compute);
    if (!q) { 
        q = context.device.get_queue(vkb::QueueType::graphics);
    }
    if (!q) {
        throw std::runtime_error(std::string("no usable queue: ") + q.error().message());
    }
    context.computeQueue = q.value();

    vkb::Result<uint32_t>  qf = context.device.get_queue_index(vkb::QueueType::compute);
    if (!qf) {
        qf = context.device.get_queue_index(vkb::QueueType::graphics);
    }
    if (!qf) {
        throw std::runtime_error(std::string("no usable family: ") + qf.error().message());
    }
    context.computeQueueFamily = qf.value();
    createVmaAllocator(context, physicalDevice.value());
    return context;
}
 
void Double::teardownShaderContext(GpuContext& context) {
    vmaDestroyAllocator(context.allocator);
    vkb::destroy_device(context.device);
    vkb::destroy_instance(context.instance);
}


void Double::createUniform(GpuContext& context) {
    ParamsUBO params{};
    params.offset = 100.0f;   
    VkBufferUsageFlagBits usageFlag = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    uint32_t bufferFlags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    GpuBuffer uboBuffer = createBuffer(context.allocator, sizeof(ParamsUBO), usageFlag, bufferFlags);
    std::memcpy(uboBuffer.info.pMappedData, &params, sizeof(ParamsUBO));
    vmaFlushAllocation(context.allocator, uboBuffer.allocation, 0, sizeof(ParamsUBO));
}

GpuBuffer Double::createBuffer(VmaAllocator allocator, VkDeviceSize size, VkBufferUsageFlags usage, VmaAllocationCreateFlags vmaFlags) {
    VkBufferCreateInfo bufferInfo {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    VmaAllocationCreateInfo allocCreateInfo {};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocCreateInfo.flags = vmaFlags;
    GpuBuffer result {};
    result.size = size;
    vmaCreateBuffer(allocator, &bufferInfo, &allocCreateInfo, &result.buffer, &result.allocation, &result.info);
    return result;
}

vk::DescriptorPool Double::createStorageBufferPool(vk::Device device, int32_t bufferCount, uint32_t maxSets = 1) {
    vk::DescriptorPoolSize size(vk::DescriptorType::eStorageBuffer, bufferCount);
    vk::DescriptorPoolCreateInfo info {};
    info.setMaxSets(maxSets);
    info.setPoolSizes(size);
    return device.createDescriptorPool(info);
}

vk::DescriptorSet Double::allocateStorageBufferSet(vk::Device device, vk::DescriptorPool pool,
        vk::DescriptorSetLayout layout, const std::vector<vk::DescriptorBufferInfo>& buffers) {
    vk::DescriptorSetAllocateInfo allocInfo {};
    allocInfo.setDescriptorPool(pool);
    allocInfo.setDescriptorSetCount(1);
    allocInfo.setPSetLayouts(&layout);
    vk::DescriptorSet set = device.allocateDescriptorSets(allocInfo).front();
    std::vector<vk::WriteDescriptorSet> writes;
    writes.reserve(buffers.size());
    for (int i = 0; i < buffers.size(); i++) {
        writes.emplace_back(set, i, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &buffers[i]);
    }
    device.updateDescriptorSets(writes, {});
    return set;
}

vk::DescriptorSetLayout Double::createDoubleLayout(vk::Device device) {
    std::vector<vk::DescriptorSetLayoutBinding> bindings(3);
    // binding 0: input SSBO
    bindings[0].setBinding(0).setDescriptorType(vk::DescriptorType::eStorageBuffer)
               .setDescriptorCount(1).setStageFlags(vk::ShaderStageFlagBits::eCompute);
    // binding 1: output SSBO
    bindings[1].setBinding(1).setDescriptorType(vk::DescriptorType::eStorageBuffer)
               .setDescriptorCount(1).setStageFlags(vk::ShaderStageFlagBits::eCompute);
    // binding 2: UBO
    bindings[2].setBinding(2).setDescriptorType(vk::DescriptorType::eUniformBuffer)
               .setDescriptorCount(1).setStageFlags(vk::ShaderStageFlagBits::eCompute);

    vk::DescriptorSetLayoutCreateInfo info{};
    info.setBindings(bindings);
    return device.createDescriptorSetLayout(info);
}
vk::DescriptorPool Double::createDoublePool(vk::Device device) {
    std::array<vk::DescriptorPoolSize, 2> sizes{
        vk::DescriptorPoolSize{ vk::DescriptorType::eStorageBuffer, 2 },  // 2 SSBOs
        vk::DescriptorPoolSize{ vk::DescriptorType::eUniformBuffer, 1 },  // 1 UBO
    };
    vk::DescriptorPoolCreateInfo info{};
    info.setMaxSets(1);
    info.setPoolSizes(sizes);
    return device.createDescriptorPool(info);
}
vk::DescriptorSet Double::allocateDoubleSet(vk::Device device, vk::DescriptorPool pool, vk::DescriptorSetLayout layout, 
        vk::DescriptorBufferInfo& inInfo, vk::DescriptorBufferInfo& outInfo, vk::DescriptorBufferInfo& uboInfo) {
    vk::DescriptorSetAllocateInfo allocInfo{};
    allocInfo.setDescriptorPool(pool).setDescriptorSetCount(1).setPSetLayouts(&layout);
    vk::DescriptorSet set = device.allocateDescriptorSets(allocInfo).front();
    std::array<vk::WriteDescriptorSet, 3> writes{
        vk::WriteDescriptorSet{ set, 0, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &inInfo },
        vk::WriteDescriptorSet{ set, 1, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &outInfo },
        vk::WriteDescriptorSet{ set, 2, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &uboInfo },
    };
    device.updateDescriptorSets(writes, {});
    return set;
}

vk::DescriptorSetLayout Double::createStorageBufferLayout(vk::Device device, uint32_t bufferCount) {
    std::vector<vk::DescriptorSetLayoutBinding> bindings;
    bindings.reserve(bufferCount);
    for (uint32_t i = 0; i < bufferCount; i++) {
        vk::DescriptorSetLayoutBinding binding {};
        binding.setBinding(i);
        binding.setDescriptorType(vk::DescriptorType::eStorageBuffer);
        binding.setDescriptorCount(1);
        binding.setStageFlags(vk::ShaderStageFlagBits::eCompute);
        bindings.emplace_back(binding);
    }
    vk::DescriptorSetLayoutCreateInfo info {};
    info.setBindings(bindings);
    return device.createDescriptorSetLayout(info);
}

void Double::printResults(int numElements, const GpuBuffer& inBuffer, const GpuBuffer& outBuffer) {
    float* inPtr  = static_cast<float*>(inBuffer.info.pMappedData);
    float* outPtr = static_cast<float*>(outBuffer.info.pMappedData);

    std::cout << "\nInput:  ";
    for (int i = 0; i < numElements; i++) {
        std::cout << inPtr[i] << " ";
    }
    std::cout << "\nOutput: ";
    for (int i = 0; i < numElements; i++) {
        std::cout << outPtr[i] << " ";
    }
    std::cout << std::endl;
}

void Double::readShader(std::string fName, std::vector<char>& shaderContents) {
    std::ifstream shaderFile{fName, std::ios::binary | std::ios::ate};
    if (!shaderFile) {
        throw std::runtime_error("cannot open shader file: " + fName);
    }
    const size_t fileSize = shaderFile.tellg();
    shaderFile.seekg(0);
    shaderContents.resize(fileSize, '\0');
    shaderFile.read(shaderContents.data(), fileSize);
}

ComputePipeline Double::createComputePipeline(std::vector<char>& shaderContents, vk::Device device, vk::DescriptorSetLayout descriptorSetLayout) {
    vk::ShaderModuleCreateInfo shaderModuleInfo({}, shaderContents.size(),
        reinterpret_cast<const uint32_t*>(shaderContents.data()));
    vk::ShaderModule shaderModule = device.createShaderModule(shaderModuleInfo);
    
    vk::PushConstantRange pushRange(
      vk::ShaderStageFlagBits::eCompute, // shader stage that reads it (compute)
      0,                //offset
      sizeof(uint32_t)  // size  
    );
    
    vk::PipelineLayoutCreateInfo pipelineLayoutInfo({}, descriptorSetLayout, pushRange);
    vk::PipelineLayout pipelineLayout = device.createPipelineLayout(pipelineLayoutInfo);
    vk::PipelineCache pipelineCache = device.createPipelineCache({});
    vk::PipelineShaderStageCreateInfo stageInfo(
        {}, vk::ShaderStageFlagBits::eCompute, shaderModule, "main");
    vk::ComputePipelineCreateInfo pipelineInfo({}, stageInfo, pipelineLayout);
    vk::Pipeline pipeline = device.createComputePipeline(pipelineCache, pipelineInfo).value;
    device.destroyShaderModule(shaderModule);
    device.destroyPipelineCache(pipelineCache);
    return { pipeline, pipelineLayout, descriptorSetLayout };
}

void Double::recordComputeDispatch(vk::CommandBuffer commandBuffer, vk::Pipeline vkPipeline, vk::PipelineLayout layout,
    uint32_t numElements, vk::DescriptorSet descriptorSet, uint32_t groupsX, uint32_t groupsY = 1, uint32_t groupsZ = 1) {
    vk::CommandBufferBeginInfo beginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    commandBuffer.begin(beginInfo);
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, vkPipeline);
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, layout, 0, { descriptorSet }, {});
    
    commandBuffer.pushConstants(
        layout,
        vk::ShaderStageFlagBits::eCompute,
        0, // offset
        sizeof(uint32_t), // size
        &numElements); // pointer to the data
    
    commandBuffer.dispatch(groupsX, groupsY, groupsZ);
    commandBuffer.end();
}

void Double::run_compute() {
    // Enable MoltenVK if needed
    #ifdef __APPLE__
        setenv("VK_ICD_FILENAMES", "/Users/mae/VulkanSDK/1.4.350.0/macOS/share/vulkan/icd.d/MoltenVK_icd.json", 1);
	#endif
    GpuContext context = setupShaderContext();
    vk::Device device = context.device.device;
    uint32_t numElements = 1000;
    uint32_t bufferSize = numElements * sizeof(float);
    VkBufferUsageFlags bufferUsageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    VmaAllocationCreateFlags vmaFlags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    GpuBuffer inBuffer = createBuffer(context.allocator, bufferSize, bufferUsageFlags, vmaFlags);
    GpuBuffer outBuffer = createBuffer(context.allocator, bufferSize, bufferUsageFlags, vmaFlags );

    float* inBufferData = static_cast<float*>(inBuffer.info.pMappedData);
    for (int i = 0; i < numElements; i++) {
        inBufferData[i] = float(i);
    }
    vmaFlushAllocation(context.allocator, inBuffer.allocation, 0, bufferSize);

    ParamsUBO params{}; // UBO
    params.offset = 100.0f;
    GpuBuffer uboBuffer = createBuffer(context.allocator, sizeof(ParamsUBO),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);
    std::memcpy(uboBuffer.info.pMappedData, &params, sizeof(ParamsUBO));
    vmaFlushAllocation(context.allocator, uboBuffer.allocation, 0, sizeof(ParamsUBO));
    vk::DescriptorPool pool = createDoublePool(device);
    vk::DescriptorSetLayout layout = createDoubleLayout(device);
    vk::DescriptorBufferInfo inInfo { inBuffer.buffer,  0, bufferSize };
    vk::DescriptorBufferInfo outInfo{ outBuffer.buffer, 0, bufferSize };
    vk::DescriptorBufferInfo uboInfo{ uboBuffer.buffer, 0, sizeof(ParamsUBO) };
    vk::DescriptorSet descriptorSet = allocateDoubleSet(device, pool, layout, inInfo, outInfo, uboInfo);    
    
    std::string fName = "/Users/mae/workspace/wave-equation/src/shaders/double.comp.spv";
    std::vector<char> shaderContents {};
    readShader(fName, shaderContents);
    ComputePipeline cp = createComputePipeline(shaderContents, context.device.device, layout);
    vk::CommandPoolCreateInfo CommandPoolCreateInfo(vk::CommandPoolCreateFlags(), context.computeQueueFamily);
    vk::CommandPool commandPool = device.createCommandPool(CommandPoolCreateInfo);
    vk::CommandBufferAllocateInfo allocInfo(commandPool, vk::CommandBufferLevel::ePrimary, 1);
    vk::CommandBuffer commandBuffer = device.allocateCommandBuffers(allocInfo).front();
    uint32_t localSizeX = 64;  // matches local_size_x in the shader (64 threads per workgroup)
    uint32_t groupsX = (numElements + localSizeX - 1) / localSizeX; // how many workgroups are dispatched
    recordComputeDispatch(commandBuffer, cp.pipeline, cp.pipelineLayout, numElements, descriptorSet, groupsX, 1, 1);
    vk::Queue Queue = device.getQueue(context.computeQueueFamily, 0);
    vk::Fence Fence = device.createFence(vk::FenceCreateInfo());
    vk::SubmitInfo SubmitInfo( 
        0, nullptr, nullptr,
        1, &commandBuffer);
    Queue.submit({ SubmitInfo }, Fence);
    (void)device.waitForFences({ Fence }, true, uint64_t(-1));
    vmaInvalidateAllocation(context.allocator, outBuffer.allocation, 0, bufferSize);
    printResults(numElements, inBuffer, outBuffer);
    device.destroyPipeline(cp.pipeline);
    device.destroyPipelineLayout(cp.pipelineLayout);
    device.destroyDescriptorSetLayout(cp.descriptorSetLayout);
    device.destroyDescriptorPool(pool);
    device.destroyFence(Fence);
    device.destroyCommandPool(commandPool);
    vmaDestroyBuffer(context.allocator, uboBuffer.buffer, uboBuffer.allocation);
    vmaDestroyBuffer(context.allocator, inBuffer.buffer, inBuffer.allocation);
    vmaDestroyBuffer(context.allocator, outBuffer.buffer, outBuffer.allocation);
    teardownShaderContext(context);
}