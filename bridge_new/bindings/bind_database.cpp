#include "bindings.h"
#include "../include/engine.h"

void init_database(py::module_&m) {

    //1.Master Student Profile Database CRUD APIs

    m.def("student_add", &student_add,
        py::arg("record"),
        "Add a new StudentRecord to the Master Database.");
    
    m.def("student_remove", &student_remove,
        py::arg("roll_number"),
        "Permanently remove a student record by roll number.");
    
    m.def("student_update", &student_update,
        py::arg("roll_number"), py::arg("updated_record"),
        "Update an existing student record.");
    
    m.def("student_get", [](const char* roll_number) {
        StudentRecord record;
        bool success = student_get(roll_number, record);
        return py::make_tuple(success, record);
    }, py::arg("roll_number"),
        "Fetch a student profile by roll number. Returns tuple: (success: bool, record: StudentRecord)");
    
    m.def("student_list_by_batch", &student_list_by_batch,
        py::arg("batch"),
        "List all students belonging to a specific batch year (e.g. '2026').");
    
    m.def("student_list_all", &student_list_all,
        "Retrieve all student records from the Master Database.");

    m.def("batch_promote", &batch_promote,
        py::arg("batch"),
        "Increment the academic year of all atudents in a specified batch. returns count of modified records.");
    
    m.def("batch_promote_all", &batch_promote_all,
        "Increment the academic year of all active students across all batches.");

    m.def("batch_delete", &batch_delete,
        py::arg("batch"),
        "Delete all student records belonging to a graduated/specified batch.");

    //Daily Gate Activity Log APIs

    m.def("log_create_day", &log_create_day,
        py::arg("data_string"),
        "Create a daily gate log file for a specific date (Format: 'DD_MM_YYYY').");

    m.def("log_day_exists", &log_day_exists,
        py::arg("date_string"),
        "Check if a daily log file exists for the given date string.");
    
    m.def("log_add_entry", &log_add_entry,
        py::arg("date_string"), py::arg("entry"),
        "Append or record a new LogEntry in the daily gate log.");

    m.def("log_update_entry", &log_update_entry,
        py::arg("date_string"), py::arg("roll_number"), py::arg("updated_entry"),
        "Update a student's existing log entry for the specified day.");

    m.def("log_get_entry", [](const char* date_string, const char* roll_number) {
        LogEntry entry;
        bool success = log_get_entry(date_string, roll_number, entry);
        return py::make_tuple(success, entry);
    }, py::arg("date_string"), py::arg("roll_number"),
        "Retrieve a student's daily log entry. Returns tuple: (success: bool, entry: LogEntry)");

    m.def("log_get_all_entries", &log_get_all_entries,
        py::arg("date_string"),
        "Retrieve all log entries recorded on a specified date.");

    m.def("log_get_entries_in_range", &log_get_entries_in_range,
        py::arg("start_date"), py::arg("end_date"),
        "Retrieve all daily log entries across a date range.");

    m.def("log_delete_day", &log_delete_day,
        py::arg("date_string"),
        "Delete the entire daily gate log for a specified date.");

    //3.Approved Home Leaves Registry APIs

    m.def("home_add", &home_add,
        py::arg("record"),
        "Record an approved home leave permit for a student.");

    m.def("home_remove", &home_remove,
        py::arg("roll_number"),
        "Remove a student's active home leave permit upon campus return.");

    m.def("home_exists", &home_exists,
        py::arg("roll_number"),
        "Check if a student currently has an active home leave recorded.");

    m.def("home_get_all", &home_get_all,
        "Retrieve all active home leave records.");
}