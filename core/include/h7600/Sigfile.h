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

/// A MENUPAGE with the knobs it shows directly (indices into Sigfile::knobs).
struct MenuPage
{
    std::string name;  // operator name, e.g. "mastersmenu"
    std::string title; // e.g. "master params"
    std::string tab;   // short soft-key label, e.g. "main"
    std::vector<std::size_t> knobs;
};

struct Sigfile
{
    std::string text;                // the sigfile text this was parsed from
    std::string programName;
    std::string author;              // TAIL argument
    std::vector<SigEntry> entries;
    std::vector<Knob> knobs;

    const Knob* findKnob(std::string_view name) const;
    const Knob* findKnobByModule(int moduleIndex) const;

    /// Pages reachable from the HEAD's menus (depth-first, as on the unit),
    /// keeping only pages that show at least one knob. Knobs on no page are
    /// collected in a final "other" page.
    std::vector<MenuPage> pages() const;
};

/// Knobs whose value in `sf` differs from what was sent (paramId -> value text), e.g. because the
/// unit ignored a message it acknowledged. Values are compared after quantizing to the knob's step.
struct KnobMismatch
{
    const Knob* knob = nullptr;
    std::string sent;
};
std::vector<KnobMismatch> knobMismatches(const Sigfile& sf, const std::vector<std::pair<std::string, std::string>>& sent);

/// Snaps a value to the knob's step and range.
double quantize(const Knob& k, double v);
/// Text sent in a ParamSet for value v: integer for integer steps, otherwise
/// as many decimals as the step needs (e.g. step 0.01 -> "0.66").
std::string valueText(const Knob& k, double v);

std::optional<Sigfile> parseSigfile(std::string_view text);

} // namespace h7600
