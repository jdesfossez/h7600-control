#include "UserStore.h"

UserStore::UserStore()
{
    juce::PropertiesFile::Options o;
    o.applicationName = "user";
    o.folderName = "H7600 Control";
    o.filenameSuffix = ".settings";
    o.osxLibrarySubFolder = "Application Support";
    o.storageFormat = juce::PropertiesFile::storeAsXML;
    file = std::make_unique<juce::PropertiesFile>(o);

    for (auto& s : juce::StringArray::fromTokens(file->getValue("favorites"), ",", ""))
        if (s.isNotEmpty()) user.favorites.insert(s.getIntValue());
    for (auto& s : juce::StringArray::fromTokens(file->getValue("recent"), ",", ""))
        if (s.isNotEmpty()) user.recent.push_back(s.getIntValue());
    auto tags = juce::JSON::parse(file->getValue("tags"));
    if (auto* obj = tags.getDynamicObject())
        for (auto& p : obj->getProperties())
            if (auto* arr = p.value.getArray())
                for (auto& t : *arr)
                    user.addTag(p.name.toString().substring(1).getIntValue(), t.toString().toStdString());
}

void UserStore::save()
{
    juce::StringArray fav, rec;
    for (int n : user.favorites) fav.add(juce::String(n));
    for (int n : user.recent) rec.add(juce::String(n));
    auto obj = std::make_unique<juce::DynamicObject>();
    for (auto& [n, list] : user.tags)
    {
        juce::Array<juce::var> arr;
        for (auto& t : list) arr.add(juce::String(t));
        obj->setProperty(juce::Identifier("p" + juce::String(n)), arr);
    }
    file->setValue("favorites", fav.joinIntoString(","));
    file->setValue("recent", rec.joinIntoString(","));
    file->setValue("tags", juce::JSON::toString(juce::var(obj.release()), true));
    file->saveIfNeeded();
}
