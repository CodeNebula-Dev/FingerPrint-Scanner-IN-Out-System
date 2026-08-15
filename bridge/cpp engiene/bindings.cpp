#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>
#include <cstring>

#include "include/engine.h"
#include "include/dailylog.h"
#include "include/fingerprint.h"
#include "include/home_db.h"
#include "include/indexer.h"
#include "include/main.h"
#include "include/master_db.h"
#include "include/serializer.h"
#include "include/touch_id.h"

namespace py = pybind11;

struct RawBuffer {
    const uint8_t* data;
    size_t length;
};

static RawBuffer extract_buffer(const py::buffer& buf) {
    py::buffer_info info = buf.request();
    if (info.ndim != 1) {
        throw std::runtime_error("Input buffer must be 1-dimensional");
    }
    return { static_cast<const uint8_t*>(info.ptr), static_cast<size_t>(info.size) };
}

// Helper to extract raw buffer pointer and size from Python bytes/bytearray/numpy array
struct RawBuffer {
    const uint8_t* date;
    size_t length;
};

static RawBuffer extract_buffer(const py::buffer& buf){
    py::buffer_info info = buf.request();
    if (info.ndim != 1) {
        throw std::runtime_error("input buffer must be 1-dimensional");
    }
    return { static_cast<const uint8_t*>(info.ptr), static_cast<size_t>(info.size) };
}

