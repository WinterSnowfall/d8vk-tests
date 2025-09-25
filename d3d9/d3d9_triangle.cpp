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

// D3D9 caps
#ifdef _MSC_VER
#define D3DCAPS2_CANAUTOGENMIPMAP               0x40000000L
#define D3DCAPS3_LINEAR_TO_SRGB_PRESENTATION    0x00000080L
#define D3DCAPS3_COPY_TO_VIDMEM                 0x00000100L
#define D3DCAPS3_COPY_TO_SYSTEMMEM              0x00000200L
#define D3DPMISCCAPS_INDEPENDENTWRITEMASKS      0x00004000L
#define D3DPMISCCAPS_PERSTAGECONSTANT           0x00008000L
#define D3DPMISCCAPS_FOGANDSPECULARALPHA        0x00010000L
#define D3DPMISCCAPS_SEPARATEALPHABLEND         0x00020000L
#define D3DPMISCCAPS_MRTINDEPENDENTBITDEPTHS    0x00040000L
#define D3DPMISCCAPS_MRTPOSTPIXELSHADERBLENDING 0x00080000L
#define D3DPMISCCAPS_FOGVERTEXCLAMPED           0x00100000L

#define D3DPRASTERCAPS_SCISSORTEST              0x01000000L
#define D3DPRASTERCAPS_SLOPESCALEDEPTHBIAS      0x02000000L
#define D3DPRASTERCAPS_DEPTHBIAS                0x04000000L
#define D3DPRASTERCAPS_MULTISAMPLE_TOGGLE       0x08000000L

#define D3DLINECAPS_ANTIALIAS                   0x00000020L
#define D3DSTENCILCAPS_TWOSIDED                 0x00000100L
#define D3DPMISCCAPS_POSTBLENDSRGBCONVERT       0x00200000L
#define D3DPBLENDCAPS_BLENDFACTOR               0x00002000L
#define D3DPBLENDCAPS_SRCCOLOR2                 0x00004000L
#define D3DPBLENDCAPS_INVSRCCOLOR2              0x00008000L

#define D3DVTXPCAPS_TEXGEN_SPHEREMAP            0x00000100L
#define D3DVTXPCAPS_NO_TEXGEN_NONLOCALVIEWER    0x00000200L
#endif

class RGBTriangle {

    public:

        static constexpr const char* TRIANGLE_ID    = "D3D9_Triangle";
        static constexpr const char* TRIANGLE_TITLE = "D3D9 Triangle - Blisto Modern Testing Edition";

        static constexpr UINT WINDOW_WIDTH  = 700;
        static constexpr UINT WINDOW_HEIGHT = 700;

