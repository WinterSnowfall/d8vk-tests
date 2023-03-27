#include <map>
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

        static const UINT WINDOW_WIDTH  = 800;
        static const UINT WINDOW_HEIGHT = 800;

        RGBTriangle(HWND hWnd) : m_hWnd(hWnd) {
            // D3D Interface
            m_d3d = Direct3DCreate8(D3D_SDK_VERSION);
            if(m_d3d.ptr() == nullptr)
                throw Error("Failed to create D3D8 interface");

            D3DADAPTER_IDENTIFIER8 adapterId;
            HRESULT status = m_d3d->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0, &adapterId);
            if (FAILED(status))
                throw Error("Failed to get D3D8 adapter identifier");

            std::cout << format("Using adapter: ", adapterId.Description) << std::endl;

            // D3D Device
            D3DDISPLAYMODE dm;
            status = m_d3d->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &dm);
            if (FAILED(status))
                throw Error("Failed to get D3D8 adapter display mode");

            ZeroMemory(&m_pp, sizeof(m_pp));

            m_pp.Windowed = TRUE;
            m_pp.hDeviceWindow = m_hWnd;
            // set to D3DSWAPEFFECT_COPY or D3DSWAPEFFECT_FLIP for no VSync
            m_pp.SwapEffect = D3DSWAPEFFECT_COPY_VSYNC;
            // according to D3D8 spec "0 is treated as 1" here
            m_pp.BackBufferCount = 0;
            m_pp.BackBufferWidth = WINDOW_WIDTH;
            m_pp.BackBufferHeight = WINDOW_HEIGHT;
            m_pp.BackBufferFormat = dm.Format;

            status = m_d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, m_hWnd,
                                         D3DCREATE_SOFTWARE_VERTEXPROCESSING, 
                                         &m_pp, &m_device);
            if (FAILED(status))
                throw Error("Failed to create D3D8 device");
        }

        // D3D Adapter Display Mode enumeration
        void listAdapterDisplayModes() {
            HRESULT          status;
            D3DDISPLAYMODE   amDM;
            UINT             adapterModeCount = 0;

            ZeroMemory(&amDM, sizeof(amDM));

            adapterModeCount = m_d3d->GetAdapterModeCount(D3DADAPTER_DEFAULT);
            if(adapterModeCount == 0)
                throw Error("Failed to query for D3D8 adapter display modes");

            // these are all the possible adapter display formats, at least in theory
            std::map<D3DFORMAT, char const*> amDMFormats = { {D3DFMT_X8R8G8B8, "D3DFMT_X8R8G8B8"}, 
                                                             {D3DFMT_X1R5G5B5, "D3DFMT_X1R5G5B5"}, 
                                                             {D3DFMT_R5G6B5, "D3DFMT_R5G6B5"} };

            for(UINT i = 0; i < adapterModeCount; i++) {
                status =  m_d3d->EnumAdapterModes(D3DADAPTER_DEFAULT, i, &amDM);
                if (FAILED(status)) {
                    std::cout << format("    Failed to get adapter display mode ", i) << std::endl;
                } else {
                    std::cout << format("    ", amDMFormats[amDM.Format], " ", amDM.Width, 
                                        " x ", amDM.Height, " @ ", amDM.RefreshRate, " Hz") << std::endl;
                }
            }

            std::cout << format("Listed a total of ", adapterModeCount, " adapter display modes") << std::endl;
        }

        // GetBackBuffer test (this shouldn't fail even with BackBufferCount set to 0)
        void testZeroBackBufferCount() {
            HRESULT status = m_device->Reset(&m_pp);
            if(FAILED(status))
                throw Error("Failed to reset D3D8 device");

            Com<IDirect3DSurface8> bbSurface;

            m_totalTests++;

            status = m_device->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &bbSurface);
            if (FAILED(status)) {
                std::cout << "  - The GetBackBuffer test has failed" << std::endl;
            } else {
                m_passedTests++;
                std::cout << "  + The GetBackBuffer test has passed" << std::endl;
            }
        }

        // BeginScene+Reset test
        void testBeginSceneReset() {
            HRESULT status = m_device->Reset(&m_pp);
            if(FAILED(status))
                throw Error("Failed to reset D3D8 device");

            m_totalTests++;

            if(SUCCEEDED(m_device->BeginScene())) {
                status = m_device->Reset(&m_pp);
                if(FAILED(status)) {
                    throw Error("Failed to reset D3D8 device");
                }
                else {       
                    // Reset() should have cleared the state so this should work properly
                    if(SUCCEEDED(m_device->BeginScene())) {
                        m_passedTests++;
                        std::cout << "  + The BeginScene+Reset test has passed" << std::endl;
                    } else {
                        std::cout << "  - The BeginScene+Reset test has failed" << std::endl;
                    }
                }
            } else {
                throw Error("Failed to begin D3D8 scene");
            }
            if(FAILED(m_device->EndScene())) {
                throw Error("Failed to end D3D8 scene");
            }
        }

        // SWVP Render State test (games like Massive Assault try to enable SWVP in PUREDEVICE mode)
        void testPureDeviceSetSWVPRenderState() {
            HRESULT status = m_device->Reset(&m_pp);
            if(FAILED(status))
                throw Error("Failed to reset D3D8 device");

            status = m_d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, m_hWnd,
                                         D3DCREATE_PUREDEVICE, 
                                         &m_pp, &m_device);
            if(FAILED(status)) {
                // apparently this is no longer supported on recent versions of Windows
                std::cout << "  ~ The PUREDEVICE mode is not supported" << std::endl;

                status = m_d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, m_hWnd,
                                             D3DCREATE_SOFTWARE_VERTEXPROCESSING, 
                                             &m_pp, &m_device);
                if (FAILED(status))
                    throw Error("Failed to create D3D8 device");
            } else {
                m_totalTests++;

                status = m_device->SetRenderState(D3DRS_SOFTWAREVERTEXPROCESSING, TRUE);
                if (FAILED(status)) {
                    std::cout << "  - The enable SWVP RS in PUREDEVICE mode test has failed" << std::endl;
                } else {
                    m_passedTests++;
                    std::cout << "  + The enable SWVP RS in PUREDEVICE mode test has passed" << std::endl;
                }
            }
        }

        // Depth Stencil format tests
        void testDepthStencilFormats() {
            HRESULT status = m_device->Reset(&m_pp);
            if(FAILED(status))
                throw Error("Failed to reset D3D8 device");

            D3DPRESENT_PARAMETERS dsPP;

            memcpy(&dsPP, &m_pp, sizeof(m_pp));
            dsPP.EnableAutoDepthStencil = TRUE;

            std::map<D3DFORMAT, char const*> dsFormats = { {D3DFMT_D16_LOCKABLE, "D3DFMT_D16_LOCKABLE"}, 
                                                           {D3DFMT_D32, "D3DFMT_D32"}, 
                                                           {D3DFMT_D15S1, "D3DFMT_D15S1"}, 
                                                           {D3DFMT_D24S8, "D3DFMT_D24S8"}, 
                                                           {D3DFMT_D16, "D3DFMT_D16"},
                                                           {D3DFMT_D24X8, "D3DFMT_D24X8"}, 
                                                           {D3DFMT_D24X4S4, "D3DFMT_D24X4S4"} };

            std::map<D3DFORMAT, char const*>::iterator dsFormatIter;
            
            for (dsFormatIter = dsFormats.begin(); dsFormatIter != dsFormats.end(); dsFormatIter++) {
                dsPP.AutoDepthStencilFormat = dsFormatIter->first;

                status = m_d3d->CheckDepthStencilMatch(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
                                                       dsPP.BackBufferFormat, dsPP.BackBufferFormat,
                                                       dsPP.AutoDepthStencilFormat);
                if (FAILED(status)) {
                    std::cout << format("  ~ The ", dsFormatIter->second, " DS format is not supported") << std::endl;
                } else {
                    m_totalTests++;
                    status = m_d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, m_hWnd,
                                                 D3DCREATE_MIXED_VERTEXPROCESSING, 
                                                 &dsPP, &m_device);
                    if (FAILED(status)) {
                        std::cout << format("  - The ", dsFormatIter->second, " DS format test has failed") << std::endl;
                    } else {
                        m_passedTests++;
                        std::cout << format("  + The ", dsFormatIter->second, " DS format test has passed") << std::endl;
                    }
                }
            }
        }

        // BackBuffer format tests
        void testBackBufferFormats(BOOL windowed) {
            HRESULT status = m_device->Reset(&m_pp);
            if(FAILED(status))
                throw Error("Failed to reset D3D8 device");

            D3DPRESENT_PARAMETERS bbPP;

            memcpy(&bbPP, &m_pp, sizeof(m_pp));

            // these are all the possible backbuffer formats, at least in theory
            std::map<D3DFORMAT, char const*> bbFormats = { {D3DFMT_A8R8G8B8, "D3DFMT_A8R8G8B8"}, 
                                                           {D3DFMT_X8R8G8B8, "D3DFMT_X8R8G8B8"}, 
                                                           {D3DFMT_A1R5G5B5, "D3DFMT_A1R5G5B5"}, 
                                                           {D3DFMT_X1R5G5B5, "D3DFMT_X1R5G5B5"},
                                                           {D3DFMT_R5G6B5, "D3DFMT_R5G6B5"} };

            std::map<D3DFORMAT, char const*>::iterator bbFormatIter;
            
            for (bbFormatIter = bbFormats.begin(); bbFormatIter != bbFormats.end(); bbFormatIter++) {
                status = m_d3d->CheckDeviceType(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
                                                bbFormatIter->first, bbFormatIter->first,
                                                windowed);
                if (FAILED(status)) {
                    std::cout << format("  ~ The ", bbFormatIter->second, " BB format is not supported") << std::endl;
                } else {
                    m_totalTests++;

                    bbPP.BackBufferFormat = bbFormatIter->first;
                    
                    status = m_d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, m_hWnd,
                                                 D3DCREATE_MIXED_VERTEXPROCESSING, 
                                                 &bbPP, &m_device);
                    if (FAILED(status)) {
                        std::cout << format("  - The ", bbFormatIter->second, " BB format test has failed") << std::endl;

                        status = m_d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, m_hWnd,
                                                     D3DCREATE_SOFTWARE_VERTEXPROCESSING, 
                                                     &m_pp, &m_device);
                        if (FAILED(status))
                            throw Error("Failed to create D3D8 device");
                    } else {
                        m_passedTests++;
                        std::cout << format("  + The ", bbFormatIter->second, " BB format test has passed") << std::endl;
                    }
                }
            }
        }

        void printTestResults() {
            std::cout << format("Passed ", m_passedTests, "/", m_totalTests, " tests") << std::endl;
        }

        void prepare() {
            HRESULT status = m_d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, m_hWnd,
                                                 D3DCREATE_HARDWARE_VERTEXPROCESSING, 
                                                 &m_pp, &m_device);
            if (FAILED(status))
                throw Error("Failed to create D3D8 device");

            status = m_device->Reset(&m_pp);
            if(FAILED(status))
                throw Error("Failed to reset D3D8 device");

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

        void render() {
            HRESULT status = m_device->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
            if (FAILED(status))
                throw Error("Failed to clear D3D8 viewport");
            if(SUCCEEDED(m_device->BeginScene())) {
                status = m_device->SetStreamSource(0, m_vb.ptr(), sizeof(RGBVERTEX));
                if (FAILED(status))
                    throw Error("Failed to set D3D8 stream source");
                status = m_device->SetVertexShader(RGBT_FVF_CODES);
                if (FAILED(status))
                    throw Error("Failed to set D3D8 vertex shader");
                status = m_device->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 1);
                if (FAILED(status))
                    throw Error("Failed to draw D3D8 triangle list");
                if(SUCCEEDED(m_device->EndScene())) {
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

        HWND                          m_hWnd;

        Com<IDirect3D8>               m_d3d;
        Com<IDirect3DDevice8>         m_device;
        Com<IDirect3DVertexBuffer8>   m_vb;
        
        D3DPRESENT_PARAMETERS         m_pp;

        UINT                          m_totalTests  = 0;
        UINT                          m_passedTests = 0;
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

    HWND hWnd = CreateWindow("D3D8_Triangle", "D3D8 Triangle - Blisto Retro Testing Edition", 
                              WS_OVERLAPPEDWINDOW, 50, 50, 
                              RGBTriangle::WINDOW_WIDTH, RGBTriangle::WINDOW_HEIGHT, 
                              GetDesktopWindow(), NULL, wc.hInstance, NULL);

    MSG msg;

    try {
        RGBTriangle rgbTriangle(hWnd);

        // D3D adapter display modes
        std::cout << std::endl << "Enumerating supported adapter display modes:" << std::endl;
        rgbTriangle.listAdapterDisplayModes();

        // D3D tests
        std::cout << std::endl << "Running D3D8 tests:" << std::endl;
        rgbTriangle.testZeroBackBufferCount();
        rgbTriangle.testBeginSceneReset();
        rgbTriangle.testPureDeviceSetSWVPRenderState();
        std::cout << "Running DS format tests:" << std::endl;
        rgbTriangle.testDepthStencilFormats();
        std::cout << "Running full screen BB format tests:" << std::endl;
        rgbTriangle.testBackBufferFormats(FALSE);
        std::cout << "Running windowed BB format tests:" << std::endl;
        rgbTriangle.testBackBufferFormats(TRUE);
        rgbTriangle.printTestResults();

        // D3D triangle
        rgbTriangle.prepare();

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
