// =============================================================================
// pty_win.cpp — Windows PTY implementation using CreatePseudoConsole()
// =============================================================================
// Requires Windows 10 build 1809+. Uses the ConPTY API to create a
// pseudo-console, then launches the shell process connected to it.
// =============================================================================

#ifdef _WIN32

#include "pty.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <consoleapi.h>
#include <process.h>
#include <vector>
#include <cstring>

namespace xterm
{

    PTY::~PTY()
    {
        kill();
    }

    bool PTY::spawn(const std::string &shell_path, int rows, int cols,
                    const std::vector<std::string> &args)
    {
        HRESULT hr;

        // Create pipes for PTY I/O
        HANDLE hPipeInRead = INVALID_HANDLE_VALUE;
        HANDLE hPipeInWrite = INVALID_HANDLE_VALUE;
        HANDLE hPipeOutRead = INVALID_HANDLE_VALUE;
        HANDLE hPipeOutWrite = INVALID_HANDLE_VALUE;

        if (!CreatePipe(&hPipeInRead, &hPipeInWrite, nullptr, 0))
            return false;
        if (!CreatePipe(&hPipeOutRead, &hPipeOutWrite, nullptr, 0))
        {
            CloseHandle(hPipeInRead);
            CloseHandle(hPipeInWrite);
            return false;
        }

        // Create the pseudo console
        COORD size;
        size.X = static_cast<SHORT>(cols);
        size.Y = static_cast<SHORT>(rows);

        HPCON hPC = nullptr;
        hr = CreatePseudoConsole(size, hPipeInRead, hPipeOutWrite, 0, &hPC);
        if (FAILED(hr))
        {
            CloseHandle(hPipeInRead);
            CloseHandle(hPipeInWrite);
            CloseHandle(hPipeOutRead);
            CloseHandle(hPipeOutWrite);
            return false;
        }

        // Close handles that the pseudo console now owns
        CloseHandle(hPipeInRead);
        CloseHandle(hPipeOutWrite);

        // Store our read/write handles
        hPipeIn_ = hPipeInWrite;
        hPipeOut_ = hPipeOutRead;
        hPC_ = hPC;

        // Set up STARTUPINFOEX with the pseudo console
        SIZE_T attrListSize = 0;
        InitializeProcThreadAttributeList(nullptr, 1, 0, &attrListSize);

        std::vector<BYTE> attrListBuf(attrListSize);
        auto pAttrList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attrListBuf.data());

        if (!InitializeProcThreadAttributeList(pAttrList, 1, 0, &attrListSize))
        {
            kill();
            return false;
        }

        if (!UpdateProcThreadAttribute(pAttrList, 0,
                                       PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
                                       hPC, sizeof(HPCON), nullptr, nullptr))
        {
            DeleteProcThreadAttributeList(pAttrList);
            kill();
            return false;
        }

        STARTUPINFOEXA si;
        ZeroMemory(&si, sizeof(si));
        si.StartupInfo.cb = sizeof(STARTUPINFOEXA);
        si.lpAttributeList = pAttrList;

        // Set environment variables for the child process
        SetEnvironmentVariableA("XELL_TERMINAL", "1");
        SetEnvironmentVariableA("COLORTERM", "truecolor");
        SetEnvironmentVariableA("TERM", "xterm-256color");

        // Build the command line
        std::string cmdLine = shell_path;
        for (const auto &arg : args)
        {
            cmdLine += " ";
            cmdLine += arg;
        }

        PROCESS_INFORMATION pi;
        ZeroMemory(&pi, sizeof(pi));

        BOOL success = CreateProcessA(
            nullptr,
            const_cast<char *>(cmdLine.c_str()),
            nullptr, nullptr,
            FALSE,
            EXTENDED_STARTUPINFO_PRESENT,
            nullptr, nullptr,
            &si.StartupInfo,
            &pi);

        DeleteProcThreadAttributeList(pAttrList);

        if (!success)
        {
            kill();
            return false;
        }

        CloseHandle(pi.hThread);
        hProcess_ = pi.hProcess;

        // Set the read pipe to non-blocking mode
        DWORD mode = PIPE_NOWAIT;
        SetNamedPipeHandleState(static_cast<HANDLE>(hPipeOut_), &mode, nullptr, nullptr);

        return true;
    }

    void PTY::write(const std::string &data)
    {
        if (!hPipeIn_)
            return;

        DWORD written = 0;
        DWORD remaining = static_cast<DWORD>(data.size());
        const char *ptr = data.data();

        while (remaining > 0)
        {
            BOOL ok = WriteFile(static_cast<HANDLE>(hPipeIn_), ptr, remaining, &written, nullptr);
            if (!ok)
                break;
            ptr += written;
            remaining -= written;
        }
    }

    std::string PTY::read()
    {
        if (!hPipeOut_)
            return {};

        char buf[4096];
        std::string result;

        while (true)
        {
            DWORD bytesRead = 0;
            BOOL ok = ReadFile(static_cast<HANDLE>(hPipeOut_), buf, sizeof(buf), &bytesRead, nullptr);
            if (ok && bytesRead > 0)
            {
                result.append(buf, bytesRead);
            }
            else
            {
                break;
            }
        }

        return result;
    }

    void PTY::resize(int rows, int cols)
    {
        if (!hPC_)
            return;

        COORD size;
        size.X = static_cast<SHORT>(cols);
        size.Y = static_cast<SHORT>(rows);

        ResizePseudoConsole(static_cast<HPCON>(hPC_), size);
    }

    bool PTY::is_alive() const
    {
        if (!hProcess_)
            return false;

        DWORD exitCode;
        if (GetExitCodeProcess(static_cast<HANDLE>(hProcess_), &exitCode))
        {
            return exitCode == STILL_ACTIVE;
        }
        return false;
    }

    void PTY::kill()
    {
        if (hProcess_)
        {
            TerminateProcess(static_cast<HANDLE>(hProcess_), 0);
            CloseHandle(static_cast<HANDLE>(hProcess_));
            hProcess_ = nullptr;
        }

        if (hPC_)
        {
            ClosePseudoConsole(static_cast<HPCON>(hPC_));
            hPC_ = nullptr;
        }

        if (hPipeIn_)
        {
            CloseHandle(static_cast<HANDLE>(hPipeIn_));
            hPipeIn_ = nullptr;
        }

        if (hPipeOut_)
        {
            CloseHandle(static_cast<HANDLE>(hPipeOut_));
            hPipeOut_ = nullptr;
        }
    }

} // namespace xterm

#endif // _WIN32
