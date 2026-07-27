#include "wave-gpu.hpp"
#include <fstream>
#include <iostream>
#include <SDL3/SDL.h>
#include "sdlGraphicsHelpers.hpp"

VmaAllocator WaveGpu::createVmaAllocator(WaveFdNS::GpuContext& context, vkb::PhysicalDevice& physicalDevice) {
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = physicalDevice;  
    allocatorInfo.device = context.device;
    allocatorInfo.instance = context.instance;
    vmaCreateAllocator(&allocatorInfo, &context.allocator);
    return context.allocator;
}

WaveFdNS::GpuContext WaveGpu::setupShaderContext() {
    WaveFdNS::GpuContext context;
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
 
void WaveGpu::teardownShaderContext(WaveFdNS::GpuContext& context) {
    vmaDestroyAllocator(context.allocator);
    vkb::destroy_device(context.device);
    vkb::destroy_instance(context.instance);
}

WaveFdNS::GpuImage WaveGpu::createImage(VmaAllocator allocator, vk::Device device, uint32_t width, uint32_t height) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R32_SFLOAT;     
    imageInfo.extent = { width, height, 1 };
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT; // storage for compute, transfer_src for readback copy
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // The image is owned by only one queue family at a time
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // use VK_IMAGE_LAYOUT_PREINITIALIZED if image is LINEAR tiled for filling the image with cpu-created image immediately after creation

    VmaAllocationCreateInfo allocCreateInfo {};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
    WaveFdNS::GpuImage result {};
    vmaCreateImage(allocator, &imageInfo, &allocCreateInfo, &result.image, &result.allocation, nullptr);
    
    vk::ImageViewCreateInfo viewInfo{};
    viewInfo.image = result.image;
    viewInfo.viewType = vk::ImageViewType::e2D;
    viewInfo.format = vk::Format::eR32Sfloat;
    viewInfo.subresourceRange = vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}; // Could use to specify mipmap level and array layer used. eDepth/eStencil are other options in addition to eColor
    result.view = device.createImageView(viewInfo);

    return result;
}

// Updates the ImageLayout when Descriptor Sets are cycled
void WaveGpu::transitionImageLayout(vk::CommandBuffer commandBuffer, vk::Image image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
    vk::AccessFlags srcAccess, vk::AccessFlags dstAccess, vk::PipelineStageFlags srcStage, vk::PipelineStageFlags dstStage) {
    vk::ImageMemoryBarrier barrier{};
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
    barrier.srcAccessMask = srcAccess;
    barrier.dstAccessMask = dstAccess;

    commandBuffer.pipelineBarrier(srcStage, dstStage, {}, {}, {}, { barrier });
}

vk::DescriptorPool WaveGpu::createStorageBufferPool(vk::Device device, int32_t bufferCount, uint32_t maxSets = 1) {
    vk::DescriptorPoolSize size(vk::DescriptorType::eStorageBuffer, bufferCount);
    vk::DescriptorPoolCreateInfo info {};
    info.setMaxSets(maxSets);
    info.setPoolSizes(size);
    return device.createDescriptorPool(info);
}

