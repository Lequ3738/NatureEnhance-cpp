#pragma once
#include "Main.h"

expReal ne_list_create(GMString info);
expReal ne_list_destroy(GMReal id);
expReal ne_map_create(GMString info);
expReal ne_map_destroy(GMReal id);
expReal ne_stack_create(GMString info);
expReal ne_stack_destroy(GMReal id);
expReal ne_queue_create(GMString info);
expReal ne_queue_destroy(GMReal id);
expReal ne_grid_create(GMReal w, GMReal h, GMString info);
expReal ne_grid_destroy(GMReal id);
expReal ne_priority_create(GMString info);
expReal ne_priority_destroy(GMReal id);
expReal ne_list_read_buffer(GMReal list, GMReal buffer, GMReal types);
expReal ne_list_write_buffer(GMReal list, GMReal buffer, GMReal types);
expReal ds_exists(GMReal id, GMReal type);

template <typename... Args>
int ne_add_list(Args... args);