        RGBTriangle(HWND hWnd) : m_hWnd(hWnd) {
            std::cout << RGBTriangle::TRIANGLE_TITLE << std::endl << std::endl;

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

            m_vendorID = adapterId.VendorId;

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
            if (m_vendorID == uint32_t(0x10de)) {
                // Newer drivers will also spill over into the upper half
                DWORD driverVersionHigh = HIWORD(adapterId.DriverVersion.LowPart);
                DWORD driverVersionLow  = LOWORD(adapterId.DriverVersion.LowPart);
                DWORD majorVersion = driverVersionLow / 100;
                // Might want to revisit this cursed logic, but it should work fine for now
                if (driverVersionHigh >= 15 && driverVersionLow < 10000)
                    majorVersion += (driverVersionHigh % 10) * 100;
                DWORD minorVersion = driverVersionLow % 100;
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
            // according to D3D9 spec "0 is treated as 1" here
            m_pp.BackBufferCount = 0;
            m_pp.BackBufferWidth = RGBTriangle::WINDOW_WIDTH;
            m_pp.BackBufferHeight = RGBTriangle::WINDOW_HEIGHT;
            m_pp.BackBufferFormat = dm.Format;

            createDeviceWithFlags(&m_pp, D3DCREATE_HARDWARE_VERTEXPROCESSING, D3DDEVTYPE_HAL, true);
        }

        // D3D Adapter Display Mode enumeration
        void listAdapterDisplayModes() {
            HRESULT          status;
            D3DDISPLAYMODE   amDM;
            UINT             totalAdapterModeCount = 0;
            UINT             adapterModeCount = 0;

            ZeroMemory(&amDM, sizeof(amDM));

            // these are all the possible adapter display formats, at least in theory
            std::map<D3DFORMAT, char const*> amDMFormats = { {D3DFMT_A2R10G10B10, "D3DFMT_A2R10G10B10"},
                                                             {D3DFMT_A8R8G8B8, "D3DFMT_A8R8G8B8"},
                                                             {D3DFMT_X8R8G8B8, "D3DFMT_X8R8G8B8"},
                                                             {D3DFMT_A1R5G5B5, "D3DFMT_A1R5G5B5"},
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

        // D3D Device back buffer formats check
        void listBackBufferFormats(BOOL windowed) {
            HRESULT status;
            D3DPRESENT_PARAMETERS bbPP;

            memcpy(&bbPP, &m_pp, sizeof(m_pp));

            // these are all the possible adapter format / backbuffer format pairs, at least in theory
            std::map<std::pair<D3DFORMAT, D3DFORMAT>, char const*> bbFormats = {
                {{D3DFMT_A2R10G10B10, D3DFMT_A2R10G10B10}, "D3DFMT_A2R10G10B10"},
                {{D3DFMT_X8R8G8B8, D3DFMT_X8R8G8B8}, "D3DFMT_X8R8G8B8"},
                {{D3DFMT_X8R8G8B8, D3DFMT_A8R8G8B8}, "D3DFMT_A8R8G8B8"},
                {{D3DFMT_X1R5G5B5, D3DFMT_X1R5G5B5}, "D3DFMT_X1R5G5B5"},
                {{D3DFMT_X1R5G5B5, D3DFMT_A1R5G5B5}, "D3DFMT_A1R5G5B5"},
                {{D3DFMT_R5G6B5, D3DFMT_R5G6B5}, "D3DFMT_R5G6B5"},
                {{D3DFMT_R5G6B5, D3DFMT_UNKNOWN}, "D3DFMT_UNKNOWN"}
            };

            std::map<std::pair<D3DFORMAT, D3DFORMAT>, char const*>::iterator bbFormatIter;

            std::cout << std::endl;
            if (windowed)
                std::cout << "Device back buffer format support (windowed):" << std::endl;
            else
                std::cout << "Device back buffer format support (full screen):" << std::endl;

            for (bbFormatIter = bbFormats.begin(); bbFormatIter != bbFormats.end(); ++bbFormatIter) {
                status = m_d3d->CheckDeviceType(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
                                                bbFormatIter->first.first, bbFormatIter->first.second,
                                                windowed);
                if (FAILED(status)) {
                    std::cout << format("  - The ", bbFormatIter->second, " format is not supported") << std::endl;
                } else {
                    bbPP.BackBufferFormat = bbFormatIter->first.second;

                    status = createDeviceWithFlags(&bbPP, D3DCREATE_HARDWARE_VERTEXPROCESSING, D3DDEVTYPE_HAL, false);
                    if (FAILED(status)) {
                        std::cout << format("  ! The ", bbFormatIter->second, " format is supported, device creation FAILED") << std::endl;
                    } else {
                        std::cout << format("  + The ", bbFormatIter->second, " format is supported, device creation SUCCEEDED") << std::endl;
                    }
                }
            }
        }

        // D3D Device surface/texture formats check
        void listSurfaceFormats(DWORD Usage) {
            resetOrRecreateDevice();

            // D3DFMT_UNKNOWN will fail everywhere, so start with the next format
            std::map<D3DFORMAT, char const*> formats = { {D3DFMT_R8G8B8,        "D3DFMT_R8G8B8"},
                                                         {D3DFMT_A8R8G8B8,      "D3DFMT_A8R8G8B8"},
                                                         {D3DFMT_X8R8G8B8,      "D3DFMT_X8R8G8B8"},
                                                         {D3DFMT_R5G6B5,        "D3DFMT_R5G6B5"},
                                                         {D3DFMT_X1R5G5B5,      "D3DFMT_X1R5G5B5"},
                                                         {D3DFMT_A1R5G5B5,      "D3DFMT_A1R5G5B5"},
                                                         {D3DFMT_A4R4G4B4,      "D3DFMT_A4R4G4B4"},
                                                         {D3DFMT_R3G3B2,        "D3DFMT_R3G3B2"},
                                                         {D3DFMT_A8,            "D3DFMT_A8"},
                                                         {D3DFMT_A8R3G3B2,      "D3DFMT_A8R3G3B2"},
                                                         {D3DFMT_X4R4G4B4,      "D3DFMT_X4R4G4B4"},
                                                         {D3DFMT_A2B10G10R10,   "D3DFMT_A2B10G10R10"},
                                                         {D3DFMT_A8B8G8R8,      "D3DFMT_A8B8G8R8"},
                                                         {D3DFMT_X8B8G8R8,      "D3DFMT_X8B8G8R8"},
                                                         {D3DFMT_G16R16,        "D3DFMT_G16R16"},
                                                         {D3DFMT_A2R10G10B10,   "D3DFMT_A2R10G10B10"},
                                                         {D3DFMT_A16B16G16R16,  "D3DFMT_A16B16G16R16"},
                                                         {D3DFMT_A8P8,          "D3DFMT_A8P8"},
                                                         {D3DFMT_P8,            "D3DFMT_P8"},
                                                         {D3DFMT_L8,            "D3DFMT_L8"},
                                                         {D3DFMT_A8L8,          "D3DFMT_A8L8"},
                                                         {D3DFMT_A4L4,          "D3DFMT_A4L4"},
                                                         {D3DFMT_V8U8,          "D3DFMT_V8U8"},
                                                         {D3DFMT_L6V5U5,        "D3DFMT_L6V5U5"},
                                                         {D3DFMT_X8L8V8U8,      "D3DFMT_X8L8V8U8"},
                                                         {D3DFMT_Q8W8V8U8,      "D3DFMT_Q8W8V8U8"},
                                                         {D3DFMT_V16U16,        "D3DFMT_V16U16"},
                                                         // D3DFMT_W11V11U10 is not supported by D3D9
                                                         {(D3DFORMAT) 65,       "D3DFMT_W11V11U10"},
                                                         {D3DFMT_A2W10V10U10,   "D3DFMT_A2W10V10U10"},
                                                         {D3DFMT_UYVY,          "D3DFMT_UYVY"},
                                                         {D3DFMT_YUY2,          "D3DFMT_YUY2"},
                                                         {D3DFMT_DXT1,          "D3DFMT_DXT1"},
                                                         {D3DFMT_DXT2,          "D3DFMT_DXT2"},
                                                         {D3DFMT_DXT3,          "D3DFMT_DXT3"},
                                                         {D3DFMT_DXT4,          "D3DFMT_DXT4"},
                                                         {D3DFMT_DXT5,          "D3DFMT_DXT5"},
                                                         {D3DFMT_MULTI2_ARGB8,  "D3DFMT_MULTI2_ARGB8"},
                                                         {D3DFMT_G8R8_G8B8,     "D3DFMT_G8R8_G8B8"},
                                                         {D3DFMT_R8G8_B8G8,     "D3DFMT_R8G8_B8G8"},
                                                         {D3DFMT_D16_LOCKABLE,  "D3DFMT_D16_LOCKABLE"},
                                                         {D3DFMT_D32,           "D3DFMT_D32"},
                                                         {D3DFMT_D15S1,         "D3DFMT_D15S1"},
                                                         {D3DFMT_D24S8,         "D3DFMT_D24S8"},
                                                         {D3DFMT_D24X8,         "D3DFMT_D24X8"},
                                                         {D3DFMT_D24X4S4,       "D3DFMT_D24X4S4"},
                                                         {D3DFMT_D16,           "D3DFMT_D16"},
                                                         {D3DFMT_L16,           "D3DFMT_L16"},
                                                         {D3DFMT_D32F_LOCKABLE, "D3DFMT_D32F_LOCKABLE"},
                                                         {D3DFMT_D24FS8,        "D3DFMT_D24FS8"},
                                                         {D3DFMT_D32_LOCKABLE,  "D3DFMT_D32_LOCKABLE"},
                                                         {D3DFMT_S8_LOCKABLE,   "D3DFMT_S8_LOCKABLE"},
                                                         {D3DFMT_Q16W16V16U16,  "D3DFMT_Q16W16V16U16"},
                                                         {D3DFMT_R16F,          "D3DFMT_R16F"},
                                                         {D3DFMT_G16R16F,       "D3DFMT_G16R16F"},
                                                         {D3DFMT_A16B16G16R16F, "D3DFMT_A16B16G16R16F"},
                                                         {D3DFMT_R32F,          "D3DFMT_R32F"},
                                                         {D3DFMT_G32R32F,       "D3DFMT_G32R32F"},
                                                         {D3DFMT_A32B32G32R32F, "D3DFMT_A32B32G32R32F"},
                                                         {D3DFMT_CxV8U8,        "D3DFMT_CxV8U8"},
                                                         {D3DFMT_A1,            "D3DFMT_A1"},
                                                         {D3DFMT_A2B10G10R10_XR_BIAS, "D3DFMT_A2B10G10R10_XR_BIAS"},
                                                         // Consacrated (enum) formats end here
                                                         {(D3DFORMAT) MAKEFOURCC('A', 'T', 'I', '1'), "D3DFMT_ATI1"},
                                                         {(D3DFORMAT) MAKEFOURCC('A', 'T', 'I', '2'), "D3DFMT_ATI2"},
                                                         {(D3DFORMAT) MAKEFOURCC('Y', 'U', 'Y', '2'), "D3DFMT_YUY2"},
                                                         {(D3DFORMAT) MAKEFOURCC('D', 'F', '1', '6'), "D3DFMT_DF16"},
                                                         {(D3DFORMAT) MAKEFOURCC('D', 'F', '2', '4'), "D3DFMT_DF24"},
                                                         {(D3DFORMAT) MAKEFOURCC('I', 'N', 'T', 'Z'), "D3DFMT_INTZ"},
                                                         {(D3DFORMAT) MAKEFOURCC('N', 'U', 'L', 'L'), "D3DFMT_NULL"},
                                                         // Nobody seems to support these, but they are queried
                                                         {(D3DFORMAT) MAKEFOURCC('N', 'V', 'H', 'S'), "D3DFMT_NVHS"},
                                                         {(D3DFORMAT) MAKEFOURCC('N', 'V', 'H', 'U'), "D3DFMT_NVHU"},
                                                         {(D3DFORMAT) MAKEFOURCC('N', 'V', 'C', 'S'), "D3DFMT_NVCS"},
                                                         {(D3DFORMAT) MAKEFOURCC('E', 'X', 'T', '1'), "D3DFMT_EXT1"},
                                                         {(D3DFORMAT) MAKEFOURCC('F', 'X', 'T', '1'), "D3DFMT_FXT1"},
                                                         {(D3DFORMAT) MAKEFOURCC('G', 'X', 'T', '1'), "D3DFMT_GXT1"},
                                                         {(D3DFORMAT) MAKEFOURCC('H', 'X', 'T', '1'), "D3DFMT_HXT1"},
                                                         {(D3DFORMAT) MAKEFOURCC('A', 'L', '1', '6'), "D3DFMT_AL16"},
                                                         {(D3DFORMAT) MAKEFOURCC('A', 'R', '1', '6'), "D3DFMT_AL16"},
                                                         {(D3DFORMAT) MAKEFOURCC(' ', 'R', '1', '6'), "D3DFMT_R16"},
                                                         // L16 already exists as a dedicated format, but some
                                                         // games also query the below FOURCC for some reason...
                                                         {(D3DFORMAT) MAKEFOURCC(' ', 'L', '1', '6'), "D3DFMT_L16_FOURCC"},
                                                         // Obscure video formats used by some arcade game ports
                                                         {(D3DFORMAT) MAKEFOURCC('W', 'V', 'C', '1'), "D3DFMT_WVC1"},
                                                         {(D3DFORMAT) MAKEFOURCC('I', 'Y', 'U', 'V'), "D3DFMT_IYUV"},
                                                         {(D3DFORMAT) MAKEFOURCC('I', '4', '2', '0'), "D3DFMT_I420"} };

            std::map<D3DFORMAT, char const*>::iterator formatIter;

            Com<IDirect3DSurface9> surface;
            Com<IDirect3DTexture9> texture;
            Com<IDirect3DCubeTexture9> cubeTexture;
            Com<IDirect3DVolumeTexture9> volumeTexture;

            uint32_t supportedFormats = 0;

            if (!Usage)
                std::cout << std::endl << "Running texture format tests:" << std::endl;
            else if (Usage == D3DUSAGE_RENDERTARGET)
                std::cout << std::endl << "Running render target format tests:" << std::endl;
            else if (Usage == D3DUSAGE_DEPTHSTENCIL)
                std::cout << std::endl << "Running depth stencil format tests:" << std::endl;

            for (formatIter = formats.begin(); formatIter != formats.end(); ++formatIter) {
                D3DFORMAT surfaceFormat = formatIter->first;

                HRESULT statusSurface       = m_d3d->CheckDeviceFormat(0, D3DDEVTYPE_HAL, m_pp.BackBufferFormat, Usage, D3DRTYPE_SURFACE, surfaceFormat);
                HRESULT statusTexture       = m_d3d->CheckDeviceFormat(0, D3DDEVTYPE_HAL, m_pp.BackBufferFormat, Usage, D3DRTYPE_TEXTURE, surfaceFormat);
                HRESULT statusCubeTexture   = m_d3d->CheckDeviceFormat(0, D3DDEVTYPE_HAL, m_pp.BackBufferFormat, Usage, D3DRTYPE_CUBETEXTURE, surfaceFormat);
                HRESULT statusVolumeTexture = m_d3d->CheckDeviceFormat(0, D3DDEVTYPE_HAL, m_pp.BackBufferFormat, Usage, D3DRTYPE_VOLUMETEXTURE, surfaceFormat);

                char surfaceSupport = '-';
                char textureSupport = '-';
                char cubeTextureSupport = '-';
                char volumeTextureSupport = '-';

                if (SUCCEEDED(statusSurface))
                    surfaceSupport = '+';
                if (SUCCEEDED(statusTexture))
                    textureSupport = '+';
                if (SUCCEEDED(statusCubeTexture))
                    cubeTextureSupport = '+';
                if (SUCCEEDED(statusVolumeTexture))
                    volumeTextureSupport = '+';

                bool     hasFailures = false;
                uint32_t testedTypes = 1;
                uint32_t supportedTypes = 0;
                char     surfaceCreate = '~';
                char     textureCreate = '~';
                char     cubeTextureCreate = '~';
                char     volumeTextureCreate = '~';

                if (!Usage) {
                    // Note: CreateOffscreenPlainSurface calls on D3DPOOL_SCRATCH should never fail, even with unsupported surface formats
                    statusSurface = m_device->CreateOffscreenPlainSurface(256, 256, surfaceFormat, D3DPOOL_SCRATCH, &surface, NULL);
                } else if (Usage == D3DUSAGE_RENDERTARGET) {
                    statusSurface = m_device->CreateRenderTarget(256, 256, surfaceFormat, D3DMULTISAMPLE_NONE, 0, FALSE, &surface, NULL);
                } else if (Usage == D3DUSAGE_DEPTHSTENCIL) {
                    statusSurface = m_device->CreateDepthStencilSurface(256, 256, surfaceFormat, D3DMULTISAMPLE_NONE, 0, FALSE, &surface, NULL);
                }

                if (FAILED(statusSurface)) {
                    surfaceCreate = '-';
                } else {
                    supportedTypes++;
                    surfaceCreate = '+';
                    surface = nullptr;
                }

                if (SUCCEEDED(statusTexture)) {
                    testedTypes++;

                    statusTexture = m_device->CreateTexture(256, 256, 1, Usage, surfaceFormat, D3DPOOL_DEFAULT, &texture, NULL);

                    if (FAILED(statusTexture)) {
                        hasFailures = true;
                        textureCreate = '-';
                    } else {
                        supportedTypes++;
                        textureCreate = '+';
                        texture = nullptr;
                    }
                }

                if (SUCCEEDED(statusCubeTexture)) {
                    testedTypes++;

                    statusCubeTexture = m_device->CreateCubeTexture(256, 1, Usage, surfaceFormat, D3DPOOL_DEFAULT, &cubeTexture, NULL);

                    if (FAILED(statusCubeTexture)) {
                        hasFailures = true;
                        cubeTextureCreate = '-';
                    } else {
                        supportedTypes++;
                        cubeTextureCreate = '+';
                        cubeTexture = nullptr;
                    }
                }

                if (SUCCEEDED(statusVolumeTexture)) {
                    testedTypes++;

                    statusVolumeTexture = m_device->CreateVolumeTexture(256, 256, 256, 1, Usage, surfaceFormat, D3DPOOL_DEFAULT, &volumeTexture, NULL);

                    if (FAILED(statusVolumeTexture)) {
                        hasFailures = true;
                        volumeTextureCreate = '-';
                    } else {
                        supportedTypes++;
                        volumeTextureCreate = '+';
                        volumeTexture = nullptr;
                    }
                }

                if (hasFailures) {
                    std::cout << format("  ! The ", formatIter->second, " format is improperly supported") << std::endl;
                } else if (testedTypes > 2 && testedTypes - 1 <= supportedTypes) {
                    supportedFormats++;
                    std::cout << format("  + The ", formatIter->second, " format is fully supported") << std::endl;
                } else if (supportedTypes) {
                    supportedFormats++;
                    std::cout << format("  ~ The ", formatIter->second, " format is partially supported") << std::endl;
                } else {
                    std::cout << format("  - The ", formatIter->second, " format is not supported") << std::endl;
                }

                if (!Usage)
                    std::cout << format("      Surface: ", surfaceSupport, "/", surfaceCreate);
                else if (Usage == D3DUSAGE_RENDERTARGET)
                    std::cout << format("      RT: ", surfaceSupport, "/", surfaceCreate);
                else if (Usage == D3DUSAGE_DEPTHSTENCIL)
                    std::cout << format("      DS: ", surfaceSupport, "/", surfaceCreate);

                std::cout << format("  Texture: ", textureSupport, "/", textureCreate,
                                    "  CubeTexture: ", cubeTextureSupport, "/", cubeTextureCreate,
                                    "  VolumeTexture: ", volumeTextureSupport, "/", volumeTextureCreate) << std::endl;
            }

            std::cout << format("A total of ", supportedFormats, " formats are supported") << std::endl;
        }

        // Multisample support check
        void listMultisampleSupport(BOOL windowed) {
            std::map<D3DMULTISAMPLE_TYPE, char const*> msTypes = { {D3DMULTISAMPLE_NONE,        "D3DMULTISAMPLE_NONE"},
                                                                   {D3DMULTISAMPLE_NONMASKABLE, "D3DMULTISAMPLE_NONMASKABLE"},
                                                                   {D3DMULTISAMPLE_2_SAMPLES,   "D3DMULTISAMPLE_2_SAMPLES"},
                                                                   {D3DMULTISAMPLE_3_SAMPLES,   "D3DMULTISAMPLE_3_SAMPLES"},
                                                                   {D3DMULTISAMPLE_4_SAMPLES,   "D3DMULTISAMPLE_4_SAMPLES"},
                                                                   {D3DMULTISAMPLE_5_SAMPLES,   "D3DMULTISAMPLE_5_SAMPLES"},
                                                                   {D3DMULTISAMPLE_6_SAMPLES,   "D3DMULTISAMPLE_6_SAMPLES"},
                                                                   {D3DMULTISAMPLE_7_SAMPLES,   "D3DMULTISAMPLE_7_SAMPLES"},
                                                                   {D3DMULTISAMPLE_8_SAMPLES,   "D3DMULTISAMPLE_8_SAMPLES"},
                                                                   {D3DMULTISAMPLE_9_SAMPLES,   "D3DMULTISAMPLE_9_SAMPLES"},
                                                                   {D3DMULTISAMPLE_10_SAMPLES,  "D3DMULTISAMPLE_10_SAMPLES"},
                                                                   {D3DMULTISAMPLE_11_SAMPLES,  "D3DMULTISAMPLE_11_SAMPLES"},
                                                                   {D3DMULTISAMPLE_12_SAMPLES,  "D3DMULTISAMPLE_12_SAMPLES"},
                                                                   {D3DMULTISAMPLE_13_SAMPLES,  "D3DMULTISAMPLE_13_SAMPLES"},
                                                                   {D3DMULTISAMPLE_14_SAMPLES,  "D3DMULTISAMPLE_14_SAMPLES"},
                                                                   {D3DMULTISAMPLE_15_SAMPLES,  "D3DMULTISAMPLE_15_SAMPLES"},
                                                                   {D3DMULTISAMPLE_16_SAMPLES,  "D3DMULTISAMPLE_16_SAMPLES"} };

            std::map<D3DMULTISAMPLE_TYPE, char const*>::iterator msTypesIter;
            DWORD qualityLevels;

            std::cout << std::endl;
            if (windowed)
                std::cout << "Multisample type support (windowed):" << std::endl;
            else
                std::cout << "Multisample type support (full screen):" << std::endl;

            for (msTypesIter = msTypes.begin(); msTypesIter != msTypes.end(); ++msTypesIter) {
                qualityLevels = 0;

                HRESULT status = m_d3d->CheckDeviceMultiSampleType(0, D3DDEVTYPE_HAL, m_pp.BackBufferFormat, windowed, msTypesIter->first, &qualityLevels);

                if (FAILED(status)) {
                    std::cout << format("  - ", msTypesIter->second ," is not supported") << std::endl;
                } else {
                    std::cout << format("  + ", msTypesIter->second ," is supported") << std::endl;
                    std::cout << format("     ~ Quality levels: ", qualityLevels) << std::endl;
                }
            }
        }

        // Vendor format hacks support check
        void listVendorFormatHacksSupport() {
            D3DFORMAT fmtNULL = (D3DFORMAT) MAKEFOURCC('N', 'U', 'L', 'L');
            D3DFORMAT fmtRESZ = (D3DFORMAT) MAKEFOURCC('R', 'E', 'S', 'Z');
            D3DFORMAT fmtATOC = (D3DFORMAT) MAKEFOURCC('A', 'T', 'O', 'C');
            D3DFORMAT fmtSSAA = (D3DFORMAT) MAKEFOURCC('S', 'S', 'A', 'A');
            D3DFORMAT fmtNVDB = (D3DFORMAT) MAKEFOURCC('N', 'V', 'D', 'B');
            D3DFORMAT fmtR2VB = (D3DFORMAT) MAKEFOURCC('R', '2', 'V', 'B');
            D3DFORMAT fmtINST = (D3DFORMAT) MAKEFOURCC('I', 'N', 'S', 'T');
            // ATI/AMD and possibly Nvidia (alterate) pixel center hack
            D3DFORMAT fmtCENT = (D3DFORMAT) MAKEFOURCC('C', 'E', 'N', 'T');

            std::cout << std::endl << "Vendor hack format support:" << std::endl;

            HRESULT statusNULL = m_d3d->CheckDeviceFormat(0, D3DDEVTYPE_HAL, m_pp.BackBufferFormat, D3DUSAGE_RENDERTARGET, D3DRTYPE_SURFACE, fmtNULL);

            if (FAILED(statusNULL)) {
                std::cout << "  - The NULL render target format is not supported" << std::endl;
            } else {
                std::cout << "  + The NULL render target format is supported" << std::endl;
            }

            HRESULT statusRESZ = m_d3d->CheckDeviceFormat(0, D3DDEVTYPE_HAL, m_pp.BackBufferFormat, D3DUSAGE_RENDERTARGET, D3DRTYPE_SURFACE, fmtRESZ);

            if (FAILED(statusRESZ)) {
                std::cout << "  - The RESZ render target format is not supported" << std::endl;
            } else {
                std::cout << "  + The RESZ render target format is supported" << std::endl;
            }

            HRESULT statusATOC = m_d3d->CheckDeviceFormat(0, D3DDEVTYPE_HAL, m_pp.BackBufferFormat, 0, D3DRTYPE_SURFACE, fmtATOC);

            if (FAILED(statusATOC)) {
                std::cout << "  - The ATOC format is not supported" << std::endl;
            } else {
                std::cout << "  + The ATOC format is supported" << std::endl;
            }

            HRESULT statusSSAA = m_d3d->CheckDeviceFormat(0, D3DDEVTYPE_HAL, m_pp.BackBufferFormat, 0, D3DRTYPE_SURFACE, fmtSSAA);

            if (FAILED(statusSSAA)) {
                std::cout << "  - The SSAA format is not supported" << std::endl;
            } else {
                std::cout << "  + The SSAA format is supported" << std::endl;
            }

            HRESULT statusNVDB = m_d3d->CheckDeviceFormat(0, D3DDEVTYPE_HAL, m_pp.BackBufferFormat, 0, D3DRTYPE_SURFACE, fmtNVDB);

            if (FAILED(statusNVDB)) {
                std::cout << "  - The NVDB format is not supported" << std::endl;
            } else {
                std::cout << "  + The NVDB format is supported" << std::endl;
            }

            HRESULT statusR2VB = m_d3d->CheckDeviceFormat(0, D3DDEVTYPE_HAL, m_pp.BackBufferFormat, 0, D3DRTYPE_SURFACE, fmtR2VB);

            if (FAILED(statusR2VB)) {
                std::cout << "  - The R2VB format is not supported" << std::endl;
            } else {
                std::cout << "  + The R2VB format is supported" << std::endl;
            }

            HRESULT statusINST = m_d3d->CheckDeviceFormat(0, D3DDEVTYPE_HAL, m_pp.BackBufferFormat, 0, D3DRTYPE_SURFACE, fmtINST);

            if (FAILED(statusINST)) {
                std::cout << "  - The INST format is not supported" << std::endl;
            } else {
                std::cout << "  + The INST format is supported" << std::endl;
            }

            HRESULT statusCENT = m_d3d->CheckDeviceFormat(0, D3DDEVTYPE_HAL, m_pp.BackBufferFormat, 0, D3DRTYPE_SURFACE, fmtCENT);

            if (FAILED(statusCENT)) {
                std::cout << "  - The CENT format is not supported" << std::endl;
            } else {
                std::cout << "  + The CENT format is supported" << std::endl;
            }
        }

        // D3D Device capabilities check
        void listDeviceCapabilities() {
            D3DCAPS9 caps9SWVP;
            D3DCAPS9 caps9HWVP;
            D3DCAPS9 caps9;

            // these are all the possible D3D9 VS versions
            std::map<DWORD, char const*> vsVersion = { {D3DVS_VERSION(0,0), "0.0"},
                                                       {D3DVS_VERSION(1,0), "1.0"},
                                                       {D3DVS_VERSION(1,1), "1.1"},
                                                       {D3DVS_VERSION(2,0), "2.0"},
                                                       {D3DVS_VERSION(3,0), "3.0"} };

            // these are all the possible D3D9 PS versions
            std::map<DWORD, char const*> psVersion = { {D3DPS_VERSION(0,0), "0.0"},
                                                       {D3DPS_VERSION(1,0), "1.0"},
                                                       {D3DPS_VERSION(1,1), "1.1"},
                                                       {D3DPS_VERSION(1,2), "1.2"},
                                                       {D3DPS_VERSION(1,3), "1.3"},
                                                       {D3DPS_VERSION(1,4), "1.4"},
                                                       {D3DPS_VERSION(2,0), "2.0"},
                                                       {D3DPS_VERSION(3,0), "3.0"} };

            // get the capabilities from the D3D device in SWVP mode
            createDeviceWithFlags(&m_pp, D3DCREATE_SOFTWARE_VERTEXPROCESSING, D3DDEVTYPE_HAL, true);
            m_device->GetDeviceCaps(&caps9SWVP);

            // get the capabilities from the D3D device in HWVP mode
            createDeviceWithFlags(&m_pp, D3DCREATE_HARDWARE_VERTEXPROCESSING, D3DDEVTYPE_HAL, true);
            m_device->GetDeviceCaps(&caps9HWVP);

            // get the capabilities from the D3D interface
            m_d3d->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &caps9);

            std::cout << std::endl << "Listing device capabilities support:" << std::endl;

            if (caps9.Caps2 & D3DCAPS2_CANAUTOGENMIPMAP)
                std::cout << "  + D3DCAPS2_CANAUTOGENMIPMAP is supported" << std::endl;
            else
                std::cout << "  - D3DCAPS2_CANAUTOGENMIPMAP is not supported" << std::endl;

            if (caps9.DevCaps & D3DDEVCAPS_QUINTICRTPATCHES)
                std::cout << "  + D3DDEVCAPS_QUINTICRTPATCHES is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_QUINTICRTPATCHES is not supported" << std::endl;

            if (caps9.DevCaps & D3DDEVCAPS_RTPATCHES)
                std::cout << "  + D3DDEVCAPS_RTPATCHES is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_RTPATCHES is not supported" << std::endl;

            if (caps9.DevCaps & D3DDEVCAPS_RTPATCHHANDLEZERO)
                std::cout << "  + D3DDEVCAPS_RTPATCHHANDLEZERO is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_RTPATCHHANDLEZERO is not supported" << std::endl;

            if (caps9.DevCaps & D3DDEVCAPS_NPATCHES)
                std::cout << "  + D3DDEVCAPS_NPATCHES is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_NPATCHES is not supported" << std::endl;

            if (caps9.Caps3 & D3DCAPS3_LINEAR_TO_SRGB_PRESENTATION)
                std::cout << "  + D3DCAPS3_LINEAR_TO_SRGB_PRESENTATION is supported" << std::endl;
            else
                std::cout << "  - D3DCAPS3_LINEAR_TO_SRGB_PRESENTATION is not supported" << std::endl;

            if (caps9.Caps3 & D3DCAPS3_COPY_TO_VIDMEM)
                std::cout << "  + D3DCAPS3_COPY_TO_VIDMEM is supported" << std::endl;
            else
                std::cout << "  - D3DCAPS3_COPY_TO_VIDMEM is not supported" << std::endl;

            if (caps9.Caps3 & D3DCAPS3_COPY_TO_SYSTEMMEM)
                std::cout << "  + D3DCAPS3_COPY_TO_SYSTEMMEM is supported" << std::endl;
            else
                std::cout << "  - D3DCAPS3_COPY_TO_SYSTEMMEM is not supported" << std::endl;

            if (caps9.PrimitiveMiscCaps & D3DPMISCCAPS_INDEPENDENTWRITEMASKS)
                std::cout << "  + D3DPMISCCAPS_INDEPENDENTWRITEMASKS is supported" << std::endl;
            else
                std::cout << "  - D3DPMISCCAPS_INDEPENDENTWRITEMASKS is not supported" << std::endl;

            if (caps9.PrimitiveMiscCaps & D3DPMISCCAPS_PERSTAGECONSTANT)
                std::cout << "  + D3DPMISCCAPS_PERSTAGECONSTANT is supported" << std::endl;
            else
                std::cout << "  - D3DPMISCCAPS_PERSTAGECONSTANT is not supported" << std::endl;

            if (caps9.PrimitiveMiscCaps & D3DPMISCCAPS_FOGANDSPECULARALPHA)
                std::cout << "  + D3DPMISCCAPS_FOGANDSPECULARALPHA is supported" << std::endl;
            else
                std::cout << "  - D3DPMISCCAPS_FOGANDSPECULARALPHA is not supported" << std::endl;

            if (caps9.PrimitiveMiscCaps & D3DPMISCCAPS_SEPARATEALPHABLEND)
                std::cout << "  + D3DPMISCCAPS_SEPARATEALPHABLEND is supported" << std::endl;
            else
                std::cout << "  - D3DPMISCCAPS_SEPARATEALPHABLEND is not supported" << std::endl;

            if (caps9.PrimitiveMiscCaps & D3DPMISCCAPS_MRTINDEPENDENTBITDEPTHS)
                std::cout << "  + D3DPMISCCAPS_MRTINDEPENDENTBITDEPTHS is supported" << std::endl;
            else
                std::cout << "  - D3DPMISCCAPS_MRTINDEPENDENTBITDEPTHS is not supported" << std::endl;

            if (caps9.PrimitiveMiscCaps & D3DPMISCCAPS_MRTPOSTPIXELSHADERBLENDING)
                std::cout << "  + D3DPMISCCAPS_MRTPOSTPIXELSHADERBLENDING is supported" << std::endl;
            else
                std::cout << "  - D3DPMISCCAPS_MRTPOSTPIXELSHADERBLENDING is not supported" << std::endl;

            if (caps9.PrimitiveMiscCaps & D3DPMISCCAPS_FOGVERTEXCLAMPED)
                std::cout << "  + D3DPMISCCAPS_FOGVERTEXCLAMPED is supported" << std::endl;
            else
                std::cout << "  - D3DPMISCCAPS_FOGVERTEXCLAMPED is not supported" << std::endl;

            if (caps9.PrimitiveMiscCaps & D3DPMISCCAPS_POSTBLENDSRGBCONVERT)
                std::cout << "  + D3DPMISCCAPS_POSTBLENDSRGBCONVERT is supported" << std::endl;
            else
                std::cout << "  - D3DPMISCCAPS_POSTBLENDSRGBCONVERT is not supported" << std::endl;

            if (caps9.RasterCaps & D3DPRASTERCAPS_SCISSORTEST)
                std::cout << "  + D3DPRASTERCAPS_SCISSORTEST is supported" << std::endl;
            else
                std::cout << "  - D3DPRASTERCAPS_SCISSORTEST is not supported" << std::endl;

            if (caps9.RasterCaps & D3DPRASTERCAPS_SLOPESCALEDEPTHBIAS)
                std::cout << "  + D3DPRASTERCAPS_SLOPESCALEDEPTHBIAS is supported" << std::endl;
            else
                std::cout << "  - D3DPRASTERCAPS_SLOPESCALEDEPTHBIAS is not supported" << std::endl;

            if (caps9.RasterCaps & D3DPRASTERCAPS_DEPTHBIAS)
                std::cout << "  + D3DPRASTERCAPS_DEPTHBIAS is supported" << std::endl;
            else
                std::cout << "  - D3DPRASTERCAPS_DEPTHBIAS is not supported" << std::endl;

            if (caps9.RasterCaps & D3DPRASTERCAPS_MULTISAMPLE_TOGGLE)
                std::cout << "  + D3DPRASTERCAPS_MULTISAMPLE_TOGGLE is supported" << std::endl;
            else
                std::cout << "  - D3DPRASTERCAPS_MULTISAMPLE_TOGGLE is not supported" << std::endl;

            if (caps9.SrcBlendCaps & D3DPBLENDCAPS_BLENDFACTOR)
                std::cout << "  + D3DPBLENDCAPS_BLENDFACTOR (Src) is supported" << std::endl;
            else
                std::cout << "  - D3DPBLENDCAPS_BLENDFACTOR (Src) is not supported" << std::endl;

            if (caps9.SrcBlendCaps & D3DPBLENDCAPS_INVSRCCOLOR2)
                std::cout << "  + D3DPBLENDCAPS_INVSRCCOLOR2 (Src) is supported" << std::endl;
            else
                std::cout << "  - D3DPBLENDCAPS_INVSRCCOLOR2 (Src) is not supported" << std::endl;

            if (caps9.SrcBlendCaps & D3DPBLENDCAPS_SRCCOLOR2)
                std::cout << "  + D3DPBLENDCAPS_SRCCOLOR2 (Src) is supported" << std::endl;
            else
                std::cout << "  - D3DPBLENDCAPS_SRCCOLOR2 (Src) is not supported" << std::endl;

            if (caps9.DestBlendCaps & D3DPBLENDCAPS_BLENDFACTOR)
                std::cout << "  + D3DPBLENDCAPS_BLENDFACTOR (Dest) is supported" << std::endl;
            else
                std::cout << "  - D3DPBLENDCAPS_BLENDFACTOR (Dest) is not supported" << std::endl;

            if (caps9.DestBlendCaps & D3DPBLENDCAPS_INVSRCCOLOR2)
                std::cout << "  + D3DPBLENDCAPS_INVSRCCOLOR2 (Dest) is supported" << std::endl;
            else
                std::cout << "  - D3DPBLENDCAPS_INVSRCCOLOR2 (Dest) is not supported" << std::endl;

            if (caps9.DestBlendCaps & D3DPBLENDCAPS_SRCCOLOR2)
                std::cout << "  + D3DPBLENDCAPS_SRCCOLOR2 (Dest) is supported" << std::endl;
            else
                std::cout << "  - D3DPBLENDCAPS_SRCCOLOR2 (Dest) is not supported" << std::endl;

            if (caps9.TextureFilterCaps & D3DPTFILTERCAPS_CONVOLUTIONMONO)
                std::cout << "  + D3DPTFILTERCAPS_CONVOLUTIONMONO is (texture) supported" << std::endl;
            else
                std::cout << "  - D3DPTFILTERCAPS_CONVOLUTIONMONO is (texture) not supported" << std::endl;

            if (caps9.TextureFilterCaps & D3DPTFILTERCAPS_MAGFPYRAMIDALQUAD)
                std::cout << "  + D3DPTFILTERCAPS_MAGFPYRAMIDALQUAD is (texture) supported" << std::endl;
            else
                std::cout << "  - D3DPTFILTERCAPS_MAGFPYRAMIDALQUAD is (texture) not supported" << std::endl;

            if (caps9.TextureFilterCaps & D3DPTFILTERCAPS_MAGFGAUSSIANQUAD)
                std::cout << "  + D3DPTFILTERCAPS_MAGFGAUSSIANQUAD (texture) is supported" << std::endl;
            else
                std::cout << "  - D3DPTFILTERCAPS_MAGFGAUSSIANQUAD (texture) is not supported" << std::endl;

            if (caps9.TextureFilterCaps & D3DPTFILTERCAPS_MINFPYRAMIDALQUAD)
                std::cout << "  + D3DPTFILTERCAPS_MINFPYRAMIDALQUAD is (texture) supported" << std::endl;
            else
                std::cout << "  - D3DPTFILTERCAPS_MINFPYRAMIDALQUAD is (texture) not supported" << std::endl;

            if (caps9.TextureFilterCaps & D3DPTFILTERCAPS_MINFGAUSSIANQUAD)
                std::cout << "  + D3DPTFILTERCAPS_MINFGAUSSIANQUAD (texture) is supported" << std::endl;
            else
                std::cout << "  - D3DPTFILTERCAPS_MINFGAUSSIANQUAD (texture) is not supported" << std::endl;

            if (caps9.CubeTextureFilterCaps & D3DPTFILTERCAPS_CONVOLUTIONMONO)
                std::cout << "  + D3DPTFILTERCAPS_CONVOLUTIONMONO is (cube texture) supported" << std::endl;
            else
                std::cout << "  - D3DPTFILTERCAPS_CONVOLUTIONMONO is (cube texture) not supported" << std::endl;

            if (caps9.CubeTextureFilterCaps & D3DPTFILTERCAPS_MAGFPYRAMIDALQUAD)
                std::cout << "  + D3DPTFILTERCAPS_MAGFPYRAMIDALQUAD is (cube texture) supported" << std::endl;
            else
                std::cout << "  - D3DPTFILTERCAPS_MAGFPYRAMIDALQUAD is (cube texture) not supported" << std::endl;

            if (caps9.CubeTextureFilterCaps & D3DPTFILTERCAPS_MAGFGAUSSIANQUAD)
                std::cout << "  + D3DPTFILTERCAPS_MAGFGAUSSIANQUAD (cube texture) is supported" << std::endl;
            else
                std::cout << "  - D3DPTFILTERCAPS_MAGFGAUSSIANQUAD (cube texture) is not supported" << std::endl;

            if (caps9.CubeTextureFilterCaps & D3DPTFILTERCAPS_MINFPYRAMIDALQUAD)
                std::cout << "  + D3DPTFILTERCAPS_MINFPYRAMIDALQUAD is (cube texture) supported" << std::endl;
            else
                std::cout << "  - D3DPTFILTERCAPS_MINFPYRAMIDALQUAD is (cube texture) not supported" << std::endl;

            if (caps9.CubeTextureFilterCaps & D3DPTFILTERCAPS_MINFGAUSSIANQUAD)
                std::cout << "  + D3DPTFILTERCAPS_MINFGAUSSIANQUAD (cube texture) is supported" << std::endl;
            else
                std::cout << "  - D3DPTFILTERCAPS_MINFGAUSSIANQUAD (cube texture) is not supported" << std::endl;

            if (caps9.VolumeTextureFilterCaps & D3DPTFILTERCAPS_CONVOLUTIONMONO)
                std::cout << "  + D3DPTFILTERCAPS_CONVOLUTIONMONO is (volume texture) supported" << std::endl;
            else
                std::cout << "  - D3DPTFILTERCAPS_CONVOLUTIONMONO is (volume texture) not supported" << std::endl;

            if (caps9.VolumeTextureFilterCaps & D3DPTFILTERCAPS_MAGFPYRAMIDALQUAD)
                std::cout << "  + D3DPTFILTERCAPS_MAGFPYRAMIDALQUAD is (volume texture) supported" << std::endl;
            else
                std::cout << "  - D3DPTFILTERCAPS_MAGFPYRAMIDALQUAD is (volume texture) not supported" << std::endl;

            if (caps9.VolumeTextureFilterCaps & D3DPTFILTERCAPS_MAGFGAUSSIANQUAD)
                std::cout << "  + D3DPTFILTERCAPS_MAGFGAUSSIANQUAD (volume texture) is supported" << std::endl;
            else
                std::cout << "  - D3DPTFILTERCAPS_MAGFGAUSSIANQUAD (volume texture) is not supported" << std::endl;

            if (caps9.VolumeTextureFilterCaps & D3DPTFILTERCAPS_MINFPYRAMIDALQUAD)
                std::cout << "  + D3DPTFILTERCAPS_MINFPYRAMIDALQUAD is (volume texture) supported" << std::endl;
            else
                std::cout << "  - D3DPTFILTERCAPS_MINFPYRAMIDALQUAD is (volume texture) not supported" << std::endl;

            if (caps9.VolumeTextureFilterCaps & D3DPTFILTERCAPS_MINFGAUSSIANQUAD)
                std::cout << "  + D3DPTFILTERCAPS_MINFGAUSSIANQUAD (volume texture) is supported" << std::endl;
            else
                std::cout << "  - D3DPTFILTERCAPS_MINFGAUSSIANQUAD (volume texture) is not supported" << std::endl;

            if (caps9.LineCaps & D3DLINECAPS_ANTIALIAS)
                std::cout << "  + D3DLINECAPS_ANTIALIAS is supported" << std::endl;
            else
                std::cout << "  - D3DLINECAPS_ANTIALIAS is not supported" << std::endl;

            if (caps9.StencilCaps & D3DSTENCILCAPS_TWOSIDED)
                std::cout << "  + D3DSTENCILCAPS_TWOSIDED is supported" << std::endl;
            else
                std::cout << "  - D3DSTENCILCAPS_TWOSIDED is not supported" << std::endl;

            if (caps9.VertexProcessingCaps & D3DVTXPCAPS_TEXGEN_SPHEREMAP)
                std::cout << "  + D3DVTXPCAPS_TEXGEN_SPHEREMAP is supported" << std::endl;
            else
                std::cout << "  - D3DVTXPCAPS_TEXGEN_SPHEREMAP is not supported" << std::endl;

            if (caps9.VertexProcessingCaps & D3DVTXPCAPS_NO_TEXGEN_NONLOCALVIEWER)
                std::cout << "  + D3DVTXPCAPS_NO_TEXGEN_NONLOCALVIEWER is supported" << std::endl;
            else
                std::cout << "  - D3DVTXPCAPS_NO_TEXGEN_NONLOCALVIEWER is not supported" << std::endl;

            if (caps9.VS20Caps.Caps & D3DVS20CAPS_PREDICATION)
                std::cout << "  + D3DVS20CAPS_PREDICATION is supported" << std::endl;
            else
                std::cout << "  - D3DVS20CAPS_PREDICATION is not supported" << std::endl;

            if (caps9.PS20Caps.Caps & D3DPS20CAPS_ARBITRARYSWIZZLE)
                std::cout << "  + D3DPS20CAPS_ARBITRARYSWIZZLE is supported" << std::endl;
            else
                std::cout << "  - D3DPS20CAPS_ARBITRARYSWIZZLE is not supported" << std::endl;

            if (caps9.PS20Caps.Caps & D3DPS20CAPS_GRADIENTINSTRUCTIONS)
                std::cout << "  + D3DPS20CAPS_GRADIENTINSTRUCTIONS is supported" << std::endl;
            else
                std::cout << "  - D3DPS20CAPS_GRADIENTINSTRUCTIONS is not supported" << std::endl;

            if (caps9.PS20Caps.Caps & D3DPS20CAPS_PREDICATION)
                std::cout << "  + D3DPS20CAPS_PREDICATION is supported" << std::endl;
            else
                std::cout << "  - D3DPS20CAPS_PREDICATION is not supported" << std::endl;

            if (caps9.PS20Caps.Caps & D3DPS20CAPS_NODEPENDENTREADLIMIT)
                std::cout << "  + D3DPS20CAPS_NODEPENDENTREADLIMIT is supported" << std::endl;
            else
                std::cout << "  - D3DPS20CAPS_NODEPENDENTREADLIMIT is not supported" << std::endl;

            if (caps9.PS20Caps.Caps & D3DPS20CAPS_NOTEXINSTRUCTIONLIMIT)
                std::cout << "  + D3DPS20CAPS_NOTEXINSTRUCTIONLIMIT is supported" << std::endl;
            else
                std::cout << "  - D3DPS20CAPS_NOTEXINSTRUCTIONLIMIT is not supported" << std::endl;

            std::cout << std::endl << "Listing VertexTextureFilterCaps support:" << std::endl;

            if (caps9.VertexTextureFilterCaps & D3DPTFILTERCAPS_CONVOLUTIONMONO)
                std::cout << "  + D3DPTFILTERCAPS_CONVOLUTIONMONO is supported" << std::endl;
            else
                std::cout << "  - D3DPTFILTERCAPS_CONVOLUTIONMONO is not supported" << std::endl;

            if (caps9.VertexTextureFilterCaps & D3DPTFILTERCAPS_MAGFPOINT)
                std::cout << "  + D3DPTFILTERCAPS_MAGFPOINT is supported" << std::endl;
            else
                std::cout << "  - D3DPTFILTERCAPS_MAGFPOINT is not supported" << std::endl;

            if (caps9.VertexTextureFilterCaps & D3DPTFILTERCAPS_MAGFLINEAR)
                std::cout << "  + D3DPTFILTERCAPS_MAGFLINEAR is supported" << std::endl;
            else
                std::cout << "  - D3DPTFILTERCAPS_MAGFLINEAR is not supported" << std::endl;

            if (caps9.VertexTextureFilterCaps & D3DPTFILTERCAPS_MAGFANISOTROPIC)
                std::cout << "  + D3DPTFILTERCAPS_MAGFANISOTROPIC is supported" << std::endl;
            else
                std::cout << "  - D3DPTFILTERCAPS_MAGFANISOTROPIC is not supported" << std::endl;

            if (caps9.VertexTextureFilterCaps & D3DPTFILTERCAPS_MAGFPYRAMIDALQUAD)
                std::cout << "  + D3DPTFILTERCAPS_MAGFPYRAMIDALQUAD is supported" << std::endl;
            else
                std::cout << "  - D3DPTFILTERCAPS_MAGFPYRAMIDALQUAD is not supported" << std::endl;

            if (caps9.VertexTextureFilterCaps & D3DPTFILTERCAPS_MAGFGAUSSIANQUAD)
                std::cout << "  + D3DPTFILTERCAPS_MAGFGAUSSIANQUAD is supported" << std::endl;
            else
                std::cout << "  - D3DPTFILTERCAPS_MAGFGAUSSIANQUAD is not supported" << std::endl;

            if (caps9.VertexTextureFilterCaps & D3DPTFILTERCAPS_MINFPOINT)
                std::cout << "  + D3DPTFILTERCAPS_MINFPOINT is supported" << std::endl;
            else
                std::cout << "  - D3DPTFILTERCAPS_MINFPOINT is not supported" << std::endl;

            if (caps9.VertexTextureFilterCaps & D3DPTFILTERCAPS_MINFLINEAR)
                std::cout << "  + D3DPTFILTERCAPS_MINFLINEAR is supported" << std::endl;
            else
                std::cout << "  - D3DPTFILTERCAPS_MINFLINEAR is not supported" << std::endl;

            if (caps9.VertexTextureFilterCaps & D3DPTFILTERCAPS_MINFANISOTROPIC)
                std::cout << "  + D3DPTFILTERCAPS_MINFANISOTROPIC is supported" << std::endl;
            else
                std::cout << "  - D3DPTFILTERCAPS_MINFANISOTROPIC is not supported" << std::endl;

            if (caps9.VertexTextureFilterCaps & D3DPTFILTERCAPS_MINFPYRAMIDALQUAD)
                std::cout << "  + D3DPTFILTERCAPS_MINFPYRAMIDALQUAD is supported" << std::endl;
            else
                std::cout << "  - D3DPTFILTERCAPS_MINFPYRAMIDALQUAD is not supported" << std::endl;

            if (caps9.VertexTextureFilterCaps & D3DPTFILTERCAPS_MINFGAUSSIANQUAD)
                std::cout << "  + D3DPTFILTERCAPS_MINFGAUSSIANQUAD is supported" << std::endl;
            else
                std::cout << "  - D3DPTFILTERCAPS_MINFGAUSSIANQUAD is not supported" << std::endl;

            if (caps9.VertexTextureFilterCaps & D3DPTFILTERCAPS_MIPFPOINT)
                std::cout << "  + D3DPTFILTERCAPS_MIPFPOINT is supported" << std::endl;
            else
                std::cout << "  - D3DPTFILTERCAPS_MIPFPOINT is not supported" << std::endl;

            if (caps9.VertexTextureFilterCaps & D3DPTFILTERCAPS_MIPFLINEAR)
                std::cout << "  + D3DPTFILTERCAPS_MIPFLINEAR is supported" << std::endl;
            else
                std::cout << "  - D3DPTFILTERCAPS_MIPFLINEAR is not supported" << std::endl;

            std::cout << std::endl << "Listing device capability limits:" << std::endl;

            std::cout << format("  ~ MaxTextureWidth: ", caps9.MaxTextureWidth) << std::endl;
            std::cout << format("  ~ MaxTextureHeight: ", caps9.MaxTextureHeight) << std::endl;
            std::cout << format("  ~ MaxVolumeExtent: ", caps9.MaxVolumeExtent) << std::endl;
            std::cout << format("  ~ MaxTextureRepeat: ", caps9.MaxTextureRepeat) << std::endl;
            std::cout << format("  ~ MaxTextureAspectRatio: ", caps9.MaxTextureAspectRatio) << std::endl;
            std::cout << format("  ~ MaxAnisotropy: ", caps9.MaxAnisotropy) << std::endl;
            std::cout << format("  ~ MaxVertexW: ", caps9.MaxVertexW) << std::endl;
            std::cout << format("  ~ GuardBandLeft: ", caps9.GuardBandLeft) << std::endl;
            std::cout << format("  ~ GuardBandTop: ", caps9.GuardBandTop) << std::endl;
            std::cout << format("  ~ GuardBandRight: ", caps9.GuardBandRight) << std::endl;
            std::cout << format("  ~ GuardBandBottom: ", caps9.GuardBandBottom) << std::endl;
            std::cout << format("  ~ ExtentsAdjust: ", caps9.ExtentsAdjust) << std::endl;
            std::cout << format("  ~ MaxTextureBlendStages: ", caps9.MaxTextureBlendStages) << std::endl;
            std::cout << format("  ~ MaxSimultaneousTextures: ", caps9.MaxSimultaneousTextures) << std::endl;
            // may vary between interface and device modes (SWVP or HWVP)
            std::cout << format("  ~ MaxActiveLights: ", caps9.MaxActiveLights, " (I), ", caps9SWVP.MaxActiveLights,
                                " (SWVP), ", caps9HWVP.MaxActiveLights, " (HWVP)") << std::endl;
            // may vary between interface and device modes (SWVP or HWVP)
            std::cout << format("  ~ MaxUserClipPlanes: ", caps9.MaxUserClipPlanes, " (I), ", caps9SWVP.MaxUserClipPlanes,
                                " (SWVP), ", caps9HWVP.MaxUserClipPlanes, " (HWVP)") << std::endl;
            // may vary between interface and device modes (SWVP or HWVP)
            std::cout << format("  ~ MaxVertexBlendMatrices: ", caps9.MaxVertexBlendMatrices, " (I), ", caps9SWVP.MaxVertexBlendMatrices,
                                " (SWVP), ", caps9HWVP.MaxVertexBlendMatrices, " (HWVP)") << std::endl;
            // may vary between interface and device modes (SWVP or HWVP)
            std::cout << format("  ~ MaxVertexBlendMatrixIndex: ", caps9.MaxVertexBlendMatrixIndex, " (I), ", caps9SWVP.MaxVertexBlendMatrixIndex,
                                " (SWVP), ", caps9HWVP.MaxVertexBlendMatrixIndex, " (HWVP)") << std::endl;
            std::cout << format("  ~ MaxPointSize: ", caps9.MaxPointSize) << std::endl;
            std::cout << format("  ~ MaxPrimitiveCount: ", caps9.MaxPrimitiveCount) << std::endl;
            std::cout << format("  ~ MaxVertexIndex: ", caps9.MaxVertexIndex) << std::endl;
            std::cout << format("  ~ MaxStreams: ", caps9.MaxStreams) << std::endl;
            std::cout << format("  ~ MaxStreamStride: ", caps9.MaxStreamStride) << std::endl;
            std::cout << format("  ~ VertexShaderVersion: ", vsVersion[caps9.VertexShaderVersion]) << std::endl;
            std::cout << format("  ~ MaxVertexShaderConst: ", caps9.MaxVertexShaderConst) << std::endl;
            std::cout << format("  ~ PixelShaderVersion: ", psVersion[caps9.PixelShaderVersion]) << std::endl;
            // typically FLT_MAX
            std::cout << format("  ~ PixelShader1xMaxValue: ", caps9.PixelShader1xMaxValue) << std::endl;
            std::cout << format("  ~ MaxNpatchTessellationLevel: ", caps9.MaxNpatchTessellationLevel) << std::endl;
            std::cout << format("  ~ NumSimultaneousRTs: ", caps9.NumSimultaneousRTs) << std::endl;
            std::cout << format("  ~ VS20Caps.DynamicFlowControlDepth: ", caps9.VS20Caps.DynamicFlowControlDepth) << std::endl;
            std::cout << format("  ~ VS20Caps.NumTemps: ", caps9.VS20Caps.NumTemps) << std::endl;
            std::cout << format("  ~ VS20Caps.StaticFlowControlDepth: ", caps9.VS20Caps.StaticFlowControlDepth) << std::endl;
            std::cout << format("  ~ PS20Caps.DynamicFlowControlDepth: ", caps9.PS20Caps.DynamicFlowControlDepth) << std::endl;
            std::cout << format("  ~ PS20Caps.NumTemps: ", caps9.PS20Caps.NumTemps) << std::endl;
            std::cout << format("  ~ PS20Caps.StaticFlowControlDepth: ", caps9.PS20Caps.StaticFlowControlDepth) << std::endl;
            std::cout << format("  ~ PS20Caps.NumInstructionSlots: ", caps9.PS20Caps.NumInstructionSlots) << std::endl;
            std::cout << format("  ~ MaxVShaderInstructionsExecuted: ", caps9.MaxVShaderInstructionsExecuted) << std::endl;
            std::cout << format("  ~ MaxPShaderInstructionsExecuted: ", caps9.MaxPShaderInstructionsExecuted) << std::endl;
            std::cout << format("  ~ MaxVertexShader30InstructionSlots: ", caps9.MaxVertexShader30InstructionSlots) << std::endl;
            std::cout << format("  ~ MaxPixelShader30InstructionSlots: ", caps9.MaxPixelShader30InstructionSlots) << std::endl;
        }

        void listVCacheQueryResult() {
            resetOrRecreateDevice();

            Com<IDirect3DQuery9> query;
            D3DDEVINFO_VCACHE vCache;

            HRESULT status = m_device->CreateQuery(D3DQUERYTYPE_VCACHE, &query);
            if (SUCCEEDED(status)) {
                status = query->GetData(&vCache, sizeof(D3DDEVINFO_VCACHE), D3DQUERYTYPE_VCACHE);
            }

            std::cout << std::endl << "Listing VCache query result:" << std::endl;

            switch (status) {
                // NVIDIA will return D3D_OK and some values in vCache
                case D3D_OK:
                    std::cout << "  ~ Response: D3D_OK" << std::endl;
                    break;
                // AMD/ATI and Intel will return D3DERR_NOTAVAILABLE
                case D3DERR_NOTAVAILABLE:
                    std::cout << "  ~ Response: D3DERR_NOTAVAILABLE" << std::endl;
                    break;
                // shouldn't be returned by any vendor
                case D3DERR_INVALIDCALL:
                    std::cout << "  ~ Response: D3DERR_INVALIDCALL" << std::endl;
                    break;
                default:
                    std::cout << "  ~ Response: UNKNOWN" << std::endl;
                    break;
            }

            if (SUCCEEDED(status)) {
                char pattern[] = "\0\0\0\0";
                if (vCache.Pattern == 0) {
                    pattern[0] = '0';
                } else {
                    for (UINT i = 0; i < 4; i++) {
                        pattern[i] = char(vCache.Pattern >> i * 8);
                    }
                }

                std::cout << format("  ~ Pattern: ", pattern) << std::endl;
                std::cout << format("  ~ OptMethod: ", vCache.OptMethod) << std::endl;
                std::cout << format("  ~ CacheSize: ", vCache.CacheSize) << std::endl;
                std::cout << format("  ~ MagicNumber: ", vCache.MagicNumber) << std::endl;
            }
        }

        void listAvailableTextureMemory() {
            resetOrRecreateDevice();

            std::cout << std::endl << "Listing available texture memory:" << std::endl;

            uint32_t availableMemory = m_device->GetAvailableTextureMem();

            std::cout << format("  ~ Bytes: ", availableMemory) << std::endl;
        }

        void startTests() {
            m_totalTests  = 0;
            m_passedTests = 0;

            std::cout << std::endl << "Running D3D9 tests:" << std::endl;
        }

        // GetBackBuffer test (this shouldn't fail even with BackBufferCount set to 0)
        void testZeroBackBufferCount() {
            resetOrRecreateDevice();

            Com<IDirect3DSurface9> bbSurface;

            m_totalTests++;

            HRESULT status = m_device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &bbSurface);
            if (FAILED(status)) {
                std::cout << "  - The GetBackBuffer test has failed" << std::endl;
            } else {
                m_passedTests++;
                std::cout << "  + The GetBackBuffer test has passed" << std::endl;
            }
        }

        // Invalid presentation interval test on a windowed swapchain
        void testInvalidPresentationInterval() {
            D3DPRESENT_PARAMETERS piPP;

            m_totalTests++;

            memcpy(&piPP, &m_pp, sizeof(m_pp));
            piPP.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

            HRESULT statusImmediate = createDeviceWithFlags(&piPP, D3DCREATE_HARDWARE_VERTEXPROCESSING, D3DDEVTYPE_HAL, false);

            memcpy(&piPP, &m_pp, sizeof(m_pp));
            piPP.PresentationInterval = D3DPRESENT_INTERVAL_ONE;

            HRESULT statusOne = createDeviceWithFlags(&piPP, D3DCREATE_HARDWARE_VERTEXPROCESSING, D3DDEVTYPE_HAL, false);

            memcpy(&piPP, &m_pp, sizeof(m_pp));
            piPP.PresentationInterval = D3DPRESENT_INTERVAL_TWO;

            HRESULT statusTwo = createDeviceWithFlags(&piPP, D3DCREATE_HARDWARE_VERTEXPROCESSING, D3DDEVTYPE_HAL, false);

            if (SUCCEEDED(statusImmediate) && SUCCEEDED(statusOne) && FAILED(statusTwo)) {
                m_passedTests++;
                std::cout << "  + The invalid presentation interval test has passed" << std::endl;
            } else {
                std::cout << "  - The invalid presentation interval test has failed" << std::endl;
            }
        }

        // BeginScene & Reset test
        void testBeginSceneReset() {
            resetOrRecreateDevice();

            m_totalTests++;

            if (SUCCEEDED(m_device->BeginScene())) {
                HRESULT status = m_device->Reset(&m_pp);
                if (FAILED(status)) {
                    std::cout << "  - The BeginScene & Reset test has failed on Reset()" << std::endl;
                }
                else {
                    // Reset() will have cleared the state
                    if (FAILED(m_device->EndScene()) && SUCCEEDED(m_device->BeginScene())) {
                        m_passedTests++;
                        std::cout << "  + The BeginScene & Reset test has passed" << std::endl;
                    } else {
                        std::cout << "  - The BeginScene & Reset test has failed" << std::endl;
                    }
                }
            } else {
                throw Error("Failed to begin D3D9 scene");
            }

            // optimistic, expected to fail if the test fails
            m_device->EndScene();
        }

        // D3DCREATE_PUREDEVICE only with D3DCREATE_HARDWARE_VERTEXPROCESSING test
        void testPureDeviceOnlyWithHWVP() {
            // native drivers will fail to create a device with D3DCREATE_PUREDEVICE unless
            // it is combined together with D3DCREATE_HARDWARE_VERTEXPROCESSING
            HRESULT statusHWVP = createDeviceWithFlags(&m_pp, D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE, D3DDEVTYPE_HAL, false);
            HRESULT statusSWVP = createDeviceWithFlags(&m_pp, D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE, D3DDEVTYPE_HAL, false);
            HRESULT statusMVP = createDeviceWithFlags(&m_pp, D3DCREATE_MIXED_VERTEXPROCESSING | D3DCREATE_PUREDEVICE, D3DDEVTYPE_HAL, false);

            m_totalTests++;

            if (SUCCEEDED(statusHWVP) && FAILED(statusSWVP) && FAILED(statusMVP)) {
                m_passedTests++;
                std::cout << "  + The PUREDEVICE mode only with HWVP test has passed" << std::endl;
            } else {
                std::cout << "  - The PUREDEVICE mode only with HWVP test has failed" << std::endl;
            }
        }

        // Set/GetClipStatus test on D3DCREATE_HARDWARE_VERTEXPROCESSING/D3DCREATE_MIXED_VERTEXPROCESSING
        void testClipStatus() {
            createDeviceWithFlags(&m_pp, D3DCREATE_MIXED_VERTEXPROCESSING, D3DDEVTYPE_HAL, true);

            D3DCLIPSTATUS9 initialClipStatus = {};
            D3DCLIPSTATUS9 setClipStatus = {D3DCS_FRONT, D3DCS_FRONT};
            D3DCLIPSTATUS9 afterSetClipStatus = {};
            D3DCLIPSTATUS9 finalClipStatus = {};
            D3DCLIPSTATUS9 testClipStatus = {};
            Com<IDirect3DVertexBuffer9> vertexBuffer;

            void* vertices;
            m_device->CreateVertexBuffer(m_rgbVerticesSize, 0, RGBT_FVF_CODES,
                                         D3DPOOL_DEFAULT, &vertexBuffer, NULL);
            vertexBuffer->Lock(0, m_rgbVerticesSize, reinterpret_cast<void**>(&vertices), 0);
            memcpy(vertices, m_rgbVertices.data(), m_rgbVerticesSize);
            vertexBuffer->Unlock();

            m_totalTests++;

            HRESULT statusMixed = m_device->GetClipStatus(&initialClipStatus);
            //std::cout << format("  * initialClipStatus.ClipUnion: ", initialClipStatus.ClipUnion) << std::endl;
            //std::cout << format("  * initialClipStatus.ClipIntersection: ", initialClipStatus.ClipIntersection) << std::endl;
            if (SUCCEEDED(statusMixed)) {
                m_device->SetClipStatus(&setClipStatus);
                m_device->GetClipStatus(&afterSetClipStatus);
                //std::cout << format("  * afterSetClipStatus.ClipUnion: ", afterSetClipStatus.ClipUnion) << std::endl;
                //std::cout << format("  * afterSetClipStatus.ClipIntersection: ", afterSetClipStatus.ClipIntersection) << std::endl;
                m_device->BeginScene();
                m_device->SetSoftwareVertexProcessing(TRUE);
                m_device->SetRenderState(D3DRS_CLIPPING, FALSE);
                m_device->SetStreamSource(0, vertexBuffer.ptr(), 0, sizeof(RGBVERTEX));
                m_device->SetFVF(RGBT_FVF_CODES);
                m_device->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 1);
                m_device->EndScene();
                // The D3D9 documentation states: "When D3DRS_CLIPPING is set to FALSE, ClipUnion and ClipIntersection are set to zero.
                // Direct3D updates the clip status during drawing calls.", however this does not happen in practice.
                m_device->GetClipStatus(&finalClipStatus);
                //std::cout << format("  * finalClipStatus.ClipUnion: ", finalClipStatus.ClipUnion) << std::endl;
                //std::cout << format("  * finalClipStatus.ClipIntersection: ", finalClipStatus.ClipIntersection) << std::endl;
            }

            createDeviceWithFlags(&m_pp, D3DCREATE_HARDWARE_VERTEXPROCESSING, D3DDEVTYPE_HAL, true);

            HRESULT statusHWVPSet = m_device->SetClipStatus(&setClipStatus);
            HRESULT statusHWVPGet = m_device->GetClipStatus(&testClipStatus);
            //std::cout << format("  * testClipStatus.ClipUnion: ", testClipStatus.ClipUnion) << std::endl;
            //std::cout << format("  * testClipStatus.ClipIntersection: ", testClipStatus.ClipIntersection) << std::endl;

            if (SUCCEEDED(statusMixed) && SUCCEEDED(statusHWVPGet) && SUCCEEDED(statusHWVPSet) &&
                initialClipStatus.ClipUnion == 0 && initialClipStatus.ClipIntersection == 0xffffffff &&
                afterSetClipStatus.ClipUnion == D3DCS_FRONT && afterSetClipStatus.ClipIntersection == D3DCS_FRONT &&
                finalClipStatus.ClipUnion == D3DCS_FRONT && finalClipStatus.ClipIntersection == D3DCS_FRONT &&
                testClipStatus.ClipUnion == D3DCS_FRONT && testClipStatus.ClipIntersection == D3DCS_FRONT) {
                m_passedTests++;
                std::cout << "  + The Set/GetClipStatus test has passed" << std::endl;
            } else {
                std::cout << "  - The Set/GetClipStatus test has failed" << std::endl;
            }
        }

