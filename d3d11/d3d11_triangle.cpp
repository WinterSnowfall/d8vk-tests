#include <array>
#include <iostream>
#include <vector>

#include <d3dcompiler.h>
#include <d3d11_1.h>
#include <dxgi1_6.h>

#include <windows.h>
#include <windowsx.h>

#include <cstring>
#include <string>
#include <sstream>

#include "../common/com.h"
#include "../common/str.h"

struct Vertex {
  float x, y;
};

struct VsConstants {
  float x, y;
  float w, h;
};

struct PsConstants {
  float r, g, b, a;
  uint32_t is_hdr;
  uint32_t dummy[3];
};

const std::string g_vertexShaderCode =
  "cbuffer vs_cb : register(b0) {\n"
  "  float2 v_offset;\n"
  "  float2 v_scale;\n"
  "};\n"
  "float4 main(float4 v_pos : IN_POSITION) : SV_POSITION {\n"
  "  return float4(v_offset + v_pos * v_scale, 0.0f, 1.0f);\n"
  "}\n";

const std::string g_pixelShaderCode =
  "Texture2D<float4> tex0 : register(t0);"
  "cbuffer ps_cb : register(b0) {\n"
  "  float4 color;\n"
  "  bool is_hdr;\n"
  "};\n"
  "float3 encode_hdr(float3 nits) {\n"
  "const float c1 = 0.8359375f;\n"
  "const float c2 = 18.8515625f;\n"
  "const float c3 = 18.6875f;\n"
  "const float m1 = 0.1593017578125f;\n"
  "const float m2 = 78.84375f;\n"
  "float3 y = saturate(nits / 10000.0f);\n"
  "float3 y_m1 = pow(y, m1.xxx);\n"
  "float3 num = c1 + c2 * y_m1;\n"
  "float3 den = 1.0f + c3 * y_m1;\n"
  "return pow(num / den, m2.xxx);\n"
  "}\n"
  "float3 encode_sdr(float3 nits) {\n"
  "  float3 cvt = nits / 80.0f;\n"
  "  float3 tonemapped = cvt / (cvt + 1.0f);\n"
  "  return pow(tonemapped, (1.0f / 2.2f).xxx);\n"
  "}\n"
  "float4 main() : SV_TARGET {\n"
  "  float4 result = color;\n"
  "  if (is_hdr)\n"
  "    result.xyz = encode_hdr(color.xyz);\n"
  "  else\n"
  "    result.xyz = encode_sdr(color.xyz);\n"
  "  return result;\n"
  "}\n";

class TriangleApp {
  
public:
  
  TriangleApp(HINSTANCE instance, HWND window)
  : m_window(window) {
    Com<ID3D11Device> device;

    D3D_FEATURE_LEVEL fl = D3D_FEATURE_LEVEL_11_1;

    HRESULT status = D3D11CreateDevice(
      nullptr, D3D_DRIVER_TYPE_HARDWARE,
      nullptr, 0, &fl, 1, D3D11_SDK_VERSION,
      &device, nullptr, nullptr);

    if (FAILED(status)) {
      std::cerr << "Failed to create D3D11 device" << std::endl;
      return;
    }
    
    if (FAILED(device->QueryInterface(IID_PPV_ARGS(&m_device)))) {
      std::cerr << "Failed to query ID3D11DeviceContext1" << std::endl;
      return;
    }

    Com<IDXGIDevice> dxgiDevice;

    if (FAILED(m_device->QueryInterface(IID_PPV_ARGS(&dxgiDevice)))) {
      std::cerr << "Failed to query DXGI device" << std::endl;
      return;
    }

    if (FAILED(dxgiDevice->GetAdapter(&m_adapter))) {
      std::cerr << "Failed to query DXGI adapter" << std::endl;
      return;
    }

    if (FAILED(m_adapter->GetParent(IID_PPV_ARGS(&m_factory)))) {
      std::cerr << "Failed to query DXGI factory" << std::endl;
      return;
    }

    m_device->GetImmediateContext1(&m_context);

    DXGI_SWAP_CHAIN_DESC1 swapDesc;
    swapDesc.Width          = m_windowSizeW;
    swapDesc.Height         = m_windowSizeH;
    swapDesc.Format         = DXGI_FORMAT_R10G10B10A2_UNORM;
    swapDesc.Stereo         = FALSE;
    swapDesc.SampleDesc     = { 1, 0 };
    swapDesc.BufferUsage    = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapDesc.BufferCount    = 3;
    swapDesc.Scaling        = DXGI_SCALING_NONE;
    swapDesc.SwapEffect     = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapDesc.AlphaMode      = DXGI_ALPHA_MODE_UNSPECIFIED;
    swapDesc.Flags          = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT
                            | DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

    DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsDesc;
    fsDesc.RefreshRate      = { 0, 0 };
    fsDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    fsDesc.Scaling          = DXGI_MODE_SCALING_UNSPECIFIED;
    fsDesc.Windowed         = TRUE;
    
    Com<IDXGISwapChain1> swapChain;
    Com<IDXGISwapChain4> swapChain4;
    if (FAILED(m_factory->CreateSwapChainForHwnd(m_device.ptr(), m_window, &swapDesc, &fsDesc, nullptr, &swapChain))) {
      std::cerr << "Failed to create DXGI swap chain" << std::endl;
      return;
    }

    if (FAILED(swapChain->QueryInterface(IID_PPV_ARGS(&m_swapChain)))) {
      std::cerr << "Failed to query DXGI swap chain interface" << std::endl;
      return;
    }

    m_latencyEvent = m_swapChain->GetFrameLatencyWaitableObject();

    if (!m_latencyEvent) {
      std::cerr << "Failed to query DXGI frame latency event" << std::endl;
      return;
    }

    UINT supportFlags = 0;

    if (SUCCEEDED(m_swapChain->CheckColorSpaceSupport(DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020, &supportFlags))
     && (supportFlags & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT))
      m_isHdr = SUCCEEDED(m_swapChain->SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020));