vk::DescriptorSet WaveGpu::allocateStorageBufferSet(vk::Device device, vk::DescriptorPool pool,
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

vk::DescriptorSetLayout WaveGpu::createLayout_wave(vk::Device device) {
    std::vector<vk::DescriptorSetLayoutBinding> bindings(3);
    // binding 0: input SSBO
    bindings[0].setBinding(0).setDescriptorType(vk::DescriptorType::eStorageImage)
               .setDescriptorCount(1).setStageFlags(vk::ShaderStageFlagBits::eCompute);
    // binding 1: output SSBO
    bindings[1].setBinding(1).setDescriptorType(vk::DescriptorType::eStorageImage)
               .setDescriptorCount(1).setStageFlags(vk::ShaderStageFlagBits::eCompute);
    // binding 2: UBO
    bindings[2].setBinding(2).setDescriptorType(vk::DescriptorType::eStorageImage)
               .setDescriptorCount(1).setStageFlags(vk::ShaderStageFlagBits::eCompute);

    vk::DescriptorSetLayoutCreateInfo info{};
    info.setBindings(bindings);
    return device.createDescriptorSetLayout(info);
}
vk::DescriptorPool WaveGpu::createPool_wave(vk::Device device) {
    //the pool needs to reserve 9 image descriptors (3 sets * 3 bindings) and maxSets = 3
    vk::DescriptorPoolSize size { vk::DescriptorType::eStorageImage, 9 };
    vk::DescriptorPoolCreateInfo info{};
    info.setMaxSets(3);
    info.setPoolSizes(size);
    return device.createDescriptorPool(info);
}
vk::DescriptorSet WaveGpu::allocateSet_wave(vk::Device device, vk::DescriptorPool pool, vk::DescriptorSetLayout layout, 
        vk::DescriptorImageInfo& G0Info, vk::DescriptorImageInfo& G1Info, vk::DescriptorImageInfo& G2Info) {
    vk::DescriptorSetAllocateInfo allocInfo{};
    allocInfo.setDescriptorPool(pool).setDescriptorSetCount(1).setPSetLayouts(&layout);
    vk::DescriptorSet set = device.allocateDescriptorSets(allocInfo).front();
    std::array<vk::WriteDescriptorSet, 3> writes{
        vk::WriteDescriptorSet{ set, 0, 0, 1, vk::DescriptorType::eStorageImage, &G0Info, nullptr },
        vk::WriteDescriptorSet{ set, 1, 0, 1, vk::DescriptorType::eStorageImage, &G1Info, nullptr },
        vk::WriteDescriptorSet{ set, 2, 0, 1, vk::DescriptorType::eStorageImage, &G2Info, nullptr },
    };
    device.updateDescriptorSets(writes, {});
    return set;
}

vk::DescriptorSetLayout WaveGpu::createStorageBufferLayout(vk::Device device, uint32_t bufferCount) {
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

void WaveGpu::readShader(std::string fName, std::vector<char>& shaderContents) {
    std::ifstream shaderFile{fName, std::ios::binary | std::ios::ate};
    if (!shaderFile) {
        throw std::runtime_error("cannot open shader file: " + fName);
    }
    const size_t fileSize = shaderFile.tellg();
    shaderFile.seekg(0);
    shaderContents.resize(fileSize, '\0');
    shaderFile.read(shaderContents.data(), fileSize);
}

std::array<vk::DescriptorSet, 3> WaveGpu::createDescriptorSets(vk::Device device, vk::DescriptorPool pool, vk::DescriptorSetLayout layout, std::array<WaveFdNS::GpuImage, 3>& images) {
    std::array<vk::DescriptorSet, 3> descSets;
    for (int i = 0; i < 3; i++) {
        int g0Idx = i;
        int g1Idx = (i + 1) % 3;
        int g2Idx = (i + 2) % 3;
        vk::DescriptorImageInfo g0Info{ nullptr, images[g0Idx].view, vk::ImageLayout::eGeneral };
        vk::DescriptorImageInfo g1Info{ nullptr, images[g1Idx].view, vk::ImageLayout::eGeneral };
        vk::DescriptorImageInfo g2Info{ nullptr, images[g2Idx].view, vk::ImageLayout::eGeneral };
        descSets[i] = allocateSet_wave(device, pool, layout, g0Info, g1Info, g2Info);
    }
    return descSets;
}

WaveFdNS::ComputePipeline WaveGpu::createComputePipeline(std::vector<char>& shaderContents, vk::Device device, vk::DescriptorSetLayout descriptorSetLayout) {
    vk::ShaderModuleCreateInfo shaderModuleInfo({}, shaderContents.size(),
        reinterpret_cast<const uint32_t*>(shaderContents.data()));
    vk::ShaderModule shaderModule = device.createShaderModule(shaderModuleInfo);
    vk::PushConstantRange pushRange(vk::ShaderStageFlagBits::eCompute, 0, sizeof(WaveFdNS::WavePushConstants));
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

// Launches the workgroup
void WaveGpu::recordComputeDispatch(vk::CommandBuffer commandBuffer, vk::Pipeline vkPipeline, vk::PipelineLayout layout,
    const std::array<vk::DescriptorSet, 3>& descriptorSets, WaveFdNS::WavePushConstants& pushConstants, uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ,
    uint32_t& rotation, int substeps) {
    vk::CommandBufferBeginInfo beginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    commandBuffer.begin(beginInfo);
    for (int s = 0; s < substeps; s++) {
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, vkPipeline);
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, layout, 0, { descriptorSets[rotation] }, {});
        commandBuffer.pushConstants(layout, vk::ShaderStageFlagBits::eCompute, 0, // offset
            sizeof(WaveFdNS::WavePushConstants) /*size*/, &pushConstants); // pointer to the data
        commandBuffer.dispatch(groupsX, groupsY, groupsZ);
        if (s < substeps - 1) {
            vk::MemoryBarrier barrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead);
        commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {barrier}, {}, {});
        }
        rotation = (rotation + 2) % 3;
    }
    commandBuffer.end();
}