        // DrawIndexedPrimitive test with various NumVertices/primCount values
        void testDrawIndexedPrimitiveMaxPrimCount() {
            resetOrRecreateDevice();

            Com<IDirect3DVertexBuffer9> vertexBuffer;
            Com<IDirect3DIndexBuffer9>  indexBuffer;

            void* vertices;
            m_device->CreateVertexBuffer(m_rgbVerticesSize, 0, RGBT_FVF_CODES,
                                         D3DPOOL_DEFAULT, &vertexBuffer, NULL);
            vertexBuffer->Lock(0, m_rgbVerticesSize, reinterpret_cast<void**>(&vertices), 0);
            memcpy(vertices, m_rgbVertices.data(), m_rgbVerticesSize);
            vertexBuffer->Unlock();

            std::array<uint32_t, 8> indices_values = { 0, 1, 2, 1, 2, 1, 0, 1 };
            const size_t indices_size = indices_values.size() * sizeof(uint32_t);
            void* indices;
            m_device->CreateIndexBuffer(256, 0, D3DFMT_INDEX32, D3DPOOL_DEFAULT, &indexBuffer, NULL);
            indexBuffer->Lock(0, indices_size, reinterpret_cast<void**>(&indices), 0);
            memcpy(indices, indices_values.data(), indices_size);
            indexBuffer->Unlock();

            m_totalTests++;

            m_device->BeginScene();
            m_device->SetStreamSource(0, vertexBuffer.ptr(), 0, sizeof(RGBVERTEX));
            m_device->SetIndices(indexBuffer.ptr());
            m_device->SetFVF(RGBT_FVF_CODES);
            // 8388607 is the highest reported cap I've seen (on modern Intel Windows drivers)
            HRESULT statusDrawOneHigh  = m_device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 8, 0, 8388608);
            HRESULT statusDrawZeroHigh = m_device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 0, 0, 8388608);
            HRESULT statusDrawOneLow   = m_device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 8, 0, 1);
            HRESULT statusDrawZeroLow  = m_device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 0, 0, 1);
            m_device->EndScene();

            if (SUCCEEDED(statusDrawOneHigh) && SUCCEEDED(statusDrawZeroHigh)
             && SUCCEEDED(statusDrawOneLow)  && SUCCEEDED(statusDrawZeroLow)) {
                m_passedTests++;
                std::cout << "  + The DrawIndexedPrimitive max primCount test has passed" << std::endl;
            } else {
                std::cout << "  - The DrawIndexedPrimitive max primCount has failed" << std::endl;
            }
        }

        // CreateDevice with various devices types test
        void testDeviceTypes() {
            // D3DDEVTYPE_REF and D3DDEVTYPE_NULLREF are available on Windows 8 and above
            HRESULT statusHHAL = createDeviceWithFlags(&m_pp, D3DCREATE_HARDWARE_VERTEXPROCESSING, D3DDEVTYPE_HAL, false);
            HRESULT statusHSW = createDeviceWithFlags(&m_pp, D3DCREATE_HARDWARE_VERTEXPROCESSING, D3DDEVTYPE_SW, false);
            HRESULT statusSHAL = createDeviceWithFlags(&m_pp, D3DCREATE_SOFTWARE_VERTEXPROCESSING, D3DDEVTYPE_HAL, false);
            HRESULT statusSSW = createDeviceWithFlags(&m_pp, D3DCREATE_SOFTWARE_VERTEXPROCESSING, D3DDEVTYPE_SW, false);

            m_totalTests++;

            if (SUCCEEDED(statusHHAL) && statusHSW == D3DERR_INVALIDCALL
             && SUCCEEDED(statusSHAL) && statusSSW == D3DERR_INVALIDCALL) {
                m_passedTests++;
                std::cout << "  + The device types test has passed" << std::endl;
            } else {
                std::cout << "  - The device types test has failed" << std::endl;
            }
        }

        // GetDeviceCaps with various device types tests
        void testGetDeviceCapsWithDeviceTypes() {
            D3DCAPS9 caps9;

            // D3DDEVTYPE_REF and D3DDEVTYPE_NULLREF are available on Windows 8 and above
            HRESULT statusHAL = m_d3d->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &caps9);
            HRESULT statusSW = m_d3d->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_SW, &caps9);

            m_totalTests++;

            if (SUCCEEDED(statusHAL) && statusSW == D3DERR_NOTAVAILABLE) {
                m_passedTests++;
                std::cout << "  + The GetDeviceCaps with device types test has passed" << std::endl;
            } else {
                std::cout << "  - The GetDeviceCaps with device types test has failed" << std::endl;
            }
        }

        // D3DPOOL_DEFAULT allocation & Reset + D3DERR_DEVICENOTRESET state test (2 in 1)
        void testDefaultPoolAllocationReset() {
            resetOrRecreateDevice();

            // create a temporary DS surface
            Com<IDirect3DSurface9> tempDS;
            m_device->CreateDepthStencilSurface(RGBTriangle::WINDOW_WIDTH, RGBTriangle::WINDOW_HEIGHT,
                                                D3DFMT_D24X8, D3DMULTISAMPLE_NONE, 0, FALSE, &tempDS, NULL);

            m_totalTests++;
            // according to D3D9 docs, I quote: "Reset will fail unless the application releases all resources
            // that are allocated in D3DPOOL_DEFAULT, including those created by the IDirect3DDevice9::CreateRenderTarget
            // and IDirect3DDevice9::CreateDepthStencilSurface methods.", so this call should fail
            HRESULT status = m_device->Reset(&m_pp);
            if (FAILED(status)) {
                m_passedTests++;
                std::cout << "  + The D3DPOOL_DEFAULT allocation & Reset test has passed" << std::endl;

                m_totalTests++;
                // check to see if the device state is D3DERR_DEVICENOTRESET
                status = m_device->TestCooperativeLevel();
                if (status == D3DERR_DEVICENOTRESET) {
                    m_passedTests++;
                    std::cout << "  + The D3DERR_DEVICENOTRESET state test has passed" << std::endl;
                } else {
                    std::cout << "  - The D3DERR_DEVICENOTRESET state test has failed" << std::endl;
                }

            } else {
                std::cout << "  - The D3DPOOL_DEFAULT allocation & Reset test has failed" << std::endl;
                std::cout << "  ~ The D3DERR_DEVICENOTRESET state test did not run" << std::endl;
            }
        }

        // CreateStateBlock & Reset test
        void testCreateStateBlockAndReset() {
            resetOrRecreateDevice();

            // create a temporary state block
            Com<IDirect3DStateBlock9> stateBlock;
            m_device->CreateStateBlock(D3DSBT_ALL, &stateBlock);

            m_totalTests++;
            // D3D9 state blocks don't survive device Reset() calls and should be counted as losable resources
            HRESULT status = m_device->Reset(&m_pp);
            if (FAILED(status)) {
                m_passedTests++;
                std::cout << "  + The CreateStateBlock & Reset test has passed" << std::endl;
            } else {
                std::cout << "  - The CreateStateBlock & Reset test has failed" << std::endl;
            }
        }

        // CreateStateBlock with invalid D3DSTATEBLOCKTYPE test
        void testInvalidStateBlockType() {
            resetOrRecreateDevice();

            //Com<IDirect3DStateBlock9> stateBlockZero;
            Com<IDirect3DStateBlock9> stateBlockAll;
            Com<IDirect3DStateBlock9> stateBlockFour;
            Com<IDirect3DStateBlock9> stateBlockFiveHundred;

            m_totalTests++;

            // 0 will be accepted, but leads to undefined behavior,
            // while everything above 3 (D3DSBT_VERTEXSTATE) will be rejected
            //HRESULT statusZero = m_device->CreateStateBlock(D3DSTATEBLOCKTYPE(0), &stateBlockZero);
            HRESULT statusAll = m_device->CreateStateBlock(D3DSBT_ALL, &stateBlockAll);
            HRESULT statusFour = m_device->CreateStateBlock(D3DSTATEBLOCKTYPE(4), &stateBlockFour);
            HRESULT statusFiveHundred = m_device->CreateStateBlock(D3DSTATEBLOCKTYPE(500), &stateBlockFiveHundred);

            if (SUCCEEDED(statusAll) && FAILED(statusFour) && FAILED(statusFiveHundred)) {
                m_passedTests++;
                std::cout << "  + The CreateStateBlock with invalid D3DSTATEBLOCKTYPE test has passed" << std::endl;
            } else {
                std::cout << "  - The CreateStateBlock with invalid D3DSTATEBLOCKTYPE test has failed" << std::endl;
            }
        }

        // BeginStateBlock & other state block calls test
        void testBeginStateBlockCalls() {
            resetOrRecreateDevice();

            Com<IDirect3DStateBlock9> createStateBlock;
            Com<IDirect3DStateBlock9> endStateBlock;
            m_device->CreateStateBlock(D3DSBT_ALL, &createStateBlock);
            m_device->BeginStateBlock();

            m_totalTests++;
            // no other calls except EndStateBlock() will succeed insides of a BeginStateBlock()
            HRESULT statusBegin = m_device->BeginStateBlock();
            HRESULT statusApply = createStateBlock->Apply();
            HRESULT statusCapture = createStateBlock->Capture();
            createStateBlock = nullptr;
            HRESULT statusCreate = m_device->CreateStateBlock(D3DSBT_ALL, &createStateBlock);
            HRESULT statusEnd = m_device->EndStateBlock(&endStateBlock);

            if (FAILED(statusBegin) && FAILED(statusApply)
             && FAILED(statusCapture) && FAILED(statusCreate) && SUCCEEDED(statusEnd)) {
                m_passedTests++;
                std::cout << "  + The BeginStateBlock & other state block calls test has passed" << std::endl;
            } else {
                std::cout << "  - The BeginStateBlock & other state block calls test has failed" << std::endl;
            }
        }

        // MultiplyTransform with state blocks test
        void testMultiplyTransformRecordingAndCapture() {
            resetOrRecreateDevice();

            D3DMATRIX idMatrix =
            {{{
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f,
            }}};
            D3DMATRIX dblMatrix =
            {{{
                2.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 2.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 2.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 2.0f,
            }}};

            Com<IDirect3DStateBlock9> endStateBlock;
            Com<IDirect3DStateBlock9> endStateBlock2;
            Com<IDirect3DStateBlock9> captureStateBlock;

            m_totalTests++;

            m_device->CreateStateBlock(D3DSBT_ALL, &captureStateBlock);

            m_device->SetTransform(D3DTS_WORLDMATRIX(0), &idMatrix);
            m_device->BeginStateBlock();
            m_device->MultiplyTransform(D3DTS_WORLDMATRIX(0), &dblMatrix);
            m_device->EndStateBlock(&endStateBlock);
            D3DMATRIX checkMatrix1;
            m_device->GetTransform(D3DTS_WORLDMATRIX(0), &checkMatrix1);

            m_device->SetTransform(D3DTS_WORLDMATRIX(0), &idMatrix);
            endStateBlock->Apply();
            D3DMATRIX checkMatrix2;
            m_device->GetTransform(D3DTS_WORLDMATRIX(0), &checkMatrix2);

            m_device->SetTransform(D3DTS_WORLDMATRIX(0), &idMatrix);
            m_device->BeginStateBlock();
            m_device->SetTransform(D3DTS_WORLDMATRIX(0), &dblMatrix);
            m_device->EndStateBlock(&endStateBlock2);
            D3DMATRIX checkMatrix3;
            m_device->GetTransform(D3DTS_WORLDMATRIX(0), &checkMatrix3);

            m_device->SetTransform(D3DTS_WORLDMATRIX(0), &idMatrix);
            captureStateBlock->Capture();
            m_device->MultiplyTransform(D3DTS_WORLDMATRIX(0), &dblMatrix);
            captureStateBlock->Apply();
            D3DMATRIX checkMatrix4;
            m_device->GetTransform(D3DTS_WORLDMATRIX(0), &checkMatrix4);

            bool firstCheck  = !memcmp(&checkMatrix1, &dblMatrix, sizeof(dblMatrix));
            bool secondCheck = !memcmp(&checkMatrix2, &idMatrix,  sizeof(idMatrix));
            bool thirdCheck  = !memcmp(&checkMatrix3, &idMatrix,  sizeof(idMatrix));
            bool fourthCheck = !memcmp(&checkMatrix4, &idMatrix,  sizeof(idMatrix));

            // Calls to MultiplyTransform are not recorded in state blocks, but are applied directly
            if (firstCheck && secondCheck && thirdCheck && fourthCheck) {
                m_passedTests++;
                std::cout << "  + The MultiplyTransform with state blocks test has passed" << std::endl;
            } else {
                std::cout << "  - The MultiplyTransform with state blocks test has failed" << std::endl;
            }
        }

        // Cursor HotSpot coordinates test
        void testCursorHotSpotCoordinates() {
            resetOrRecreateDevice();

            Com<IDirect3DSurface9> surface;
            D3DLOCKED_RECT rect;

            std::vector<uint8_t> bitmap(256 * 256 * 4, 1);

            HRESULT status = m_device->CreateOffscreenPlainSurface(256, 256, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &surface, NULL);

            if (SUCCEEDED(status)) {
                m_totalTests++;

                surface->LockRect(&rect, NULL, 0);
                uint8_t* data  = reinterpret_cast<uint8_t*>(rect.pBits);
                memcpy(&data[0], &bitmap[0], 256 * 256 * 4);
                surface->UnlockRect();

                // HotSpot coordinates outside of the cursor bitmap will cause this call to fail
                HRESULT statusCursor = m_device->SetCursorProperties(256, 256, surface.ptr());

                if (FAILED(statusCursor)) {
                    m_passedTests++;
                    std::cout << "  + The Cursor HotSpot coordinates test has passed" << std::endl;
                } else {
                    std::cout << "  - The Cursor HotSpot coordinates test has failed" << std::endl;
                }
            } else {
                std::cout << "  ~ The Cursor HotSpot coordinates test has not run" << std::endl;
            }
        }

        // Invalid (outside of the render target) viewports test
        void testInvalidViewports() {
            resetOrRecreateDevice();

            DWORD offset = 100;
            D3DVIEWPORT9 vp1 = {0, 0, m_pp.BackBufferWidth - 1,
                                m_pp.BackBufferHeight - 1, 1.0, 0.0};
            D3DVIEWPORT9 vp2 = {0, 0, m_pp.BackBufferWidth - 1,
                                m_pp.BackBufferHeight - 1, 1.0, 1.0};
            D3DVIEWPORT9 vp3 = {0, 0, m_pp.BackBufferWidth + offset,
                                m_pp.BackBufferHeight + offset, 0.0, 1.0};
            D3DVIEWPORT9 vp4 = {offset * 2, offset * 2, m_pp.BackBufferWidth - offset / 2,
                                m_pp.BackBufferHeight - offset / 2, 0.0, 1.0};
            D3DVIEWPORT9 vp5 = {65536, 65536, 65536, 65536, 9.9, 0.0};
            D3DVIEWPORT9 vpr1 = { };
            D3DVIEWPORT9 vpr2 = { };

            m_totalTests++;
            // MinZ > MaxZ. In this situation MaxZ will
            // automagically be set to MinZ + 0.001f.
            HRESULT statusVP1 = m_device->SetViewport(&vp1);
            m_device->GetViewport(&vpr1);
            //std::cout << format("  * vpr.MinZ: ", vpr.MinZ) << std::endl;
            //std::cout << format("  * vpr.MaxZ: ", vpr.MaxZ) << std::endl;
            // MinZ = MaxZ. Behaves the same as MinZ > MaxZ.
            HRESULT statusVP2 = m_device->SetViewport(&vp2);
            m_device->GetViewport(&vpr2);
            // this works just fine in D3D9
            HRESULT statusVP3 = m_device->SetViewport(&vp3);
            // this also works just fine in D3D9
            HRESULT statusVP4 = m_device->SetViewport(&vp4);
            // this is a totally bullshit viewport (will work in D3D9)
            HRESULT statusVP5 = m_device->SetViewport(&vp5);
            // using a null viewport will outright crash on the native implementation
            //HRESULT statusVP6 = m_device->SetViewport(NULL);

            if (SUCCEEDED(statusVP1) && vpr1.MaxZ == vpr1.MinZ + 0.001f &&
                SUCCEEDED(statusVP2) && vpr2.MaxZ == vpr2.MinZ + 0.001f &&
                SUCCEEDED(statusVP3) && SUCCEEDED(statusVP4) && SUCCEEDED(statusVP5)) {
                m_passedTests++;
                std::cout << "  + The invalid viewports test has passed" << std::endl;
            } else {
                std::cout << "  - The invalid viewports test has failed" << std::endl;
            }
        }

        // Create and use a device with a NULL HWND
        void testDeviceWithoutHWND() {
            // use a separate device for this test
            Com<IDirect3DDevice9> device;
            Com<IDirect3DSurface9> surface;
            Com<IDirect3DVertexBuffer9> vertexBuffer;

            void* vertices;
            m_device->CreateVertexBuffer(m_rgbVerticesSize, 0, RGBT_FVF_CODES,
                                         D3DPOOL_DEFAULT, &vertexBuffer, NULL);
            vertexBuffer->Lock(0, m_rgbVerticesSize, reinterpret_cast<void**>(&vertices), 0);
            memcpy(vertices, m_rgbVertices.data(), m_rgbVerticesSize);
            vertexBuffer->Unlock();

            D3DPRESENT_PARAMETERS presentParams;
            ZeroMemory(&presentParams, sizeof(presentParams));

            presentParams.Windowed = TRUE;
            presentParams.hDeviceWindow = NULL;
            // set to D3DSWAPEFFECT_COPY or D3DSWAPEFFECT_FLIP for no VSync
            presentParams.SwapEffect = D3DSWAPEFFECT_COPY;
            // according to D3D9 spec "0 is treated as 1" here
            presentParams.BackBufferCount = 0;
            presentParams.BackBufferWidth = RGBTriangle::WINDOW_WIDTH;
            presentParams.BackBufferHeight = RGBTriangle::WINDOW_HEIGHT;
            presentParams.BackBufferFormat = m_pp.BackBufferFormat;

            DWORD behaviorFlags = D3DCREATE_HARDWARE_VERTEXPROCESSING;

            HRESULT status = m_d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, NULL,
                                                 behaviorFlags, &presentParams, &device);

            if(FAILED(status)) {
                std::cout << "  ~ The device does not support creation with a NULL HWND" << std::endl;
            } else {
                m_totalTests++;

                HRESULT statusBB      = device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &surface);

                /*D3DSURFACE_DESC desc;
                surface->GetDesc(&desc);
                std::cout << format("  ~ Back buffer width: ",  desc.Width)  << std::endl;
                std::cout << format("  ~ Back buffer height: ", desc.Height) << std::endl;*/

                device->BeginScene();
                device->SetStreamSource(0, vertexBuffer.ptr(), 0, sizeof(RGBVERTEX));
                device->SetFVF(RGBT_FVF_CODES);
                HRESULT statusDraw    = device->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 1);
                device->EndScene();
                HRESULT statusPresent = device->Present(NULL, NULL, NULL, NULL);

                if (FAILED(statusBB) || FAILED(statusDraw) || FAILED(statusPresent)) {
                    std::cout << "  - The device with NULL HWND test has failed" << std::endl;
                } else {
                    m_passedTests++;
                    std::cout << "  + The device with NULL HWND test has passed" << std::endl;
                }
            }
        }

        // Tests patch related calls
        void testPatchCalls() {
            resetOrRecreateDevice();

            float             numSegs = 3;
            D3DRECTPATCH_INFO rectPatchInfo = { };
            D3DTRIPATCH_INFO  triPatchInfo = { };

            m_totalTests++;

            // All draw calls will return D3D_OK, even on a driver that
            // does not implement any form of patches/TruForm
            HRESULT rectStatusInv  = m_device->DrawRectPatch(0, &numSegs, NULL);
            HRESULT triStatusInv   = m_device->DrawTriPatch(0, &numSegs, NULL);
            HRESULT rectStatus     = m_device->DrawRectPatch(1, &numSegs, NULL);
            HRESULT triStatus      = m_device->DrawTriPatch(2, &numSegs, NULL);
            HRESULT rectStatus2    = m_device->DrawRectPatch(3, &numSegs, &rectPatchInfo);
            HRESULT triStatus2     = m_device->DrawTriPatch(4, &numSegs, &triPatchInfo);
            // All delete calls will fail on drivers that
            // do not implement any form of patches/TruForm
            HRESULT delOneStatus   = m_device->DeletePatch(1);
            HRESULT delTwoStatus   = m_device->DeletePatch(2);
            HRESULT delThreeStatus = m_device->DeletePatch(3);
            HRESULT delFourStatus  = m_device->DeletePatch(4);
            HRESULT delFiveStatus  = m_device->DeletePatch(5);

            if (SUCCEEDED(rectStatusInv) && SUCCEEDED(triStatusInv)
                && SUCCEEDED(rectStatus) && SUCCEEDED(triStatus)
                && SUCCEEDED(rectStatus2) && SUCCEEDED(triStatus2)
                && FAILED(delOneStatus) && FAILED(delTwoStatus) && FAILED(delThreeStatus)
                && FAILED(delFourStatus) && FAILED(delFiveStatus)) {
                m_passedTests++;
                std::cout << "  + The patch calls test has passed" << std::endl;
            // The test might fail on systems that actually support TruForm
            } else {
                std::cout << "  - The patch calls test has failed" << std::endl;
            }
        }

        void testCheckDeviceFormatWithBuffers() {
            resetOrRecreateDevice();

            m_totalTests++;

            HRESULT resVB = m_d3d->CheckDeviceFormat(0, D3DDEVTYPE_HAL, m_pp.BackBufferFormat, 0, D3DRTYPE_VERTEXBUFFER, D3DFMT_VERTEXDATA);
            HRESULT resIB = m_d3d->CheckDeviceFormat(0, D3DDEVTYPE_HAL, m_pp.BackBufferFormat, 0, D3DRTYPE_INDEXBUFFER, D3DFMT_INDEX16);

            HRESULT resVB2 = m_d3d->CheckDeviceFormat(0, D3DDEVTYPE_HAL, m_pp.BackBufferFormat, 0, D3DRTYPE_VERTEXBUFFER, m_pp.BackBufferFormat);
            HRESULT resIB2 = m_d3d->CheckDeviceFormat(0, D3DDEVTYPE_HAL, m_pp.BackBufferFormat, 0, D3DRTYPE_INDEXBUFFER, m_pp.BackBufferFormat);

            if (resVB  != D3DERR_INVALIDCALL ||
                resIB  != D3DERR_INVALIDCALL ||
                resVB2 != D3DERR_INVALIDCALL ||
                resIB2 != D3DERR_INVALIDCALL) {
                std::cout << "  - The CheckDeviceFormat with index/vertex buffers test has failed" << std::endl;
            } else {
                m_passedTests++;
                std::cout << "  + The CheckDeviceFormat with index/vertex buffers test has passed" << std::endl;
            }
        }

        // Clear with unbound depth stencil test
        void testClearWithUnboundDepthStencil() {
            resetOrRecreateDevice();

            Com<IDirect3DSurface9> renderTarget;

            m_device->CreateRenderTarget(256, 256, m_pp.BackBufferFormat, D3DMULTISAMPLE_NONE, 0, TRUE, &renderTarget, NULL);
            HRESULT statusRT = m_device->SetRenderTarget(0, renderTarget.ptr());
            HRESULT statusDS = m_device->SetDepthStencilSurface(NULL);

            if (SUCCEEDED(statusRT) && SUCCEEDED(statusDS)) {
                m_totalTests++;

                // D3DCLEAR_ZBUFFER or D3DCLEAR_STENCIL will fail if a depth stencil isn't bound
                HRESULT clearZStatus = m_device->Clear(0, NULL, D3DCLEAR_ZBUFFER, D3DCOLOR_RGBA(0, 0, 0, 0), 1.0f, 0);
                HRESULT clearSStatus = m_device->Clear(0, NULL, D3DCLEAR_STENCIL, D3DCOLOR_RGBA(0, 0, 0, 0), 1.0f, 0);

                if (FAILED(clearZStatus) && FAILED(clearSStatus)) {
                    m_passedTests++;
                    std::cout << "  + The D3DCLEAR_ZBUFFER with bound depth stencil test has passed" << std::endl;
                } else {
                    std::cout << "  - The D3DCLEAR_ZBUFFER with bound depth stencil test has failed" << std::endl;
                }
            } else {
                std::cout << "  ~ The D3DCLEAR_ZBUFFER with bound depth stencil test did not run" << std::endl;
            }
        }

        // Test return pointer initialization in CreateVertexShader
        void testCreateVertexShaderInit() {
            resetOrRecreateDevice();

            m_totalTests++;

            DWORD function = 0;
            IDirect3DVertexShader9* vs = reinterpret_cast<IDirect3DVertexShader9*>(0xFFFFFFFF);
            m_device->CreateVertexShader(&function, &vs);

            if (vs == reinterpret_cast<IDirect3DVertexShader9*>(0xFFFFFFFF)) {
                m_passedTests++;
                std::cout << "  + The CreateVertexShader initialization test has passed" << std::endl;
            } else {
                std::cout << "  - The CreateVertexShader initialization test has failed" << std::endl;
            }
        }

        // Test UpdateTexture calls with various source/destination texture sizes
        void testUpdateTextureSizes() {
            resetOrRecreateDevice();

            m_totalTests++;

            Com<IDirect3DTexture9> textureSrc1;
            Com<IDirect3DTexture9> textureDst1;
            Com<IDirect3DTexture9> textureSrc2;
            Com<IDirect3DTexture9> textureDst2;
            Com<IDirect3DTexture9> textureSrc3;
            Com<IDirect3DTexture9> textureDst3;

            // This passes on native
            m_device->CreateTexture(320, 554, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &textureSrc1, NULL);
            m_device->CreateTexture(320, 556, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &textureDst1, NULL);
            HRESULT status1 = m_device->UpdateTexture(textureSrc1.ptr(), textureDst1.ptr());
            // This also passes on native
            m_device->CreateTexture(160, 278, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &textureSrc2, NULL);
            m_device->CreateTexture(320, 556, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &textureDst2, NULL);
            // ... and even this passes on native
            HRESULT status2 = m_device->UpdateTexture(textureSrc2.ptr(), textureDst2.ptr());
            m_device->CreateTexture(360, 590, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &textureSrc3, NULL);
            m_device->CreateTexture(320, 556, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &textureDst3, NULL);
            HRESULT status3 = m_device->UpdateTexture(textureSrc3.ptr(), textureDst3.ptr());

            if (SUCCEEDED(status1) && SUCCEEDED(status2) && SUCCEEDED(status3)) {
                m_passedTests++;
                std::cout << "  + The UpdateTexture sizes test has passed" << std::endl;
            } else {
                std::cout << "  - The UpdateTexture sizes test has failed" << std::endl;
            }
        }

        // Rect/Box clearing behavior on lock test
        void testRectBoxClearingOnLock() {
            resetOrRecreateDevice();

            Com<IDirect3DSurface9> surface;
            Com<IDirect3DSurface9> sysmemSurface;
            Com<IDirect3DTexture9> texture;
            Com<IDirect3DTexture9> sysmemTexture;
            Com<IDirect3DCubeTexture9> cubeTexture;
            Com<IDirect3DCubeTexture9> sysmemCubeTexture;
            Com<IDirect3DVolumeTexture9> volumeTexture;
            Com<IDirect3DVolumeTexture9> sysmemVolumeTexture;

            D3DLOCKED_RECT surfaceRect;
            D3DLOCKED_RECT sysmemSurfaceRect;
            D3DLOCKED_RECT textureRect;
            D3DLOCKED_RECT sysmemTextureRect;
            D3DLOCKED_RECT cubeTextureRect;
            D3DLOCKED_RECT sysmemCubeTextureRect;
            D3DLOCKED_BOX volumeTextureBox;
            D3DLOCKED_BOX sysmemVolumeTextureBox;

            m_totalTests++;

            m_device->CreateOffscreenPlainSurface(256, 256, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &surface, NULL);
            m_device->CreateOffscreenPlainSurface(256, 256, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, &sysmemSurface, NULL);
            m_device->CreateTexture(256, 256, 1, 0, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &texture, NULL);
            m_device->CreateTexture(256, 256, 1, 0, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, &sysmemTexture, NULL);
            m_device->CreateVolumeTexture(256, 256, 256, 1, 0, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &volumeTexture, NULL);
            m_device->CreateVolumeTexture(256, 256, 256, 1, 0, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, &sysmemVolumeTexture, NULL);
            m_device->CreateCubeTexture(256, 1, 0, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &cubeTexture, NULL);
            m_device->CreateCubeTexture(256, 1, 0, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, &sysmemCubeTexture, NULL);

            // the second lock will fail, but the LockRect contents
            // should be cleared for everything outside of D3DPOOL_DEFAULT.
            // LockBox clears the content universally.
            surface->LockRect(&surfaceRect, NULL, 0);
            surfaceRect.pBits = reinterpret_cast<void*>(0xABCDABCD);
            surfaceRect.Pitch = 10;
            HRESULT surfaceStatus = surface->LockRect(&surfaceRect, NULL, 0);
            surface->UnlockRect();
            sysmemSurface->LockRect(&sysmemSurfaceRect, NULL, 0);
            sysmemSurfaceRect.pBits = reinterpret_cast<void*>(0xABCDABCD);
            sysmemSurfaceRect.Pitch = 10;
            HRESULT sysmemSurfaceStatus = sysmemSurface->LockRect(&sysmemSurfaceRect, NULL, 0);
            sysmemSurface->UnlockRect();
            texture->LockRect(0, &textureRect, NULL, 0);
            textureRect.pBits = reinterpret_cast<void*>(0xABCDABCD);
            textureRect.Pitch = 10;
            HRESULT textureStatus = texture->LockRect(0, &textureRect, NULL, 0);
            texture->UnlockRect(0);
            sysmemTexture->LockRect(0, &sysmemTextureRect, NULL, 0);
            sysmemTextureRect.pBits = reinterpret_cast<void*>(0xABCDABCD);
            sysmemTextureRect.Pitch = 10;
            HRESULT sysmemTextureStatus = sysmemTexture->LockRect(0, &sysmemTextureRect, NULL, 0);
            sysmemTexture->UnlockRect(0);
            cubeTexture->LockRect(D3DCUBEMAP_FACE_POSITIVE_X, 0, &cubeTextureRect, NULL, 0);
            cubeTextureRect.pBits = reinterpret_cast<void*>(0xABCDABCD);
            cubeTextureRect.Pitch = 10;
            HRESULT cubeTextureStatus = cubeTexture->LockRect(D3DCUBEMAP_FACE_POSITIVE_X, 0, &cubeTextureRect, NULL, 0);
            cubeTexture->UnlockRect(D3DCUBEMAP_FACE_POSITIVE_X, 0);
            sysmemCubeTexture->LockRect(D3DCUBEMAP_FACE_POSITIVE_X, 0, &sysmemCubeTextureRect, NULL, 0);
            sysmemCubeTextureRect.pBits = reinterpret_cast<void*>(0xABCDABCD);
            sysmemCubeTextureRect.Pitch = 10;
            HRESULT sysmemCubeTextureStatus = sysmemCubeTexture->LockRect(D3DCUBEMAP_FACE_POSITIVE_X, 0, &sysmemCubeTextureRect, NULL, 0);
            sysmemCubeTexture->UnlockRect(D3DCUBEMAP_FACE_POSITIVE_X, 0);
            volumeTexture->LockBox(0, &volumeTextureBox, NULL, 0);
            volumeTextureBox.pBits = reinterpret_cast<void*>(0xABCDABCD);
            volumeTextureBox.RowPitch = 10;
            volumeTextureBox.SlicePitch = 10;
            HRESULT volumeTextureStatus = volumeTexture->LockBox(0, &volumeTextureBox, NULL, 0);
            volumeTexture->UnlockBox(0);
            sysmemVolumeTexture->LockBox(0, &sysmemVolumeTextureBox, NULL, 0);
            sysmemVolumeTextureBox.pBits = reinterpret_cast<void*>(0xABCDABCD);
            sysmemVolumeTextureBox.RowPitch = 10;
            sysmemVolumeTextureBox.SlicePitch = 10;
            HRESULT sysmemVolumeTextureStatus = volumeTexture->LockBox(0, &sysmemVolumeTextureBox, NULL, 0);
            sysmemVolumeTexture->UnlockBox(0);

            //std::cout << format("  * surfaceRect pBits: ", surfaceRect.pBits, " Pitch: ", surfaceRect.Pitch) << std::endl;
            //std::cout << format("  * sysmemSurfaceRect pBits: ", sysmemSurfaceRect.pBits, " Pitch: ", sysmemSurfaceRect.Pitch) << std::endl;
            //std::cout << format("  * textureRect pBits: ", textureRect.pBits, " Pitch: ", textureRect.Pitch) << std::endl;
            //std::cout << format("  * sysmemTextureRect pBits: ", sysmemTextureRect.pBits, " Pitch: ", sysmemTextureRect.Pitch) << std::endl;
            //std::cout << format("  * cubeTextureRect pBits: ", cubeTextureRect.pBits, " Pitch: ", cubeTextureRect.Pitch) << std::endl;
            //std::cout << format("  * sysmemCubeTextureRect pBits: ", sysmemCubeTextureRect.pBits, " Pitch: ", sysmemCubeTextureRect.Pitch) << std::endl;
            //std::cout << format("  * volumeTextureBox pBits: ", volumeTextureBox.pBits, " RowPitch: ", volumeTextureBox.RowPitch,
            //                    " SlicePitch: ", volumeTextureBox.SlicePitch) << std::endl;
            //std::cout << format("  * sysmemVolumeTextureBox pBits: ", sysmemVolumeTextureBox.pBits, " RowPitch: ", sysmemVolumeTextureBox.RowPitch,
            //                    " SlicePitch: ", sysmemVolumeTextureBox.SlicePitch) << std::endl;

            if (SUCCEEDED(surfaceStatus) ||
                SUCCEEDED(sysmemSurfaceStatus) ||
                SUCCEEDED(textureStatus) ||
                SUCCEEDED(sysmemTextureStatus) ||
                SUCCEEDED(cubeTextureStatus) ||
                SUCCEEDED(sysmemCubeTextureStatus) ||
                SUCCEEDED(volumeTextureStatus) ||
                SUCCEEDED(sysmemVolumeTextureStatus) ||
                surfaceRect.pBits != nullptr ||
                surfaceRect.Pitch != 0 ||
                sysmemSurfaceRect.pBits != reinterpret_cast<void*>(0xABCDABCD) ||
                sysmemSurfaceRect.Pitch != 10 ||
                textureRect.pBits != nullptr ||
                textureRect.Pitch != 0 ||
                sysmemTextureRect.pBits != reinterpret_cast<void*>(0xABCDABCD) ||
                sysmemTextureRect.Pitch != 10 ||
                cubeTextureRect.pBits != nullptr ||
                cubeTextureRect.Pitch != 0 ||
                sysmemCubeTextureRect.pBits != reinterpret_cast<void*>(0xABCDABCD) ||
                sysmemCubeTextureRect.Pitch != 10 ||
                volumeTextureBox.pBits != nullptr ||
                volumeTextureBox.RowPitch != 0 ||
                volumeTextureBox.SlicePitch != 0 ||
                sysmemVolumeTextureBox.pBits != nullptr ||
                sysmemVolumeTextureBox.RowPitch != 0 ||
                sysmemVolumeTextureBox.SlicePitch != 0) {
                std::cout << "  - The Rect/Box clearing on lock test has failed" << std::endl;
            } else {
                m_passedTests++;
                std::cout << "  + The Rect/Box clearing on lock test has passed" << std::endl;
            }
        }

        // Tests Lock calls on textures in various pool with D3DLOCK_DISCARD | D3DLOCK_READONLY
        void testPoolLockingFlagBehavior() {
            resetOrRecreateDevice();

            m_totalTests++;

            Com<IDirect3DTexture9> textureDefault;
            Com<IDirect3DTexture9> textureManaged;
            Com<IDirect3DTexture9> textureSystemMem;
            Com<IDirect3DTexture9> textureScratch;

            D3DLOCKED_RECT lockedRect;

            m_device->CreateTexture(256, 256, 1, 0, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &textureDefault, NULL);
            m_device->CreateTexture(256, 256, 1, 0, D3DFMT_X8R8G8B8, D3DPOOL_MANAGED, &textureManaged, NULL);
            m_device->CreateTexture(256, 256, 1, 0, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, &textureSystemMem, NULL);
            m_device->CreateTexture(256, 256, 1, 0, D3DFMT_X8R8G8B8, D3DPOOL_SCRATCH, &textureScratch, NULL);

            HRESULT statusDefault = textureDefault->LockRect(0, &lockedRect, NULL, D3DLOCK_DISCARD | D3DLOCK_READONLY);
            HRESULT statusManaged = textureManaged->LockRect(0, &lockedRect, NULL, D3DLOCK_DISCARD | D3DLOCK_READONLY);
            HRESULT statusSystemMem = textureSystemMem->LockRect(0, &lockedRect, NULL, D3DLOCK_DISCARD | D3DLOCK_READONLY);
            HRESULT statusScratch = textureScratch->LockRect(0, &lockedRect, NULL, D3DLOCK_DISCARD | D3DLOCK_READONLY);

            // LockRect calls will fail on DEFAULT but work on MANAGED/SYSTEMMEM/SCRATCH
            if (FAILED(statusDefault) && SUCCEEDED(statusManaged) && SUCCEEDED(statusSystemMem) && SUCCEEDED(statusScratch)) {
                m_passedTests++;
                std::cout << "  + The pool locking flag behavior test has passed" << std::endl;
            } else {
                std::cout << "  - The pool locking flag behavior test has failed" << std::endl;
            }
        }

        // DF formats CheckDeviceFormat tests
        void testDFFormatsCheckDeviceFormat() {
            resetOrRecreateDevice();

            HRESULT statusDF16 = m_d3d->CheckDeviceFormat(0, D3DDEVTYPE_HAL, m_pp.BackBufferFormat, 0, D3DRTYPE_SURFACE, (D3DFORMAT) MAKEFOURCC('D', 'F', '1', '6'));
            HRESULT statusDF24 = m_d3d->CheckDeviceFormat(0, D3DDEVTYPE_HAL, m_pp.BackBufferFormat, 0, D3DRTYPE_SURFACE, (D3DFORMAT) MAKEFOURCC('D', 'F', '2', '4'));

            // DF formats will not be supported on Nvidia, but should work on AMD/Intel
            if (FAILED(statusDF16) && FAILED(statusDF24)) {
                std::cout << "  ~ The DF formats test was skipped" << std::endl;
            } else {
                m_totalTests++;
                m_passedTests++;
                std::cout << "  + The DF formats test has passed" << std::endl;
            }
        }

        // Various CheckDeviceMultiSampleType validation tests
        void testCheckDeviceMultiSampleTypeValidation() {
            resetOrRecreateDevice();

            constexpr D3DFORMAT D3DFMT_NULL = (D3DFORMAT) MAKEFOURCC('N', 'U', 'L', 'L');

            m_totalTests++;

            // The call will fail with anything above D3DMULTISAMPLE_16_SAMPLES
            HRESULT statusSample = m_d3d->CheckDeviceMultiSampleType(0, D3DDEVTYPE_HAL, m_pp.BackBufferFormat, FALSE,
                                                                     (D3DMULTISAMPLE_TYPE) ((UINT) D3DMULTISAMPLE_16_SAMPLES * 2), NULL);

            // The call will fail with D3DFMT_UNKNOWN
            HRESULT statusUnknown = m_d3d->CheckDeviceMultiSampleType(0, D3DDEVTYPE_HAL, D3DFMT_UNKNOWN,
                                                                      FALSE, D3DMULTISAMPLE_NONE, NULL);

            // The call will pass with D3DFMT_NULL
            HRESULT statusNull = m_d3d->CheckDeviceMultiSampleType(0, D3DDEVTYPE_HAL, D3DFMT_NULL,
                                                                   FALSE, D3DMULTISAMPLE_NONE, NULL);

            // The call will fail with D3DFMT_NULL and anything above D3DMULTISAMPLE_2_SAMPLES
            // on cards that don't support the format/vendor hack, or above D3DMULTISAMPLE_16_SAMPLES on cards that do
            HRESULT statusNullSamples = m_d3d->CheckDeviceMultiSampleType(0, D3DDEVTYPE_HAL, D3DFMT_NULL, FALSE,
                                                                          (D3DMULTISAMPLE_TYPE) ((UINT) D3DMULTISAMPLE_16_SAMPLES * 2), NULL);

            if (FAILED(statusSample) && FAILED(statusUnknown) && SUCCEEDED(statusNull) && FAILED(statusNullSamples)) {
                m_passedTests++;
                std::cout << "  + The CheckDeviceMultiSampleType validation test has passed" << std::endl;
            } else {
                std::cout << "  - The CheckDeviceMultiSampleType validation test has failed" << std::endl;
            }
        }

        // Various CheckDeviceMultiSampleType formats test
        void testCheckDeviceMultiSampleTypeFormats() {
            resetOrRecreateDevice();

            std::map<D3DFORMAT, char const*> sfFormats = { {D3DFMT_D16_LOCKABLE,  "D3DFMT_D16_LOCKABLE"},
                                                           {D3DFMT_D32F_LOCKABLE, "D3DFMT_D32F_LOCKABLE"},
                                                           {(D3DFORMAT) MAKEFOURCC('I', 'N', 'T', 'Z'), "D3DFMT_INTZ"},
                                                           {D3DFMT_DXT1, "D3DFMT_DXT1"},
                                                           {D3DFMT_DXT2, "D3DFMT_DXT2"},
                                                           {D3DFMT_DXT3, "D3DFMT_DXT3"},
                                                           {D3DFMT_DXT4, "D3DFMT_DXT4"},
                                                           {D3DFMT_DXT5, "D3DFMT_DXT5"} };

            std::map<D3DFORMAT, char const*>::iterator sfFormatIter;

            std::cout << std::endl << "Running CheckDeviceMultiSampleType formats tests:" << std::endl;

            for (sfFormatIter = sfFormats.begin(); sfFormatIter != sfFormats.end(); ++sfFormatIter) {
                D3DFORMAT surfaceFormat = sfFormatIter->first;

                m_totalTests++;

                // None of the above textures are supported with anything beside D3DMULTISAMPLE_NONE
                HRESULT status = m_d3d->CheckDeviceMultiSampleType(0, D3DDEVTYPE_HAL, surfaceFormat,
                                                                   FALSE, D3DMULTISAMPLE_2_SAMPLES, 0);

                if (SUCCEEDED(status)) {
                    std::cout << format("  - The ", sfFormatIter->second , " format test has failed") << std::endl;
                } else {
                    m_passedTests++;
                    std::cout << format("  + The ", sfFormatIter->second ," format test has passed") << std::endl;
                }
            }
        }

        void printTestResults() {
            std::cout << std::endl << format("Passed ", m_passedTests, "/", m_totalTests, " tests") << std::endl;
        }

        void prepare() {
            createDeviceWithFlags(&m_pp, D3DCREATE_HARDWARE_VERTEXPROCESSING, D3DDEVTYPE_HAL, true);

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

            status = m_device->CreateVertexBuffer(m_rgbVerticesSize, 0, RGBT_FVF_CODES,
                                                  D3DPOOL_DEFAULT, &m_vb, NULL);
            if (FAILED(status))
                throw Error("Failed to create D3D9 vertex buffer");

            status = m_vb->Lock(0, m_rgbVerticesSize, reinterpret_cast<void**>(&vertices), 0);
            if (FAILED(status))
                throw Error("Failed to lock D3D9 vertex buffer");
            memcpy(vertices, m_rgbVertices.data(), m_rgbVerticesSize);
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
                                      D3DDEVTYPE deviceType,
                                      bool throwErrorOnFail) {
            if (m_d3d == nullptr)
                throw Error("The D3D9 interface hasn't been initialized");

            m_device = nullptr;

            HRESULT status = m_d3d->CreateDevice(D3DADAPTER_DEFAULT, deviceType, m_hWnd,
                                                 behaviorFlags, presentParams, &m_device);

            if (throwErrorOnFail && FAILED(status))
                throw Error("Failed to create D3D9 device");

            return status;
        }

        HRESULT resetOrRecreateDevice() {
            if (m_d3d == nullptr)
                throw Error("The D3D9 interface hasn't been initialized");

            HRESULT status = D3D_OK;

            // return early if the call to Reset() works
            if (m_device != nullptr) {
                if(SUCCEEDED(m_device->Reset(&m_pp)))
                    return status;
                // prepare to clear the device otherwise
                m_device = nullptr;
            }

            status = m_d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, m_hWnd,
                                         D3DCREATE_HARDWARE_VERTEXPROCESSING,
                                         &m_pp, &m_device);

            if (FAILED(status))
                throw Error("Failed to create D3D9 device");

            return status;
        }

        HWND                          m_hWnd;

        DWORD                         m_vendorID;

        Com<IDirect3D9>               m_d3d;
        Com<IDirect3DDevice9>         m_device;
        Com<IDirect3DVertexBuffer9>   m_vb;

        D3DPRESENT_PARAMETERS         m_pp;

        // tailored for 1024x768 and the appearance of being centered
        std::array<RGBVERTEX, 3>      m_rgbVertices = {{ { 60.0f, 625.0f, 0.5f, 1.0f, D3DCOLOR_XRGB(255, 0, 0),},
                                                         {350.0f,  45.0f, 0.5f, 1.0f, D3DCOLOR_XRGB(0, 255, 0),},
                                                         {640.0f, 625.0f, 0.5f, 1.0f, D3DCOLOR_XRGB(0, 0, 255),} }};
        const size_t                  m_rgbVerticesSize = m_rgbVertices.size() * sizeof(RGBVERTEX);

        UINT                          m_totalTests;
        UINT                          m_passedTests;
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
                     RGBTriangle::TRIANGLE_ID, NULL};
    RegisterClassEx(&wc);

    HWND hWnd = CreateWindow(RGBTriangle::TRIANGLE_ID, RGBTriangle::TRIANGLE_TITLE,
                             WS_OVERLAPPEDWINDOW, 50, 50,
                             RGBTriangle::WINDOW_WIDTH, RGBTriangle::WINDOW_HEIGHT,
                             GetDesktopWindow(), NULL, wc.hInstance, NULL);

    MSG msg;

    try {
        RGBTriangle rgbTriangle(hWnd);

        // list various D3D Device stats
        rgbTriangle.listAdapterDisplayModes();
        rgbTriangle.listBackBufferFormats(FALSE);
        rgbTriangle.listBackBufferFormats(TRUE);
        rgbTriangle.listSurfaceFormats(0);
        rgbTriangle.listSurfaceFormats(D3DUSAGE_RENDERTARGET);
        rgbTriangle.listSurfaceFormats(D3DUSAGE_DEPTHSTENCIL);
        rgbTriangle.listMultisampleSupport(FALSE);
        rgbTriangle.listMultisampleSupport(TRUE);
        rgbTriangle.listVendorFormatHacksSupport();
        rgbTriangle.listDeviceCapabilities();
        rgbTriangle.listVCacheQueryResult();
        rgbTriangle.listAvailableTextureMemory();

        // run D3D Device tests
        rgbTriangle.startTests();
        rgbTriangle.testZeroBackBufferCount();
        rgbTriangle.testInvalidPresentationInterval();
        rgbTriangle.testBeginSceneReset();
        rgbTriangle.testPureDeviceOnlyWithHWVP();
        rgbTriangle.testClipStatus();
        rgbTriangle.testDrawIndexedPrimitiveMaxPrimCount();
        rgbTriangle.testDeviceTypes();
        rgbTriangle.testGetDeviceCapsWithDeviceTypes();
        rgbTriangle.testDefaultPoolAllocationReset();
        rgbTriangle.testCreateStateBlockAndReset();
        rgbTriangle.testInvalidStateBlockType();
        rgbTriangle.testBeginStateBlockCalls();
        rgbTriangle.testMultiplyTransformRecordingAndCapture();
        rgbTriangle.testCursorHotSpotCoordinates();
        rgbTriangle.testInvalidViewports();
        rgbTriangle.testDeviceWithoutHWND();
        // outright crashes on certain native drivers/hardware
        //rgbTriangle.testPatchCalls();
        rgbTriangle.testCheckDeviceFormatWithBuffers();
        rgbTriangle.testClearWithUnboundDepthStencil();
        rgbTriangle.testCreateVertexShaderInit();
        rgbTriangle.testUpdateTextureSizes();
        // outright crashes on certain native drivers/hardware
        //rgbTriangle.testRectBoxClearingOnLock();
        rgbTriangle.testPoolLockingFlagBehavior();
        rgbTriangle.testDFFormatsCheckDeviceFormat();
        rgbTriangle.testCheckDeviceMultiSampleTypeValidation();
        rgbTriangle.testCheckDeviceMultiSampleTypeFormats();
        rgbTriangle.printTestResults();

        // D3D9 triangle
        rgbTriangle.prepare();

        ShowWindow(hWnd, SW_SHOWDEFAULT);
        UpdateWindow(hWnd);

        while (true) {
            if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);

                if (msg.message == WM_QUIT) {
                    UnregisterClass(RGBTriangle::TRIANGLE_ID, wc.hInstance);
                    return msg.wParam;
                }
            } else {
                rgbTriangle.render();
            }
        }

    } catch (const Error& e) {
        std::cerr << e.message() << std::endl;
        UnregisterClass(RGBTriangle::TRIANGLE_ID, wc.hInstance);
        return msg.wParam;
    }
}
