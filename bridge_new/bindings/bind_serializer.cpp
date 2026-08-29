#include "bindings.h"
#include "../include/serializer.h"
#include "../include/engine.h"

#include <string>
#include <vector>

void init_serializer(py::module_ &m) {

    //Binary File Disk Serialization & Deserialization APIs

    m.def("serializer_write_student", &serializer_write_student,
        py::arg("filepath"), py::arg("record"),
        "Write a single StudentRecord binary file (.dat) to disk.");

    m.def("serializer_read_student", [](const std::string& filepath) {
        StudentRecord record;
        bool success = serializer_read_student(filepath, record);
        return py::make_tuple(success, record);
    }, py::arg("filepath"),
        "Read a StudentRecord binary file from disk. Returns tuple (success: bool, record: StudentRecord)");

    m.def("serializer_write_log_entry", &serializer_write_log_entry,
        py::arg("filepath"), py::arg("entry"),
        "Append or write a LogEntry to a daily log file.");

    m.def("serializer_read_log_entries", [](const std::string& filepath) {
        std::vector<LogEntry> entries;
        bool success = serializer_read_log_entries(filepath, entries);
        return py::make_tuple(success, entries);
    }, py::arg("filepath"),
        "Read all LogEntry records from a binary log file. Returns tuple: (success: bool, entries: list[LogEntry])");

    m.def("serializer_write_home_record", &serializer_write_home_record,
            py::arg("filepath"), py::arg("record"),
            "Append or write a HomeRecord permit to the home leaves database file.");

    m.def("serializer_read_home_records", [](const std::string& filepath) {
        std::vector<HomeRecord> records;
        bool success = serializer_read_home_records(filepath, records);
        return py::make_tuple(success, records);
    }, py::arg("filepath"),
        "Read all HomeRecord permits from the disk. Returns tuple: (success: bool, records: list[HomeRecord])");

    m.def("serializer_append_rejection", [](const std::string& filepath, py::buffer scan_data) {
        RawBuffer buf = extract_buffer(scan_data);
        return serializer_append_rejection(filepath, buf.data, static_cast<int> (buf.length));
    }, py::arg("filepath"), py::arg("scan_data"),
        "Append a failed/unmatched biometric scan payload to the rejection audit log file.");
}