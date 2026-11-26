#pragma once
// Favorites, user tags and recents, persisted in a properties file on the Mac
// (~/Library/Application Support/H7600 Control/user.settings). Never sent to the unit.

#include <juce_data_structures/juce_data_structures.h>

#include "h7600/PresetCatalog.h"

class UserStore
{
public:
    UserStore();
    h7600::UserData& data() { return user; }
    void save();

private:
    std::unique_ptr<juce::PropertiesFile> file;
    h7600::UserData user;
};
