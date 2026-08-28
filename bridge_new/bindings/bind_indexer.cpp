#include "bindings.h"
#include "../include/indexer.h"

#include <vector>
#include <string>

void init_indexer(py::module_&m) {
    //In-Memory Level-1 Search INdexer & Hashing APIs

    m.def("indexer_init", &indexer_init,
        "Initialize or re-initialize the Level-1 in-memory search indexer.");
    
    m.def("indexer_clear", &indexer_clear,
        "Clear all indexed candidate hashes from memory.");

    m.def("indexer_insert", [](const char* roll_number, py::buffer encrypted_template) {
        RawBuffer buf = extract_buffer(encrypted_template);
        indexer_insert(roll_number, buf.data, buf.length);
    }, py::arg("roll_number"), py::arg("encrypted_template"),
        "Insert or update a student's template hash in the in-memory Level-1 lookup table.");

    m.def("indexer_remove", &indexer_remove,
        py::arg("roll_number"),
        "Remove a student's entry from the in-memory indexer by roll number.");

    m.def("indexer_hash_template", [](py::buffer encrypted_template) {
        RawBuffer buf = extract_buffer(encrypted_template);
        return indexer_hash_template(buf.data, buf.length);
    }, py::arg("encrypted_template"),
        "Compute the 64-bit FNV-1a Level-1 locality-preserving index hash for a template buffer.");

    m.def("indexer_lookup_candidates", &indexer_lookup_candidates,
        py::arg("template_hash"),
        "Look up matching student roll numbers from RAM using their template hash.");
}