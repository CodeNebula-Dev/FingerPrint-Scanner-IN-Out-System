#include "bindings.h"
#include "../include/engine.h"

void init_engine(py::module_&m){
    //Global Engine Lifecycle & Environment Management APIs

    m.def("engine_init", &engine_init,
        py::arg("project_root_path") = "",
        "Initialize the biometric engine, ensure required directory structures, "
        "load the master crypto key, and hydrate the in-memory Level-1 search indexer.");
    
    m.def("engine_shutdown", &engine_shutdown,
        "Safely shutdown the engine, clear in-memory RAM indexers, and flush pending buffers.");

    m.def("engine_wipe_all_data", &engine_wipe_all_data,
        "[DEV/ADMIN ONLY] Permanently wipe all student database, daily logs, home records, "
        "and rejections from disk.");
}