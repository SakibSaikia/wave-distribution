#include <demo.h>
#include <shadercompiler.h>

// Aliased types
using DXGIFactory_t = IDXGIFactory4;
using DXGIAdapter_t = IDXGIAdapter;
using DXGISwapChain_t = IDXGISwapChain3;
using D3DDebug_t = ID3D12Debug;
using D3DDevice_t = ID3D12Device5;
using D3DCommandQueue_t = ID3D12CommandQueue;
using D3DCommandAllocator_t = ID3D12CommandAllocator;
using D3DDescriptorHeap_t = ID3D12DescriptorHeap;
using D3DHeap_t = ID3D12Heap;
using D3DResource_t = ID3D12Resource;
using D3DCommandList_t = ID3D12GraphicsCommandList4;
using D3DFence_t = ID3D12Fence1;
using D3DPipelineState_t = ID3D12PipelineState;
using D3DRootSignature_t = ID3D12RootSignature;

inline void AssertIfFailed(HRESULT hr)
{
#if defined _DEBUG
	if (FAILED(hr))
	{
		std::string message = std::system_category().message(hr);
		OutputDebugStringA(message.c_str());
		_CrtDbgBreak();
	}
#endif
}

inline void DebugAssert(bool success, const char* msg = nullptr)
{
#if defined _DEBUG
	if (!success)
	{
		if (msg)
		{
			OutputDebugStringA("\n*****\n");
			OutputDebugStringA(msg);
			OutputDebugStringA("\n*****\n");
		}

		_CrtDbgBreak();
	}
#endif
}

namespace Renderer
{
	void Initialize(const HWND& windowHandle, const uint32_t resX, const uint32_t resY);
	void CreatePipelineStates();
	void RenderSingleFrame();
	void Flush();

	winrt::com_ptr<DXGIAdapter_t> EnumerateAdapters(DXGIFactory_t* dxgiFactory);
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE descriptorHeapType, uint32_t descriptorIndex);
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE descriptorHeapType, uint32_t descriptorIndex);

	uint32_t s_resX, s_resY;
	winrt::com_ptr<DXGIFactory_t> s_dxgiFactory;
	winrt::com_ptr<D3DDevice_t> s_d3dDevice;
	winrt::com_ptr<DXGISwapChain_t> s_swapChain;
	winrt::com_ptr<D3DResource_t> s_backBuffers[2];
	uint32_t s_currentBufferIndex;
	uint32_t s_descriptorSize[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
	winrt::com_ptr<D3DDescriptorHeap_t> s_descriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
	winrt::com_ptr<D3DCommandQueue_t> s_graphicsQueue;
	winrt::com_ptr<D3DCommandAllocator_t> s_commandAllocator;
	winrt::com_ptr<D3DCommandList_t> s_commandList;
	winrt::com_ptr<D3DRootSignature_t> s_rootsig;
	winrt::com_ptr<D3DPipelineState_t> s_pipelineState;

#if defined (_DEBUG)
	winrt::com_ptr<D3DDebug_t> s_debugController;
#endif
}

bool InitializeDemo(const HWND& windowHandle, const uint32_t resX, const uint32_t resY)
{
	ShaderCompiler::Initialize();
	Renderer::Initialize(windowHandle, resX, resY);
	return true;
}

void RenderDemo()
{
	Renderer::RenderSingleFrame();
}

void WINAPI DestroyDemo()
{
	Renderer::Flush();
}

