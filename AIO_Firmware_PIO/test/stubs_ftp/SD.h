#ifndef AIO_FTP_STUB_SD_H
#define AIO_FTP_STUB_SD_H

#include "Arduino.h"
#include "FS.h"
#include <map>
#include <set>
#include <vector>
#include <memory>

// In-memory SD card for the FTP harness.
//
// Files are stored as raw byte vectors keyed by absolute path. Directories
// are tracked separately in dirs_; SD.exists() returns true for both.
//
// File handles are POD-value-typed in the FTP server (`File file;` is a
// member, reassigned by SD.open()). State must survive copies, so the
// real bytes live in a shared_ptr-managed Backing struct mirrored from
// what FakeSD owns; the File handle just carries a shared_ptr.

class File {
public:
    struct Backing {
        std::string path;
        std::vector<uint8_t> *data = nullptr;  // points into FakeSD
        size_t pos = 0;
        bool is_dir = false;
        // For dir iteration: snapshot of child entries at open() time.
        std::vector<File> children;
        size_t child_idx = 0;
    };

    File() : b_(std::make_shared<Backing>()) {}
    explicit File(std::shared_ptr<Backing> b) : b_(b) {}

    operator bool() const { return b_ && (b_->data || b_->is_dir); }
    bool isDirectory() const { return b_ && b_->is_dir; }
    const char *name() const { return b_ ? b_->path.c_str() : ""; }
    size_t size() const { return (b_ && b_->data) ? b_->data->size() : 0; }
    void close() { /* no-op; data owned by FakeSD */ }

    // Read for RETR
    int read(uint8_t *dst, size_t n) {
        if (!b_ || !b_->data) return 0;
        size_t left = b_->data->size() - b_->pos;
        size_t take = n < left ? n : left;
        if (take == 0) return 0;
        memcpy(dst, b_->data->data() + b_->pos, take);
        b_->pos += take;
        return (int)take;
    }
    size_t readBytes(char *dst, size_t n) { return (size_t)read((uint8_t *)dst, n); }
    size_t readBytes(uint8_t *dst, size_t n) { return (size_t)read(dst, n); }

    // Write for STOR
    size_t write(const uint8_t *src, size_t n) {
        if (!b_ || !b_->data) return 0;
        b_->data->insert(b_->data->end(), src, src + n);
        return n;
    }

    // Directory iteration for LIST/MLSD
    File openNextFile() {
        if (!b_ || !b_->is_dir) return File();
        if (b_->child_idx >= b_->children.size()) return File();
        return b_->children[b_->child_idx++];
    }

private:
    std::shared_ptr<Backing> b_;
    friend class FakeSD;
};

class FakeSD {
public:
    File open(const char *path, const char *mode = "r") {
        std::string p = norm_(path);
        if (dirs_.count(p)) {
            auto b = std::make_shared<File::Backing>();
            b->path = p;
            b->is_dir = true;
            // Snapshot children: any file/dir whose parent is p
            for (auto &kv : files_) {
                if (parent_of(kv.first) == p) {
                    auto cb = std::make_shared<File::Backing>();
                    cb->path = kv.first;
                    cb->data = &kv.second;
                    b->children.push_back(File(cb));
                }
            }
            for (auto &d : dirs_) {
                if (d != p && parent_of(d) == p) {
                    auto cb = std::make_shared<File::Backing>();
                    cb->path = d;
                    cb->is_dir = true;
                    b->children.push_back(File(cb));
                }
            }
            return File(b);
        }
        bool writing = mode && (mode[0] == 'w' || mode[0] == 'a');
        if (!writing && !files_.count(p)) return File();
        if (writing) {
            files_[p].clear();  // truncate on "w"
        } else if (!files_.count(p)) {
            return File();
        }
        auto b = std::make_shared<File::Backing>();
        b->path = p;
        b->data = &files_[p];
        return File(b);
    }
    bool exists(const char *path) {
        std::string p = norm_(path);
        return files_.count(p) || dirs_.count(p);
    }
    bool remove(const char *path) {
        std::string p = norm_(path);
        return files_.erase(p) > 0;
    }
    bool mkdir(const char *path) {
        std::string p = norm_(path);
        if (files_.count(p) || dirs_.count(p)) return false;
        dirs_.insert(p);
        return true;
    }
    bool rmdir(const char *path) {
        std::string p = norm_(path);
        return dirs_.erase(p) > 0;
    }
    bool rename(const char *from, const char *to) {
        std::string f = norm_(from), t = norm_(to);
        auto it = files_.find(f);
        if (it == files_.end()) return false;
        files_[t] = std::move(it->second);
        files_.erase(it);
        return true;
    }

    // Test helpers — populate the fake SD before driving the server.
    void seed_file(const char *path, const char *contents) {
        std::string p = norm_(path);
        files_[p] = std::vector<uint8_t>(contents, contents + strlen(contents));
        // Auto-create parent dirs for LIST snapshot to find them.
        std::string parent = parent_of(p);
        while (parent != "/" && !parent.empty()) {
            dirs_.insert(parent);
            parent = parent_of(parent);
        }
        dirs_.insert("/");
    }
    void clear() { files_.clear(); dirs_.clear(); dirs_.insert("/"); }

private:
    std::map<std::string, std::vector<uint8_t>> files_;
    std::set<std::string> dirs_;

    static std::string norm_(const char *p) {
        if (!p || !*p) return "/";
        std::string s = p;
        if (s.empty() || s[0] != '/') s = "/" + s;
        // Strip trailing slash except for root
        while (s.length() > 1 && s.back() == '/') s.pop_back();
        return s;
    }
    static std::string parent_of(const std::string &p) {
        auto pos = p.find_last_of('/');
        if (pos == std::string::npos || pos == 0) return "/";
        return p.substr(0, pos);
    }
};

extern FakeSD SD;

#endif