WaveFdNS::GpuBuffer WaveGpu::createBuffer(VmaAllocator allocator, VkDeviceSize size, VkBufferUsageFlags usage, VmaAllocationCreateFlags vmaFlags) {
    VkBufferCreateInfo bufferInfo {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    VmaAllocationCreateInfo allocCreateInfo {};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocCreateInfo.flags = vmaFlags;
    WaveFdNS::GpuBuffer result {};
    result.size = size;
    vmaCreateBuffer(allocator, &bufferInfo, &allocCreateInfo, &result.buffer, &result.allocation, &result.info);
    return result;
}

void WaveGpu::initializeImageData(vk::Device device, VmaAllocator allocator, vk::Queue queue, vk::CommandPool commandPool,
        std::vector<float>& data, std::vector<vk::Image>& targetImages, uint32_t width, uint32_t height) {
    WaveFdNS::GpuBuffer stagingBuffer = createBuffer(allocator, width * height * sizeof(float),
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);
    std::memcpy(stagingBuffer.info.pMappedData, data.data(), data.size() * sizeof(float));

    vk::CommandBufferAllocateInfo allocInfo(commandPool, vk::CommandBufferLevel::ePrimary, 1);
    vk::CommandBuffer cmd = device.allocateCommandBuffers(allocInfo).front();
    cmd.begin(vk::CommandBufferBeginInfo{vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    vk::BufferImageCopy region{};
    region.imageSubresource = vk::ImageSubresourceLayers{vk::ImageAspectFlagBits::eColor, 0, 0, 1};
    region.imageExtent = vk::Extent3D{width, height, 1};

    for (vk::Image image : targetImages) {
        cmd.copyBufferToImage(stagingBuffer.buffer, image, vk::ImageLayout::eGeneral, {region});
    }
    cmd.end();
    vk::SubmitInfo submit(0, nullptr, nullptr, 1, &cmd);
    queue.submit({submit}, nullptr);
    queue.waitIdle();

    vmaDestroyBuffer(allocator, stagingBuffer.buffer, stagingBuffer.allocation);
}

void WaveGpu::run_compute(uint32_t gridWidth, uint32_t gridHeight, float dx, float velocity,
                SDL_Surface* surface, SDL_Texture* texture, SDL_Renderer* sdlRenderer) {
    // Enable MoltenVK if needed
    #ifdef __APPLE__
        setenv("VK_ICD_FILENAMES", "/Users/mae/VulkanSDK/1.4.350.0/macOS/share/vulkan/icd.d/MoltenVK_icd.json", 1);
	#endif

    WaveFdNS::GpuContext context = setupShaderContext();
    vk::Device device = context.device.device;

    std::array<WaveFdNS::GpuImage, 3> images;
    images[0] = createImage(context.allocator, device, gridWidth, gridHeight);
    vk::DescriptorImageInfo G0Info { nullptr, images[0].view, vk::ImageLayout::eGeneral };
    images[1] = createImage(context.allocator, device, gridWidth, gridHeight);
    vk::DescriptorImageInfo G1Info { nullptr, images[1].view, vk::ImageLayout::eGeneral };
    images[2] = createImage(context.allocator, device, gridWidth, gridHeight);
    vk::DescriptorImageInfo G2Info { nullptr, images[2].view, vk::ImageLayout::eGeneral };

    vk::DescriptorPool pool = createPool_wave(device);
    vk::DescriptorSetLayout layout = createLayout_wave(device);
    std::array<vk::DescriptorSet, 3> descriptorSets = createDescriptorSets(device, pool, layout, images);    
    std::string fName = "/Users/mae/workspace/wave-equation/src/shaders/wave.comp.spv";
    std::vector<char> shaderContents {};
    readShader(fName, shaderContents);
    WaveFdNS::ComputePipeline pipeline = createComputePipeline(shaderContents, context.device.device, layout);
    vk::CommandPoolCreateInfo CommandPoolCreateInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, context.computeQueueFamily); //allows individual command buffers from command pool to be reset independently
    vk::CommandPool commandPool = device.createCommandPool(CommandPoolCreateInfo);
    vk::CommandBufferAllocateInfo allocInfo(commandPool, vk::CommandBufferLevel::ePrimary, 1);
    vk::CommandBuffer commandBuffer = device.allocateCommandBuffers(allocInfo).front();
    uint32_t localSizeX = 16; uint32_t localSizeY = 16; 
    uint32_t groupsX = (gridWidth + localSizeX - 1) / localSizeX; // how many workgroups are dispatched in X and Y dims
    uint32_t groupsY = (gridHeight + localSizeY - 1) / localSizeY; 
    vk::Queue Queue = device.getQueue(context.computeQueueFamily, 0);
    
    vk::CommandBufferAllocateInfo allocCommandBuffer(commandPool, vk::CommandBufferLevel::ePrimary, 1);
    vk::CommandBuffer oneShotCmd = device.allocateCommandBuffers(allocCommandBuffer).front();
    oneShotCmd.begin(vk::CommandBufferBeginInfo{vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    for (int i = 0; i < 3; i++) {
        transitionImageLayout(oneShotCmd, images[i].image,
            vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral,
            {}, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
            vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eComputeShader);
    }
    oneShotCmd.end();
    vk::SubmitInfo submit(0, nullptr, nullptr, 1, &oneShotCmd);
    Queue.submit({submit}, nullptr);
    Queue.waitIdle();

    // Create stagingBuffer as a CPU-readable intermediate buffer
    WaveFdNS::GpuBuffer stagingBuffer = createBuffer(context.allocator, gridWidth*gridHeight*sizeof(float), VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);
    vk::Fence Fence = device.createFence(vk::FenceCreateInfo());

    std::vector<float> restData(gridWidth * gridHeight, 127.0f);
    std::vector<vk::Image> allThreeImages = { images[0].image, images[1].image, images[2].image };
    initializeImageData(device, context.allocator, Queue, commandPool, restData, allThreeImages, gridWidth, gridHeight);

    //std::vector<float> impulseData(gridWidth * gridHeight, 127.0f);
    //impulseData[(gridHeight / 2) * gridWidth + (gridWidth / 2)] = 2000.0f;
    std::vector<float> impulseData(gridWidth * gridHeight, 127.0f);
    float cx = gridWidth / 2.0f;
    float cy = gridHeight / 2.0f;
    float sigma = 6.0f; // initial displacment radius
    float amplitude = 60.0f; // peak displacement above rest
    for (uint32_t y = 0; y < gridHeight; y++) {
        for (uint32_t x = 0; x < gridWidth; x++) {
            float dx = x - cx;
            float dy = y - cy;
            float r2 = dx * dx + dy * dy;
            impulseData[y * gridWidth + x] += amplitude * std::exp(-r2 / (2.0f * sigma * sigma));
        }
    }
    std::vector<vk::Image> impulseImages = { images[1].image };
    initializeImageData(device, context.allocator, Queue, commandPool, impulseData, impulseImages, gridWidth, gridHeight);
   
    uint32_t rotation = 0;
    SDL_Event e;
    bool quit = false;
    uint64_t lastTime = SDL_GetTicks();
    float time = 0;
    while (!quit) {
        uint64_t currentTime = SDL_GetTicks();
        float deltaTime = (currentTime - lastTime) / 1000.0f; 
        deltaTime = std::min(deltaTime, 1.0f / 60.0f);
        lastTime = currentTime;
        time += deltaTime;
        float C_MAX = 0.5f; // stability bound
        float courant = velocity * deltaTime / dx;
        uint32_t substeps = std::max(1, (int)std::ceil(courant / C_MAX));
        float subDt = deltaTime / substeps;
        float dt2v2_dx2 = (subDt * velocity / dx);
        dt2v2_dx2 *= dt2v2_dx2;

        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) quit = true;
        }
        WaveFdNS::WavePushConstants pushConstants{ gridWidth, gridHeight, dt2v2_dx2 };
        commandBuffer.reset();
        recordComputeDispatch(commandBuffer, pipeline.pipeline, pipeline.pipelineLayout, descriptorSets, pushConstants, groupsX, groupsY, 1, rotation, substeps);
        device.resetFences({Fence});
        vk::SubmitInfo submitInfo(0, nullptr, nullptr, 1, &commandBuffer);
        Queue.submit({submitInfo}, Fence);
        device.waitForFences({Fence}, true, UINT64_MAX);

        //readback, needs its own command buffer + submit + wait
        commandBuffer.reset();
        commandBuffer.begin(vk::CommandBufferBeginInfo{vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
        transitionImageLayout(commandBuffer, images[rotation].image,
            vk::ImageLayout::eGeneral, vk::ImageLayout::eTransferSrcOptimal,
            vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eTransferRead,
            vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer);

        vk::BufferImageCopy region{};
        region.imageSubresource = vk::ImageSubresourceLayers{vk::ImageAspectFlagBits::eColor, 0, 0, 1};
        region.imageExtent = vk::Extent3D{gridWidth, gridHeight, 1};
        commandBuffer.copyImageToBuffer(images[rotation].image, vk::ImageLayout::eTransferSrcOptimal, stagingBuffer.buffer, {region});
        transitionImageLayout(commandBuffer, images[rotation].image, vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eGeneral,
            vk::AccessFlagBits::eTransferRead, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader);
        commandBuffer.end();
        device.resetFences({Fence});
        vk::SubmitInfo copySubmit(0, nullptr, nullptr, 1, &commandBuffer);
        Queue.submit({copySubmit}, Fence);
        device.waitForFences({Fence}, true, UINT64_MAX);
        std::vector<float> hostData(gridWidth * gridHeight);
        std::memcpy(hostData.data(), stagingBuffer.info.pMappedData, hostData.size() * sizeof(float));
        
        //Render hostData image to surface
        Color backgroundColor = hueToColor(time / 6.);
        SDL_LockSurface(surface);
        renderSimTexture(hostData, surface, gridWidth, gridHeight, backgroundColor);
        SDL_UnlockSurface(surface);
        // Display the image
        SDL_SetRenderTarget(sdlRenderer, nullptr);
        SDL_FRect dst = { 0, 0, (float)gridWidth, (float)gridHeight };
        SDL_UpdateTexture(texture, nullptr, surface->pixels, surface->pitch);
        SDL_RenderTexture(sdlRenderer, texture, NULL, NULL);
        SDL_RenderPresent(sdlRenderer);
    }
    device.destroyPipeline(pipeline.pipeline);
    device.destroyPipelineLayout(pipeline.pipelineLayout);
    device.destroyDescriptorSetLayout(pipeline.descriptorSetLayout);
    device.destroyDescriptorPool(pool);
    device.destroyFence(Fence);
    device.destroyCommandPool(commandPool);
    device.destroyImageView(images[0].view);
    device.destroyImageView(images[1].view);
    device.destroyImageView(images[2].view);
    vmaDestroyImage(context.allocator, images[0].image, images[0].allocation);
    vmaDestroyImage(context.allocator, images[1].image, images[1].allocation);
    vmaDestroyImage(context.allocator, images[2].image, images[2].allocation);
    vmaDestroyBuffer(context.allocator, stagingBuffer.buffer, stagingBuffer.allocation);
    teardownShaderContext(context);
}