void Renderer::Initialize(const HWND& windowHandle, const uint32_t resX, const uint32_t resY)
{
	s_resX = resX;
	s_resY = resY;

	UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
	// Debug Layer
	AssertIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(s_debugController.put())));
	s_debugController->EnableDebugLayer();
	dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

	// DXGI Factory
	AssertIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(s_dxgiFactory.put())));

	// Adapter
	winrt::com_ptr<DXGIAdapter_t> adapter = EnumerateAdapters(s_dxgiFactory.get());

	// Device
	AssertIfFailed(D3D12CreateDevice(
		adapter.get(),
		D3D_FEATURE_LEVEL_12_1,
		IID_PPV_ARGS(s_d3dDevice.put())));

	// Feature Support
	D3D12_FEATURE_DATA_D3D12_OPTIONS1 waveOpsInfo = {};
	AssertIfFailed(s_d3dDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS1, &waveOpsInfo, sizeof(waveOpsInfo)));
	DebugAssert(waveOpsInfo.WaveOps == TRUE, "Wave Intrinsics not supported");

	// Cache descriptor sizes
	for (int typeId = 0; typeId < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++typeId)
	{
		s_descriptorSize[typeId] = s_d3dDevice->GetDescriptorHandleIncrementSize(static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(typeId));
	}

	// Command Queue
	D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};
	cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	AssertIfFailed(s_d3dDevice->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(s_graphicsQueue.put())));
	s_graphicsQueue->SetName(L"graphics_queue");

	// Commandlist
	AssertIfFailed(s_d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(s_commandAllocator.put())));
	AssertIfFailed(s_d3dDevice->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		s_commandAllocator.get(),
		nullptr,
		IID_PPV_ARGS(s_commandList.put())));

	// RTV heap
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.NumDescriptors = 2;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	AssertIfFailed(
		s_d3dDevice->CreateDescriptorHeap(
			&rtvHeapDesc,
			IID_PPV_ARGS(s_descriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_RTV].put()))
	);
	s_descriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_RTV]->SetName(L"rtv_heap");

	// Swap chain
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.Width = resX;
	swapChainDesc.Height = resY;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.Scaling = DXGI_SCALING_NONE;
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = 2;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	winrt::com_ptr<IDXGISwapChain1> swapChain;
	AssertIfFailed(s_dxgiFactory->CreateSwapChainForHwnd(
		s_graphicsQueue.get(),
		windowHandle,
		&swapChainDesc,
		nullptr,
		nullptr,
		swapChain.put()));

	AssertIfFailed(swapChain->QueryInterface(IID_PPV_ARGS(s_swapChain.put())));

	// Back buffers
	for (size_t bufferIdx = 0; bufferIdx < 2; bufferIdx++)
	{
		AssertIfFailed(s_swapChain->GetBuffer(bufferIdx, IID_PPV_ARGS(s_backBuffers[bufferIdx].put())));

		std::wstringstream s;
		s << L"back_buffer_" << bufferIdx;
		s_backBuffers[bufferIdx]->SetName(s.str().c_str());

		D3D12_CPU_DESCRIPTOR_HANDLE rtvDescriptor = GetCPUDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, bufferIdx);
		s_d3dDevice->CreateRenderTargetView(s_backBuffers[bufferIdx].get(), nullptr, rtvDescriptor);
	}

	s_currentBufferIndex = s_swapChain->GetCurrentBackBufferIndex();

	// PSOs
	CreatePipelineStates();
}

winrt::com_ptr<DXGIAdapter_t> Renderer::EnumerateAdapters(DXGIFactory_t* dxgiFactory)
{
	winrt::com_ptr<DXGIAdapter_t> bestAdapter;
	size_t maxVram = 0;

	winrt::com_ptr<DXGIAdapter_t> adapter;
	uint32_t i = 0;

	while (dxgiFactory->EnumAdapters(i++, adapter.put()) != DXGI_ERROR_NOT_FOUND)
	{
		DXGI_ADAPTER_DESC desc;
		adapter->GetDesc(&desc);

		if (desc.DedicatedVideoMemory > maxVram)
		{
			maxVram = desc.DedicatedVideoMemory;

			bestAdapter = nullptr;
			bestAdapter = adapter;
		}

		adapter = nullptr;
	}

	DXGI_ADAPTER_DESC desc;
	bestAdapter->GetDesc(&desc);

	std::wstringstream out;
	out << L"*** Adapter : " << desc.Description << std::endl;
	OutputDebugString(out.str().c_str());

	return bestAdapter;
}

D3D12_CPU_DESCRIPTOR_HANDLE Renderer::GetCPUDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t descriptorIndex)
{
	D3D12_CPU_DESCRIPTOR_HANDLE descriptor;
	descriptor.ptr = s_descriptorHeaps[type]->GetCPUDescriptorHandleForHeapStart().ptr +
		descriptorIndex * s_descriptorSize[type];

	return descriptor;
}

D3D12_GPU_DESCRIPTOR_HANDLE Renderer::GetGPUDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t descriptorIndex)
{
	D3D12_GPU_DESCRIPTOR_HANDLE descriptor;
	descriptor.ptr = s_descriptorHeaps[type]->GetGPUDescriptorHandleForHeapStart().ptr +
		descriptorIndex * s_descriptorSize[type];

	return descriptor;
}

