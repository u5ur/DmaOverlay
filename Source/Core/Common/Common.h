// Common.h
#pragma once
#define NOMINMAX
#include <Windows.h>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include <memory>
#include <utility>
#include <cmath>
#include <algorithm>
#include <mutex>
#include <thread>
#include <type_traits>

#define LOG(format, ...) printf(format, ##__VA_ARGS__)
#ifdef _DEBUG
#define DEBUG(format, ...) printf(format, ##__VA_ARGS__)
#else
#define DEBUG(format, ...) ((void)0)
#endif