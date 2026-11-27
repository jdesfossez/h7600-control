#include "ModuleDbStore.h"

#include "h7600/Protocol.h"

juce::File ModuleDbStore::cacheFile()
{
    auto base = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
   #if JUCE_MAC
    base = base.getChildFile("Application Support");
   #endif
    return base.getChildFile("H7600 Control").getChildFile("sigdbase.txt");
}

ModuleDbStore::ModuleDbStore()
{
    auto f = cacheFile();
    if (f.existsAsFile())
        if (auto parsed = h7600::ModuleDb::parse(f.loadFileAsString().toStdString()))
            db = std::move(*parsed);
}

juce::String ModuleDbStore::describe() const
{
    if (! db) return "No module database yet";
    return "Module database: " + juce::String((int) db->modules().size()) + " modules";
}

bool ModuleDbStore::setFromText(const std::string& text, juce::String* error)
{
    std::string err;
    auto parsed = h7600::ModuleDb::parse(text, &err);
    if (! parsed)
    {
        if (error) *error = "Not a module database: " + juce::String(err);
        return false;
    }
    db = std::move(*parsed);
    auto f = cacheFile();
    f.getParentDirectory().createDirectory();
    f.replaceWithData(text.data(), text.size());
    return true;
}

bool ModuleDbStore::importFile(const juce::File& f, juce::String* error)
{
    juce::MemoryBlock mb;
    if (! f.loadFileAsData(mb))
    {
        if (error) *error = "Cannot read " + f.getFullPathName();
        return false;
    }
    std::string text(static_cast<const char*>(mb.getData()), mb.getSize());
    // A raw 0C capture (.syx): take the payload text.
    if (auto frame = h7600::parseFrame(h7600::Bytes(text.begin(), text.end())))
        if (frame->code == static_cast<std::uint8_t>(h7600::Code::SigdbaseDump))
            text = h7600::payloadText(*frame);
    if (auto nul = text.find('\0'); nul != std::string::npos) text.resize(nul);
    return setFromText(text, error);
}
