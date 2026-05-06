#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>        // open(), O_RDONLY, O_WRONLY, O_CREAT, O_EXCL, O_APPEND
#include <unistd.h>       // read(), write(), close(), lseek(), symlink(), unlink()
#include <sys/stat.h>     // stat(), lstat(), chmod(), mkdir(), struct stat
#include <sys/types.h>    // mode_t, off_t, etc.
#include <time.h>         // time_t, time(), ctime()

// ---------------------------------------------------------------------------
// Fixed field sizes — DO NOT change these after you start creating binary files.
// Changing them means every existing reports.dat file becomes unreadable,
// because the program will try to read records of the wrong size.
// ---------------------------------------------------------------------------
#define NAME_LEN      64
#define CATEGORY_LEN  32
#define DESC_LEN      256

// ---------------------------------------------------------------------------
// The Report struct — this is the exact layout of one record in reports.dat.
// Every field has a fixed size, which is what makes the binary file work:
// record N starts at byte (N * sizeof(Report)) in the file, always.
// ---------------------------------------------------------------------------
typedef struct {
    int    id;                      // 4 bytes  — unique report identifier
    char   inspector[NAME_LEN];     // 64 bytes — who filed the report
    double latitude;                // 8 bytes  — GPS coordinates
    double longitude;               // 8 bytes
    char   category[CATEGORY_LEN]; // 32 bytes — e.g. "road", "lighting"
    int    severity;                // 4 bytes  — 1=minor, 2=moderate, 3=critical
    time_t timestamp;               // 8 bytes  — Unix timestamp (seconds since 1970)
    char   description[DESC_LEN];  // 256 bytes — free-text description
} Report;
// Total size per record: sizeof(Report) — print it once with printf to see the exact value.



// ---------------------------------------------------------------------------
// BUILD_PATH: a convenience macro that glues a district name and a filename
// together into one path string. For example:
//   BUILD_PATH(buf, "downtown", "reports.dat")
// fills buf with "downtown/reports.dat".
// It uses snprintf so it will never write more characters than buf can hold.
// ---------------------------------------------------------------------------
#define BUILD_PATH(buf, district, filename) \
    snprintf((buf), sizeof(buf), "%s/%s", (district), (filename))



// ---------------------------------------------------------------------------
// Forward declarations — the compiler needs to know these functions exist
// before main() tries to call them.
// ---------------------------------------------------------------------------
void ensure_district_exists(const char *district, const char *role, const char *user);
void log_action(const char *district, const char *role, const char *user, const char *action);
void cmd_add(const char *role, const char *user, const char *district);
void cmd_list(const char *role, const char *user, const char *district);
void cmd_view(const char *role, const char *user, const char *district, int report_id);
void cmd_remove_report(const char *role, const char *user, const char *district, int report_id);
void cmd_update_threshold(const char *role, const char *user, const char *district, int value);
void cmd_filter(const char *role, const char *user, const char *district, int argc, char *argv[], int cond_start);