    m_factory->MakeWindowAssociation(m_window, 0);

    Com<ID3DBlob> vertexShaderBlob;
    Com<ID3DBlob> pixelShaderBlob;
    
    if (FAILED(D3DCompile(g_vertexShaderCode.data(), g_vertexShaderCode.size(),
        "Vertex shader", nullptr, nullptr, "main", "vs_5_0", 0, 0, &vertexShaderBlob, nullptr))) {
      std::cerr << "Failed to compile vertex shader" << std::endl;
      return;
    }
    
    if (FAILED(D3DCompile(g_pixelShaderCode.data(), g_pixelShaderCode.size(),
        "Pixel shader", nullptr, nullptr, "main", "ps_5_0", 0, 0, &pixelShaderBlob, nullptr))) {
      std::cerr << "Failed to compile pixel shader" << std::endl;
      return;
    }
    
    if (FAILED(m_device->CreateVertexShader(
        vertexShaderBlob->GetBufferPointer(),
        vertexShaderBlob->GetBufferSize(),
        nullptr, &m_vs))) {
      std::cerr << "Failed to create vertex shader" << std::endl;
      return;
    }
    
    if (FAILED(m_device->CreatePixelShader(
        pixelShaderBlob->GetBufferPointer(),
        pixelShaderBlob->GetBufferSize(),
        nullptr, &m_ps))) {
      std::cerr << "Failed to create pixel shader" << std::endl;
      return;
    }
    
