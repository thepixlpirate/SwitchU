#include "PirateFoilSync.hpp"

#include "DebugLog.hpp"
#include "themeshop/ThemeHttp.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <vector>

namespace piratefoil {
namespace {

using nlohmann::json;

struct RecordEntry {
    std::int64_t updated = 0;
    std::string format;   // "webp" or "jpg"
};
using Record = std::map<std::string, RecordEntry>;

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

bool contains(const std::string& haystack, const char* needle) {
    return lower(haystack).find(lower(needle)) != std::string::npos;
}

bool validTitleId(const std::string& id) {
    if (id.size() != 16) return false;
    return std::all_of(id.begin(), id.end(),
                       [](unsigned char c) { return std::isxdigit(c) != 0; });
}

std::string upperId(std::string id) {
    std::transform(id.begin(), id.end(), id.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return id;
}

std::string pathFor(const std::string& titleId, const std::string& format) {
    return std::string(kVideoFolder) + "/" + titleId + "." + format;
}

bool fileExists(const std::string& path) {
    std::error_code ec;
    return std::filesystem::is_regular_file(path, ec);
}

bool removeFile(const std::string& path) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) return true;
    return std::filesystem::remove(path, ec) && !ec;
}

Record loadRecord() {
    Record record;
    std::ifstream in(kRecordFile, std::ios::binary);
    if (!in.is_open()) return record;
    try {
        json j = json::parse(in, nullptr, true, true);
        if (!j.is_object()) return record;
        for (auto it = j.begin(); it != j.end(); ++it) {
            if (!it.value().is_object()) continue;
            const std::string id = upperId(it.key());
            if (!validTitleId(id)) continue;
            RecordEntry e;
            e.updated = it.value().value("updated", static_cast<std::int64_t>(0));
            e.format = lower(it.value().value("format", std::string()));
            if (e.format != "webp" && e.format != "jpg") continue;
            record[id] = e;
        }
    } catch (const std::exception& ex) {
        DebugLog::log("[piratefoil] record unreadable: %s", ex.what());
    }
    return record;
}

bool saveRecord(const Record& record) {
    json j = json::object();
    for (const auto& [id, e] : record)
        j[id] = {{"updated", e.updated}, {"format", e.format}};
    const std::string tmp = std::string(kRecordFile) + ".part";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) return false;
        out << j.dump(2);
        out.flush();
        if (!out.good()) return false;
    }
    removeFile(kRecordFile);
    return std::rename(tmp.c_str(), kRecordFile) == 0;
}

// The server's {"error": "..."} text, if the body carries one.
std::string serverError(const std::string& httpMessage) {
    const auto colon = httpMessage.find(": ");
    if (colon == std::string::npos) return {};
    try {
        json j = json::parse(httpMessage.substr(colon + 2));
        if (j.is_object() && j.contains("error") && j["error"].is_string())
            return j["error"].get<std::string>();
    } catch (...) {
    }
    return {};
}

int httpStatus(const std::string& message) {
    const auto pos = message.find("HTTP error ");
    if (pos == std::string::npos) return 0;
    return std::atoi(message.c_str() + pos + 11);
}

// Turns a transport failure into something a person can act on.
std::string describeListError(const std::string& raw, const std::string& address) {
    const int status = httpStatus(raw);
    if (status == 401 || status == 403) {
        const std::string detail = serverError(raw);
        return "PirateFoil rejected the API key" +
               (detail.empty() ? std::string(".") : " (" + detail + ").") +
               " Check PirateFoil API Key.";
    }
    if (status == 404)
        return "That server has no PirateFoil backgrounds list. Check PirateFoil Address.";
    if (status != 0) {
        const std::string detail = serverError(raw);
        return "PirateFoil answered with an error (HTTP " + std::to_string(status) + ")" +
               (detail.empty() ? std::string(".") : ": " + detail);
    }
    if (contains(raw, "Internet connection is not ready") || contains(raw, "nifm"))
        return "No network connection. Connect to Wi-Fi and try again.";
    if (contains(raw, "runtime is unavailable"))
        return "Networking could not be started. Try again in a moment.";
    if (contains(raw, "resolve"))
        return "Couldn't find " + address + ". Check PirateFoil Address.";
    if (contains(raw, "SSL") || contains(raw, "certificate") || contains(raw, "TLS"))
        return "The secure (https) connection to " + address +
               " failed. Check the address, or try its http:// address.";
    if (contains(raw, "timed out") || contains(raw, "timeout"))
        return "PirateFoil at " + address + " didn't answer in time. Is the server running?";
    if (contains(raw, "connect") || contains(raw, "refused") || contains(raw, "reach"))
        return "Couldn't reach PirateFoil at " + address +
               ". Check the address and that the server is running.";
    if (contains(raw, "URL") || contains(raw, "malformed") || contains(raw, "protocol"))
        return "PirateFoil Address isn't a valid web address.";
    return "Couldn't reach PirateFoil: " + raw;
}

