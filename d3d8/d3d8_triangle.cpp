#include <array>
#include <iostream>

#include <d3d8.h>

#include "../common/com.h"
#include "../common/error.h"
#include "../common/str.h"

struct RGBVERTEX {
    FLOAT x, y, z, rhw;
    DWORD colour;
};

#define RGBT_FVF_CODES (D3DFVF_XYZRHW | D3DFVF_DIFFUSE)

class RGBTriangle {
    
    public:

        static const UINT WINDOW_WIDTH = 800;
        static const UINT WINDOW_HEIGHT = 800;

        RGBTriangle(HWND hWnd) {
            // D3D Interface
            m_d3d = Direct3DCreate8(D3D_SDK_VERSION);
            if(m_d3d.ptr() == nullptr)
                throw Error("Failed to create D3D8 interface");

            // D3D Device
            D3DDISPLAYMODE dm;
            HRESULT status = m_d3d->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &dm);
            if (FAILED(status))
                throw Error("Failed to get D3D8 adapter display mode");

            D3DPRESENT_PARAMETERS pp;
            ZeroMemory(&pp, sizeof(pp));

            pp.Windowed = TRUE;
            // alternatively, set to D3DSWAPEFFECT_FLIP for no VSync
            pp.SwapEffect = D3DSWAPEFFECT_COPY_VSYNC;
            // be stupid about the backbuffer count, like some D3D8 apps are
            pp.BackBufferCount = 0;
            pp.BackBufferWidth = WINDOW_WIDTH;
            pp.BackBufferHeight = WINDOW_HEIGHT;
            pp.BackBufferFormat = dm.Format;