    std::array<D3D11_INPUT_ELEMENT_DESC, 1> vertexFormatDesc = {{
      { "IN_POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    }};
    
    if (FAILED(m_device->CreateInputLayout(
        vertexFormatDesc.data(),
        vertexFormatDesc.size(),
        vertexShaderBlob->GetBufferPointer(),
        vertexShaderBlob->GetBufferSize(),
        &m_vertexFormat))) {
      std::cerr << "Failed to create input layout" << std::endl;
      return;
    }

    std::array<Vertex, 6> vertexData = {{
      Vertex { -0.3f, 0.1f },
      Vertex {  0.5f, 0.9f },
      Vertex {  1.3f, 0.1f },
      Vertex { -0.3f, 0.9f },
      Vertex {  1.3f, 0.9f },
      Vertex {  0.5f, 0.1f },
    }};

    D3D11_BUFFER_DESC vboDesc;
    vboDesc.ByteWidth           = sizeof(vertexData);
    vboDesc.Usage               = D3D11_USAGE_IMMUTABLE;
    vboDesc.BindFlags           = D3D11_BIND_VERTEX_BUFFER;
    vboDesc.CPUAccessFlags      = 0;
    vboDesc.MiscFlags           = 0;
    vboDesc.StructureByteStride = 0;

    D3D11_SUBRESOURCE_DATA vboData;
    vboData.pSysMem             = vertexData.data();
    vboData.SysMemPitch         = vboDesc.ByteWidth;
    vboData.SysMemSlicePitch    = vboDesc.ByteWidth;

    if (FAILED(m_device->CreateBuffer(&vboDesc, &vboData, &m_vbo))) {
      std::cerr << "Failed to create index buffer" << std::endl;
      return;
    }

    std::array<uint32_t, 6> indexData = {{ 0, 1, 2, 3, 4, 5 }};

    D3D11_BUFFER_DESC iboDesc;
    iboDesc.ByteWidth           = sizeof(indexData);
    iboDesc.Usage               = D3D11_USAGE_IMMUTABLE;
    iboDesc.BindFlags           = D3D11_BIND_INDEX_BUFFER;
    iboDesc.CPUAccessFlags      = 0;
    iboDesc.MiscFlags           = 0;
    iboDesc.StructureByteStride = 0;

    D3D11_SUBRESOURCE_DATA iboData;
    iboData.pSysMem             = indexData.data();
    iboData.SysMemPitch         = iboDesc.ByteWidth;
    iboData.SysMemSlicePitch    = iboDesc.ByteWidth;

    if (FAILED(m_device->CreateBuffer(&iboDesc, &iboData, &m_ibo))) {
      std::cerr << "Failed to create index buffer" << std::endl;
      return;
    }

    D3D11_BUFFER_DESC cbDesc;
    cbDesc.ByteWidth            = sizeof(PsConstants);
    cbDesc.Usage                = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags            = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags       = D3D11_CPU_ACCESS_WRITE;
    cbDesc.MiscFlags            = 0;
    cbDesc.StructureByteStride  = 0;

    if (FAILED(m_device->CreateBuffer(&cbDesc, nullptr, &m_cbPs))) {
      std::cerr << "Failed to create constant buffer" << std::endl;
      return;
    }

    cbDesc.ByteWidth            = sizeof(VsConstants);

    if (FAILED(m_device->CreateBuffer(&cbDesc, nullptr, &m_cbVs))) {
      std::cerr << "Failed to create constant buffer" << std::endl;
      return;
    }

    m_initialized = true;
  }
  
  
  ~TriangleApp() {
    m_context->ClearState();
  }
  
  
  bool run() {
    if (!m_initialized)
      return false;

    if (m_occluded) {
      HRESULT hr = m_swapChain->Present(0, DXGI_PRESENT_TEST);
      m_occluded = hr == DXGI_STATUS_OCCLUDED;

      if (m_occluded)
        return true;
    }

    if (!beginFrame())
      return true;

    setBrightness(400.0f);
    drawTriangle(0.0f, 0.0f, 0);

    setBrightness(200.0f);
    drawTriangle(1.0f, 0.0f, 3);
    drawTriangle(-1.0f, 0.0f, 3);
    drawTriangle(0.0f, -1.0f, 3);

    setBrightness(100.0f);
    drawTriangle(-2.0f, 1.0f, 3);
    drawTriangle(-1.0f, 1.0f, 0);
    drawTriangle(0.0f, 1.0f, 3);
    drawTriangle(1.0f, 1.0f, 0);
    drawTriangle(2.0f, 1.0f, 3);

    setBrightness(80.0f);
    drawTriangle(-4.0f, 1.0f, 3);
    drawTriangle(-3.0f, 1.0f, 0);
    drawTriangle(-3.0f, 0.0f, 3);
    drawTriangle(-2.0f, 0.0f, 0);
    drawTriangle(-2.0f, -1.0f, 3);
    drawTriangle(-1.0f, -1.0f, 0);
    drawTriangle(-1.0f, -2.0f, 3);
    drawTriangle(0.0f, -2.0f, 0);
    drawTriangle(0.0f, -3.0f, 3);
    drawTriangle(1.0f, -2.0f, 3);
    drawTriangle(4.0f, 1.0f, 3);
    drawTriangle(3.0f, 1.0f, 0);
    drawTriangle(3.0f, 0.0f, 3);
    drawTriangle(2.0f, 0.0f, 0);
    drawTriangle(2.0f, -1.0f, 3);
    drawTriangle(1.0f, -1.0f, 0);

    setBrightness(60.0f);
    drawTriangle(-5.0f, 2.0f, 3);
    drawTriangle(-4.0f, 2.0f, 0);
    drawTriangle(-3.0f, 2.0f, 3);
    drawTriangle(-2.0f, 2.0f, 0);
    drawTriangle(-1.0f, 2.0f, 3);
    drawTriangle(0.0f, 2.0f, 0);
    drawTriangle(1.0f, 2.0f, 3);
    drawTriangle(2.0f, 2.0f, 0);
    drawTriangle(3.0f, 2.0f, 3);
    drawTriangle(4.0f, 2.0f, 0);
    drawTriangle(5.0f, 2.0f, 3);

    if (!endFrame())
      return false;

    updateFps();
    return true;
  }


  void setBrightness(float nits) {
    PsConstants constants;
    constants.r = nits;
    constants.g = nits;
    constants.b = nits;
    constants.a = 1.0f;
    constants.is_hdr = m_isHdr;

    D3D11_MAPPED_SUBRESOURCE sr = { };
    m_context->Map(m_cbPs.ptr(), 0, D3D11_MAP_WRITE_DISCARD, 0, &sr);
    memcpy(sr.pData, &constants, sizeof(constants));
    m_context->Unmap(m_cbPs.ptr(), 0);
  }


  void drawTriangle(float x, float y, uint32_t index) {
    VsConstants constants;
    constants.x = float(x) / 16.0f;
    constants.y = float(y) / 9.0f;
    constants.w = 1.0f / 16.0f;
    constants.h = 1.0f / 9.0f;

    D3D11_MAPPED_SUBRESOURCE sr = { };
    m_context->Map(m_cbVs.ptr(), 0, D3D11_MAP_WRITE_DISCARD, 0, &sr);
    memcpy(sr.pData, &constants, sizeof(constants));
    m_context->Unmap(m_cbVs.ptr(), 0);

    m_context->DrawIndexedInstanced(3, 1, index, 0, 0);
  }


  bool beginFrame() {
    WaitForSingleObject(m_latencyEvent, INFINITE);

    // Make sure we can actually render to the window
    RECT windowRect = { 0, 0, 1024, 600 };
    GetClientRect(m_window, &windowRect);
    
    uint32_t newWindowSizeW = uint32_t(windowRect.right - windowRect.left);
    uint32_t newWindowSizeH = uint32_t(windowRect.bottom - windowRect.top);

    if (m_windowSizeW != newWindowSizeW || m_windowSizeH != newWindowSizeH) {
      m_context->ClearState();

      DXGI_SWAP_CHAIN_DESC1 desc;
      m_swapChain->GetDesc1(&desc);

      if (FAILED(m_swapChain->ResizeBuffers(desc.BufferCount,
          newWindowSizeW, newWindowSizeH, desc.Format, desc.Flags))) {
        std::cerr << "Failed to resize back buffers" << std::endl;
        return false;
      }

      m_windowSizeW = newWindowSizeW;
      m_windowSizeH = newWindowSizeH;
    }
      
    Com<ID3D11Texture2D> backBuffer;
    Com<ID3D11RenderTargetView> rtv;

    if (FAILED(m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer)))) {
      std::cerr << "Failed to get swap chain back buffer" << std::endl;
      return false;
    }
    
    D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
    rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Format        = DXGI_FORMAT_R10G10B10A2_UNORM;
    rtvDesc.Texture2D     = { 0u };

    if (FAILED(m_device->CreateRenderTargetView(backBuffer.ptr(), &rtvDesc, &rtv))) {
      std::cerr << "Failed to create render target view" << std::endl;
      return false;
    }

    // Set up render state
    FLOAT color_sdr[4] = { 0.61f, 0.61f, 0.61f, 1.0f };
    FLOAT color_hdr[4] = { 0.42f, 0.42f, 0.42f, 1.0f };
    m_context->OMSetRenderTargets(1, &rtv, nullptr);
    m_context->ClearRenderTargetView(rtv.ptr(), m_isHdr ? color_hdr : color_sdr);

    m_context->VSSetShader(m_vs.ptr(), nullptr, 0);
    m_context->PSSetShader(m_ps.ptr(), nullptr, 0);

    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->IASetInputLayout(m_vertexFormat.ptr());

    D3D11_VIEWPORT viewport;
    viewport.TopLeftX     = 0.0f;
    viewport.TopLeftY     = 0.0f;
    viewport.Width        = float(m_windowSizeW);
    viewport.Height       = float(m_windowSizeH);
    viewport.MinDepth     = 0.0f;
    viewport.MaxDepth     = 1.0f;
    m_context->RSSetViewports(1, &viewport);

    uint32_t vsStride = sizeof(Vertex);
    uint32_t vsOffset = 0;
    m_context->IASetVertexBuffers(0, 1, &m_vbo, &vsStride, &vsOffset);
    m_context->IASetIndexBuffer(m_ibo.ptr(), DXGI_FORMAT_R32_UINT, 0);

    m_context->VSSetConstantBuffers(0, 1, &m_cbVs);
    m_context->PSSetConstantBuffers(0, 1, &m_cbPs);
    return true;
  }


