#include "bindings.h"

PYBIND11_MODULE(fingerprint_engine, m){
    m.doc() = "Python bindings for C++ Biometric Fingerprint Engine v2.0 (Windows)";
    
    //1.Data Structures & Classes 
    //Registers StudentRecord, LogEntry, HomeRecord, MathResult, IndexEntry, BioHashConfig
    init_types(m);

    //2.Global Engine Lifecycle PYTHON_API_STRING
    //Registers engine_init, engine_shutdown, engine_wipe_all_data
    init_engine(m);

    //3. Database Operations (Master DB, Daily Activity Los, Home Leaves)
    //Registers student_add/get/list, log_create_day/add, home_add/list, batch_promote
    init_database(m);

    //4. Biometric & Hardware Driver APIs
    //Registers fingerprint_match, fingerprint_enroll, windows_biometric_authenticate, COM ports
    init_biometric(m);

    //5. High-Speed In-Memory Indexer APIs
    //Registers indexer_insert, indexer_loopup_candidates, indexer_hash_template
    init_indexer(m);

    //6. Binary Disk Persistence APIs
    // Registers serializer_write_student, serializer_read_log_entries, etc.
    init_serializer(m);
}