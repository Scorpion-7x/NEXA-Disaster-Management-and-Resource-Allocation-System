#pragma once
#include <cstdio>
static int g_fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); g_fails++; } } while (0)
#define TEST_END() do { std::printf(g_fails ? "FAILED (%d)\n" : "ALL PASSED\n", g_fails); return g_fails ? 1 : 0; } while (0)
