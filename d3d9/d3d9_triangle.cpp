#include <map>
#include <array>
#include <iostream>

#include <d3d9.h>
#include <d3d9caps.h>

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

        static const UINT WINDOW_WIDTH  = 700;
        static const UINT WINDOW_HEIGHT = 700;

        RGBTriangle(HWND hWnd) : m_hWnd(hWnd) {
            decltype(Direct3DCreate9)* Direct3DCreate9 = nullptr;
            HMODULE hm = LoadLibraryA("d3d9.dll");
            Direct3DCreate9 = (decltype(Direct3DCreate9))GetProcAddress(hm, "Direct3DCreate9");

            // D3D Interface
            m_d3d = Direct3DCreate9(D3D_SDK_VERSION);
            if (m_d3d.ptr() == nullptr)
                throw Error("Failed to create D3D9 interface");

            D3DADAPTER_IDENTIFIER9 adapterId;
            HRESULT status = m_d3d->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0, &adapterId);
            if (FAILED(status))
                throw Error("Failed to get D3D9 adapter identifier");

            std::cout << format("Using adapter: ", adapterId.Description) << std::endl;

            // DWORD to hex printout
            char vendorID[16];
            char deviceID[16];
            sprintf(vendorID, "VendorId: %04lx", adapterId.VendorId);
            sprintf(deviceID, "DeviceId: %04lx", adapterId.DeviceId);
            std::cout << "  ~ " << vendorID << std::endl;
            std::cout << "  ~ " << deviceID << std::endl;

            std::cout << format("  ~ Driver: ", adapterId.Driver) << std::endl;

            // NVIDIA stores the driver version in the lower half of the lower DWORD
            if (adapterId.VendorId == uint32_t(0x10de)) {
                DWORD driverVersion = LOWORD(adapterId.DriverVersion.LowPart);
                DWORD majorVersion = driverVersion / 100;
                DWORD minorVersion = driverVersion % 100;
                std::cout << format("  ~ Version: ", majorVersion, ".", minorVersion) << std::endl;
            }
            // for other vendors simply list the entire DriverVersion long int
            else {
                DWORD product = HIWORD(adapterId.DriverVersion.HighPart);
                DWORD version = LOWORD(adapterId.DriverVersion.HighPart);
                DWORD subVersion = HIWORD(adapterId.DriverVersion.LowPart);
                DWORD build = LOWORD(adapterId.DriverVersion.LowPart);
                std::cout << format("  ~ Version: ", product, ".", version, ".",
                    subVersion, ".", build) << std::endl;
            }

            // D3D Device
            D3DDISPLAYMODE dm;
            status = m_d3d->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &dm);
            if (FAILED(status))
                throw Error("Failed to get D3D9 adapter display mode");

            ZeroMemory(&m_pp, sizeof(m_pp));

            m_pp.Windowed = TRUE;
            m_pp.hDeviceWindow = m_hWnd;
            // set to D3DSWAPEFFECT_COPY or D3DSWAPEFFECT_FLIP for no VSync
            m_pp.SwapEffect = D3DSWAPEFFECT_COPY;
            // according to D3D8 spec "0 is treated as 1" here
            m_pp.BackBufferCount = 0;
            m_pp.BackBufferWidth = WINDOW_WIDTH;
            m_pp.BackBufferHeight = WINDOW_HEIGHT;
            m_pp.BackBufferFormat = dm.Format;

            createDeviceWithFlags(&m_pp, D3DCREATE_HARDWARE_VERTEXPROCESSING, true);
        }

        // D3D Adapter Display Mode enumeration
        void listAdapterDisplayModes() {
            HRESULT          status;
            D3DDISPLAYMODE   amDM;
            UINT             totalAdapterModeCount = 0;
            UINT             adapterModeCount = 0;

            ZeroMemory(&amDM, sizeof(amDM));

            // these are all the possible adapter display formats, at least in theory
            std::map<D3DFORMAT, char const*> amDMFormats = { {D3DFMT_A1R5G5B5, "D3DFMT_A1R5G5B5"},
                                                             {D3DFMT_A2R10G10B10, "D3DFMT_A2R10G10B10"},
                                                             {D3DFMT_A8R8G8B8, "D3DFMT_A8R8G8B8"},
                                                             {D3DFMT_X8R8G8B8, "D3DFMT_X8R8G8B8"},
                                                             {D3DFMT_X1R5G5B5, "D3DFMT_X1R5G5B5"},
                                                             {D3DFMT_R5G6B5, "D3DFMT_R5G6B5"} };

            std::cout << std::endl << "Enumerating supported adapter display modes:" << std::endl;

            for (auto const& amDMFormat : amDMFormats) {
                adapterModeCount = m_d3d->GetAdapterModeCount(D3DADAPTER_DEFAULT, amDMFormat.first);
                totalAdapterModeCount += adapterModeCount;

                for (UINT i = 0; i < adapterModeCount; i++ ) {
                    status =  m_d3d->EnumAdapterModes(D3DADAPTER_DEFAULT, amDMFormat.first, i, &amDM);

                    if (FAILED(status)) {
                        std::cout << format("    Failed to get adapter display mode ", i) << std::endl;
                    } else {
                        std::cout << format("    ", amDMFormats[amDM.Format], " ", amDM.Width,
                                            " x ", amDM.Height, " @ ", amDM.RefreshRate, " Hz") << std::endl;
                    }
                }
            }

            std::cout << format("Listed a total of ", totalAdapterModeCount, " adapter display modes") << std::endl;
        }

        void prepare() {
            createDeviceWithFlags(&m_pp, D3DCREATE_HARDWARE_VERTEXPROCESSING, true);

            // don't need any of these for 2D rendering
            HRESULT status = m_device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
            if (FAILED(status))
                throw Error("Failed to set D3D9 render state for D3DRS_ZENABLE");
            status = m_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
            if (FAILED(status))
                throw Error("Failed to set D3D9 render state for D3DRS_CULLMODE");
            status = m_device->SetRenderState(D3DRS_LIGHTING, FALSE);
            if (FAILED(status))
                throw Error("Failed to set D3D9 render state for D3DRS_LIGHTING");

            // Vertex Buffer
            void* vertices = nullptr;

            // tailored for 1024x768 and the appearance of being centered
            std::array<RGBVERTEX, 3> rgbVertices = {{
                { 60.0f, 625.0f, 0.5f, 1.0f, D3DCOLOR_XRGB(255, 0, 0),},
                {350.0f,  45.0f, 0.5f, 1.0f, D3DCOLOR_XRGB(0, 255, 0),},
                {640.0f, 625.0f, 0.5f, 1.0f, D3DCOLOR_XRGB(0, 0, 255),},
            }};
            const size_t rgbVerticesSize = rgbVertices.size() * sizeof(RGBVERTEX);

            status = m_device->CreateVertexBuffer(rgbVerticesSize, 0, RGBT_FVF_CODES,
                                                  D3DPOOL_DEFAULT, &m_vb, NULL);
            if (FAILED(status))
                throw Error("Failed to create D3D9 vertex buffer");

            status = m_vb->Lock(0, rgbVerticesSize, (void**)&vertices, 0);
            if (FAILED(status))
                throw Error("Failed to lock D3D9 vertex buffer");
            memcpy(vertices, rgbVertices.data(), rgbVerticesSize);
            status = m_vb->Unlock();
            if (FAILED(status))
                throw Error("Failed to unlock D3D9 vertex buffer");
        }

        void render() {
            HRESULT status = m_device->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
            if (FAILED(status))
                throw Error("Failed to clear D3D9 viewport");
            if (SUCCEEDED(m_device->BeginScene())) {
                status = m_device->SetStreamSource(0, m_vb.ptr(), 0, sizeof(RGBVERTEX));
                if (FAILED(status))
                    throw Error("Failed to set D3D9 stream source");
                status = m_device->SetFVF(RGBT_FVF_CODES);
                if (FAILED(status))
                    throw Error("Failed to set D3D9 vertex shader");
                status = m_device->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 1);
                if (FAILED(status))
                    throw Error("Failed to draw D3D9 triangle list");
                if (SUCCEEDED(m_device->EndScene())) {
                    status = m_device->Present(NULL, NULL, NULL, NULL);
                    if (FAILED(status))
                        throw Error("Failed to present");
                } else {
                    throw Error("Failed to end D3D9 scene");
                }
            } else {
                throw Error("Failed to begin D3D9 scene");
            }
        }

    private:

        HRESULT createDeviceWithFlags(D3DPRESENT_PARAMETERS* presentParams,
                                      DWORD behaviorFlags,
                                      bool throwErrorOnFail) {
            if (m_d3d == nullptr)
                throw Error("The D3D9 interface hasn't been initialized");

            if (m_device != nullptr)
                m_device = nullptr;

            HRESULT status = m_d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, m_hWnd,
                                                 behaviorFlags, presentParams, &m_device);
            if (throwErrorOnFail && FAILED(status))
                throw Error("Failed to create D3D9 device");

            return status;
        }

        HWND                          m_hWnd;

        Com<IDirect3D9>               m_d3d;
        Com<IDirect3DDevice9>         m_device;
        Com<IDirect3DVertexBuffer9>   m_vb;

        D3DPRESENT_PARAMETERS         m_pp;
};

