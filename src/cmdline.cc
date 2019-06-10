//---------------------------------------------------------------------------------------------------------------------
// Command processing.
//---------------------------------------------------------------------------------------------------------------------

#include <core.h>
#include <cmdline.h>

//---------------------------------------------------------------------------------------------------------------------
// Constructor

CmdLine::CmdLine(int argc, char** argv)
{
    //
    // Get executable path name
    //

    int len = MAX_PATH;
    for (;;)
    {
        char* buf = new char[len];
        if (buf)
        {
            DWORD pathLen = GetModuleFileNameA(0, buf, len);
            if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
            {
                len = 3 * len / 2;
                delete[] buf;
                continue;
            }

            while (buf[pathLen] != '\\') --pathLen;
            buf[pathLen] = 0;

            m_exePath = buf;
            delete[] buf;
            break;
        }
    }

    //
    // Get command
    //

    if (argc > 1)
    {
        m_command = argv[1];
        argv += 2;
    }

    //
    // Get parameters and flags
    //

    while (*argv)
    {
        if (**argv == '-')
        {
            if ((*argv)[1] == '-')
            {
                if ((*argv)[2] == 0)
                {
                    // Double hyphen - start of secondary parameters.
                    ++argv;
                    while (*argv != 0)
                    {
                        m_secondaryParams.emplace_back(*argv);
                        ++argv;
                    }
                    break;
                }
                else
                {
                    // Long flags: --<word> <parameter>
                    if (**(argv + 1) == 0)
                    {
                        m_longFlags[*argv + 2] = string();
                    }
                    else
                    {
                        m_longFlags[*argv + 2] = string(*(argv + 1));
                        ++argv;
                    }
                }
            }
            else
            {
                // Normal flag -<letter>
                for (char* scan = &(*argv)[1]; *scan != 0; ++scan)
                {
                    m_flags.emplace(*scan);
                }
            }
        }
        else
        {
            m_params.emplace_back(*argv);
        }

        ++argv;
    }
}

//---------------------------------------------------------------------------------------------------------------------
// command
// Returns the command parameter

func CmdLine::command() const -> const string&
{
    return m_command;
}

//---------------------------------------------------------------------------------------------------------------------
// exePath
// Returns the full path of this executable (not including the filename).

func CmdLine::exePath() const -> const string&
{
    return m_exePath;
}

//---------------------------------------------------------------------------------------------------------------------
// numParams
// Returns the number of parameters passed to the command (i.e. non-flag parameters).

func CmdLine::numParams() const -> i64
{
    return m_params.size();
}

//---------------------------------------------------------------------------------------------------------------------
// param
// Returns the nth parameter in the command.

func CmdLine::param(i64 i) const -> const string&
{
    return m_params[i];
}

//---------------------------------------------------------------------------------------------------------------------
// flag
// Returns true if the flag was given (i.e. via '-x')

func CmdLine::flag(char flag) const -> bool
{
    return m_flags.find(flag) != m_flags.end();
}

//----------------------------------------------------------------------------------------------------------------------
// longFlag
// Returns the string associated with that flag (i.e. via '--flag <param>')

func CmdLine::longFlag(string name) const -> string
{
    auto it = m_longFlags.find(name);
    if (it != m_longFlags.end())
    {
        return it->second;
    }
    else
    {
        return "";
    }
}

//----------------------------------------------------------------------------------------------------------------------
// secondaryParams

func CmdLine::secondaryParams() const -> const vector<string>&
{
    return m_secondaryParams;
}

//----------------------------------------------------------------------------------------------------------------------
// addCommand

func CmdLine::addCommand(string&& cmd, CmdHandler handler) -> void
{
    m_handlers[move(cmd)] = handler;
}

//----------------------------------------------------------------------------------------------------------------------
// dispatch

func CmdLine::dispatch() -> int
{
    auto it = m_handlers.find(m_command);
    if (it != m_handlers.end())
    {
        return it->second(*this);
    }
    else
    {
        return -1;
    }
}

//---------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------
