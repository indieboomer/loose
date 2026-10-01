#include <stdexcept>
#include <string>
#include "renderer.hpp"
#include "vulkan_api.hpp"
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <fstream>
#include <iostream>
#include <array>
#include <vector>
#include <filesystem>
#include <atomic>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#undef APIENTRY
#include <windows.h>
#else
#include <dlfcn.h>
#endif
namespace loose {
namespace {
struct Vertex {vec3 position,normal;};
struct alignas(16) Uniform {mat4 view,projection,invView,invProjection,lightVP;vec4 light,eye,options,viewport;};
struct Push {mat4 model;vec4 color;};
struct Buffer {VkBuffer handle{};VkDeviceMemory memory{};VkDeviceSize size=0;void* mapped=nullptr;};
struct Image {VkImage handle{};VkDeviceMemory memory{};VkImageView view{};};
std::atomic_uint errors{0};
VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,VkDebugUtilsMessageTypeFlagsEXT,const VkDebugUtilsMessengerCallbackDataEXT* data,void*){
    if(severity&VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)++errors;
    std::cerr<<"Vulkan validation: "<<data->pMessage<<'\n';return VK_FALSE;
}
void loadFunctions(VkInstance instance){
#define LOAD(name) name=reinterpret_cast<PFN_##name>(vkGetInstanceProcAddr(instance,#name));
LOOSE_VK_FUNCTIONS(LOAD)
#undef LOAD
}
std::vector<Vertex> boxVertices(){
    std::vector<Vertex> v;
    auto face=[&](vec3 n,vec3 a,vec3 b,vec3 c,vec3 d){for(vec3 p:{a,b,c,a,c,d})v.push_back({p,n});};
    face({0,0,1},{-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1});
    face({0,0,-1},{1,-1,-1},{-1,-1,-1},{-1,1,-1},{1,1,-1});
    face({1,0,0},{1,-1,1},{1,-1,-1},{1,1,-1},{1,1,1});
    face({-1,0,0},{-1,-1,-1},{-1,-1,1},{-1,1,1},{-1,1,-1});
    face({0,1,0},{-1,1,1},{1,1,1},{1,1,-1},{-1,1,-1});
    face({0,-1,0},{-1,-1,-1},{1,-1,-1},{1,-1,1},{-1,-1,1});
    return v;
}
}
struct Renderer::Impl {
    GLFWwindow* window;
    Level level;
    std::string gpu;
    VkInstance instance{};VkDebugUtilsMessengerEXT messenger{};VkSurfaceKHR surface{};
    VkPhysicalDevice physical{};VkDevice device{};VkQueue queue{};uint32_t family=0;
    VkPhysicalDeviceMemoryProperties memory{};
    VkSwapchainKHR swapchain{};VkFormat swapFormat{};VkExtent2D extent{};
    std::vector<VkImage> swapImages;
    std::vector<VkImageView> swapViews;
    std::vector<VkFramebuffer> swapFrames;
    std::vector<VkSemaphore> finished;
    VkSemaphore acquired{};VkFence fence{};
    VkCommandPool commands{};VkCommandBuffer command{};
    VkRenderPass geometryPass{},shadowPass{},lightingPass{},outputPass{};
    VkFramebuffer geometryFrame{},shadowFrame{},lightingFrame{};
    Image albedo,normal,depth,shadow,lit;
    VkSampler sampler{},shadowSampler{},linearSampler{};
    VkDescriptorPool descriptors{};VkDescriptorSetLayout setLayout{};VkDescriptorSet set{};
    VkPipelineLayout pipelineLayout{};
    VkPipeline geometryPipeline{},shadowPipeline{},lightingPipeline{},postPipeline{};
    Buffer vertices,uniform,captureBuffer;
    bool uiReady=false,swapDirty=false,captureSupported=false,validationEnabled=false;
    uint32_t minImages=2;
#ifdef _WIN32
    HMODULE loader=nullptr;
#else
    void* loader=nullptr;
#endif
    explicit Impl(GLFWwindow* w,const Level& l):window(w),level(l){}
    uint32_t memoryType(uint32_t bits,VkMemoryPropertyFlags flags){
        for(uint32_t i=0;i<memory.memoryTypeCount;i++)if((bits&(1u<<i))&&(memory.memoryTypes[i].propertyFlags&flags)==flags)return i;
        throw std::runtime_error("No suitable Vulkan memory type");
    }
    Buffer makeBuffer(VkDeviceSize size,VkBufferUsageFlags usage){
        Buffer b;b.size=size;
        VkBufferCreateInfo ci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};ci.size=size;ci.usage=usage;ci.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
        vkCheck(vkCreateBuffer(device,&ci,nullptr,&b.handle));
        VkMemoryRequirements req;vkGetBufferMemoryRequirements(device,b.handle,&req);
        VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};ai.allocationSize=req.size;
        ai.memoryTypeIndex=memoryType(req.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        vkCheck(vkAllocateMemory(device,&ai,nullptr,&b.memory));vkCheck(vkBindBufferMemory(device,b.handle,b.memory,0));
        vkCheck(vkMapMemory(device,b.memory,0,size,0,&b.mapped));return b;
    }
    void destroy(Buffer& b){if(b.mapped)vkUnmapMemory(device,b.memory);if(b.handle)vkDestroyBuffer(device,b.handle,nullptr);if(b.memory)vkFreeMemory(device,b.memory,nullptr);b={};}
    Image makeImage(uint32_t width,uint32_t height,VkFormat format,VkImageUsageFlags usage,VkImageAspectFlags aspect){
        Image image;
        VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};ci.imageType=VK_IMAGE_TYPE_2D;ci.format=format;ci.extent={width,height,1};ci.mipLevels=1;ci.arrayLayers=1;ci.samples=VK_SAMPLE_COUNT_1_BIT;ci.tiling=VK_IMAGE_TILING_OPTIMAL;ci.usage=usage;ci.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
        vkCheck(vkCreateImage(device,&ci,nullptr,&image.handle));VkMemoryRequirements req;vkGetImageMemoryRequirements(device,image.handle,&req);
        VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};ai.allocationSize=req.size;ai.memoryTypeIndex=memoryType(req.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vkCheck(vkAllocateMemory(device,&ai,nullptr,&image.memory));vkCheck(vkBindImageMemory(device,image.handle,image.memory,0));
        image.view=makeView(image.handle,format,aspect);return image;
    }
    VkImageView makeView(VkImage image,VkFormat format,VkImageAspectFlags aspect){
        VkImageViewCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};ci.image=image;ci.viewType=VK_IMAGE_VIEW_TYPE_2D;ci.format=format;ci.subresourceRange={aspect,0,1,0,1};VkImageView view;
        vkCheck(vkCreateImageView(device,&ci,nullptr,&view));return view;
    }
    void destroy(Image& image){if(image.view)vkDestroyImageView(device,image.view,nullptr);if(image.handle)vkDestroyImage(device,image.handle,nullptr);if(image.memory)vkFreeMemory(device,image.memory,nullptr);image={};}
    void init(bool validation){
#ifdef _WIN32
        loader=LoadLibraryW(L"vulkan-1.dll");if(loader)vkGetInstanceProcAddr=reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(loader,"vkGetInstanceProcAddr"));
