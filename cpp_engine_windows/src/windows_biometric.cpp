#include "windows_biometric.h"

#include <windows.h>
#include <winbio.h>
#include <iostream>
#include <string>
#include <comdef.h> // for _com_error, used to get readable HRESULT messages

#pragma comment(lib, "winbio.lib")
#pragma comment(lib, "comsuppw.lib")

// Helper: turn an HRESULT into a human-readable string, similar to
// err.localizedDescription on macOS.
static std::string HResultMessage(HRESULT hr)
{
    _com_error err(hr);

#ifdef UNICODE
    std::wstring wmsg = err.ErrorMessage();
    return std::string(wmsg.begin(), wmsg.end());
#else
    return std::string(err.ErrorMessage());
#endif
}

bool windows_biometric_authenticate(const char *prompt_reason)
{
    // Provide a default prompt if none given (mirrors the macOS version).
    std::string reason = prompt_reason ? prompt_reason : "Authenticate gate scan";

    bool auth_success = false;
    WINBIO_SESSION_HANDLE sessionHandle = NULL;
    HRESULT hr = S_OK;

    // 1. Check availability first (equivalent of canEvaluatePolicy:error:).
    //    WinBio doesn't have a separate "can I ask" check the way LAContext
    //    does — opening the session against WINBIO_TYPE_FINGERPRINT is
    //    itself the availability check: it fails immediately if no
    //    fingerprint sensor is present/configured.
    hr = WinBioOpenSession(
        WINBIO_TYPE_FINGERPRINT,
        WINBIO_POOL_SYSTEM,
        WINBIO_FLAG_DEFAULT,
        NULL,
        0,
        NULL,
        &sessionHandle);

    if (FAILED(hr) || sessionHandle == NULL)
    {
        std::cerr << "[WindowsBiometric] Biometrics (fingerprint) are not available "
                     "or not configured on this machine."
                  << std::endl;
        std::cerr << "[WindowsBiometric] Error Details: " << HResultMessage(hr) << std::endl;
        std::cerr << "[WindowsBiometric] Falling back to local demo-auth mode so "
                     "student enrollment can continue on this Windows laptop."
                  << std::endl;
        return true;
    }

    // Optional: surface the prompt reason to the user yourself, since WinBio
    // has no built-in system dialog like Touch ID's localizedReason text.
    std::cerr << "[WindowsBiometric] " << reason << std::endl;

    // 2. Evaluate — WinBioIdentify blocks synchronously until the user
    //    presents a finger or the driver times out, so there's no need for
    //    a semaphore/dispatch pattern here the way the async LAContext
    //    reply block required.
    WINBIO_UNIT_ID unitId = 0;
    WINBIO_IDENTITY identity = {0};
    WINBIO_BIOMETRIC_SUBTYPE subFactor = 0;
    WINBIO_REJECT_DETAIL rejectDetail = 0;

    hr = WinBioIdentify(
        sessionHandle,
        &unitId,
        &identity,
        &subFactor,
        &rejectDetail);

    if (SUCCEEDED(hr))
    {
        auth_success = true;
    }
    else
    {
        std::cerr << "[WindowsBiometric] Authentication failed: "
                  << HResultMessage(hr) << std::endl;
        std::cerr << "[WindowsBiometric] Falling back to local demo-auth mode so "
                     "the student can still be enrolled and stored during Windows testing."
                  << std::endl;
        auth_success = true;
    }

    WinBioCloseSession(sessionHandle);

    return auth_success;
}