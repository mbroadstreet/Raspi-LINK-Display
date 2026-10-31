#include "RuntimeInstanceGuard.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <sys/wait.h>
#include <signal.h>

namespace fs = std::filesystem;
using runtime_admission::RuntimeInstanceGuard;
namespace {
int checks = 0;
void syscallOk(int result, const char* operation) {
    if (result != 0) throw std::runtime_error(std::string(operation) + ": " + std::strerror(errno));
}
void require(bool ok, const std::string& label) {
    if (!ok) throw std::runtime_error("FAIL " + label);
    ++checks; std::cout << "PASS " << label << std::endl;
}
bool refused(const std::string& root) {
    try { RuntimeInstanceGuard lease(root); return false; }
    catch (const std::exception& e) {
        return std::string(e.what()).find("Runtime admission refused:") == 0;
    }
}
struct Fixture {
    fs::path root;
    explicit Fixture(const std::string& parent) {
        std::string name = parent + "/guard-XXXXXX";
        std::vector<char> buffer(name.begin(), name.end()); buffer.push_back('\0');
        char* p = ::mkdtemp(buffer.data());
        if (!p) throw std::runtime_error("mkdtemp failed");
        root = p;
    }
    ~Fixture() { std::error_code ignored; fs::remove_all(root, ignored); }
    std::string name() const { return root.string(); }
    fs::path directory() const { return root / "raspi-link-display"; }
    fs::path lock() const { return directory() / "runtime.lock"; }
    void setup() { fs::create_directory(directory()); ::chmod(directory().c_str(), 0700); }
    void file() { setup(); std::ofstream(lock()) << "unchanged\n"; ::chmod(lock().c_str(), 0600); }
};
std::string read(const fs::path& p) {
    std::ifstream file(p); return std::string(std::istreambuf_iterator<char>(file), {});
}
struct stat statFile(const fs::path& p) {
    struct stat st {}; if (::lstat(p.c_str(), &st) != 0) throw std::runtime_error("stat failed"); return st;
}
int waitChild(pid_t pid) {
    int status = 0;
    if (::waitpid(pid, &status, 0) != pid) throw std::runtime_error("waitpid failed");
    return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
}
std::string identity(pid_t pid) {
    const auto base = fs::path("/proc") / std::to_string(pid);
    const std::string line = read(base / "stat");
    const auto split = line.substr(line.rfind(')') + 2);
    std::istringstream fields(split); std::string field, ticks;
    for (int i = 0; i <= 19; ++i) { fields >> field; if (i == 19) ticks = field; }
    struct stat image {}; if (::stat((base / "exe").c_str(), &image) != 0) throw std::runtime_error("cannot identify child image");
    return "pid=" + std::to_string(pid) + " start_ticks=" + ticks
        + " exe=" + fs::read_symlink(base / "exe").string()
        + " dev=" + std::to_string(image.st_dev) + " ino=" + std::to_string(image.st_ino);
}
}
int main(int argc, char** argv) {
    try {
        if (argc == 3 && std::string(argv[1]) == "--exec-probe") {
            for (const auto& entry : fs::directory_iterator("/proc/self/fd")) {
                std::error_code error;
                if (fs::read_symlink(entry.path(), error) == fs::path(argv[2])) return 1;
            }
            return 0;
        }
        if (argc != 2) throw std::runtime_error("usage: runtime_instance_guard_tests EXISTING_PRIVATE_FIXTURE_PARENT");
        const std::string parent = argv[1];
        {
            Fixture f(parent); struct stat first {};
            {
                RuntimeInstanceGuard lease(f.name()); first = statFile(f.lock());
                require((statFile(f.directory()).st_mode & 07777) == 0700, "creates private directory 0700");
                require((first.st_mode & 07777) == 0600 && first.st_uid == ::getuid() && first.st_nlink == 1, "creates user single-link regular lock 0600");
                require(refused(f.name()), "same-process independent open fails fast");
                const auto pid = ::fork(); if (pid < 0) throw std::runtime_error("fork failed");
                if (pid == 0) _exit(refused(f.name()) ? 0 : 1);
                require(waitChild(pid) == 0, "cross-process contender refuses");
                const auto exec = ::fork(); if (exec < 0) throw std::runtime_error("fork failed");
                if (exec == 0) { ::execl(argv[0], argv[0], "--exec-probe", f.lock().c_str(), nullptr); _exit(2); }
                require(waitChild(exec) == 0, "lease descriptor is close-on-exec");
                const auto unchanged = statFile(f.lock());
                require(first.st_ino == unchanged.st_ino && first.st_dev == unchanged.st_dev, "contention does not replace inode");
            }
            require(fs::exists(f.lock()), "destructor leaves stable lock file");
            { RuntimeInstanceGuard next(f.name()); require(statFile(f.lock()).st_ino == first.st_ino, "graceful release and same-inode reacquisition"); }
            // lifecycle.lock can be held by the controller while the app admits.
            const auto lifecycle = f.directory() / "lifecycle.lock";
            const int fd = ::open(lifecycle.c_str(), O_CREAT | O_RDWR | O_CLOEXEC, 0600);
            require(fd >= 0 && ::flock(fd, LOCK_EX | LOCK_NB) == 0, "fixture controller holds distinct lifecycle.lock");
            { RuntimeInstanceGuard lease(f.name()); require(true, "no helper/application lock-order deadlock"); }
            ::close(fd);
        }
        {
            Fixture f(parent); f.file(); const auto contents = read(f.lock());
            { RuntimeInstanceGuard lease(f.name()); }
            require(read(f.lock()) == contents, "existing lock contents never truncated");
        }
        for (const mode_t mode : {0644, 0660, 0400, 0000, 04600}) {
            Fixture f(parent); f.file(); ::chmod(f.lock().c_str(), mode);
            require(refused(f.name()), "rejects unsafe file mode " + std::to_string(mode));
            require((statFile(f.lock()).st_mode & 07777) == mode, "does not repair unsafe file mode " + std::to_string(mode));
        }
        for (const mode_t mode : {0755, 0770, 01700}) {
            Fixture f(parent); f.setup(); ::chmod(f.directory().c_str(), mode);
            require(refused(f.name()), "rejects nonprivate child directory " + std::to_string(mode));
        }
        {
            Fixture f(parent); ::chmod(f.root.c_str(), 0755);
            require(refused(f.name()), "rejects nonprivate runtime root");
        }
        {
            Fixture f(parent); f.setup(); syscallOk(::symlink("missing", f.lock().c_str()), "symlink lock");
            require(refused(f.name()) && fs::is_symlink(f.lock()), "rejects symlink lock without replacing it");
        }
        {
            Fixture f(parent); Fixture target(parent); syscallOk(::symlink(target.name().c_str(), f.directory().c_str()), "symlink directory");
            require(refused(f.name()) && fs::is_symlink(f.directory()), "rejects private-directory symlink");
        }
        {
            Fixture f(parent); const auto alias = f.root / "alias"; syscallOk(::symlink(f.name().c_str(), alias.c_str()), "symlink root");
            require(refused(alias.string()), "rejects runtime-root symlink");
        }
        {
            Fixture f(parent); Fixture target(parent); const auto alias = f.root / "alias"; syscallOk(::symlink(target.name().c_str(), alias.c_str()), "symlink ancestor");
            require(refused((alias / "child").string()), "rejects symlink ancestor");
        }
        {
            Fixture f(parent); f.file(); const auto other = f.root / "hardlink"; syscallOk(::link(f.lock().c_str(), other.c_str()), "hardlink lock");
            require(refused(f.name()) && statFile(f.lock()).st_nlink == 2, "rejects multi-link regular inode");
        }
        {
            Fixture f(parent); f.setup(); fs::create_directory(f.lock());
            require(refused(f.name()), "rejects lock pathname directory");
        }
        {
            Fixture f(parent); f.setup(); ::mkfifo(f.lock().c_str(), 0600);
            require(refused(f.name()), "rejects FIFO without blocking");
        }
        {
            Fixture f(parent);
            require(refused(f.name() + "/../" + f.root.filename().string()), "rejects dot-dot components");
            require(refused("relative/path"), "rejects relative fixture root");
            require(refused(f.name() + "/"), "rejects ambiguous trailing slash");
            require(refused("/run"), "rejects root-owned nonprivate runtime directory");
        }
        {
            Fixture f(parent);
            try { RuntimeInstanceGuard lease(f.name()); throw std::runtime_error("fixture runtime startup failure"); }
            catch (const std::runtime_error&) {}
            { RuntimeInstanceGuard recovered(f.name()); require(true, "exception unwinding releases lease"); }
        }
        {
            Fixture f(parent); f.file(); ::chmod(f.lock().c_str(), 0644);
            const auto countFds = []() {
                return std::distance(fs::directory_iterator("/proc/self/fd"), fs::directory_iterator());
            };
            const auto before = countFds(); bool allRefused = true;
            for (int i = 0; i < 100; ++i) allRefused = refused(f.name()) && allRefused;
            require(allRefused && countFds() == before, "100 rejected admissions leak no descriptors");
        }
        {
            Fixture f(parent); int start[2], results[2], release[2];
            if (::pipe(start) || ::pipe(results) || ::pipe(release)) throw std::runtime_error("race pipe failed");
            struct RaceCleanup {
                int releaseFd; int startFd; std::vector<pid_t> pids;
                ~RaceCleanup() {
                    if (startFd >= 0) ::close(startFd);
                    if (releaseFd >= 0) ::close(releaseFd);
                    for (const auto pid : pids) { int status; while (::waitpid(pid, &status, 0) < 0 && errno == EINTR) {} }
                }
            } race {release[1], start[1], {}};
            for (int i = 0; i < 8; ++i) {
                const auto pid = ::fork(); if (pid < 0) throw std::runtime_error("race fork failed");
                if (pid == 0) {
                    ::close(start[1]); ::close(results[0]); ::close(release[1]);
                    char c; if (::read(start[0], &c, 1) != 1) _exit(2);
                    try {
                        RuntimeInstanceGuard lease(f.name()); const char won = 'W';
                        if (::write(results[1], &won, 1) != 1) _exit(3);
                        const auto released = ::read(release[0], &c, 1); _exit(released >= 0 ? 0 : 4);
                    } catch (...) {
                        const char lost = 'L'; const auto written = ::write(results[1], &lost, 1); _exit(written == 1 ? 0 : 5);
                    }
                }
                race.pids.push_back(pid);
            }
            ::close(start[0]); ::close(results[1]); ::close(release[0]);
            const char go[] = "12345678";
            if (::write(start[1], go, 8) != 8) throw std::runtime_error("race barrier write failed");
            ::close(start[1]); race.startFd = -1;
            int winners = 0, losers = 0;
            for (int i = 0; i < 8; ++i) {
                char result = 0; if (::read(results[0], &result, 1) != 1) throw std::runtime_error("race result missing");
                winners += result == 'W'; losers += result == 'L';
            }
            ::close(results[0]);
            require(winners == 1 && losers == 7, "eight simultaneous cooperating entrants yield exactly one holder");
            ::close(race.releaseFd); race.releaseFd = -1;
            bool clean = true;
            for (const auto pid : race.pids) clean = waitChild(pid) == 0 && clean;
            race.pids.clear();
            require(clean, "all simultaneous fixture children reaped cleanly");
        }
        {
            Fixture f(parent); int ready[2], release[2];
            if (::pipe(ready) || ::pipe(release)) throw std::runtime_error("pipe failed");
            const auto pid = ::fork(); if (pid < 0) throw std::runtime_error("fork failed");
            if (pid == 0) {
                ::close(ready[0]); ::close(release[1]);
                try {
                    RuntimeInstanceGuard lease(f.name()); const char r='R';
                    if (::write(ready[1], &r, 1) != 1) _exit(3);
                    char c; const auto released = ::read(release[0], &c, 1);
                    _exit(released >= 0 ? 0 : 4);
                }
                catch (...) { _exit(2); }
            }
            ::close(ready[1]); ::close(release[0]); char r=0;
            struct ChildCleanup {
                pid_t pid; int releaseFd;
                ~ChildCleanup() {
                    // Closing the exact fixture pipe releases the child on any
                    // test exception before Fixture can remove its directory.
                    if (releaseFd >= 0) ::close(releaseFd);
                    if (pid > 0) { int status; while (::waitpid(pid, &status, 0) < 0 && errno == EINTR) {} }
                }
            } child {pid, release[1]};
            if (::read(ready[0], &r, 1) != 1 || r != 'R') throw std::runtime_error("child admission failed");
            const auto registered = identity(pid); std::cout << "REGISTERED_PRIVATE_FIXTURE " << registered << std::endl;
            const auto inode = statFile(f.lock()).st_ino;
            require(refused(f.name()), "live fixture lease-holder excludes contender");
            require(identity(pid) == registered, "exact fixture PID/start ticks/ELF revalidated before abnormal kill");
            if (::kill(pid, SIGKILL) != 0) throw std::runtime_error("exact fixture kill failed");
            const auto childExit = waitChild(pid); child.pid = -1;
            require(childExit == 128 + SIGKILL, "fixture-only abnormal child exit captured");
            ::close(ready[0]); ::close(child.releaseFd); child.releaseFd = -1;
            { RuntimeInstanceGuard recovered(f.name()); require(statFile(f.lock()).st_ino == inode, "kernel crash release permits same-inode reacquisition"); }
        }
        std::cout << "TOTAL " << checks << " checks passed (private primitive fixtures; not full SDL/Link acceptance)" << std::endl;
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << std::endl; return 1; }
}
