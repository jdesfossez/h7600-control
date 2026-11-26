#pragma once
// Parser for the "quick" sigfile text the H7600 returns for SigfileWantQuick.
// A sigfile is one module (operator) per entry: "<TYPE> <name> <args...>".
// Entries may wrap onto continuation lines; a new entry starts with an
// upper-case operator word at the start of a line.

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace h7600
{
struct SigEntry
{
    int moduleIndex = 0;              // 1-based position in the file (HEAD* is 1)
    std::string type;                 // "KNOB", "MENUPAGE", ...
    std::vector<std::string> tokens;  // arguments after the type, quotes stripped
    std::string name() const { return tokens.empty() ? std::string{} : tokens[0]; }
};

enum class KnobKind { Knob, PercentKnob, TextKnob };

struct Knob
{
    KnobKind kind = KnobKind::Knob;
    int moduleIndex = 0;
    std::string name;    // e.g. "Km_dlys"
    std::string format;  // printf-style label, e.g. "m_delay: %3.0f %%"
    std::string unit;
    double min = 0, max = 0, step = 1, value = 0;
    std::vector<std::string> options; // TextKnob choices; value is an index

    std::string paramId() const;      // for makeParamSet
    std::string label() const;        // format text before the first ':' or '%'
    std::string displayValue() const; // value rendered with format
};

struct Sigfile
{
    std::string programName;
    std::string author;              // TAIL argument
    std::vector<SigEntry> entries;
    std::vector<Knob> knobs;

    const Knob* findKnob(std::string_view name) const;
    const Knob* findKnobByModule(int moduleIndex) const;
};

std::optional<Sigfile> parseSigfile(std::string_view text);

} // namespace h7600
