#pragma once

// Source-private Linux admission primitive. This is not managed provenance and
// does not replace the controller's distinct lifecycle.lock or legacy scans.
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <string>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

namespace runtime_admission
{
class RuntimeInstanceGuard final
{
    class Fd
    {
    public:
        explicit Fd(int fd = -1) noexcept : fd_(fd) {}
        ~Fd() { if (fd_ >= 0) ::close(fd_); }
        Fd(const Fd&) = delete;
        Fd& operator=(const Fd&) = delete;
        int get() const noexcept { return fd_; }
        int release() noexcept { const int fd = fd_; fd_ = -1; return fd; }
        void reset(int fd) noexcept { if (fd_ >= 0) ::close(fd_); fd_ = fd; }
    private:
        int fd_;
    };

    static void fail(const std::string& reason)
    {
        throw std::runtime_error("Runtime admission refused: " + reason);
    }

    static int opened(int fd, const char* what)
    {
        if (fd < 0) fail(std::string(what) + ": " + std::strerror(errno));
        return fd;
    }

    static struct stat metadata(int fd)
    {
        struct stat st {};
        if (::fstat(fd, &st) != 0) fail("cannot inspect runtime inode");
        return st;
    }

    static void privateDirectory(int fd)
    {
        const auto st = metadata(fd);
        if (!S_ISDIR(st.st_mode) || st.st_uid != ::getuid()
            || (st.st_mode & 07777) != 0700)
            fail("runtime directory must be owned by this user with mode 0700");
    }

    // Traverse descriptor-relative so no ancestor may redirect through a
    // symlink. Ancestors must not be writable by another user/group.
    static int openRoot(const std::string& root)
    {
        if (root.empty() || root.front() != '/' || root.back() == '/')
            fail("runtime root must be an absolute directory without trailing slash");
        Fd directory(opened(::open("/", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC), "open root"));
        std::size_t begin = 1;
        while (begin < root.size())
        {
            const auto end = root.find('/', begin);
            const auto component = root.substr(begin, end == std::string::npos ? end : end - begin);
            if (component.empty() || component == "." || component == "..")
                fail("unsafe runtime path component");
            Fd next(opened(::openat(directory.get(), component.c_str(),
                O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC), "open runtime directory"));
            const auto st = metadata(next.get());
            if (!S_ISDIR(st.st_mode) || (st.st_uid != 0 && st.st_uid != ::getuid())
                || (st.st_mode & 0022) != 0)
                fail("unsafe runtime directory ancestor");
            directory.reset(next.release());
            if (end == std::string::npos) break;
            begin = end + 1;
        }
        privateDirectory(directory.get());
        return directory.release();
    }

    static void safeLock(int fd)
    {
        const auto st = metadata(fd);
        if (!S_ISREG(st.st_mode) || st.st_uid != ::getuid() || st.st_nlink != 1
            || (st.st_mode & 07777) != 0600)
            fail("runtime.lock must be a single-linked regular file owned by this user with mode 0600");
    }

public:
    RuntimeInstanceGuard()
        : RuntimeInstanceGuard("/run/user/" + std::to_string(::getuid())) {}

    // Explicit constructor exists only for private native test fixtures. No
    // environment variable or application CLI can redirect the production lease.
    explicit RuntimeInstanceGuard(const std::string& privateRuntimeRoot)
    {
        if (::geteuid() != ::getuid()) fail("real/effective user mismatch");
        Fd root(openRoot(privateRuntimeRoot));
        if (::mkdirat(root.get(), "raspi-link-display", 0700) != 0 && errno != EEXIST)
            fail(std::string("create private runtime directory: ") + std::strerror(errno));
        Fd directory(opened(::openat(root.get(), "raspi-link-display",
            O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC), "open private runtime directory"));
        privateDirectory(directory.get());
        Fd lock(opened(::openat(directory.get(), "runtime.lock",
            O_RDWR | O_CREAT | O_NONBLOCK | O_NOFOLLOW | O_CLOEXEC, 0600), "open runtime.lock"));
        safeLock(lock.get());
        if (::flock(lock.get(), LOCK_EX | LOCK_NB) != 0)
        {
            if (errno == EWOULDBLOCK || errno == EAGAIN)
                fail("another cooperating GUI/headless runtime holds runtime.lock");
            fail(std::string("lock runtime.lock: ") + std::strerror(errno));
        }
        // Refuse replacement during admission; never unlink or repair a lock.
        struct stat named {};
        const auto held = metadata(lock.get());
        if (::fstatat(directory.get(), "runtime.lock", &named, AT_SYMLINK_NOFOLLOW) != 0
            || named.st_dev != held.st_dev || named.st_ino != held.st_ino)
            fail("runtime.lock changed during admission");
        safeLock(lock.get());
        lock_ = lock.release();
    }

    ~RuntimeInstanceGuard() { if (lock_ >= 0) ::close(lock_); }
    RuntimeInstanceGuard(const RuntimeInstanceGuard&) = delete;
    RuntimeInstanceGuard& operator=(const RuntimeInstanceGuard&) = delete;
    RuntimeInstanceGuard(RuntimeInstanceGuard&&) = delete;
    RuntimeInstanceGuard& operator=(RuntimeInstanceGuard&&) = delete;

private:
    int lock_ = -1;
};
}
