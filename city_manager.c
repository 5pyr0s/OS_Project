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
int check_permission(const char *path, const char *role, int need_read, int need_write);
int get_next_id(const char *district);
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
// ===========================================================================
// HELPER FUNCTIONS
// ===========================================================================


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
// check_permission: checks whether the declared role has the required access
// on a given file, based on the Unix permission bits stored in st_mode.
//
// The simulation maps roles to permission tiers like this:
//   manager   -> owner  (user bits:  S_IRUSR, S_IWUSR)
//   inspector -> group  (group bits: S_IRGRP, S_IWGRP)
//
// Pass need_read=1 if the operation needs read access, need_write=1 if it
// needs write access. You can pass both as 1 if you need both.
//
// Returns 1 if the role has the required access, 0 if not.
// Prints a clear error message before returning 0.
// ---------------------------------------------------------------------------

int check_permission(const char *path, const char *role, int need_read, int need_write) {
    struct stat st;

    // stat() fills in the st struct with metadata about the file, including
    // st_mode which contains all the permission bits packed into one integer.

    if (stat(path, &st) < 0) {
        perror("check_permission: stat failed");
        return 0; // If we can't stat the file, treat it as no permission.
    }

    mode_t mode = st.st_mode;

    if (strcmp(role, "manager") == 0) {
        // Manager maps to the owner tier — check user (owner) bits.
        if (need_read && !(mode & S_IRUSR)) {
            fprintf(stderr, "Permission denied: manager cannot read '%s'\n", path);
            return 0;
        }
        if (need_write && !(mode & S_IWUSR)) {
            fprintf(stderr, "Permission denied: manager cannot write to '%s'\n", path);
            return 0;
        }
    } else if (strcmp(role, "inspector") == 0) {
        // Inspector maps to the group tier — check group bits.
        if (need_read && !(mode & S_IRGRP)) {
            fprintf(stderr, "Permission denied: inspector cannot read '%s'\n", path);
            return 0;
        }
        if (need_write && !(mode & S_IWGRP)) {
            fprintf(stderr, "Permission denied: inspector cannot write to '%s'\n", path);
            return 0;
        }
    } else {
        fprintf(stderr, "Permission denied: unknown role '%s'\n", role);
        return 0;
    }

    return 1; // All required permissions are present
}

// ---------------------------------------------------------------------------
// get_next_id: returns the ID that the next new report should receive.
//
// Because every record in reports.dat is exactly sizeof(Report) bytes,
// the number of existing records is simply: file_size / sizeof(Report).
// The new report gets that count as its ID, giving IDs 0, 1, 2, 3, ...
// ---------------------------------------------------------------------------

int get_next_id(const char *district) {
    char path[512];
    BUILD_PATH(path, district, "reports.dat");

    struct stat st;
    if (stat(path, &st) < 0) {
        perror("get_next_id: stat failed");
        return -1; // Return -1 on error — caller should check for this.
    }

    off_t file_size = st.st_size;
    int next_id = file_size / sizeof(Report);
    return next_id;
}


// ---------------------------------------------------------------------------
// cmd_add: collects report fields from the user interactively, assembles a
// Report struct, and appends it to reports.dat as a fixed-size binary record.
//
// Both roles are allowed to add reports (reports.dat is 664, so both owner
// and group have write access). We still run the permission check explicitly
// because the spec requires a stat() check before every operation.
// ---------------------------------------------------------------------------

void cmd_add(const char *role, const char *user, const char *district) {
    ensure_district_exists(district, role, user);
    

    char reports_path[512];
    BUILD_PATH(reports_path, district, "reports.dat");

    if(!check_permission(reports_path, role, 0, 1)) {
        return; // Permission check failed — error message already printed.
    }


    Report new_report;
    memset(&new_report, 0, sizeof(Report)); // Clear the struct to start with
    new_report.id = get_next_id(district);
    if (new_report.id < 0) {
        fprintf(stderr, "cmd_add: could not determine next report ID\n");
        return;
    }
    new_report.timestamp = time(NULL); // Current time as Unix timestamp

    //The inspector name comes from the --user argument, not user input
    strncpy(new_report.inspector, user, NAME_LEN - 1);

    printf("--- Filing new report for district '%s' ---\n", district);
    printf("\tReport ID will be: %d\n", new_report.id);

    printf("\t\tLatitude: ");
    scanf("%lf", &new_report.latitude);

    printf("\t\tLongitude: ");
    scanf("%lf", &new_report.longitude);

    //We call getchar() to consume the leftover newline after scanf, so it doesn't mess up our next fgets call.

    printf("\t\tCategory (e.g. 'road', 'lighting'): ");
    getchar(); // Consume leftover newline
    fgets(new_report.category, CATEGORY_LEN, stdin);
    new_report.category[strcspn(new_report.category, "\n")] = '\0'; // Remove trailing newline

    printf("\t\tSeverity (1=minor, 2=moderate, 3=critical): ");
    scanf("%d", &new_report.severity);

    //Clamp severity to the valid range of 1-3
    if (new_report.severity < 1) new_report.severity = 1;
    if (new_report.severity > 3) new_report.severity = 3;

    printf("\t\tDescription: ");
    getchar(); // Consume leftover newline
    fgets(new_report.description, DESC_LEN, stdin);
    new_report.description[strcspn(new_report.description, "\n")] = '\0';

    // --- Write the struct to disk ---
    //O_WRONLY | O_APPEND: open for writing, and always write at the end of the file (append mode).
    // Each write() call appends exactly one record after all existing ones.
    int fd = open(reports_path, O_WRONLY | O_APPEND);
    if (fd < 0) {
        perror("cmd_add: could not open reports.dat");
        return;
    }

    // write() sends exactly sizeof(Report) bytes in one call — the entire
    // struct. Since every record is the same size, the file is a flat array
    // of Report structs with no separators, and record N is always at byte
    // offset N * sizeof(Report). This is what makes random access possible.
    ssize_t bytes_written = write(fd, &new_report, sizeof(Report));
    if (bytes_written != (ssize_t)sizeof(Report)) {
        perror("cmd_add: write failed or was incomplete");
        close(fd);
        return;
    } 

    close(fd);

    printf("Report %d filed successfully in district '%s'.\n", new_report.id, district);

    // Log the action with the report ID included for clarity.
    char action_desc[256];
    snprintf(action_desc, sizeof(action_desc), "added_report_id=%d", new_report.id);
    log_action(district, role, user, action_desc);
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