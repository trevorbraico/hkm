#pragma once

#include <cstdint>

void randname(char *buf);

int create_shm_file();

int allocate_shm_file(std::size_t size);
