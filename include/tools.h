#pragma once
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

int inputAvailable();

int stat_mkdir(const char *filepath, mode_t mode);

int mkdir_path(const char *filepath, mode_t mode);