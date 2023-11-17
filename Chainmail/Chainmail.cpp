// Chainmail.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <boost/program_options.hpp>
#include <regex>
#include <numeric>
#include <ranges>
#include <string>
#include <format>
#include <Windows.h>
#include <tlhelp32.h>

namespace po = boost::program_options;

bool run_next(const std::string& cmd)
{
    STARTUPINFOA        si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    char* csCmd = (char*) malloc(cmd.length() + 1);

    if (!csCmd)
    {
        std::cerr << "Failed to allocate memory";
        return false;
    }

    strcpy(csCmd, cmd.c_str());

    if (!CreateProcessA(NULL, csCmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
    {
        DWORD dwErr     = GetLastError();
        LPSTR errorText = NULL;

        FormatMessageA(
            // use system message tables to retrieve error text
            FORMAT_MESSAGE_FROM_SYSTEM
                // allocate buffer on local heap for error text
                | FORMAT_MESSAGE_ALLOCATE_BUFFER
                // Important! will fail otherwise, since we're not
                // (and CANNOT) pass insertion parameters
                | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, // unused with FORMAT_MESSAGE_FROM_SYSTEM
            dwErr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPSTR) &errorText, // output
            0,                  // minimum size for output buffer
            NULL);              // arguments - see note

        if (errorText != NULL)
        {
            std::cerr << errorText << std::endl;

            // release memory allocated by FormatMessage()
            LocalFree(errorText);
            errorText = NULL;
        }

        return false;
    }

    // Clean up handles
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return true;
}

/// Wait for processes by handles
bool wait_for_processes(const std::vector<HANDLE>& handles)
{
    bool bResult   = true;
    auto processes = handles | std::views::transform(GetProcessId) |
                     std::views::transform(static_cast<std::string (*)(DWORD)>(std::to_string));

    std::cout << "Waiting on processes: "
              << std::accumulate(processes.begin(), processes.end(), std::string(""),
                                 [](const std::string& a, const std::string& b) {
                                     if (a.empty())
                                     {
                                         return b;
                                     }

                                     if (b.empty())
                                     {
                                         return a;
                                     }
                                     return a + ", " + b;
                                 })
              << "...\n";

    DWORD dwWaitResult = WaitForMultipleObjects((DWORD) handles.size(), handles.data(), TRUE, INFINITE);

    try
    {
        if ((dwWaitResult >= WAIT_ABANDONED_0 && dwWaitResult <= (WAIT_ABANDONED_0 + handles.size() - 1)) ||
            dwWaitResult == WAIT_TIMEOUT || dwWaitResult == WAIT_FAILED)
        {
            throw std::runtime_error("Wait for processes failed.");
        }

        std::cout << "Finished waiting for processes.\n";
    }
    catch (const std::runtime_error& error)
    {
        std::cerr << error.what() << std::endl;
        bResult = false;
    }

    return bResult;
}

std::vector<HANDLE> get_process_handles_by_glob(const std::vector<std::string>& globs)
{
    HANDLE              hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    std::vector<HANDLE> handles;

    if (hSnap == INVALID_HANDLE_VALUE)
    {
        DWORD dwErr     = GetLastError();
        LPSTR errorText = NULL;

        FormatMessageA(
            // use system message tables to retrieve error text
            FORMAT_MESSAGE_FROM_SYSTEM
                // allocate buffer on local heap for error text
                | FORMAT_MESSAGE_ALLOCATE_BUFFER
                // Important! will fail otherwise, since we're not
                // (and CANNOT) pass insertion parameters
                | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, // unused with FORMAT_MESSAGE_FROM_SYSTEM
            dwErr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPSTR) &errorText, // output
            0,                  // minimum size for output buffer
            NULL);              // arguments - see note

        if (errorText != NULL)
        {
            std::cerr << errorText << std::endl;

            // release memory allocated by FormatMessage()
            LocalFree(errorText);
            errorText = NULL;
        }

        return handles;
    }

    PROCESSENTRY32 pe;
    ZeroMemory(&pe, sizeof(PROCESSENTRY32));
    pe.dwSize = sizeof(PROCESSENTRY32);
    if (!Process32First(hSnap, &pe))
    {
        std::cerr << "Failed to read first process. Your system probably has bigger issues. Good luck!\n";
        return handles;
    }

    do
    {
        std::string procName(pe.szExeFile);

        for (auto& sGlob : globs)
        {
            std::regex reg(sGlob, std::regex_constants::ECMAScript | std::regex_constants::icase);

            std::smatch match;

            if (std::regex_match(procName, match, reg))
            {
                HANDLE hProc = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_INFORMATION, FALSE, pe.th32ProcessID);
                if (hProc != NULL)
                {
                    handles.push_back(hProc);
                }
                else
                {
                    std::cerr << std::format("Failed to open process {}\n", procName);
                }
            }
        }
    } while (Process32Next(hSnap, &pe));

    return handles;
}

std::vector<HANDLE> get_process_handles_by_pid(const std::vector<unsigned long>& pids)
{
    std::vector<HANDLE> handles;

    for (auto pid : pids)
    {
        HANDLE hProc = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_INFORMATION, FALSE, pid);
        if (hProc != NULL)
        {
            handles.push_back(hProc);
        }
        else
        {
            std::cerr << std::format("Failed to open process {}\n", pid);
        }
    }

    return handles;
}

bool close_handles(const std::vector<HANDLE>& handles)
{
    bool bResult = true;
    for (auto& hProc : handles)
    {
        bResult &= (bool) CloseHandle(hProc);
    }

    return bResult;
}

int main(int argc, char* argv[])
{
    po::options_description options("Available arguments");
    options.add_options()("help,h", "Show this help message")("pids,ps", po::value<std::vector<unsigned long>>(),
                                                              "PIDs of processes to wait for")(
        "procs", po::value<std::vector<std::string>>(), "Names of processes to wait for. Supports globbing")(
        "delay,d", po::value<long>()->default_value(0L), "Delay, in seconds, before running the next command")(
        "next,n", po::value<std::string>(), "Command to run after waited processes exit");

    po::variables_map var_map;

    try
    {
        po::store(po::parse_command_line(argc, argv, options), var_map);

        if (var_map.count("help"))
        {
            std::cout << options << std::endl;
            return 0;
        }

        if (!var_map.count("next"))
        {
            std::cout << "Missing command to run after processes end\n";
            std::cout << options << std::endl;
            return -1;
        }

        std::vector<HANDLE> handles;
        if (var_map.count("pids"))
        {
            handles = get_process_handles_by_pid(var_map["pids"].as<std::vector<unsigned long>>());
        }
        else if (var_map.count("procs"))
        {
            handles = get_process_handles_by_glob(var_map["procs"].as<std::vector<std::string>>());
        }
        else
        {
            std::cerr << "Neither \"pids\", not \"procs\" specified. What are we waiting for?\n";
            return -1;
        }

        if (!wait_for_processes(handles))
        {
            return -1;
        }

        if (var_map.count("delay"))
        {
            long delay = var_map["delay"].as<long>();
            if (delay > 0)
            {
                std::cout << std::format("Sleeping for {} second(s)\n", delay);
                Sleep((DWORD) delay * 1000);
            }
        }

        return run_next(var_map["next"].as<std::string>()) ? 0 : -1;
    }
    catch (std::exception& e)
    {
        std::cerr << std::format("Error: {}\n", e.what());
    }
    catch (...)
    {
        std::cerr << "Exception of unknown type!\n";
    }

    return -1;
}