PYBIND11_MODULE(fingerprint_engine, m){
    m.doc()= "Python bindings for C++ Biometric FingerPrint Engine.";
    
    // 1.STRUCTS AND DATATYPES

    //STUDENT RECORD STRUCTURE
    py::class_<StudentRecord>(m, "StudentRecord")
        .def(py::init<>())
        .def_property("roll_number",
                [](const StudentRecord& r) { return std::string(r.roll_number); },
                [](StudentRecord& r, const std::string& val) { std::strncpy(r.roll_number, val.c_str(), sizeof(r.roll_number) - 1); })
        .def_property("name",
            [](const StudentRecord& r) {return std::string(r.name); },
            [] (StudentRecord& r, const std::string& val) {std::strncpy(r.name, val.c_str(), sizeof(r.name)-1); })
        .def_property("program", 
            [](const StudentRecord r) {return std::string(r.program); },
            [](StudentRecord& r, const std::string& val) {std::strncpy(r.program, val.c_str(), sizeof((r.program)-1)); })
        .def_property("batch",
                [](const StudentRecord& r) { return std::string(r.batch); },
                [](StudentRecord& r, const std::string& val) { std::strncpy(r.batch, val.c_str(), sizeof(r.batch) - 1); })
        .def_readwrite("year", &StudentRecord::year)
        .def_property("phone_number",
                [](const StudentRecord& r) { return std::string(r.phone_number); },
                [](StudentRecord& r, const std::string& val) { std::strncpy(r.phone_number, val.c_str(), sizeof(r.phone_number) - 1); })
        .def_readwrite("is_hosteller", &StudentRecord::is_hosteller)
        .def_property("fingerprint_template",
                [](const StudentRecord& r) { return py::bytes(reinterpret_cast<const char*>(r.fingerprint_template), 512); },
                [](StudentRecord& r, py::buffer buf) {
                    RawBuffer raw = extract_buffer(buf);
                    size_t len = raw.length < 512 ? raw.length : 512;
                    std::memcpy(r.fingerprint_template, raw.data, len);
                    if (len < 512) std::memset(r.fingerprint_template + len, 0, 512 - len);
                });
    
    //LogEntry Struct
    py::class_<LogEntry>(m, "LogEntry")
        .def(py::init<>())
        .def_property("roll_number",
            [](const LogEntry& e) { return std::string(e.roll_number); },
            [](LogEntry& e, const std::string& val) { std::strncpy(e.roll_number, val.c_str(), sizeof(e.roll_number) - 1); })
        .def_property("name", 
            [](const LogEntry& e) { return std::string(e.name); }, 
            [](LogEntry& e, const std::string& val) { std::strncpy(e.name, val.c_str(), sizeof(e.name)-1); })
        .def_readwrite("year", &LogEntry::year)
        .def_property("reason", 
            [](const LogEntry& e) { return std::string(e.reason); },
            [](LogEntry& e, const std::string& val) { std::strncpy(e.reason, val.c_str(), sizeof(e.reason) - 1); })
        .def_readwrite("gate_count", &LogEntry::gate_count)
        .def_property("status", 
            [](const LogEntry& e) { return std::string(e.status); },
            [](LogEntry& e, const std::string& val) { std::strncpy(e.status, val.c_str(), sizeof(e.status) - 1); })
        .def_readwrite("late_return", &LogEntry::late_return)
        .def_readwrite("timestamp_count", &LogEntry::timestamp_count)
        .def_property_readonly("timestamps", [](const LogEntry& e){
            std::vector<std::string> ts_list;
            for (int i = 0; i < e.timestamp_count && i < MAX_TIMESTAMPS; ++i){
                ts_list.push_back(std::string(e.timestamps[i]));
            }
            return ts_list;
        });

    //HomeRecord STRUCT
    py::class_<HomeRecord>(m, "HomeRecord")
        .def(py::init<>())
        .def_property("roll_number",
        [](const HomeRecord& r) { return std::string(r.roll_number); },
        [](HomeRecord& r, const std::string& val) {
            std::strncpy(r.roll_number, val.c_str(), sizeof(r.roll_number) - 1);
            r.roll_number[sizeof(r.roll_number) - 1] = '\0';
        });
    
    // MatchResult Struct
    py::class_<MatchResult>(m, "MatchResult")
        .def(py::init<>())
        .def_readwrite("matched", &MatchResult::matched)
        .def_readwrite("year", &MatchResult::year)
        .def_readwrite("is_hosteller", &MatchResult::is_hosteller)
        .def_readwrite("confidence_score", &MatchResult::confidence_score)
        .def_readwrite("match_count", &MatchResult::match_count)
        .def_property("roll_number",
            [](const MatchResult& r) { return std::string(r.roll_number); },
            [](MatchResult& r, const std::string& val) { std::strncpy(r.roll_number, val.c_str(), sizeof(r.roll_number) - 1); })
        .def_property("name",
            [](const MatchResult& r) { return std::string(r.name); },
            [](MatchResult& r, const std::string& val) { std::strncpy(r.name, val.c_str(), sizeof(r.name) - 1); })
        .def_property("program",
            [](const MatchResult& r) { return std::string(r.program); },
            [](MatchResult& r, const std::string& val) { std::strncpy(r.program, val.c_str(), sizeof(r.program) - 1); })
        .def_property("batch",
            [](const MatchResult& r) { return std::string(r.batch); },
            [](MatchResult& r, const std::string& val) { std::strncpy(r.batch, val.c_str(), sizeof(r.batch) - 1); })
        .def_property("phone_number",
            [](const MatchResult& r) { return std::string(r.phone_number); },
            [](MatchResult& r, const std::string& val) { std::strncpy(r.phone_number, val.c_str(), sizeof(r.phone_number) - 1); })
        .def("__repr__", [](const MatchResult& r) {
            if (!r.matched) return std::string("<MatchResult matched=False>");
            return "<MatchResult matched=True name='" + std::string(r.name) + 
                   "' roll='" + std::string(r.roll_number) + 
                   "' score=" + std::to_string(r.confidence_score) + ">";
        });

    // RejectionEntry Struct
    py::class_<RejectionEntry>(m, "RejectionEntry")
        .def(py::init<>())
        .def_property_readonly("timestamp", [](const RejectionEntry& self) { return std::string(self.timestamp); })
        .def_property_readonly("failed_template", [](const RejectionEntry& self) {
            return py::bytes(reinterpret_cast<const char*>(self.failed_template), 512);
        });

    // CachedFingerprint Struct
    py::class_<CachedFingerprint>(m, "CachedFingerprint")
        .def(py::init<>())
        .def_readwrite("hash", &CachedFingerprint::hash)
        .def_property("roll_number",
            [](const CachedFingerprint& c) { return std::string(c.roll_number); },
            [](CachedFingerprint& c, const std::string& val) { std::strncpy(c.roll_number, val.c_str(), sizeof(c.roll_number) - 1); })
        .def_property("file_path",
            [](const CachedFingerprint& c) { return std::string(c.file_path); },
            [](CachedFingerprint& c, const std::string& val) { std::strncpy(c.file_path, val.c_str(), sizeof(c.file_path) - 1); });
    
    // BatchIndexEntry Struct
    py::class_<BatchIndexEntry>(m, "BatchIndexEntry")
        .def(py::init<>())
        .def_property("roll_number",
            [](const BatchIndexEntry& b) { return std::string(b.roll_number); },
            [](BatchIndexEntry& b, const std::string& val) { std::strncpy(b.roll_number, val.c_str(), sizeof(b.roll_number) - 1); });

    //2.ENGINE LIFECYCLE AND CORE APIS

    m.def("engine_init", &engine_init, py::arg("project_root_path"), "Initialize the engine with project root dictionary.");
    m.def("engine_shutdown", &engine_shutdown, "Shutdown the engine and cleanup");
    m.def("engine_wipe_all_data", &engine_wipe_all_data, "[DEV ONLY] Wipe all database data");

    //3.MASTER DATABASE OPERATIONS
    m.def("student_add", &student_add, py::arg("record"), "Add a new Student Record");
    m.def("student_remove", &student_remove, py::arg("roll_number"), "Remove a student by roll number");
    m.def("student_update", &student_update, py::arg("roll_number"), py::arg("update_record"), "Updates an existing student record");

    m.def("student_get", [](const char* roll_number) {
        StudentRecord record;
        bool success = student_get(roll_number, record);
        return py::make_tuple(success, record);
    }, py::arg("roll_number"), "Get student record by roll number. Returns tuple: (success: bool, record: StudentRecord)");

    m.def("student_list_by_batch", &student_list_by_batch, py::arg("batch"));
    m.def("student_list_all", &student_list_all);
    m.def("batch_promote", &batch_promote, py::arg("batch"));
    m.def("batch_promote_all", &batch_promote_all);
    m.def("batch_delete", &batch_delete, py::arg("batch"));

    // 4. FINGERPRINT & MATCHING OPERATIONS
    m.def("fingerprint_match", [](py::buffer live_scan) {
        RawBuffer buf = extract_buffer(live_scan);
        return fingerprint_match(buf.data, static_cast<int>(buf.length));
    }, py::arg("live_scan"), "Match live scan template buffer against database.");

    m.def("fingerprint_enroll", [](const char* roll_number, py::buffer template_data) {
        RawBuffer buf = extract_buffer(template_data);
        return fingerprint_enroll(roll_number, buf.data, static_cast<int>(buf.length));
    }, py::arg("roll_number"), py::arg("template_data"), "Enroll/update fingerprint template for a student.");

    m.def("rejection_log_write", [](const char* date_string, py::buffer failed_scan) {
        RawBuffer buf = extract_buffer(failed_scan);
        return rejection_log_write(date_string, buf.data, static_cast<int>(buf.length));
    }, py::arg("date_string"), py::arg("failed_scan"), "Log unmatched template scan.");

    //5.DAILY LOG OPERATIONS
    m.def("log_create_day", &log_create_day, py::arg("date_string"));
    m.def("log_day_exists", &log_day_exists, py::arg("date_string"));
    m.def("log_add_entry", &log_add_entry, py::arg("date_string"), py::arg("entry"));
    m.def("log_update_entry", &log_update_entry, py::arg("date_string"), py::arg("roll_number"), py::arg("updated_entry"));

    m.def("log_got_entry", [](const char* date_string, const char* roll_number) {
        LogEntry entry;
        bool success = log_get_entry(date_string, roll_number, entry);
        return py::make_tuple(success, entry);
    }, py::arg("date_string"), py::arg("roll_number"), "Returns tuple: (success: bool, entry: LogEntry)");

    m.def("log_get_all_entries", &log_get_all_entries, py::arg("date_string"));
    m.def("log_get_entries_in_range", &log_get_entries_in_range, py::arg("start_date"), py::arg("end_date"));
    m.def("log_delete_day", &log_delete_day, py::arg("date_string"));

    //6.hOME DATABASE OPERATIONS
    m.def("home_add", &home_add, py::arg("record"));
    m.def("home_remove", &home_remove, py::arg("roll_number"));
    m.def("home_exists", &home_exists, py::arg("roll_number"));
    m.def("home_get_all", &home_get_all);

    //7.INDEXER OPERATIONS
    m.def("compute_fnv1a_hash", [](py::buffer data) {
        RawBuffer buf = extract_buffer(data);
        return compute_fnv1a_hash(buf.data, buf.length);
    }, py::arg("data"));
    m.def("indexer_load", &indexer_load);
    m.def("indexer_add_or_update", &indexer_add_or_update, py::arg("roll_number"), py::arg("hash"), py::arg("relative_path"));
    m.def("indexer_remove", &indexer_remove, py::arg("roll_number"));

    //8.SERIALIZER OPERATIONS
    m.def("serialize_student", &serialize_student, py::arg("filepath"), py::arg("student"));
    
    m.def("deserialize_student", [](const std::string& filepath) {
        StudentRecord student;
        bool success = deserialize_student(filepath, student);
        return py::make_tuple(success, student);
    }, py::arg("filepath"), "Deserializes student record. Returns tuple: (success: bool, student: StudentRecord)");

    m.def("serialize_fingerprint", [](const std::string& filepath, py::buffer template_data) {
        RawBuffer buf = extract_buffer(template_data);
        return serialize_fingerprint(filepath, buf.data, static_cast<int>(buf.length));
    }, py::arg("filepath"), py::arg("template_data"));

    m.def("deserialize_fingerprint", [](const std::string& filepath, int max_length = 512) {
        std::vector<uint8_t> buffer(max_length);
        bool success = deserialize_fingerprint(filepath, buffer.data(), max_length);
        return py::make_tuple(success, py::bytes(reinterpret_cast<const char*>(buffer.data()), max_length));
    }, py::arg("filepath"), py::arg("max_length") = 512, "Returns tuple: (success: bool, template_bytes: bytes)");

    m.def("serialize_log_entries", &serialize_log_entries, py::arg("filepath"), py::arg("entries"));
    
    m.def("deserialize_log_entries", [](const std::string& filepath) {
        std::vector<LogEntry> entries;
        bool success = deserialize_log_entries(filepath, entries);
        return py::make_tuple(success, entries);
    }, py::arg("filepath"), "Returns tuple: (success: bool, entries: list[LogEntry])");

    m.def("serialize_home_records", &serialize_home_records, py::arg("filepath"), py::arg("records"));
    
    m.def("deserialize_home_records", [](const std::string& filepath) {
        std::vector<HomeRecord> records;
        bool success = deserialize_home_records(filepath, records);
        return py::make_tuple(success, records);
    }, py::arg("filepath"), "Returns tuple: (success: bool, records: list[HomeRecord])");

    //9.macOS TOUCH ID MODULE
    m.def("macos_touch_id_authenticate", &macos_touch_id_authenticate, py::arg("prompt_reason"), 
          "Triggers native macOS Touch ID biometric authentication popup.");
}