// ---------------------------------------------------------------------------
// main: parses all command-line arguments, validates them, and routes to
// the correct command handler function.
// ---------------------------------------------------------------------------
int main(int argc, char *argv[]) {
    char *role     = NULL;
    char *user     = NULL;
    char *command  = NULL;
    char *district = NULL;

    int report_id               = -1;
    int threshold               = -1;
    int filter_conditions_start = -1;

    for (int i = 1; i < argc; i++) {

        if (strcmp(argv[i], "--role") == 0) {
            if (i + 1 >= argc) { fprintf(stderr, "Error: --role requires a value\n"); return 1; }
            role = argv[++i];

        } else if (strcmp(argv[i], "--user") == 0) {
            if (i + 1 >= argc) { fprintf(stderr, "Error: --user requires a value\n"); return 1; }
            user = argv[++i];

        } else if (strcmp(argv[i], "--add") == 0) {
            command = "add";
            if (i + 1 >= argc) { fprintf(stderr, "Error: --add requires a district\n"); return 1; }
            district = argv[++i];

        } else if (strcmp(argv[i], "--list") == 0) {
            command = "list";
            if (i + 1 >= argc) { fprintf(stderr, "Error: --list requires a district\n"); return 1; }
            district = argv[++i];

        } else if (strcmp(argv[i], "--view") == 0) {
            command = "view";
            if (i + 2 >= argc) { fprintf(stderr, "Error: --view requires a district and report ID\n"); return 1; }
            district  = argv[++i];
            report_id = atoi(argv[++i]);

        } else if (strcmp(argv[i], "--remove_report") == 0) {
            command = "remove_report";
            if (i + 2 >= argc) { fprintf(stderr, "Error: --remove_report requires a district and report ID\n"); return 1; }
            district  = argv[++i];
            report_id = atoi(argv[++i]);

        } else if (strcmp(argv[i], "--update_threshold") == 0) {
            command = "update_threshold";
            if (i + 2 >= argc) { fprintf(stderr, "Error: --update_threshold requires a district and value\n"); return 1; }
            district  = argv[++i];
            threshold = atoi(argv[++i]);

        } else if (strcmp(argv[i], "--filter") == 0) {
            command = "filter";
            if (i + 1 >= argc) { fprintf(stderr, "Error: --filter requires a district\n"); return 1; }
            district = argv[++i];
            filter_conditions_start = i + 1;
        }
    }

    // Validate all required arguments before doing anything else.
    if (!role) {
        fprintf(stderr, "Error: --role is required (manager or inspector)\n");
        return 1;
    }
    if (!user) {
        fprintf(stderr, "Error: --user is required\n");
        return 1;
    }
    if (strcmp(role, "manager") != 0 && strcmp(role, "inspector") != 0) {
        fprintf(stderr, "Error: role must be 'manager' or 'inspector'\n");
        return 1;
    }
    if (!command) {
        fprintf(stderr, "Error: no command specified\n");
        fprintf(stderr, "Usage: city_manager --role <role> --user <user> --<command> <args>\n");
        return 1;
    }

    // Route to the correct handler.
    if      (strcmp(command, "add")              == 0) cmd_add(role, user, district);
    else if (strcmp(command, "list")             == 0) cmd_list(role, user, district);
    else if (strcmp(command, "view")             == 0) cmd_view(role, user, district, report_id);
    else if (strcmp(command, "remove_report")    == 0) cmd_remove_report(role, user, district, report_id);
    else if (strcmp(command, "update_threshold") == 0) cmd_update_threshold(role, user, district, threshold);
    else if (strcmp(command, "filter")           == 0) cmd_filter(role, user, district, argc, argv, filter_conditions_start);

    return 0;
}


// ---------------------------------------------------------------------------
// log_action: writes one timestamped line to a district's operation log.
// Every command calls this so there's a permanent record of who did what.
// ---------------------------------------------------------------------------
void log_action(const char *district, const char *role,
                const char *user, const char *action) {

    char path[512];
    BUILD_PATH(path, district, "logged_district");

    // O_WRONLY  = open for writing only (we never need to read the log here)
    // O_CREAT   = create the file if it doesn't already exist
    // O_APPEND  = always write at the END of the file, never the beginning.
    //             Without this, each call would overwrite from byte 0 and
    //             destroy all previous log entries.
    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) {
        perror("log_action: open failed");
        return; // logging failure is not fatal — we just warn and continue
    }

    // time(NULL) returns the current time as a Unix timestamp (a big integer).
    // ctime() converts that integer into a readable string like:
    //   "Tue May  6 14:32:00 2026\n"
    // The \n at the end is annoying, so we remove it with strcspn.
    time_t now = time(NULL);
    char *ts = ctime(&now);
    ts[strcspn(ts, "\n")] = '\0'; // replace the trailing newline with a null terminator

    char buf[512];
    int len = snprintf(buf, sizeof(buf),
                       "[%s] role=%s user=%s action=%s\n",
                       ts, role, user, action);

    write(fd, buf, len);
    close(fd); // always close what you open — the OS has a limited number of file descriptors
}



