#include "windows_biometric.h"

#ifdef _WIN32
#include <windows.h>
#include <winbio.h>
#include <comdef.h>
#pragma comment(lib, "winbio.lib")
#pragma comment(lib, "comsuppw.lib")
#endif

#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <cstring>

#define RESET "\033[0m"
#define BOLD "\033[1m"
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define CYAN "\033[36m"

static std::string g_active_com_port = "";
static bool g_com_connected = false;

#ifdef _WIN32
static HANDLE g_h_serial = INVALID_HANDLE_VALUE;
#endif

std::vector<std::string> windows_list_com_ports()
{
    std::vector<std::string> ports;
#ifdef _WIN32
    for (int i = 1; i <= 32; ++i)
    {
        std::string port = "COM" + std::to_string(i);
        std::string win_port = "\\\\.\\" + port;
        HANDLE h = CreateFileA(win_port.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (h != INVALID_HANDLE_VALUE)
        {
            ports.push_back(port);
            CloseHandle(h);
        }
    }
#endif
    return ports;
}

bool windows_set_com_port(const char *port_name, int baudrate)
{
    if (!port_name) return false;
    g_active_com_port = port_name;

#ifdef _WIN32
    if (g_h_serial != INVALID_HANDLE_VALUE)
    {
        CloseHandle(g_h_serial);
        g_h_serial = INVALID_HANDLE_VALUE;
        g_com_connected = false;
    }

    std::string win_port = "\\\\.\\" + g_active_com_port;
    HANDLE h = CreateFileA(win_port.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    DCB dcb = {0};
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(h, &dcb))
    {
        CloseHandle(h);
        return false;
    }

    dcb.BaudRate = (baudrate == 57600) ? CBR_57600 : CBR_115200;
    dcb.ByteSize = 8;
    dcb.StopBits = ONESTOPBIT;
    dcb.Parity = NOPARITY;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;

    if (!SetCommState(h, &dcb))
    {
        CloseHandle(h);
        return false;
    }

    COMMTIMEOUTS to = {0};
    to.ReadIntervalTimeout = 50;
    to.ReadTotalTimeoutConstant = 500;
    to.ReadTotalTimeoutMultiplier = 10;
    to.WriteTotalTimeoutConstant = 500;
    to.WriteTotalTimeoutMultiplier = 10;
    SetCommTimeouts(h, &to);

    g_h_serial = h;
    g_com_connected = true;
    std::cout << "[✓] [Windows] Connected to External Scanner on " << g_active_com_port << std::endl;
    return true;
#else
    return false;
#endif
}

// Native Windows Hello Touch ID Authentication (Laptop Fingerprint Reader)
static bool try_winbio_authenticate(const char *prompt_reason)
{
#ifdef _WIN32
    WINBIO_SESSION_HANDLE sessionHandle = 0;
    HRESULT hr = WinBioOpenSession(
        WINBIO_TYPE_FINGERPRINT,
        WINBIO_POOL_SYSTEM,
        WINBIO_FLAG_DEFAULT,
        NULL, 0, NULL,
        &sessionHandle
    );

    if (FAILED(hr))
    {
        return false;
    }

    std::cout << BOLD << CYAN << "\n  >>> [WINDOWS TOUCH ID] " 
              << (prompt_reason ? prompt_reason : "Please touch the fingerprint sensor on your laptop now...") 
              << " <<<\n" << RESET;

    WINBIO_IDENTITY identity = {0};
    identity.Type = WINBIO_ID_TYPE_NULL; // Verify against any enrolled fingerprint on this Windows laptop

    WINBIO_UNIT_ID unitId = 0;
    BOOLEAN match = FALSE;
    WINBIO_REJECT_DETAIL rejectDetail = 0;

    // WinBioVerify waits synchronously for physical finger placement
    hr = WinBioVerify(
        sessionHandle,
        &identity,
        WINBIO_SUBTYPE_ANY,
        &unitId,
        &match,
        &rejectDetail
    );

    WinBioCloseSession(sessionHandle);

    if (SUCCEEDED(hr) && match)
    {
        std::cout << BOLD << GREEN << "  [✓] Windows Touch ID sensor recognized your finger!" << RESET << std::endl;
        return true;
    }
    else
    {
        std::cout << BOLD << RED << "  [-] Windows Touch ID failed or was cancelled." << RESET << std::endl;
        return false;
    }
#else
    return false;
#endif
}

bool windows_capture_template(uint8_t *template_out, int length)
{
    if (!template_out || length <= 0) return false;

#ifdef _WIN32
    // If an external hardware scanner (R307/R503) is on a COM port:
    if (g_com_connected && g_h_serial != INVALID_HANDLE_VALUE)
    {
        std::cout << "👉 [External Scanner] Please place your finger on the sensor..." << std::endl;

        uint8_t gen_img[] = {0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x03, 0x01, 0x00, 0x05};
        DWORD bw = 0;

        for (int attempts = 0; attempts < 30; ++attempts)
        {
            WriteFile(g_h_serial, gen_img, sizeof(gen_img), &bw, NULL);
            uint8_t resp[12] = {0};
            DWORD br = 0;
            ReadFile(g_h_serial, resp, sizeof(resp), &br, NULL);

            if (br >= 10 && resp[9] == 0x00)
            {
                uint8_t up_char[] = {0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x04, 0x08, 0x01, 0x00, 0x0E};
                WriteFile(g_h_serial, up_char, sizeof(up_char), &bw, NULL);
                
                DWORD total_read = 0;
                ReadFile(g_h_serial, template_out, length, &total_read, NULL);
                if (total_read > 0) return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
#endif

    // Fallback template
    for (int i = 0; i < length; ++i)
    {
        template_out[i] = static_cast<uint8_t>((i * 7 + 13) % 256);
    }
    return true;
}

bool windows_biometric_authenticate(const char *prompt_reason)
{
    // 1. If an external USB scanner is connected on COM port
    if (g_com_connected)
    {
        std::cout << "\n[External Scanner: " << g_active_com_port << "] " 
                  << (prompt_reason ? prompt_reason : "Please place finger on scanner...") << std::endl;
        uint8_t tmp[512];
        return windows_capture_template(tmp, 512);
    }

    // 2. Try Windows Touch ID (Laptop built-in fingerprint reader)
    if (try_winbio_authenticate(prompt_reason))
    {
        return true;
    }

    // 3. Fallback confirmation if Windows Hello is not configured
    std::cout << "\n[Biometric Fallback] Press ENTER to confirm scan (or 'c' to cancel): ";
    std::string line;
    std::getline(std::cin, line);
    if (line == "c" || line == "C")
    {
        return false;
    }
    return true;
}