  bool endFrame() {
    HRESULT hr = m_swapChain->Present(1, 0);
    m_occluded = hr == DXGI_STATUS_OCCLUDED;
    return true;
  }

  void updateFps() {
    if (!m_qpcFrequency.QuadPart)
      QueryPerformanceFrequency(&m_qpcFrequency);

    if (!m_qpcLastUpdate.QuadPart)
      QueryPerformanceCounter(&m_qpcLastUpdate);

    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);

    m_frameCount++;

    if (now.QuadPart - m_qpcLastUpdate.QuadPart < m_qpcFrequency.QuadPart)
      return;

    double seconds = double(now.QuadPart - m_qpcLastUpdate.QuadPart) / double(m_qpcFrequency.QuadPart);
    double fps = double(m_frameCount) / seconds;

    std::wstringstream str;
    str << L"D3D11 triangle (" << fps << L" FPS) (" << (m_isHdr ? "HDR" : "SDR") << ")";

    SetWindowTextW(m_window, str.str().c_str());

    m_qpcLastUpdate = now;
    m_frameCount = 0;
  }

private:
  
  HWND                          m_window;
  uint32_t                      m_windowSizeW = 1024;
  uint32_t                      m_windowSizeH = 600;
  bool                          m_initialized = false;
  bool                          m_occluded = false;
  bool                          m_isHdr = false;
  
  Com<IDXGIFactory3>            m_factory;
  Com<IDXGIAdapter>             m_adapter;
  Com<ID3D11Device1>            m_device;
  Com<ID3D11DeviceContext1>     m_context;
  Com<IDXGISwapChain4>          m_swapChain;

  Com<ID3D11Buffer>             m_ibo;
  Com<ID3D11Buffer>             m_vbo;
  Com<ID3D11InputLayout>        m_vertexFormat;

  Com<ID3D11Buffer>             m_cbPs;
  Com<ID3D11Buffer>             m_cbVs;

  Com<ID3D11VertexShader>       m_vs;
  Com<ID3D11PixelShader>        m_ps;

  LARGE_INTEGER                 m_qpcLastUpdate = { };
  LARGE_INTEGER                 m_qpcFrequency  = { };

  HANDLE                        m_latencyEvent = nullptr;

  uint32_t                      m_frameCount = 0;
  
};

