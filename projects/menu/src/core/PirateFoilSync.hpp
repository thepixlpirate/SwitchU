#pragma once

// "Sync backgrounds from PirateFoil": downloads per-game clips and pictures
// from a PirateFoil server into /config/SwitchU/videos. Runs only when the
// player chooses Sync Now (never automatically).
//
// Only files listed in the record file (.piratefoil-sync.json) are ever
// replaced or deleted; anything the player copied in by hand is left alone.

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

namespace piratefoil {

constexpr const char* kVideoFolder = "sdmc:/config/SwitchU/videos";
constexpr const char* kRecordFile = "sdmc:/config/SwitchU/videos/.piratefoil-sync.json";
constexpr std::uint64_t kMaxFileBytes = 10ull * 1024ull * 1024ull;

// Shared between the worker and the UI (polled each frame).
struct Progress {
    std::mutex mutex;
    std::string message;
    float progress01 = -1.f;     // < 0 = indeterminate
    std::uint64_t revision = 0;
};

struct Result {
    bool ok = false;             // the list was fetched (individual files may still fail)
    bool cancelled = false;
    std::string error;           // a readable reason when !ok
    int added = 0;
    int updated = 0;
    int removed = 0;
    int failed = 0;
    int tooBig = 0;              // skipped: over 10 MB
    int keptYours = 0;           // skipped: a file of yours already has that name
    int unchanged = 0;
    int listed = 0;
};

// Trims spaces and trailing slashes; adds http:// when no scheme is given.
std::string normaliseAddress(const std::string& address);

// Blocking; run it on a worker thread.
Result runSync(const std::string& address, const std::string& apiKey,
               const std::shared_ptr<Progress>& progress,
               const std::shared_ptr<std::atomic<bool>>& cancel);

// "3 added, 1 updated, 0 removed, 0 failed" (+ skipped notes).
std::string summaryText(const Result& result);

} // namespace piratefoil