// ---------------------------------------------------------------------------
// ensure_district_exists: creates the district directory and all required
// files if they don't already exist, then sets exact permission bits.
//
// This is called at the start of EVERY command so that no command ever has
// to worry about whether the directory structure is in place.
// ---------------------------------------------------------------------------
void ensure_district_exists(const char *district, const char *role, const char *user) {
    char path[512];

    // --- 1. District directory (rwxr-x--- = 0750) ---
    // mkdir returns 0 on success (directory was just created).
    // It returns -1 if the directory already exists — that's fine, we just skip.
    if (mkdir(district, 0750) == 0) {
        // chmod forces the exact permissions we want, bypassing the umask.
        // The umask is a system-level filter that silently strips certain
        // permission bits from newly created files. chmod ignores it entirely.
        chmod(district, 0750);
    }

    // --- 2. reports.dat (rw-rw-r-- = 0664) ---
    BUILD_PATH(path, district, "reports.dat");
    // O_CREAT | O_EXCL together mean: "create this file, but ONLY if it does
    // not already exist. If it does exist, fail with -1 and do nothing."
    // This is the safe way to create a file exactly once.
    int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0664);
    if (fd >= 0) {
        // fd >= 0 means the file was just created fresh — safe to chmod it.
        chmod(path, 0664);
        close(fd);
        // If fd was -1, the file already existed — we leave it completely alone.
    }

    // --- 3. district.cfg (rw-r----- = 0640) with a default threshold ---
    BUILD_PATH(path, district, "district.cfg");
    fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0640);
    if (fd >= 0) {
        chmod(path, 0640);
        // Write a default severity threshold so the file is never empty.
        // The manager can change this later with update_threshold.
        const char *default_cfg = "threshold=1\n";
        write(fd, default_cfg, strlen(default_cfg));
        close(fd);
    }

    // --- 4. logged_district (rw-r--r-- = 0644) ---
    BUILD_PATH(path, district, "logged_district");
    fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0644);
    if (fd >= 0) {
        chmod(path, 0644);
        close(fd);
        // Starts empty — log_action will fill it as commands are run.
    }

    // --- 5. Symbolic link: active_reports-<district> -> <district>/reports.dat ---
    char link_name[512];
    char link_target[512];
    snprintf(link_name,   sizeof(link_name),   "active_reports-%s", district);
    snprintf(link_target, sizeof(link_target),  "%s/reports.dat",   district);
    // symlink(target, link_name): note the order — target comes first.
    // It silently does nothing if the link already exists, which is fine.
    symlink(link_target, link_name);

    // Log that this district was accessed (or created for the first time).
    log_action(district, role, user, "district_initialized_or_accessed");
}


// ---------------------------------------------------------------------------
// Command stubs — each one calls ensure_district_exists first, then prints
// a TODO message. We will fill these in one by one in the coming steps.
// ---------------------------------------------------------------------------

void cmd_add(const char *role, const char *user, const char *district) {
    ensure_district_exists(district, role, user);
    printf("[TODO] add report to district: %s\n", district);
}

void cmd_list(const char *role, const char *user, const char *district) {
    ensure_district_exists(district, role, user);
    printf("[TODO] list reports in district: %s\n", district);
}

void cmd_view(const char *role, const char *user, const char *district, int report_id) {
    ensure_district_exists(district, role, user);
    printf("[TODO] view report %d in district: %s\n", report_id, district);
}

void cmd_remove_report(const char *role, const char *user, const char *district, int report_id) {
    ensure_district_exists(district, role, user);
    printf("[TODO] remove report %d from district: %s\n", report_id, district);
}

void cmd_update_threshold(const char *role, const char *user, const char *district, int value) {
    ensure_district_exists(district, role, user);
    printf("[TODO] update threshold to %d in district: %s\n", value, district);
}

void cmd_filter(const char *role, const char *user, const char *district, int argc, char *argv[], int cond_start) {
    ensure_district_exists(district, role, user);
    printf("[TODO] filter reports in district: %s\n", district);
}