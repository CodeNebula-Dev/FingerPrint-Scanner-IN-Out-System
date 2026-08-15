#ifndef MAIN_H
#define MAIN_H

#include "engine.h"
#include "touch_id.h"
#include "indexer.h"
#include <string>
#include <vector>

bool is_after_curfew();
void generate_mock_template(const std::string& roll, uint8_t* template_out);
void print_header(const std::string& title);
void print_success(const std::string& msg);
void print_warning(const std::string& msg);
void print_error(const std::string& msg);
void do_enroll();
void do_scan();
void do_view_logs();
void do_curfew_check();
void do_home_list();
void do_batch_promote();
void do_view_master_db();

#endif 