            status = m_d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,
                                         D3DCREATE_SOFTWARE_VERTEXPROCESSING, 
                                         &pp, &m_device);
            if (FAILED(status))
                throw Error("Failed to create D3D8 device");

            // don't need any of these for 2D rendering
            status = m_device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
            if (FAILED(status))
                throw Error("Failed to set D3D8 render state for D3DRS_ZENABLE");
            status = m_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
            if (FAILED(status))
                throw Error("Failed to set D3D8 render state for D3DRS_CULLMODE");
            status = m_device->SetRenderState(D3DRS_LIGHTING, FALSE);
            if (FAILED(status))
                throw Error("Failed to set D3D8 render state for D3DRS_LIGHTING");
            
            // Vertex Buffer
            void* vertices = nullptr;
            
            // tailored for 800 x 800 and the appearance of being centered
            std::array<RGBVERTEX, 3> rgbVertices = {{
                {100.0f, 675.0f, 0.5f, 1.0f, D3DCOLOR_XRGB(255, 0, 0),},
                {400.0f, 75.0f, 0.5f, 1.0f, D3DCOLOR_XRGB(0, 255, 0),},
                {700.0f, 675.0f, 0.5f, 1.0f, D3DCOLOR_XRGB(0, 0, 255),},
            }};
            const size_t rgbVerticesSize = rgbVertices.size() * sizeof(RGBVERTEX);

            status = m_device->CreateVertexBuffer(rgbVerticesSize, 0, RGBT_FVF_CODES,
                                                  D3DPOOL_DEFAULT, &m_vb);
            if (FAILED(status))
                throw Error("Failed to create D3D8 vertex buffer");

            status = m_vb->Lock(0, rgbVerticesSize, (BYTE**)&vertices, 0);
            if (FAILED(status))
                throw Error("Failed to lock D3D8 vertex buffer");
            memcpy(vertices, rgbVertices.data(), rgbVerticesSize);
            status = m_vb->Unlock();
            if (FAILED(status))
                throw Error("Failed to unlock D3D8 vertex buffer");
        }

        void test() {
            UINT passedTests = 0;
            // to be updated when tests are added
            UINT totalTests = 2;

            if(m_device.ptr() == nullptr)
                throw Error("Failed to get a valid D3D8 device for running tests");

            HRESULT status = m_device->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
            if (FAILED(status))
                throw Error("Failed to clear D3D8 viewport");

            std::cout << "Running D3D8 tests:" << std::endl;
            // GetBackBuffer test (this shouldn't fail even with BackBufferCount set to 0)
            status = m_device->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &m_bbs);
            if (FAILED(status)) {
                std::cout << "  - The GetBackBuffer test has failed" << std::endl;
            } else {
                passedTests++;
                std::cout << "  + The GetBackBuffer test has passed" << std::endl;
            }
            // nested BeginScene test
            if(m_device->BeginScene() == D3D_OK) {
                // this call should fail according to D#D8 spec
                if(m_device->BeginScene() == D3D_OK) {
                    std::cout << "  - The nested BeginScene test has failed" << std::endl;
                } else {
                    passedTests++;
                    std::cout << "  + The nested BeginScene test has passed" << std::endl;
                }
            } else {
                throw Error("Failed to begin D3D8 scene");
            }
            if(m_device->EndScene() != D3D_OK) {
                throw Error("Failed to end D3D8 scene");
            }

            std::cout << format("Passed ", passedTests, "/", totalTests, " tests") << std::endl;
        }

        void render() {
            if(m_device.ptr() == nullptr)
                throw Error("Failed to get a valid D3D8 device for rendering");

            HRESULT status = m_device->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
            if (FAILED(status))
                throw Error("Failed to clear D3D8 viewport");
            if(m_device->BeginScene() == D3D_OK) {
                status = m_device->SetStreamSource(0, m_vb.ptr(), sizeof(RGBVERTEX));
                if (FAILED(status))
                    throw Error("Failed to set D3D8 stream source");
                status = m_device->SetVertexShader(RGBT_FVF_CODES);
                if (FAILED(status))
                    throw Error("Failed to set D3D8 vertex shader");
                status = m_device->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 1);
                if (FAILED(status))
                    throw Error("Failed to draw D3D8 triangle list");
                if(m_device->EndScene() == D3D_OK) {
                    status = m_device->Present(NULL, NULL, NULL, NULL);
                    if (FAILED(status))
                        throw Error("Failed to present");
                } else {
                    throw Error("Failed to end D3D8 scene");
                }
            } else {
                throw Error("Failed to begin D3D8 scene");
            }
        }
    
    private:

        Com<IDirect3D8> m_d3d;
        Com<IDirect3DDevice8> m_device;
        Com<IDirect3DSurface8> m_bbs;
        Com<IDirect3DVertexBuffer8> m_vb;
};

LRESULT WINAPI WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch(message) {
        case WM_CLOSE:
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}

INT WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInstance, 
                   LPSTR lpCmdLine, INT nCmdShow) {
    WNDCLASSEX wc = {sizeof(WNDCLASSEX), CS_CLASSDC, WindowProc, 0L, 0L,
                     GetModuleHandle(NULL), NULL, NULL, NULL, NULL,
                     "D3D8_Triangle", NULL};
    RegisterClassEx(&wc);

    HWND hWnd = CreateWindow("D3D8_Triangle", "D3D8 Triangle - Blisto Retro Edition", 
                              WS_OVERLAPPEDWINDOW, 50, 50, 
                              RGBTriangle::WINDOW_WIDTH, RGBTriangle::WINDOW_HEIGHT, 
                              GetDesktopWindow(), NULL, wc.hInstance, NULL);

    MSG msg;

    try {
        RGBTriangle rgbTriangle(hWnd);

        ShowWindow(hWnd, SW_SHOWDEFAULT);
        UpdateWindow(hWnd);

        rgbTriangle.test();

        while (true) {
            if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
                
                if (msg.message == WM_QUIT) {
                    UnregisterClass("D3D8_Triangle", wc.hInstance);
                    return msg.wParam;
                }
            } else {
                rgbTriangle.render();
            }
        }

    } catch (const Error& e) {
        std::cerr << e.message() << std::endl;
        UnregisterClass("D3D8_Triangle", wc.hInstance);
        return msg.wParam;
    }
}