bool looksLike(const std::vector<std::uint8_t>& bytes, const std::string& format) {
    if (format == "webp")
        return bytes.size() >= 12 && std::equal(bytes.begin(), bytes.begin() + 4, "RIFF") &&
               std::equal(bytes.begin() + 8, bytes.begin() + 12, "WEBP");
    // jpg
    return bytes.size() >= 3 && bytes[0] == 0xFF && bytes[1] == 0xD8 && bytes[2] == 0xFF;
}

bool writeFile(const std::string& path, const std::vector<std::uint8_t>& bytes) {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const bool ok = std::fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size();
    const bool closed = std::fclose(f) == 0;
    return ok && closed;
}

void report(const std::shared_ptr<Progress>& progress, const std::string& message, float value) {
    if (!progress) return;
    std::lock_guard<std::mutex> lock(progress->mutex);
    progress->message = message;
    progress->progress01 = value;
    ++progress->revision;
}

struct ListEntry {
    std::string titleId;
    std::string name;
    std::string format;   // "webp" or "jpg"
    std::int64_t size = 0;
    std::int64_t updated = 0;
    std::string url;
};

} // namespace

std::string normaliseAddress(const std::string& address) {
    std::string a = address;
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    a.erase(a.begin(), std::find_if(a.begin(), a.end(), notSpace));
    a.erase(std::find_if(a.rbegin(), a.rend(), notSpace).base(), a.end());
    while (!a.empty() && a.back() == '/') a.pop_back();
    if (!a.empty() && a.find("://") == std::string::npos) a = "http://" + a;
    return a;
}

std::string summaryText(const Result& r) {
    std::string s = std::to_string(r.added) + " added, " + std::to_string(r.updated) +
                    " updated, " + std::to_string(r.removed) + " removed, " +
                    std::to_string(r.failed) + " failed";
    if (r.tooBig > 0) s += ", " + std::to_string(r.tooBig) + " over 10 MB skipped";
    if (r.keptYours > 0) s += ", " + std::to_string(r.keptYours) + " kept (your own file)";
    return s;
}

