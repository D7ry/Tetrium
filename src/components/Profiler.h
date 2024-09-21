#pragma once

// inspired by cgc profiler

// Scope-based profiler utility that records time and 
// call hierarchy information for subroutines.
class Profiler
{
  public:
#ifdef __linux__
    using TimeUnit = std::chrono::time_point<
    std::chrono::system_clock,
    std::chrono::duration<long, std::ratio<1, 1000000000>>>;
#else 
    using TimeUnit = std::chrono::time_point<
    std::chrono::steady_clock,
    std::chrono::duration<long, std::ratio<1, 1000000000>>>;
#endif

    struct Profling
    {
        Profiler* parent;
        int id;
        Profling() = delete;

        Profling(Profiler* parent, const char* name) : parent(parent) {
            id = parent->Push(name);
        }

        ~Profling() { parent->Pop(id); }
    };

    struct Entry
    {
        const char* name;
        TimeUnit begin;
        TimeUnit end;
        int level;
    };

    Profiler() {
        _currEntryLevel = 0;
        _profileData = std::make_unique<std::vector<Profiler::Entry>>();
    }


    int Push(const char* name) {
        Entry entry;
        entry.name = name;
        entry.begin = std::chrono::high_resolution_clock::now();
        entry.level = _currEntryLevel;

        _profileData->push_back(entry);

        int entryId = _profileData->size() - 1;
        _currEntryLevel++;
        return entryId;
    }

    void Pop(int entryId) {
        Entry& entry = _profileData->at(entryId);
        entry.end = std::chrono::high_resolution_clock::now();
        _currEntryLevel--;
    }

    // Clears all entries that has been profiled, 
    // returns all entries that has been profiled. should be called every Tick
    std::unique_ptr<std::vector<Profiler::Entry>> NewProfile() {
        _currEntryLevel = 0;
        auto lastProfileData = std::move(_profileData);
        _profileData = std::make_unique<std::vector<Profiler::Entry>>();
        return lastProfileData;
    }

  private:
    int _currEntryLevel = 0;
    std::unique_ptr<std::vector<Profiler::Entry>> _profileData;
};

// profiler macros

#define USE_PROFILER

#ifdef USE_PROFILER
#define PROFILE_SCOPE(profiler, name)                                          \
    const auto __profling = Profiler::Profling(profiler, name);
#else
#define PROFILE_SCOPE(profiler) (0)
#endif
