#pragma once

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>
#include <cstdint>
#include <cstddef>
#include <stdexcept>

namespace py= pybind11;

// 1.Shared Buffer Utilities (accross serializer, biometric, and types)

struct RawBuffer {
    const uint8_t* data;
    size_t length;
};

// Safely extracts a raw byte pointer from Python bytes, bytearray, or 1D numpy array
inline RawBuffer extract_buffer(const py::buffer& buf) {
    py::buffer_info info = buf.request();
    if (info.ndim != 1) {
        throw std::runtime_error("Input buffer must be 1-dimensional");
    }
    return { static_cast<const uint8_t*>(info.ptr), static_cast<size_t>(info.size)};
}

// 2. Susystem Binding Declarations

//Registers shared structs: StudentRecord, LogEntry, HomeRecord, MathResult, IndexEntry
void init_types(py::module_&m);

//Registers engine lifecycle (engine_init, shutdown, wipe) and student CURD
void init_engine(py::module_ &m);

//Registers biometric operations (fingerprint_match, enroll, windows hardware biometric)
void init_biometric(py::module_ &m);

//Registers high-performance in-memory indexer operations
void init_indexer(py::module_ &m);

//Registers low-level serialization / disk persistence functions
void init_serializer(py::module_ &m);

// Registers Master Student, Daily Log, and Home Leave database CRUD operations
void init_database(py::module_ &m);