Result runSync(const std::string& rawAddress, const std::string& apiKey,
               const std::shared_ptr<Progress>& progress,
               const std::shared_ptr<std::atomic<bool>>& cancel) {
    Result result;
    const std::string address = normaliseAddress(rawAddress);
    auto cancelled = [&cancel]() { return cancel && cancel->load(); };

    if (address.empty()) {
        result.error = "Set PirateFoil Address first.";
        return result;
    }
    if (apiKey.empty()) {
        result.error = "Set PirateFoil API Key first.";
        return result;
    }
    const std::string scheme = lower(address.substr(0, address.find("://")));
    if (scheme != "http" && scheme != "https") {
        result.error = "PirateFoil Address must start with http:// or https://.";
        return result;
    }

    std::error_code ec;
    std::filesystem::create_directories(kVideoFolder, ec);

    const std::list<std::string> headers = {"X-Api-Key: " + apiKey, "Accept: application/json"};

    // 1) The list.
    report(progress, "Contacting PirateFoil...", -1.f);
    std::string body;
    try {
        body = themeshop::http::getText(address + "/api/v1/backgrounds", headers,
                                        [cancelled]() { return cancelled(); });
    } catch (const std::exception& ex) {
        if (cancelled()) { result.cancelled = true; result.error = "Sync cancelled."; return result; }
        DebugLog::log("[piratefoil] list failed: %s", ex.what());
        result.error = describeListError(ex.what(), address);
        return result;
    } catch (...) {
        result.error = "Couldn't reach PirateFoil at " + address + ".";
        return result;
    }

    std::vector<ListEntry> entries;
    try {
        json j = json::parse(body);
        if (!j.is_object()) throw std::runtime_error("not an object");
        if (!j.value("success", false)) {
            std::string err = j.contains("error") && j["error"].is_string()
                                  ? j["error"].get<std::string>() : std::string();
            result.error = "PirateFoil said no" + (err.empty() ? std::string(".") : ": " + err);
            return result;
        }
        const json& list = j.at("backgrounds");
        if (!list.is_array()) throw std::runtime_error("backgrounds is not a list");
        for (const auto& item : list) {
            if (!item.is_object()) continue;
            ListEntry e;
            e.titleId = upperId(item.value("title_id", std::string()));
            e.name = item.value("name", std::string());
            const std::string type = lower(item.value("type", std::string()));
            e.format = type == "clip" ? "webp" : type == "picture" ? "jpg" : std::string();
            e.size = item.value("size", static_cast<std::int64_t>(0));
            e.updated = item.value("updated", static_cast<std::int64_t>(0));
            e.url = item.value("url", std::string());
            if (!e.url.empty() && e.url[0] == '/') e.url = address + e.url;
            entries.push_back(std::move(e));
        }
    } catch (const std::exception& ex) {
        DebugLog::log("[piratefoil] bad list: %s", ex.what());
        result.error = "That address didn't answer like PirateFoil. Check PirateFoil Address.";
        return result;
    }
    result.ok = true;
    result.listed = static_cast<int>(entries.size());

    Record record = loadRecord();
    std::set<std::string> listedIds;

    // 2) Decide what needs downloading.
    struct Job { ListEntry entry; bool isNew; };
    std::vector<Job> jobs;
    for (const auto& e : entries) {
        if (!validTitleId(e.titleId) || e.format.empty() || e.url.empty()) {
            DebugLog::log("[piratefoil] skipping malformed entry '%s'", e.titleId.c_str());
            ++result.failed;
            if (validTitleId(e.titleId)) listedIds.insert(e.titleId);
            continue;
        }
        listedIds.insert(e.titleId);
        auto rec = record.find(e.titleId);
        const bool known = rec != record.end();
        if (known && rec->second.updated == e.updated && rec->second.format == e.format &&
            fileExists(pathFor(e.titleId, e.format))) {
            ++result.unchanged;
            continue;
        }
        // Never overwrite a file that SwitchU didn't download itself.
        const bool targetOurs = known && rec->second.format == e.format;
        if (!targetOurs && fileExists(pathFor(e.titleId, e.format))) {
            ++result.keptYours;
            continue;
        }
        if (e.size > static_cast<std::int64_t>(kMaxFileBytes)) {
            ++result.tooBig;
            continue;
        }
        jobs.push_back({e, !known || !fileExists(pathFor(e.titleId, rec->second.format))});
    }

    // 3) Download.
    const int total = static_cast<int>(jobs.size());
    for (int i = 0; i < total; ++i) {
        if (cancelled()) { result.cancelled = true; break; }
        const ListEntry& e = jobs[static_cast<std::size_t>(i)].entry;
        const bool isNew = jobs[static_cast<std::size_t>(i)].isNew;
        const std::string label = "Downloading " + std::to_string(i + 1) + " of " +
                                  std::to_string(total) +
                                  (e.name.empty() ? std::string() : ": " + e.name);
        report(progress, label, static_cast<float>(i) / static_cast<float>(total));

        auto oversize = std::make_shared<std::atomic<bool>>(false);
        std::vector<std::uint8_t> bytes;
        try {
            bytes = themeshop::http::getBytes(
                e.url, headers,
                [&, oversize](std::uint64_t done, std::uint64_t size) {
                    if (done > kMaxFileBytes || size > kMaxFileBytes) oversize->store(true);
                    if (size > 0)
                        report(progress, label,
                               (static_cast<float>(i) +
                                std::min(1.f, static_cast<float>(done) / static_cast<float>(size))) /
                                   static_cast<float>(total));
                },
                [cancelled, oversize]() { return cancelled() || oversize->load(); });
        } catch (const std::exception& ex) {
            if (oversize->load()) { ++result.tooBig; continue; }
            if (cancelled()) { result.cancelled = true; break; }
            DebugLog::log("[piratefoil] %s failed: %s", e.titleId.c_str(), ex.what());
            ++result.failed;
            continue;
        } catch (...) {
            ++result.failed;
            continue;
        }
        if (bytes.size() > kMaxFileBytes) { ++result.tooBig; continue; }
        if (bytes.empty() || !looksLike(bytes, e.format)) {
            DebugLog::log("[piratefoil] %s: not a %s file (%zu bytes)", e.titleId.c_str(),
                          e.format.c_str(), bytes.size());
            ++result.failed;
            continue;
        }

        const std::string target = pathFor(e.titleId, e.format);
        const std::string temp = std::string(kVideoFolder) + "/.piratefoil-" + e.titleId + ".part";
        if (!writeFile(temp, bytes)) {
            removeFile(temp);
            ++result.failed;
            continue;
        }
        bytes.clear();
        bytes.shrink_to_fit();
        // FAT can't rename over an existing file; the target is ours (checked above).
        removeFile(target);
        if (std::rename(temp.c_str(), target.c_str()) != 0) {
            DebugLog::log("[piratefoil] %s: rename failed", e.titleId.c_str());
            removeFile(temp);
            ++result.failed;
            continue;
        }
        // Clip <-> picture: drop the old file SwitchU downloaded for this game.
        auto rec = record.find(e.titleId);
        if (rec != record.end() && rec->second.format != e.format)
            removeFile(pathFor(e.titleId, rec->second.format));
        record[e.titleId] = RecordEntry{e.updated, e.format};
        saveRecord(record);   // a crash mid-sync still leaves an accurate record
        if (isNew) ++result.added; else ++result.updated;
    }

    // 4) Backgrounds removed in PirateFoil.
    if (!result.cancelled) {
        for (auto it = record.begin(); it != record.end();) {
            if (listedIds.count(it->first)) { ++it; continue; }
            if (removeFile(pathFor(it->first, it->second.format))) {
                ++result.removed;
                it = record.erase(it);
            } else {
                ++result.failed;
                ++it;
            }
        }
    }

    if (!saveRecord(record)) {
        DebugLog::log("[piratefoil] could not save the record file");
        result.error = "Downloaded, but the sync record couldn't be saved.";
    }
    report(progress, "Finishing...", 1.f);
    DebugLog::log("[piratefoil] done: %s", summaryText(result).c_str());
    return result;
}

} // namespace piratefoil
