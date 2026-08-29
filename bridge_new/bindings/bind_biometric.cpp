#include "bindings.h"
#include "../include/engine.h"
#include "../include/windows_biometric.h"
#include "../include/crypto_placeholder.h"

#include <vector>
#include <string>

void init_biometric(py::module_&m) {

    //1. Biometric Matching & Enrollemnt APIs

    m.def("fingerprint_match", [](py::buffer live_scan) {
        RawBuffer buf = extract_buffer(live_scan);
        return fingerprint_match(buf.data, static_cast<int>(buf.length));
    }, py::arg("live_scan"),
        "Match a live biometric scan buffer against the encrypted student database. Returns MatchResult.");

    m.def("fingerprint_enroll", [](const char* roll_number, py::buffer template_data) {
        RawBuffer buf = extract_buffer(template_data);
        return fingerprint_enroll(roll_number, buf.data, static_cast<int>(buf.length));
    }, py::arg("roll_number"), py::arg("template_data"),
       "Transform via BioHash and enroll a 512-byte biometric template for an existing student record.");

    m.def("rejection_log_write", [](const char* date_string, py::buffer failed_scan) {
        RawBuffer buf = extract_buffer(failed_scan);
        return rejection_log_write(date_string, buf.data, static_cast<int>(buf.length));
    }, py::arg("date_string"), py::arg("failed_scan"),
        "Log an unrecognized or failed biometric scan attempt to disk.");

    //2.Windows Hardware Scanner & Driver APIs

    m.def("windows_biometric_authenticate", &windows_biometric_authenticate,
        py::arg("prompt_reason") = "Biometric Verification Required",
        "Trigger authentication via active scanner (Hardware COM -> WinBio -> simulation fallback).");

    m.def("windows_capture_template", [](int length) {
        if (length <= 0) length = 512;
        std::vector<uint8_t> temp_buf(length, 0);
        bool success = windows_capture_template(temp_buf.data(), length);
        py::bytes captured_bytes = py::bytes(reinterpret_cast<const char*> (temp_buf.data()), length);
        return py::make_tuple(success, captured_bytes);
    }, py::arg("length") = 512,
        "Capture raw biometric template from connected scanner. Returns tuple: (success: bool, template_bytes: bytes)");

    m.def("windows_set_com_port", &windows_set_com_port,
        py::arg("port_name"), py::arg("baudrate") = 57600,
        "Configure active serial COM port for optical scanner (e.g. 'COM3', baudrate=57600.");

    m.def("windows_list_com_ports", &windows_list_com_ports,
        "Scan Windows Device Manager and list all available hardware COM serial ports.");

    //3. BioHash Security & Key Management APIs

    m.def("crypto_get_scheme_name", &crypto_get_scheme_name,
        "Get human-readable name of the active cancelable biometric encryption scheme.");

    m.def("crypto_get_active_scheme", &crypto_get_active_scheme,
        "Get active CancelableCryptoScheme enum value.");

    m.def("crypto_set_key", &crypto_set_key,
        py::arg("seed"),
        "Set master BioHash encryption seed value.");

    m.def("crypto_rotate_key", []() {
        uint64_t new_seed = 0;
        bool success = crypto_rotate_key(new_seed);
        return py::make_tuple(success, new_seed);
    }, "Rotate encryption key without re-scanning fingers. Returns tuple: (success: bool, new_seed: int)");
}