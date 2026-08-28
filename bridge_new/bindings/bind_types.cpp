#include "bindings.h"
#include "../include/engine.h"
#include "../include/indexer.h"
#include "../include/crypto_placeholder.h"

#include <cstring>
#include <string>
#include <vector>

void init_types(py::module_ &m) {
    //1.Cancelable Crypto Scheme Enum & Config
    py::enum_<CancelableCryptoScheme>(m, "CancelableCryptoScheme")
        .value("PLACEHOLDER_PASSTHROUGH", CancelableCryptoScheme::PLACEHOLDER_PASSTHROUGH)
        .value("BIOHASHING_STANDARD", CancelableCryptoScheme::BIOHASHING_STANDARD)
        .value("MATRIX_PROJECTION", CancelableCryptoScheme::MATRIX_PROJECTION)
        .value("POLYPROTECT_STANDARD", CancelableCryptoScheme::POLYPROTECT_STANDARD)
        .export_values();

    py::class_<BioHashConfig>(m, "BioHashConfig")
        .def(py::init<>())
        .def_readwrite("seed", &BioHashConfig::seed)
        .def_readwrite("projection_dim", &BioHashConfig::projection_dim)
        .def_readwrite("key_loaded", &BioHashConfig::key_loaded)
        .def("__repr__", [](const BioHashConfig& c) { 
            return "<BioHashConfig seed=" + std::to_string(c.seed) + " dim=" + std::to_string(c.projection_dim) + " loaded=" + (c.key_loaded ? "True" : "False") + ">";
        });

    //2.StudentRecord (Master Database Model)
    py::class_<StudentRecord>(m, "StudentRecord")
        .def(py::init<>())
        .def_property("roll_number",
            [](const StudentRecord& r) { return std::string(r.roll_number); },
            [](StudentRecord& r, const std::string& val) {
                std::strncpy(r.roll_number, val.c_str(), sizeof(r.roll_number) - 1);
                r.roll_number[sizeof(r.roll_number) - 1] = '\0' ;
            })
        .def_property("name",
            [](const StudentRecord& r) { return std::string(r.name); },
            [](StudentRecord& r, const std::string& val) {
                std::strncpy(r.name, val.c_str(), sizeof(r.name)- 1);
                r.name[sizeof(r.name) - 1] = '\0';
            })
        .def_property("program",
            [](const StudentRecord& r) { return std::string(r.program); },
            [](StudentRecord& r, const std::string& val) {
                std::strncpy(r.program, val.c_str(), sizeof(r.program) - 1);
                r.program[sizeof(r.program) - 1] = '\0';
            })
        .def_property("batch",
            [](const StudentRecord& r) { return std::string(r.batch); },
            [](StudentRecord& r, const std::string& val) {
                std::strncpy(r.batch, val.c_str(), sizeof(r.batch) - 1);
                r.batch[sizeof(r.batch) - 1] = '\0';
            })
        .def_readwrite("year", &StudentRecord::year)
        .def_property("phone_number",
            [](const StudentRecord& r) { return std::string(r.phone_number); },
            [](StudentRecord& r, const std::string& val) {
                std::strncpy(r.phone_number, val.c_str(), sizeof(r.phone_number) - 1);
                r.phone_number[sizeof(r.phone_number) - 1] = '\0';
            })
        .def_readwrite("is_hosteller", &StudentRecord::is_hosteller)
        .def_property("encrypted_template",
            [](const StudentRecord& r) {
                return py::bytes(reinterpret_cast<const char*> (r.encrypted_template), TEMPLATE_SIZE);
            },
            [](StudentRecord& r, py::buffer buf) {
                RawBuffer raw = extract_buffer(buf);
                size_t len = raw.length < TEMPLATE_SIZE ? raw.length : TEMPLATE_SIZE;
                std::memcpy(r.encrypted_template, raw.data, len);
                if (len < TEMPLATE_SIZE) {
                    std::memset(r.encrypted_template + len, 0, TEMPLATE_SIZE - len);
                }
            })
        .def("__repr__", [](const StudentRecord& r) {
            return "<StudentRecd roll='" + std::string(r.roll_number) + "'name='" + std::string(r.name) + "'batch='" + std::string(r.batch) + "'>";
        });

    //3. LogEntry (Daily Gate Activity Model)
    py::class_<LogEntry>(m, "LogEntry")
        .def(py::init<>())
        .def_property("roll_number",
            [](const LogEntry& e) { return std::string(e.roll_number); },
            [](LogEntry& e, const std::string& val) {
                std::strncpy(e.roll_number, val.c_str(), sizeof(e.roll_number) - 1);
                e.roll_number[sizeof(e.roll_number) - 1] = '\0';
            })
        .def_property("name",
            [](const LogEntry& e) { return std::string(e.name); },
            [](LogEntry& e, const std::string& val) {
                std::strncpy(e.name, val.c_str(), sizeof(e.name) - 1);
                e.name[sizeof(e.name) - 1] = '\0';
            })
        .def_readwrite("year", &LogEntry::year)
        .def_property("reason",
            [](const LogEntry& e) { return std::string(e.reason);},
            [](LogEntry& e, const std::string& val) {
                std::strncpy(e.reason, val.c_str(), sizeof(e.reason) - 1);
                e.reason[sizeof(e.reason) - 1] = '\0';
            })
        .def_readwrite("gate_count", &LogEntry::gate_count)
        .def_property("status",
            [](const LogEntry& e) { return std::string(e.status); },
            [](LogEntry& e, const std::string& val) {
                std::strncpy(e.status, val.c_str(), sizeof(e.status) - 1);
                e.status[sizeof(e.status) - 1] = '\0';
            })
        .def_readwrite("late_return", &LogEntry::late_return)
        .def_readwrite("timestamp_count", &LogEntry::timestamp_count)
        .def_property_readonly("timestamps", [](const LogEntry& e) {
            std::vector<std::string> ts_list;
            for (int i = 0; i < e.timestamp_count && i <MAX_TIMESTAMPS; ++i) {
                ts_list.push_back(std::string(e.timestamps[i]));
            }
            return ts_list;
        })
        .def("__repr__", [](const LogEntry& e) {
            return "<LogEntry roll='" + std::string(e.roll_number) + 
            "' status='" + std::string(e.status) + 
            "' scans=" + std::to_string (e.gate_count) + ">";
        });
    
    //4.HomeRecord (Approved Home Leave Model)
    py::class_<HomeRecord>(m, "HomeRecord")
        .def(py::init<>())
        .def_property("roll_number",
            [](const HomeRecord& r) { return std::string(r.roll_number); },
            [](HomeRecord& r, const std::string& val) {
                std::strncpy(r.roll_number, val.c_str(), sizeof(r.roll_number) - 1);
                r.roll_number[sizeof(r.roll_number) - 1] = '\0';
            })
        .def_property("name",
            [](const HomeRecord& r) { return std::string(r.name); },
            [](HomeRecord& r, const std::string& val) {
                std::strncpy(r.name, val.c_str(), sizeof(r.name) - 1);
                r.name[sizeof(r.name) - 1] = '\0';
            })
        .def_readwrite("year", &HomeRecord::year)
        .def_property("phone_number",
            [](const HomeRecord& r) { return std::string(r.phone_number); },
            [](HomeRecord& r, const std::string& val) {
                std::strncpy(r.phone_number, val.c_str(), sizeof(r.phone_number) - 1);
                r.phone_number[sizeof(r.phone_number) - 1] = '\0';
            })
        .def_property("date_of_leaving",
            [](const HomeRecord& r) { return std::string(r.date_of_leaving); },
            [](HomeRecord& r, const std::string& val) {
                std::strncpy(r.date_of_leaving, val.c_str(), sizeof(r.date_of_leaving) - 1);
                r.date_of_leaving[sizeof(r.date_of_leaving) - 1] = '\0';
            })
        .def_property("time_of_leaving",
            [](const HomeRecord& r) { return std::string(r.time_of_leaving); },
            [](HomeRecord& r, const std::string& val) {
                std::strncpy(r.time_of_leaving, val.c_str(), sizeof(r.time_of_leaving) - 1);
                r.time_of_leaving[sizeof(r.time_of_leaving) - 1] = '\0';
            })
        .def("__repr__", [](const HomeRecord& r) {
            return "<HomeRecord roll='" + std::string(r.roll_number) + "' date='" + std::string(r.date_of_leaving) + "'>";
        });

    //5. MatchResult (Biometric Evaluation Model)
    py::class_<MatchResult>(m, "MatchResult")
        .def(py::init<>())
        .def_readwrite("matched", &MatchResult::matched)
        .def_property("roll_number",
            [](const MatchResult& r) { return std::string(r.roll_number); },
            [](MatchResult& r, const std::string& val) {
                std::strncpy(r.roll_number, val.c_str(), sizeof(r.roll_number) - 1);
                r.roll_number[sizeof(r.roll_number) - 1] = '\0';
            })
        .def_property("name",
            [](const MatchResult& r) { return std::string(r.name); },
            [](MatchResult& r, const std::string& val) {
                std::strncpy(r.name, val.c_str(), sizeof(r.name) - 1);
                r.name[sizeof(r.name) - 1] = '\0';
            })
        .def_property("program",
            [](const MatchResult& r) { return std::string(r.program); },
            [](MatchResult& r, const std::string& val) {
                std::strncpy(r.program, val.c_str(), sizeof(r.program) - 1);
                r.program[sizeof(r.program) - 1] = '\0';
            })
        .def_property("batch",
            [](const MatchResult& r) { return std::string(r.batch); },
            [](MatchResult& r, const std::string& val) {
                std::strncpy(r.batch, val.c_str(), sizeof(r.batch) - 1);
                r.batch[sizeof(r.batch) - 1] = '\0';
            })
        .def_readwrite("year", &MatchResult::year)
        .def_property("phone_number",
            [](const MatchResult& r) { return std::string(r.phone_number); },
            [](MatchResult& r, const std::string& val) {
                std::strncpy(r.phone_number, val.c_str(), sizeof(r.phone_number) - 1);
                r.phone_number[sizeof(r.phone_number) - 1] = '\0';
            })
        .def_readwrite("is_hosteller", &MatchResult::is_hosteller)
        .def_readwrite("confidence_score", &MatchResult::confidence_score)
        .def_readwrite("match_count", &MatchResult::match_count)
        .def("__repr__", [](const MatchResult& r) {
            if (!r.matched) return std::string("<MatchResult matched=False>");
            return "<MatchResult matched=True name='" + std::string(r.name) +
                "' roll='" + std::string(r.roll_number) +
                "' score=" + std::to_string(r.confidence_score) + ">";
        });

        //6. IndexEntry (RAM Indexer Model)
        py::class_<IndexEntry>(m, "IndexEntry")
            .def(py::init<>())
            .def_property("roll_number",
                [](const IndexEntry& i) { return std::string(i.roll_number); },
                [](IndexEntry& i, const std::string& val) {
                    std::strncpy(i.roll_number, val.c_str(), sizeof(i.roll_number) - 1);
                    i.roll_number[sizeof(i.roll_number) - 1] = '\0';
                })
            .def_readwrite("template_hash", &IndexEntry::template_hash)
            .def("__repr__", [](const IndexEntry& i) {
                return "<IndexEntry roll='" + std::string(i.roll_number) +
                "' hash=" + std::to_string(i.template_hash) + ">";
            });
}