void Renderer::CreatePipelineStates()
{
	// Root signature
	winrt::com_ptr<IDxcBlob> rootsigBlob;
	AssertIfFailed(ShaderCompiler::CompileRootsignature(L"wave-viz.hlsl", L"rootsig", L"rootsig_1_1", rootsigBlob.put()));
	s_d3dDevice->CreateRootSignature(0, rootsigBlob->GetBufferPointer(), rootsigBlob->GetBufferSize(), IID_PPV_ARGS(s_rootsig.put()));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.NodeMask = 1;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.pRootSignature = s_rootsig.get();
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	// PSO - Rasterizer State
	{
		D3D12_RASTERIZER_DESC& desc = psoDesc.RasterizerState;
		desc.FillMode = D3D12_FILL_MODE_SOLID;
		desc.CullMode = D3D12_CULL_MODE_BACK;
		desc.FrontCounterClockwise = FALSE;
		desc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
		desc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
		desc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
		desc.DepthClipEnable = TRUE;
		desc.MultisampleEnable = FALSE;
		desc.AntialiasedLineEnable = FALSE;
		desc.ForcedSampleCount = 0;
		desc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
	}

	// PSO - Blend State
	{
		D3D12_BLEND_DESC& desc = psoDesc.BlendState;
		desc.AlphaToCoverageEnable = FALSE;
		desc.IndependentBlendEnable = FALSE;
		desc.RenderTarget[0].BlendEnable = FALSE;
		desc.RenderTarget[0].LogicOpEnable = FALSE;
		desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	}

	// PSO - Depth Stencil State
	{
		D3D12_DEPTH_STENCIL_DESC& desc = psoDesc.DepthStencilState;
		desc.DepthEnable = FALSE;
		desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		desc.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		desc.StencilEnable = FALSE;
	}

	winrt::com_ptr<IDxcBlob> vsBlob[3], psBlob[3];

	AssertIfFailed(ShaderCompiler::CompileShader(L"wave-viz.hlsl", L"vs_main", L"", L"vs_6_4", vsBlob[0].put()));
	AssertIfFailed(ShaderCompiler::CompileShader(L"wave-viz.hlsl", L"ps_main", L"", L"ps_6_4", psBlob[0].put()));
	psoDesc.VS.pShaderBytecode = vsBlob[0]->GetBufferPointer();
	psoDesc.VS.BytecodeLength = vsBlob[0]->GetBufferSize();
	psoDesc.PS.pShaderBytecode = psBlob[0]->GetBufferPointer();
	psoDesc.PS.BytecodeLength = psBlob[0]->GetBufferSize();
	AssertIfFailed(s_d3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(s_pipelineState.put())));
}

void Renderer::Flush()
{
	winrt::com_ptr<D3DFence_t> flushFence;
	AssertIfFailed(s_d3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(flushFence.put())));

	HANDLE flushEvent;
	flushEvent = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);

	s_graphicsQueue->Signal(flushFence.get(), 0xFF);
	flushFence->SetEventOnCompletion(0xFF, flushEvent);
	WaitForSingleObject(flushEvent, INFINITE);
}

void Renderer::RenderSingleFrame()
{
	D3D12_RESOURCE_BARRIER barrierDesc = {};
	barrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrierDesc.Transition.pResource = s_backBuffers[s_currentBufferIndex].get();
	barrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	barrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	s_commandList->ResourceBarrier(1, &barrierDesc);

	// Draw Commands
	{
		s_commandList->SetGraphicsRootSignature(s_rootsig.get());

		D3DPipelineState_t* pso = s_pipelineState.get();
		s_commandList->SetPipelineState(pso);

		D3D12_VIEWPORT viewport{ 0.f, 0.f, (float)s_resX, (float)s_resY, 0.f, 1.f };
		D3D12_RECT screenRect{ 0, 0, (LONG)s_resX, (LONG)s_resY };
		s_commandList->RSSetViewports(1, &viewport);
		s_commandList->RSSetScissorRects(1, &screenRect);

		D3D12_CPU_DESCRIPTOR_HANDLE rtv = Renderer::GetCPUDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, s_currentBufferIndex);
		s_commandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

		float clearColor[] = { .9f, .9f, 0.9f, 0.f };
		s_commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);

		s_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		s_commandList->DrawInstanced(6, 1, 0, 0);
	}

	barrierDesc = {};
	barrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrierDesc.Transition.pResource = s_backBuffers[s_currentBufferIndex].get();
	barrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	barrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	s_commandList->ResourceBarrier(1, &barrierDesc);

	// Execute and wait
	s_commandList->Close();
	ID3D12CommandList* cmdLists[] = { s_commandList.get() };
	s_graphicsQueue->ExecuteCommandLists(1, cmdLists);
	Renderer::Flush();
	s_commandAllocator->Reset();
	s_commandList->Reset(s_commandAllocator.get(), nullptr);

	// Present and flip
	s_swapChain->Present(1, 0);
	s_currentBufferIndex = 1 - s_currentBufferIndex;
}