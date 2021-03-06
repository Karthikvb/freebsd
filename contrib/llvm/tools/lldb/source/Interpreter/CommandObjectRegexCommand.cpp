//===-- CommandObjectRegexCommand.cpp ---------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "lldb/lldb-python.h"

#include "lldb/Interpreter/CommandObjectRegexCommand.h"

// C Includes
// C++ Includes
// Other libraries and framework includes
// Project includes
#include "lldb/Interpreter/CommandInterpreter.h"
#include "lldb/Interpreter/CommandReturnObject.h"

using namespace lldb;
using namespace lldb_private;

//----------------------------------------------------------------------
// CommandObjectRegexCommand constructor
//----------------------------------------------------------------------
CommandObjectRegexCommand::CommandObjectRegexCommand
(
    CommandInterpreter &interpreter,
    const char *name,
    const char *help,
    const char *syntax,
    uint32_t max_matches,
    uint32_t completion_type_mask
) :
    CommandObjectRaw (interpreter, name, help, syntax),
    m_max_matches (max_matches),
    m_completion_type_mask (completion_type_mask),
    m_entries ()
{
}

//----------------------------------------------------------------------
// Destructor
//----------------------------------------------------------------------
CommandObjectRegexCommand::~CommandObjectRegexCommand()
{
}


bool
CommandObjectRegexCommand::DoExecute
(
    const char *command,
    CommandReturnObject &result
)
{
    if (command)
    {
        EntryCollection::const_iterator pos, end = m_entries.end();
        for (pos = m_entries.begin(); pos != end; ++pos)
        {
            RegularExpression::Match regex_match(m_max_matches);

            if (pos->regex.Execute (command, &regex_match))
            {
                std::string new_command(pos->command);
                std::string match_str;
                char percent_var[8];
                size_t idx, percent_var_idx;
                for (uint32_t match_idx=1; match_idx <= m_max_matches; ++match_idx)
                {
                    if (regex_match.GetMatchAtIndex (command, match_idx, match_str))
                    {
                        const int percent_var_len = ::snprintf (percent_var, sizeof(percent_var), "%%%u", match_idx);
                        for (idx = 0; (percent_var_idx = new_command.find(percent_var, idx)) != std::string::npos; )
                        {
                            new_command.erase(percent_var_idx, percent_var_len);
                            new_command.insert(percent_var_idx, match_str);
                            idx += percent_var_idx + match_str.size();
                        }
                    }
                }
                // Interpret the new command and return this as the result!
                if (m_interpreter.GetExpandRegexAliases())
                    result.GetOutputStream().Printf("%s\n", new_command.c_str());
                // Pass in true for "no context switching".  The command that called us should have set up the context
                // appropriately, we shouldn't have to redo that.
                return m_interpreter.HandleCommand(new_command.c_str(), eLazyBoolCalculate, result, nullptr, true, true);
            }
        }
        result.SetStatus(eReturnStatusFailed);
        if (GetSyntax() != nullptr)
            result.AppendError (GetSyntax());
        else
            result.AppendErrorWithFormat ("Command contents '%s' failed to match any regular expression in the '%s' regex command.\n",
                                          command,
                                          m_cmd_name.c_str());
        return false;
    }
    result.AppendError("empty command passed to regular expression command");
    result.SetStatus(eReturnStatusFailed);
    return false;
}


bool
CommandObjectRegexCommand::AddRegexCommand (const char *re_cstr, const char *command_cstr)
{
    m_entries.resize(m_entries.size() + 1);
    // Only add the regular expression if it compiles
    if (m_entries.back().regex.Compile (re_cstr, REG_EXTENDED))
    {
        m_entries.back().command.assign (command_cstr);
        return true;
    }
    // The regex didn't compile...
    m_entries.pop_back();
    return false;
}

int
CommandObjectRegexCommand::HandleCompletion (Args &input,
                                             int &cursor_index,
                                             int &cursor_char_position,
                                             int match_start_point,
                                             int max_return_elements,
                                             bool &word_complete,
                                             StringList &matches)
{
    if (m_completion_type_mask)
    {
        std::string completion_str (input.GetArgumentAtIndex (cursor_index), cursor_char_position);
        CommandCompletions::InvokeCommonCompletionCallbacks (m_interpreter,
                                                             m_completion_type_mask,
                                                             completion_str.c_str(),
                                                             match_start_point,
                                                             max_return_elements,
                                                             nullptr,
                                                             word_complete,
                                                             matches);
        return matches.GetSize();
    }
    else
    {
        matches.Clear();
        word_complete = false;
    }
    return 0;
}
