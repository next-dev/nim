//----------------------------------------------------------------------------------------------------------------------
// Command line processing
//----------------------------------------------------------------------------------------------------------------------

#pragma once

#include <set>

//----------------------------------------------------------------------------------------------------------------------

class CmdLine
{
public:
    using CmdHandler = function<int(const CmdLine&)>;
public:
    CmdLine(int argc, char** argv);

    func command() const -> const string&;
    func exePath() const -> const string&;
    func numParams() const -> i64;
    func param(i64 i) const -> const string&;
    func flag(char flag) const -> bool;
    func longFlag(string name) const->string;
    func secondaryParams() const -> const vector<string>&;

    func addCommand(string&& cmd, CmdHandler handler) -> void;
    func dispatch() -> int;

private:
    string m_exePath;
    string m_command;
    vector<string> m_params;
    vector<string> m_secondaryParams;
    set<char> m_flags;
    map<string, string> m_longFlags;
    map<string, CmdHandler> m_handlers;
};

