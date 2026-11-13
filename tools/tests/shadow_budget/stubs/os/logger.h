#pragma once
namespace fixture { template<class... T> void Log(const char*, T&&...) {} }
#define LOG_INFO(...) fixture::Log(__VA_ARGS__)