#else
        loader=dlopen("libvulkan.so.1",RTLD_NOW);if(loader)vkGetInstanceProcAddr=reinterpret_cast<PFN_vkGetInstanceProcAddr>(dlsym(loader,"vkGetInstanceProcAddr"));
#endif
        if(!vkGetInstanceProcAddr)throw std::runtime_error("Vulkan loader missing. Install a Vulkan-capable GPU driver.");
        loadFunctions(VK_NULL_HANDLE);
        uint32_t count=0;const char** required=glfwGetRequiredInstanceExtensions(&count);
        if(!required)throw std::runtime_error("GLFW cannot find Vulkan surface support");
        std::vector<const char*> extensions(required,required+count),layers;
        uint32_t layerCount=0;vkCheck(vkEnumerateInstanceLayerProperties(&layerCount,nullptr));std::vector<VkLayerProperties> available(layerCount);vkCheck(vkEnumerateInstanceLayerProperties(&layerCount,available.data()));
        if(validation)for(auto& a:available)if(std::string(a.layerName)=="VK_LAYER_KHRONOS_validation"){layers.push_back("VK_LAYER_KHRONOS_validation");extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);}
        if(validation&&layers.empty())std::cout<<"Validation layer unavailable; rendering without validation.\n";
        validationEnabled=!layers.empty();
        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};app.pApplicationName="LOOSE - LEVEL ZERO";app.applicationVersion=VK_MAKE_VERSION(0,1,0);app.pEngineName="LOOSE";app.apiVersion=VK_API_VERSION_1_1;
        VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};ci.pApplicationInfo=&app;ci.enabledExtensionCount=uint32_t(extensions.size());ci.ppEnabledExtensionNames=extensions.data();ci.enabledLayerCount=uint32_t(layers.size());ci.ppEnabledLayerNames=layers.data();
        vkCheck(vkCreateInstance(&ci,nullptr,&instance));loadFunctions(instance);
        if(!layers.empty()){
            VkDebugUtilsMessengerCreateInfoEXT di{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};di.messageSeverity=VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;di.messageType=VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;di.pfnUserCallback=debugCallback;
            auto create=reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance,"vkCreateDebugUtilsMessengerEXT"));vkCheck(create(instance,&di,nullptr,&messenger));
        }
        vkCheck(glfwCreateWindowSurface(instance,window,nullptr,&surface));
        uint32_t n=0;vkCheck(vkEnumeratePhysicalDevices(instance,&n,nullptr));std::vector<VkPhysicalDevice> devices(n);vkCheck(vkEnumeratePhysicalDevices(instance,&n,devices.data()));
        int best=-1;
        for(auto d:devices){
            VkPhysicalDeviceProperties deviceProperties;vkGetPhysicalDeviceProperties(d,&deviceProperties);
            if(deviceProperties.apiVersion<VK_API_VERSION_1_1)continue;
            uint32_t ec=0;vkCheck(vkEnumerateDeviceExtensionProperties(d,nullptr,&ec,nullptr));std::vector<VkExtensionProperties> ex(ec);vkCheck(vkEnumerateDeviceExtensionProperties(d,nullptr,&ec,ex.data()));
            bool swap=false;for(auto& e:ex)if(std::string(e.extensionName)==VK_KHR_SWAPCHAIN_EXTENSION_NAME)swap=true;if(!swap)continue;
            VkFormatProperties fp{};vkGetPhysicalDeviceFormatProperties(d,VK_FORMAT_D32_SFLOAT,&fp);
            if((fp.optimalTilingFeatures&(VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))!=(VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))continue;
            uint32_t qc=0;vkGetPhysicalDeviceQueueFamilyProperties(d,&qc,nullptr);std::vector<VkQueueFamilyProperties> qs(qc);vkGetPhysicalDeviceQueueFamilyProperties(d,&qc,qs.data());
            for(uint32_t q=0;q<qc;q++){VkBool32 present=false;vkCheck(vkGetPhysicalDeviceSurfaceSupportKHR(d,q,surface,&present));if(present&&(qs[q].queueFlags&VK_QUEUE_GRAPHICS_BIT)){
                VkPhysicalDeviceProperties props;vkGetPhysicalDeviceProperties(d,&props);int score=props.deviceType==VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU?2:1;
                if(score>best){best=score;physical=d;family=q;gpu=props.deviceName;}break;
            }}
        }
        if(!physical)throw std::runtime_error("No Vulkan GPU supports graphics, presentation and sampled depth. Update the GPU driver.");
        vkGetPhysicalDeviceMemoryProperties(physical,&memory);float priority=1;
        VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};qi.queueFamilyIndex=family;qi.queueCount=1;qi.pQueuePriorities=&priority;
        const char* deviceExtensions[]={VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        VkDeviceCreateInfo dc{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};dc.queueCreateInfoCount=1;dc.pQueueCreateInfos=&qi;dc.enabledExtensionCount=1;dc.ppEnabledExtensionNames=deviceExtensions;
        vkCheck(vkCreateDevice(physical,&dc,nullptr,&device));vkGetDeviceQueue(device,family,0,&queue);
        VkCommandPoolCreateInfo pc{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pc.queueFamilyIndex=family;pc.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        vkCheck(vkCreateCommandPool(device,&pc,nullptr,&commands));
        VkCommandBufferAllocateInfo ca{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ca.commandPool=commands;ca.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ca.commandBufferCount=1;vkCheck(vkAllocateCommandBuffers(device,&ca,&command));
        VkFenceCreateInfo fc{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};fc.flags=VK_FENCE_CREATE_SIGNALED_BIT;vkCheck(vkCreateFence(device,&fc,nullptr,&fence));
        VkSemaphoreCreateInfo sc{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};vkCheck(vkCreateSemaphore(device,&sc,nullptr,&acquired));
        auto mesh=boxVertices();vertices=makeBuffer(mesh.size()*sizeof(Vertex),VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);memcpy(vertices.mapped,mesh.data(),size_t(vertices.size));
        uniform=makeBuffer(sizeof(Uniform),VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        initDescriptors();initRenderPasses();initPipelines();
        shadow=makeImage(2048,2048,VK_FORMAT_D32_SFLOAT,VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,VK_IMAGE_ASPECT_DEPTH_BIT);
        shadowFrame=makeFrame(shadowPass,{shadow.view},2048,2048);
        recreateSwapchain();
        ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;ImGui::StyleColorsDark();
        ImGui::GetStyle().WindowRounding=5;ImGui::GetStyle().FrameRounding=3;
        ImGui_ImplGlfw_InitForVulkan(window,true);
        ImGui_ImplVulkan_LoadFunctions([](const char* name,void* data){return vkGetInstanceProcAddr(static_cast<Impl*>(data)->instance,name);},this);
        initUI();uiReady=true;
        std::cout<<"GPU: "<<gpu<<" | "<<extent.width<<'x'<<extent.height<<" | Vulkan 1.1 | FIFO\n";
    }
    void initDescriptors(){
        VkDescriptorPoolSize sizes[]={{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,16},{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,128}};
        VkDescriptorPoolCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};ci.flags=VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;ci.maxSets=128;ci.poolSizeCount=2;ci.pPoolSizes=sizes;vkCheck(vkCreateDescriptorPool(device,&ci,nullptr,&descriptors));
        std::array<VkDescriptorSetLayoutBinding,6> bindings{};
        bindings[0]={0,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,nullptr};
        for(uint32_t i=1;i<6;i++)bindings[i]={i,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr};
        VkDescriptorSetLayoutCreateInfo sl{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};sl.bindingCount=6;sl.pBindings=bindings.data();vkCheck(vkCreateDescriptorSetLayout(device,&sl,nullptr,&setLayout));
        VkDescriptorSetAllocateInfo sa{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};sa.descriptorPool=descriptors;sa.descriptorSetCount=1;sa.pSetLayouts=&setLayout;vkCheck(vkAllocateDescriptorSets(device,&sa,&set));
        VkPushConstantRange push{VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(Push)};
        VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};pl.setLayoutCount=1;pl.pSetLayouts=&setLayout;pl.pushConstantRangeCount=1;pl.pPushConstantRanges=&push;vkCheck(vkCreatePipelineLayout(device,&pl,nullptr,&pipelineLayout));
        VkSamplerCreateInfo sm{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};sm.magFilter=sm.minFilter=VK_FILTER_NEAREST;sm.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;sm.addressModeU=sm.addressModeV=sm.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;sm.maxLod=0;
        vkCheck(vkCreateSampler(device,&sm,nullptr,&sampler));
        sm.magFilter=sm.minFilter=VK_FILTER_LINEAR;
        vkCheck(vkCreateSampler(device,&sm,nullptr,&linearSampler));
        sm.magFilter=sm.minFilter=VK_FILTER_NEAREST;
        sm.addressModeU=sm.addressModeV=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;sm.borderColor=VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        vkCheck(vkCreateSampler(device,&sm,nullptr,&shadowSampler));
    }
    VkRenderPass makePass(const std::vector<VkFormat>& colors,bool withDepth,bool present){
        std::vector<VkAttachmentDescription> attachments;
        std::vector<VkAttachmentReference> refs;
        for(uint32_t i=0;i<colors.size();i++){
            VkAttachmentDescription a{};a.format=colors[i];a.samples=VK_SAMPLE_COUNT_1_BIT;a.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;a.storeOp=VK_ATTACHMENT_STORE_OP_STORE;a.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;a.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;a.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;a.finalLayout=present?VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            attachments.push_back(a);refs.push_back({i,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});
        }
        VkAttachmentReference dr{uint32_t(colors.size()),VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
        if(withDepth){VkAttachmentDescription a{};a.format=VK_FORMAT_D32_SFLOAT;a.samples=VK_SAMPLE_COUNT_1_BIT;a.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;a.storeOp=VK_ATTACHMENT_STORE_OP_STORE;a.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;a.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;a.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;a.finalLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;attachments.push_back(a);}
        VkSubpassDescription sub{};sub.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;sub.colorAttachmentCount=uint32_t(refs.size());sub.pColorAttachments=refs.data();if(withDepth)sub.pDepthStencilAttachment=&dr;
        std::array<VkSubpassDependency,2> deps{};
        deps[0].srcSubpass=VK_SUBPASS_EXTERNAL;deps[0].dstSubpass=0;deps[0].srcStageMask=VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;deps[0].dstStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;deps[0].srcAccessMask=VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT;deps[0].dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        deps[1].srcSubpass=0;deps[1].dstSubpass=VK_SUBPASS_EXTERNAL;deps[1].srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;deps[1].dstStageMask=present?VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT:VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;deps[1].srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;deps[1].dstAccessMask=present?0:VK_ACCESS_SHADER_READ_BIT;
        VkRenderPassCreateInfo ci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};ci.attachmentCount=uint32_t(attachments.size());ci.pAttachments=attachments.data();ci.subpassCount=1;ci.pSubpasses=&sub;ci.dependencyCount=2;ci.pDependencies=deps.data();VkRenderPass pass;vkCheck(vkCreateRenderPass(device,&ci,nullptr,&pass));return pass;
    }
    void initRenderPasses(){geometryPass=makePass({VK_FORMAT_R8G8B8A8_UNORM,VK_FORMAT_R16G16B16A16_SFLOAT},true,false);shadowPass=makePass({},true,false);lightingPass=makePass({VK_FORMAT_R16G16B16A16_SFLOAT},false,false);}
    VkFramebuffer makeFrame(VkRenderPass pass,std::vector<VkImageView> views,uint32_t width,uint32_t height){
        VkFramebufferCreateInfo ci{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};ci.renderPass=pass;ci.attachmentCount=uint32_t(views.size());ci.pAttachments=views.data();ci.width=width;ci.height=height;ci.layers=1;VkFramebuffer frame;vkCheck(vkCreateFramebuffer(device,&ci,nullptr,&frame));return frame;
    }
    VkShaderModule shader(const char* name){
        std::filesystem::path directory=LOOSE_SHADER_DIR;
#ifdef _WIN32
        wchar_t executable[32768];DWORD lengthPath=GetModuleFileNameW(nullptr,executable,32768);
        if(lengthPath){auto local=std::filesystem::path(executable).parent_path()/"shaders";if(std::filesystem::exists(local))directory=local;}
#endif
        std::ifstream file(directory/(std::string(name)+".spv"),std::ios::binary|std::ios::ate);
        if(!file)throw std::runtime_error(std::string("Missing compiled shader: ")+name);
        auto length=file.tellg();std::vector<uint32_t> code(size_t(length)/4);file.seekg(0);file.read(reinterpret_cast<char*>(code.data()),length);
        VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};ci.codeSize=size_t(length);ci.pCode=code.data();VkShaderModule module;vkCheck(vkCreateShaderModule(device,&ci,nullptr,&module));return module;
    }
    VkPipeline makePipeline(VkRenderPass pass,const char* vs,const char* fs,int colorCount,bool depthTest){
        VkShaderModule vert=shader(vs),frag=fs?shader(fs):VK_NULL_HANDLE;
        VkPipelineShaderStageCreateInfo stages[2]{};stages[0]={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,nullptr,0,VK_SHADER_STAGE_VERTEX_BIT,vert,"main",nullptr};if(fs)stages[1]={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,nullptr,0,VK_SHADER_STAGE_FRAGMENT_BIT,frag,"main",nullptr};
        VkVertexInputBindingDescription binding{0,sizeof(Vertex),VK_VERTEX_INPUT_RATE_VERTEX};
        VkVertexInputAttributeDescription attributes[]={{0,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(Vertex,position)},{1,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(Vertex,normal)}};
        VkPipelineVertexInputStateCreateInfo vertex{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};if(depthTest){vertex.vertexBindingDescriptionCount=1;vertex.pVertexBindingDescriptions=&binding;vertex.vertexAttributeDescriptionCount=2;vertex.pVertexAttributeDescriptions=attributes;}
        VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};assembly.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};viewport.viewportCount=1;viewport.scissorCount=1;
        VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};raster.polygonMode=VK_POLYGON_MODE_FILL;raster.cullMode=depthTest?VK_CULL_MODE_BACK_BIT:VK_CULL_MODE_NONE;raster.frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE;raster.lineWidth=1;
        if(!fs){raster.depthBiasEnable=VK_TRUE;raster.depthBiasConstantFactor=1.2f;raster.depthBiasSlopeFactor=1.4f;}
        VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
        VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};ds.depthTestEnable=depthTest;ds.depthWriteEnable=depthTest;ds.depthCompareOp=VK_COMPARE_OP_LESS;
        std::vector<VkPipelineColorBlendAttachmentState> blends(colorCount);for(auto& b:blends)b.colorWriteMask=15;
        VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};blend.attachmentCount=colorCount;blend.pAttachments=blends.data();
        VkDynamicState dynamicStates[]={VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};dynamic.dynamicStateCount=2;dynamic.pDynamicStates=dynamicStates;
        VkGraphicsPipelineCreateInfo ci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};ci.stageCount=fs?2:1;ci.pStages=stages;ci.pVertexInputState=&vertex;ci.pInputAssemblyState=&assembly;ci.pViewportState=&viewport;ci.pRasterizationState=&raster;ci.pMultisampleState=&ms;ci.pDepthStencilState=&ds;ci.pColorBlendState=&blend;ci.pDynamicState=&dynamic;ci.layout=pipelineLayout;ci.renderPass=pass;
        VkPipeline pipeline;VkResult result=vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&ci,nullptr,&pipeline);vkDestroyShaderModule(device,vert,nullptr);if(frag)vkDestroyShaderModule(device,frag,nullptr);vkCheck(result);return pipeline;
    }
    void initPipelines(){geometryPipeline=makePipeline(geometryPass,"geometry.vert","geometry.frag",2,true);shadowPipeline=makePipeline(shadowPass,"shadow.vert",nullptr,0,true);lightingPipeline=makePipeline(lightingPass,"fullscreen.vert","lighting.frag",1,false);}
    void initUI(){
        ImGui_ImplVulkan_InitInfo info{};info.Instance=instance;info.PhysicalDevice=physical;info.Device=device;info.QueueFamily=family;info.Queue=queue;info.DescriptorPool=descriptors;info.RenderPass=outputPass;info.MinImageCount=minImages;info.ImageCount=uint32_t(swapImages.size());info.MSAASamples=VK_SAMPLE_COUNT_1_BIT;info.CheckVkResultFn=[](VkResult r){vkCheck(r);};
        ImGui_ImplVulkan_Init(&info);ImGui_ImplVulkan_CreateFontsTexture();
    }
    void clearSwapchain(){
        for(auto f:swapFrames)vkDestroyFramebuffer(device,f,nullptr);swapFrames.clear();
        for(auto v:swapViews)vkDestroyImageView(device,v,nullptr);swapViews.clear();
        for(auto s:finished)vkDestroySemaphore(device,s,nullptr);finished.clear();
        if(geometryFrame)vkDestroyFramebuffer(device,geometryFrame,nullptr);geometryFrame={};
        if(lightingFrame)vkDestroyFramebuffer(device,lightingFrame,nullptr);lightingFrame={};
        destroy(albedo);destroy(normal);destroy(depth);destroy(lit);destroy(captureBuffer);
        if(postPipeline)vkDestroyPipeline(device,postPipeline,nullptr);postPipeline={};
        if(outputPass)vkDestroyRenderPass(device,outputPass,nullptr);outputPass={};
        if(swapchain)vkDestroySwapchainKHR(device,swapchain,nullptr);swapchain={};
    }
    bool recreateSwapchain(){
        int w,h;glfwGetFramebufferSize(window,&w,&h);if(w==0||h==0)return false;
        vkCheck(vkDeviceWaitIdle(device));if(uiReady)ImGui_ImplVulkan_Shutdown();clearSwapchain();
        VkSurfaceCapabilitiesKHR caps;vkCheck(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical,surface,&caps));
        extent=caps.currentExtent;if(extent.width==UINT32_MAX)extent={std::clamp(uint32_t(w),caps.minImageExtent.width,caps.maxImageExtent.width),std::clamp(uint32_t(h),caps.minImageExtent.height,caps.maxImageExtent.height)};
        uint32_t n=0;vkCheck(vkGetPhysicalDeviceSurfaceFormatsKHR(physical,surface,&n,nullptr));std::vector<VkSurfaceFormatKHR> formats(n);vkCheck(vkGetPhysicalDeviceSurfaceFormatsKHR(physical,surface,&n,formats.data()));
        bool found=false;VkSurfaceFormatKHR format{};for(auto f:formats)if((f.format==VK_FORMAT_B8G8R8A8_SRGB||f.format==VK_FORMAT_R8G8B8A8_SRGB)&&f.colorSpace==VK_COLOR_SPACE_SRGB_NONLINEAR_KHR){format=f;found=true;break;}
        if(!found)throw std::runtime_error("No sRGB Vulkan surface format available");swapFormat=format.format;
        minImages=std::max(2u,caps.minImageCount);uint32_t imageCount=minImages+1;if(caps.maxImageCount)imageCount=std::min(imageCount,caps.maxImageCount);
        captureSupported=(caps.supportedUsageFlags&VK_IMAGE_USAGE_TRANSFER_SRC_BIT)!=0;
        VkSwapchainCreateInfoKHR ci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};ci.surface=surface;ci.minImageCount=imageCount;ci.imageFormat=format.format;ci.imageColorSpace=format.colorSpace;ci.imageExtent=extent;ci.imageArrayLayers=1;ci.imageUsage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|(captureSupported?VK_IMAGE_USAGE_TRANSFER_SRC_BIT:0);ci.imageSharingMode=VK_SHARING_MODE_EXCLUSIVE;ci.preTransform=caps.currentTransform;ci.compositeAlpha=VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;ci.presentMode=VK_PRESENT_MODE_FIFO_KHR;ci.clipped=VK_TRUE;
        vkCheck(vkCreateSwapchainKHR(device,&ci,nullptr,&swapchain));vkCheck(vkGetSwapchainImagesKHR(device,swapchain,&n,nullptr));swapImages.resize(n);vkCheck(vkGetSwapchainImagesKHR(device,swapchain,&n,swapImages.data()));
        outputPass=makePass({swapFormat},false,true);postPipeline=makePipeline(outputPass,"fullscreen.vert","post.frag",1,false);
        for(auto image:swapImages){swapViews.push_back(makeView(image,swapFormat,VK_IMAGE_ASPECT_COLOR_BIT));swapFrames.push_back(makeFrame(outputPass,{swapViews.back()},extent.width,extent.height));VkSemaphoreCreateInfo si{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};VkSemaphore s;vkCheck(vkCreateSemaphore(device,&si,nullptr,&s));finished.push_back(s);}
        albedo=makeImage(extent.width,extent.height,VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,VK_IMAGE_ASPECT_COLOR_BIT);
        normal=makeImage(extent.width,extent.height,VK_FORMAT_R16G16B16A16_SFLOAT,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,VK_IMAGE_ASPECT_COLOR_BIT);
        depth=makeImage(extent.width,extent.height,VK_FORMAT_D32_SFLOAT,VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,VK_IMAGE_ASPECT_DEPTH_BIT);
        geometryFrame=makeFrame(geometryPass,{albedo.view,normal.view,depth.view},extent.width,extent.height);
        lit=makeImage(extent.width,extent.height,VK_FORMAT_R16G16B16A16_SFLOAT,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,VK_IMAGE_ASPECT_COLOR_BIT);
        lightingFrame=makeFrame(lightingPass,{lit.view},extent.width,extent.height);
        if(captureSupported)captureBuffer=makeBuffer(VkDeviceSize(extent.width)*extent.height*4,VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        VkDescriptorBufferInfo ub{uniform.handle,0,sizeof(Uniform)};
        VkDescriptorImageInfo images[]={{sampler,albedo.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},{sampler,normal.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},{sampler,depth.view,VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL},{shadowSampler,shadow.view,VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL},{linearSampler,lit.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}};
        std::array<VkWriteDescriptorSet,6> writes{};for(uint32_t i=0;i<6;i++){auto& wr=writes[i];wr.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;wr.dstSet=set;wr.dstBinding=i;wr.descriptorCount=1;wr.descriptorType=i?VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;if(i)wr.pImageInfo=&images[i-1];else wr.pBufferInfo=&ub;}
        vkUpdateDescriptorSets(device,6,writes.data(),0,nullptr);
        if(uiReady)initUI();swapDirty=false;return true;
    }
    void startPass(VkRenderPass pass,VkFramebuffer frame,uint32_t width,uint32_t height,std::vector<VkClearValue> clears){
        VkRenderPassBeginInfo bi{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};bi.renderPass=pass;bi.framebuffer=frame;bi.renderArea.extent={width,height};bi.clearValueCount=uint32_t(clears.size());bi.pClearValues=clears.data();vkCmdBeginRenderPass(command,&bi,VK_SUBPASS_CONTENTS_INLINE);
        VkViewport vp{0,0,float(width),float(height),0,1};VkRect2D scissor{{0,0},{width,height}};vkCmdSetViewport(command,0,1,&vp);vkCmdSetScissor(command,0,1,&scissor);
        vkCmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_GRAPHICS,pipelineLayout,0,1,&set,0,nullptr);
    }
    void scene(const Game& game){
        VkDeviceSize offset=0;vkCmdBindVertexBuffers(command,0,1,&vertices.handle,&offset);
        for(auto& box:level.boxes){Push push{glm::translate(mat4(1),box.center)*glm::scale(mat4(1),box.half),vec4(box.color,1)};vkCmdPushConstants(command,pipelineLayout,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(Push),&push);vkCmdDraw(command,36,1,0,0);}
        Pose cube=game.renderCube();Push push{glm::translate(mat4(1),cube.position)*glm::mat4_cast(cube.rotation)*glm::scale(mat4(1),vec3(level.cubeSide*.5f)),vec4(level.cubeColor,1)};
        vkCmdPushConstants(command,pipelineLayout,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(Push),&push);vkCmdDraw(command,36,1,0,0);
    }
    void captureImage(uint32_t index){
        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};barrier.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;barrier.oldLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;barrier.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;barrier.image=swapImages[index];barrier.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
        vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
        VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={extent.width,extent.height,1};vkCmdCopyImageToBuffer(command,swapImages[index],VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,captureBuffer.handle,1,&copy);
        barrier.srcAccessMask=VK_ACCESS_TRANSFER_READ_BIT;barrier.dstAccessMask=0;barrier.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;barrier.newLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,0,0,nullptr,0,nullptr,1,&barrier);
        VkBufferMemoryBarrier host{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;host.srcQueueFamilyIndex=host.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;host.buffer=captureBuffer.handle;host.size=VK_WHOLE_SIZE;
        vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&host,0,nullptr);
    }
    void saveCapture(const std::string& path){
        if(!captureSupported)throw std::runtime_error("Surface does not support screenshot readback");
        auto parent=std::filesystem::path(path).parent_path();if(!parent.empty())std::filesystem::create_directories(parent);
        std::ofstream file(path,std::ios::binary);if(!file)throw std::runtime_error("Cannot write screenshot: "+path);
        file<<"P6\n"<<extent.width<<' '<<extent.height<<"\n255\n";auto* data=static_cast<unsigned char*>(captureBuffer.mapped);
        for(size_t i=0;i<size_t(extent.width)*extent.height;i++){unsigned char rgb[3]={data[4*i],data[4*i+1],data[4*i+2]};if(swapFormat==VK_FORMAT_B8G8R8A8_SRGB)std::swap(rgb[0],rgb[2]);file.write(reinterpret_cast<char*>(rgb),3);}
        std::cout<<"Captured "<<path<<'\n';
    }
    bool draw(const Game& game,const RenderSettings& settings,const std::string& capture){
        int w,h;glfwGetFramebufferSize(window,&w,&h);if(!w||!h)return false;
        if(swapDirty||uint32_t(w)!=extent.width||uint32_t(h)!=extent.height){swapDirty=true;return false;}
        vkCheck(vkWaitForFences(device,1,&fence,VK_TRUE,UINT64_MAX));
        uint32_t index;VkResult acquire=vkAcquireNextImageKHR(device,swapchain,UINT64_MAX,acquired,VK_NULL_HANDLE,&index);
        if(acquire==VK_ERROR_OUT_OF_DATE_KHR){swapDirty=true;return false;}if(acquire!=VK_SUCCESS&&acquire!=VK_SUBOPTIMAL_KHR)vkCheck(acquire);
        Uniform u{};vec3 eye=game.renderEye();
        u.view=glm::mat4_cast(glm::conjugate(game.orientation.camera()))*glm::translate(mat4(1),-eye);
        float aspect=float(extent.width)/extent.height;float vertical=2*std::atan(std::tan(glm::radians(settings.horizontalFov)*.5f)/aspect);
        u.projection=glm::perspective(vertical,aspect,.06f,50.f);u.projection[1][1]*=-1;
        u.invView=glm::inverse(u.view);u.invProjection=glm::inverse(u.projection);
        mat4 lp=glm::perspective(glm::radians(130.f),1.f,.1f,20.f);lp[1][1]*=-1;
        u.lightVP=lp*glm::lookAt(level.light,level.light+vec3(0,-1,0),vec3(0,0,-1));
        u.light=vec4(level.light,1);u.eye=vec4(eye,1);u.options={settings.shadows?1.f:0.f,settings.ao?1.f:0.f,float(settings.debugView),settings.exposure};u.viewport={float(extent.width),float(extent.height),1.f/extent.width,1.f/extent.height};memcpy(uniform.mapped,&u,sizeof(u));
        vkCheck(vkResetFences(device,1,&fence));vkCheck(vkResetCommandBuffer(command,0));VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};bi.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;vkCheck(vkBeginCommandBuffer(command,&bi));
        VkClearValue depthClear{};depthClear.depthStencil={1,0};VkClearValue colorClear{};
        startPass(shadowPass,shadowFrame,2048,2048,{depthClear});vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,shadowPipeline);scene(game);vkCmdEndRenderPass(command);
        startPass(geometryPass,geometryFrame,extent.width,extent.height,{colorClear,colorClear,depthClear});vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,geometryPipeline);scene(game);vkCmdEndRenderPass(command);
        startPass(lightingPass,lightingFrame,extent.width,extent.height,{colorClear});vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,lightingPipeline);vkCmdDraw(command,3,1,0,0);vkCmdEndRenderPass(command);
        startPass(outputPass,swapFrames[index],extent.width,extent.height,{colorClear});vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,postPipeline);vkCmdDraw(command,3,1,0,0);ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(),command);vkCmdEndRenderPass(command);
        if(!capture.empty()&&captureSupported)captureImage(index);
        vkCheck(vkEndCommandBuffer(command));
        VkPipelineStageFlags waitStage=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.waitSemaphoreCount=1;submit.pWaitSemaphores=&acquired;submit.pWaitDstStageMask=&waitStage;submit.commandBufferCount=1;submit.pCommandBuffers=&command;submit.signalSemaphoreCount=1;submit.pSignalSemaphores=&finished[index];vkCheck(vkQueueSubmit(queue,1,&submit,fence));
        if(!capture.empty()){vkCheck(vkWaitForFences(device,1,&fence,VK_TRUE,UINT64_MAX));saveCapture(capture);}
        VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};present.waitSemaphoreCount=1;present.pWaitSemaphores=&finished[index];present.swapchainCount=1;present.pSwapchains=&swapchain;present.pImageIndices=&index;
        VkResult result=vkQueuePresentKHR(queue,&present);if(result==VK_ERROR_OUT_OF_DATE_KHR||result==VK_SUBOPTIMAL_KHR||acquire==VK_SUBOPTIMAL_KHR)swapDirty=true;else vkCheck(result);
        return true;
    }
    ~Impl(){
        if(device)vkDeviceWaitIdle(device);
        if(uiReady){ImGui_ImplVulkan_Shutdown();ImGui_ImplGlfw_Shutdown();ImGui::DestroyContext();}
        if(device){
            clearSwapchain();destroy(vertices);destroy(uniform);
            if(shadowFrame)vkDestroyFramebuffer(device,shadowFrame,nullptr);
            destroy(shadow);
            if(geometryPipeline)vkDestroyPipeline(device,geometryPipeline,nullptr);if(shadowPipeline)vkDestroyPipeline(device,shadowPipeline,nullptr);
            if(lightingPipeline)vkDestroyPipeline(device,lightingPipeline,nullptr);
            if(geometryPass)vkDestroyRenderPass(device,geometryPass,nullptr);if(shadowPass)vkDestroyRenderPass(device,shadowPass,nullptr);if(lightingPass)vkDestroyRenderPass(device,lightingPass,nullptr);
            if(pipelineLayout)vkDestroyPipelineLayout(device,pipelineLayout,nullptr);if(setLayout)vkDestroyDescriptorSetLayout(device,setLayout,nullptr);
            if(descriptors)vkDestroyDescriptorPool(device,descriptors,nullptr);if(sampler)vkDestroySampler(device,sampler,nullptr);if(shadowSampler)vkDestroySampler(device,shadowSampler,nullptr);if(linearSampler)vkDestroySampler(device,linearSampler,nullptr);
            if(acquired)vkDestroySemaphore(device,acquired,nullptr);if(fence)vkDestroyFence(device,fence,nullptr);if(commands)vkDestroyCommandPool(device,commands,nullptr);
            vkDestroyDevice(device,nullptr);
        }
        if(surface)vkDestroySurfaceKHR(instance,surface,nullptr);
        if(messenger){auto destroy=reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance,"vkDestroyDebugUtilsMessengerEXT"));destroy(instance,messenger,nullptr);}
        if(instance)vkDestroyInstance(instance,nullptr);
#ifdef _WIN32
        if(loader)FreeLibrary(loader);
#else
        if(loader)dlclose(loader);
#endif
    }
};
Renderer::Renderer(GLFWwindow* w,const Level& l,bool validation):p(std::make_unique<Impl>(w,l)){p->init(validation);}
Renderer::~Renderer()=default;
void Renderer::beginUI(){
    int w,h;glfwGetFramebufferSize(p->window,&w,&h);
    if(w&&h&&(p->swapDirty||uint32_t(w)!=p->extent.width||uint32_t(h)!=p->extent.height))p->recreateSwapchain();
    ImGui_ImplVulkan_NewFrame();ImGui_ImplGlfw_NewFrame();ImGui::NewFrame();
}
bool Renderer::draw(const Game& game,const RenderSettings& settings,const std::string& capture){return p->draw(game,settings,capture);}
const std::string& Renderer::hardware()const{return p->gpu;}
uint32_t Renderer::validationErrors()const{return errors.load();}
bool Renderer::validationActive()const{return p->validationEnabled;}
}
