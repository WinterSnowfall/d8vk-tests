#include <array>
#include <iostream>

#include <d3d8.h>

#include "../common/com.h"
#include "../common/error.h"

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
            m_d3d = Direct3DCreate8(D3D_SDK_VERSION);
            if(m_d3d.ptr() == nullptr)
                throw Error("Failed to create D3D8 interface");

            D3DDISPLAYMODE dm;
            HRESULT status = m_d3d->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &dm);
            if (FAILED(status))
                throw Error("Failed to get D3D8 adapter display mode");

            D3DPRESENT_PARAMETERS pp;
            ZeroMemory(&pp, sizeof(pp));

            pp.Windowed = TRUE;
            pp.SwapEffect = D3DSWAPEFFECT_COPY_VSYNC;
            pp.BackBufferWidth = WINDOW_WIDTH;
            pp.BackBufferHeight = WINDOW_HEIGHT;
            pp.BackBufferFormat = dm.Format;

            status = m_d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,
                                         D3DCREATE_SOFTWARE_VERTEXPROCESSING, 
                                         &pp, &m_device);
            if (FAILED(status))
                throw Error("Failed to create D3D8 device");

            //Don't need any of this for 2D rendering
            m_device->SetRenderState(D3DRS_ZENABLE, FALSE);
            m_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
            m_device->SetRenderState(D3DRS_LIGHTING, FALSE);
            
            //Vertex Buffer
            void* vertices = nullptr;
            
            std::array<RGBVERTEX, 3> rgbVertices = {{
                {100.0f, 675.0f, 0.5f, 1.0f, D3DCOLOR_XRGB(255, 0, 0),},
                {400.0f, 75.0f, 0.5f, 1.0f, D3DCOLOR_XRGB(0, 255, 0),},
                {700.0f, 675.0f, 0.5f, 1.0f, D3DCOLOR_XRGB(0, 0, 255),},
            }};

            status = m_device->CreateVertexBuffer(rgbVertices.size() * sizeof(RGBVERTEX),
                                                  0, RGBT_FVF_CODES,
                                                  D3DPOOL_DEFAULT, &m_vb);
            if (FAILED(status))
                throw Error("Failed to create D3D8 vertex buffer");

            status = m_vb->Lock(0, sizeof(rgbVertices), (BYTE**)&vertices, 0);
            if (FAILED(status))
                throw Error("Failed to lock D3D8 vertex buffer");
            memcpy(vertices, rgbVertices.data(), sizeof(rgbVertices));
            status = m_vb->Unlock();
            if (FAILED(status))
                throw Error("Failed to unlock D3D8 vertex buffer");
        }

        void render() {
            if(m_device.ptr() == nullptr)
                throw Error("Failed to get a valid D3D8 device for present");

            m_device->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
            m_device->BeginScene();
            m_device->SetStreamSource(0, m_vb.ptr(), sizeof(RGBVERTEX));
            m_device->SetVertexShader(RGBT_FVF_CODES);
            m_device->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 1);
            m_device->EndScene();
            m_device->Present(NULL, NULL, NULL, NULL);
        }
    
    private:

        Com<IDirect3D8> m_d3d;
        Com<IDirect3DDevice8> m_device;
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