LRESULT CALLBACK WindowProc(HWND hWnd,
                            UINT message,
                            WPARAM wParam,
                            LPARAM lParam);

int main(int argc, char** argv) {
  HINSTANCE hInstance = GetModuleHandle(nullptr);
  int nCmdShow = SW_SHOWDEFAULT;
  WNDCLASSEXW wc = { };
  wc.cbSize = sizeof(wc);
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = WindowProc;
  wc.hInstance = hInstance;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = HBRUSH(COLOR_WINDOW);
  wc.lpszClassName = L"WindowClass";
  RegisterClassExW(&wc);

  HWND hWnd = CreateWindowExW(0, L"WindowClass", L"D3D11 triangle",
    WS_OVERLAPPEDWINDOW, 300, 300, 1024, 600,
    nullptr, nullptr, hInstance, nullptr);
  ShowWindow(hWnd, nCmdShow);

  TriangleApp app(hInstance, hWnd);

  MSG msg;

  while (true) {
    if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
      
      if (msg.message == WM_QUIT)
        return msg.wParam;
    } else {
      if (!app.run())
        break;
    }
  }

  return msg.wParam;
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
  switch (message) {
    case WM_CLOSE:
      PostQuitMessage(0);
      return 0;
  }

  return DefWindowProcW(hWnd, message, wParam, lParam);
}
