#include <GLFW/glfw3.h>
#include <webgpu/webgpu_cpp.h>
#include <cstdio>
// #define __EMSCRIPTEN__
#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#else
#include <webgpu/webgpu_glfw.h>
#endif

#include <iostream>
#include <vector>
#include <algorithm>
#include <chrono>

// Function to perform Quick Sort
void quickSort(std::vector<int>& arr, int low, int high)
{
	if (low < high)
	{
		int pivot = arr[high];
		int i = low - 1;

		for (int j = low; j <= high - 1; ++j)
		{
			if (arr[j] < pivot)
			{
				++i;
				std::swap(arr[i], arr[j]);
			}
		}

		std::swap(arr[i + 1], arr[high]);

		int pivotIndex = i + 1;

		quickSort(arr, low, pivotIndex - 1);
		quickSort(arr, pivotIndex + 1, high);
	}
}

void sort_bench()
{
	uint32_t n = 1000000;
	// Reset the vector
	std::vector<int>data(n);
	std::generate(data.begin(), data.end(), [n]() { return rand() % n; });

	// Benchmark Quick Sort
	auto quick_start = std::chrono::high_resolution_clock::now();
	quickSort(data, 0, data.size() - 1);
	auto quick_end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> quick_duration = quick_end - quick_start;

	// Reset the vector
	data = std::vector<int>(n);
	std::generate(data.begin(), data.end(), [n]() { return rand() % n; });

	// Benchmark Standard Sort
	auto std_sort_start = std::chrono::high_resolution_clock::now();
	std::sort(data.begin(), data.end());
	auto std_sort_end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> std_sort_duration = std_sort_end - std_sort_start;

	// Output benchmark results
	std::cout << "Quick Sort Time: " << quick_duration.count() << " seconds\n";
	std::cout << "Standard Sort Time: " << std_sort_duration.count() << " seconds\n";
}


wgpu::Instance instance; // the instance of wgpu
wgpu::Device device; //	the gpu device
wgpu::RenderPipeline pipeline;

wgpu::SwapChain swapChain;
const uint32_t kWidth = 512;
const uint32_t kHeight = 512;

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	switch (key)
	{
	case GLFW_KEY_ESCAPE:
		if (action == GLFW_PRESS)
			glfwSetWindowShouldClose(window, true);break;
	default:
		break;
	}
}

void shader_compilation_callback(
	WGPUCompilationInfoRequestStatus status,
	WGPUCompilationInfo const* info,
	void* userdata)
{
	printf("compilation info request: ");
	switch (status)
	{
	case WGPUCompilationInfoRequestStatus_Success: printf("success!");break;
	case WGPUCompilationInfoRequestStatus_Error: printf("error!");break;
	case WGPUCompilationInfoRequestStatus_DeviceLost: printf("device lost!");break;
	case WGPUCompilationInfoRequestStatus_Unknown: printf("unknown!");break;
	case WGPUCompilationInfoRequestStatus_Force32: printf("force32!");break;
	}
	printf("\n");
	printf("info message count: %llu\n", info->messageCount);
	if (info->messageCount && info->messages && info->messages->message)
	{
		printf("[line %llu pos %llu] ",
			info->messages->lineNum,
			info->messages->linePos);
		switch (info->messages->type)
		{
		case WGPUCompilationMessageType_Error: printf("error");break;
		case WGPUCompilationMessageType_Warning: printf("warning");break;
		case WGPUCompilationMessageType_Info: printf("info");break;
		case WGPUCompilationMessageType_Force32: printf("force_32");break;
		}
		printf(": %s\n",
			info->messages->message);
	}
}

void SetupSwapChain(wgpu::Surface surface)
{
	wgpu::SwapChainDescriptor scDesc
	{
		.usage = wgpu::TextureUsage::RenderAttachment,
		.format = wgpu::TextureFormat::BGRA8Unorm,
		.width = kWidth,
		.height = kHeight,
		.presentMode = wgpu::PresentMode::Fifo
	};
	swapChain = device.CreateSwapChain(surface, &scDesc);
}