LRESULT WINAPI WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch(message) {
        case WM_CLOSE:
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}

int main(int, char**) {
    WNDCLASSEX wc = {sizeof(WNDCLASSEX), CS_CLASSDC, WindowProc, 0L, 0L,
                     GetModuleHandle(NULL), NULL, LoadCursor(nullptr, IDC_ARROW), NULL, NULL,
                     "D3D9_Triangle", NULL};
    RegisterClassEx(&wc);

    HWND hWnd = CreateWindow("D3D9_Triangle", "D3D9 Triangle - Blisto Modern Testing Edition",
                              WS_OVERLAPPEDWINDOW, 50, 50,
                              RGBTriangle::WINDOW_WIDTH, RGBTriangle::WINDOW_HEIGHT,
                              GetDesktopWindow(), NULL, wc.hInstance, NULL);

    MSG msg;

    try {
        RGBTriangle rgbTriangle(hWnd);

        // list various D3D Device stats
        rgbTriangle.listAdapterDisplayModes();

        // D3D9 triangle
        rgbTriangle.prepare();

        ShowWindow(hWnd, SW_SHOWDEFAULT);
        UpdateWindow(hWnd);

        while (true) {
            if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);

                if (msg.message == WM_QUIT) {
                    UnregisterClass("D3D9_Triangle", wc.hInstance);
                    return msg.wParam;
                }
            } else {
                rgbTriangle.render();
            }
        }

    } catch (const Error& e) {
        std::cerr << e.message() << std::endl;
        UnregisterClass("D3D9_Triangle", wc.hInstance);
        return msg.wParam;
    }
}