void GetDevice(void (*callback)(wgpu::Device))
{
	instance.RequestAdapter(
		nullptr,
		// TODO(https://bugs.chromium.org/p/dawn/issues/detail?id=1892): Use
		// wgpu::RequestAdapterStatus, wgpu::Adapter, and wgpu::Device.
		[](WGPURequestAdapterStatus status, WGPUAdapter cAdapter, const char* message, void* userdata)
		{
			if (status != WGPURequestAdapterStatus_Success)
				exit(0);
			wgpu::Adapter adapter = wgpu::Adapter::Acquire(cAdapter);
			adapter.RequestDevice(
				nullptr,
				[](WGPURequestDeviceStatus status, WGPUDevice cDevice, const char* message, void* userdata)
				{
					wgpu::Device device = wgpu::Device::Acquire(cDevice);
					reinterpret_cast<void (*)(wgpu::Device)>(userdata)(device);
				},
				userdata);
		},
		reinterpret_cast<void*>(callback));
}

const char shaderCode[] = R"(
    @vertex fn vertexMain(@builtin(vertex_index) i : u32) ->
      @builtin(position) vec4f
	{
        const pos = array(vec2f(0, 0.5f), vec2f(-1, -1), vec2f(1, -1));
        return vec4f(pos[i], 0, 1);
    }
    @fragment fn fragmentMain() -> @location(0) vec4f
	{
        return vec4f(1, 0, 0, 1);
    }
)";

void CreateRenderPipeline()
{
	wgpu::ShaderModuleWGSLDescriptor wgslDesc{};
	wgslDesc.code = shaderCode;

	wgpu::ShaderModuleDescriptor shaderModuleDescriptor{ .nextInChain = &wgslDesc };
	wgpu::ShaderModule shaderModule = device.CreateShaderModule(&shaderModuleDescriptor);

#if !defined(__EMSCRIPTEN__)
	shaderModule.GetCompilationInfo(shader_compilation_callback, nullptr);
#endif
	wgpu::ColorTargetState colorTargetState{ .format = wgpu::TextureFormat::BGRA8Unorm };
	wgpu::FragmentState fragmentState{ .module = shaderModule,
									  .entryPoint = "fragmentMain",
									  .targetCount = 1,
									  .targets = &colorTargetState };

	wgpu::RenderPipelineDescriptor descriptor
	{
		.vertex = {.module = shaderModule, .entryPoint = "vertexMain"},
		.fragment = &fragmentState
	};
	pipeline = device.CreateRenderPipeline(&descriptor);
}

void Render()
{
	wgpu::RenderPassColorAttachment attachment
	{
		.view = swapChain.GetCurrentTextureView(),
		.loadOp = wgpu::LoadOp::Clear,
		.storeOp = wgpu::StoreOp::Store
	};

	wgpu::RenderPassDescriptor renderpass{ .colorAttachmentCount = 1,
										  .colorAttachments = &attachment };

	wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
	wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderpass);
	pass.SetPipeline(pipeline);
	pass.Draw(3);
	pass.End();
	wgpu::CommandBuffer commands = encoder.Finish();
	device.GetQueue().Submit(1, &commands);
}

void Start()
{
	if (!glfwInit())
	{
		return;
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	GLFWwindow* window = glfwCreateWindow(kWidth, kHeight, "WebGPU window", nullptr, nullptr);

#if defined(__EMSCRIPTEN__)
	wgpu::SurfaceDescriptorFromCanvasHTMLSelector canvasDesc{};
	canvasDesc.selector = "#canvas";

	wgpu::SurfaceDescriptor surfaceDesc{ .nextInChain = &canvasDesc };
	wgpu::Surface surface = instance.CreateSurface(&surfaceDesc);
#else
	wgpu::Surface surface = wgpu::glfw::CreateSurfaceForWindow(instance, window);
#endif

	SetupSwapChain(surface);
	CreateRenderPipeline();

#if defined(__EMSCRIPTEN__)
	emscripten_set_main_loop(Render, 0, false);
#else
	glfwSetKeyCallback(window, key_callback);
	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
		Render();
		swapChain.Present();
	}
#endif
}

int main()
{
	printf("Hello\n");
	sort_bench();

	instance = wgpu::CreateInstance();
	GetDevice([](wgpu::Device dev)
		{
			device = dev;
			Start();
		});
}
