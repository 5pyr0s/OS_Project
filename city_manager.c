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
void mode_to_string(mode_t mode, char *out);
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
            if (i + 1 >= argc) { fprintf(stderr, "Main_Error: --role requires a value\n"); return 1; }
            role = argv[++i];

        } else if (strcmp(argv[i], "--user") == 0) {
            if (i + 1 >= argc) { fprintf(stderr, "Error: --user requires a value\n"); return 1; }
            user = argv[++i];

        } else if (strcmp(argv[i], "--add") == 0) {
            command = "add";
            if (i + 1 >= argc) { fprintf(stderr, "Main_Error: --add requires a district\n"); return 1; }
            district = argv[++i];

        } else if (strcmp(argv[i], "--list") == 0) {
            command = "list";
            if (i + 1 >= argc) { fprintf(stderr, "Main_Error: --list requires a district\n"); return 1; }
            district = argv[++i];

        } else if (strcmp(argv[i], "--view") == 0) {
            command = "view";
            if (i + 2 >= argc) { fprintf(stderr, "Main_Error: --view requires a district and report ID\n"); return 1; }
            district  = argv[++i];
            report_id = atoi(argv[++i]);

        } else if (strcmp(argv[i], "--remove_report") == 0) {
            command = "remove_report";
            if (i + 2 >= argc) { fprintf(stderr, "Main_Error: --remove_report requires a district and report ID\n"); return 1; }
            district  = argv[++i];
            report_id = atoi(argv[++i]);

        } else if (strcmp(argv[i], "--update_threshold") == 0) {
            command = "update_threshold";
            if (i + 2 >= argc) { fprintf(stderr, "Main_Error: --update_threshold requires a district and value\n"); return 1; }
            district  = argv[++i];
            threshold = atoi(argv[++i]);

        } else if (strcmp(argv[i], "--filter") == 0) {
            command = "filter";
            if (i + 1 >= argc) { fprintf(stderr, "Main_Error: --filter requires a district\n"); return 1; }
            district = argv[++i];
            filter_conditions_start = i + 1;
        }
    }

    // Validate all required arguments before doing anything else.
    if (!role) {
        fprintf(stderr, "Main_Error: --role is required (manager or inspector)\n");
        return 1;
    }
    if (!user) {
        fprintf(stderr, "Main_Error: --user is required\n");
        return 1;
    }
    if (strcmp(role, "manager") != 0 && strcmp(role, "inspector") != 0) {
        fprintf(stderr, "Main_Error: role must be 'manager' or 'inspector'\n");
        return 1;
    }
    if (!command) {
        fprintf(stderr, "Main_Error: no command specified\n");
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
// GENERAL HELPER FUNCTIONS
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
        perror("log_action_error: open failed");
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
        perror("check_permission_error: stat failed");
        return 0; // If we can't stat the file, treat it as no permission.
    }

    mode_t mode = st.st_mode;

    if (strcmp(role, "manager") == 0) {
        // Manager maps to the owner tier — check user (owner) bits.
        if (need_read && !(mode & S_IRUSR)) {
            fprintf(stderr, "check_permission_error: manager cannot read '%s'\n", path);
            return 0;
        }
        if (need_write && !(mode & S_IWUSR)) {
            fprintf(stderr, "check_permission_error: manager cannot write to '%s'\n", path);
            return 0;
        }
    } else if (strcmp(role, "inspector") == 0) {
        // Inspector maps to the group tier — check group bits.
        if (need_read && !(mode & S_IRGRP)) {
            fprintf(stderr, "check_permission_error: inspector cannot read '%s'\n", path);
            return 0;
        }
        if (need_write && !(mode & S_IWGRP)) {
            fprintf(stderr, "check_permission_error: inspector cannot write to '%s'\n", path);
            return 0;
        }
    } else {
        fprintf(stderr, "check_permission_error: unknown role '%s'\n", role);
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
        perror("get_next_id_error: stat failed");
        return -1; // Return -1 on error — caller should check for this.
    }

    off_t file_size = st.st_size;
    int next_id = file_size / sizeof(Report);
    return next_id;
}


// ===========================================================================
//                                CMD_ADD
// ===========================================================================


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
        perror("cmd_add_error: could not open reports.dat");
        return;
    }

    // write() sends exactly sizeof(Report) bytes in one call — the entire
    // struct. Since every record is the same size, the file is a flat array
    // of Report structs with no separators, and record N is always at byte
    // offset N * sizeof(Report). This is what makes random access possible.
    ssize_t bytes_written = write(fd, &new_report, sizeof(Report));
    if (bytes_written != (ssize_t)sizeof(Report)) {
        perror("cmd_add_error: write failed or was incomplete");
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

// ===========================================================================
// HELPER FUNCTIONS FOR CMD_LIST
// ===========================================================================

// ---------------------------------------------------------------------------
// mode_to_string: converts the 9 permission bits of a mode_t value into a
// human-readable string like "rw-rw-r--", stored in `out`.
//
// `out` must point to a buffer of at least 10 bytes (9 chars + null terminator).
//
// How it works: st_mode packs many bits into one integer. The permission bits
// are the lowest 9 bits, grouped in threes — owner, group, other. For each
// position, we check the relevant bit with bitwise AND (&). If the bit is set,
// the result is nonzero (truthy) and we write the letter. If not, we write '-'.
//
// The spec explicitly requires that you write this yourself rather than use
// an external tool like stat or ls, so understand every line here.
// ---------------------------------------------------------------------------

void mode_to_string(mode_t mode, char *out) {
    //Owner (user) bits
    out[0] = (mode & S_IRUSR) ? 'r' : '-'; // owner read
    out[1] = (mode & S_IWUSR) ? 'w' : '-'; // owner write
    out[2] = (mode & S_IXUSR) ? 'x' : '-'; // owner execute

    //Group bits
    out[3] = (mode & S_IRGRP) ? 'r' : '-'; // group read
    out[4] = (mode & S_IWGRP) ? 'w' : '-'; // group write
    out[5] = (mode & S_IXGRP) ? 'x' : '-'; // group execute

    //Other bits
    out[6] = (mode & S_IROTH) ? 'r' : '-'; // other read
    out[7] = (mode & S_IWOTH) ? 'w' : '-'; // other write
    out[8] = (mode & S_IXOTH) ? 'x' : '-'; // other execute

    out[9] = '\0'; // Null terminator to end the string
}

// ===========================================================================
//                                CMD_LIST
// ===========================================================================

// ---------------------------------------------------------------------------
// cmd_list: prints a summary of every report in a district, preceded by
// metadata about reports.dat (permissions in symbolic form, size, mtime).
//
// The permission string must be built by mode_to_string() — the spec forbids
// using external tools for this conversion.
//
// Both roles may read reports.dat (permissions 664 — group read is set).
// ---------------------------------------------------------------------------

void cmd_list(const char *role, const char *user, const char *district) {
    ensure_district_exists(district, role, user);
    

    char reports_path[512];
    BUILD_PATH(reports_path, district, "reports.dat");

    //Both roles can read reports.dat, but we can check the bits explicitly to be sure,
    //since the spec requires a stat() check before every operation.
    if(!check_permission(reports_path, role, 1, 0)) {
        return; // Permission check failed — error message already printed.
    }


    // --- Print file metadata ---
    // stat() gives us st_size (bytes), st_mode (permissions), and st_mtime (last modified time).
    struct stat st;
    if (stat(reports_path, &st) < 0) {
        perror("cmd_list_error: stat failed on reports.dat");
        return;
    }

    // Convert the raw permission bits into a human-readable string.
    char perm_str[10]; // 9 chars for permissions + 1 for null terminator
    mode_to_string(st.st_mode, perm_str);

    // ctime() converts st_mtime to human-readable string)
    // It appends a '\n' at the end, which we can trim off.
    char *mtime_str = ctime(&st.st_mtime);
    mtime_str[strcspn(mtime_str, "\n")] = '\0';

    printf("===--------===--------===--------===--------=== District: %s ===--------===--------===--------===--------===\n", district);
    printf("reports.dat  \n");
    printf("permissions: %s  \n", perm_str);
    printf("size: %lld bytes  \n", (long long)st.st_size);
    printf("last modified: %s \n\n\n", mtime_str);

    // --- Read and print every report ---
    // O_RDONLY: open for reading only, we never write during a list.
    
    int fd = open(reports_path, O_RDONLY);
    if (fd < 0) {
        perror("cmd_list: could not open reports.dat");
        return;
    }
    printf("\t|===|=================|===========|============|================|==========|=====================|\n");
    printf("\t|---|-----------------|-----------|------------|----------------|----------|---------------------|\n");
    printf("\t|ID | Inspector       | Latitude  | Longitude  | Category       | Severity | Timestamp           |\n");
    printf("\t|---|-----------------|-----------|------------|----------------|----------|---------------------|\n");
    
    Report report;
    int count = 0;
    ssize_t bytes_read;

    // read() returns the number of bytes actually read.
    // When the file is exhausted it returns 0 - that's our loop exit condition.
    // We compare against sizeof(Report) to guard against partial reads.
    while((bytes_read = read(fd, &report, sizeof(Report))) == (ssize_t)sizeof(Report)) {

        // Convert the stored Unix timestamp into a human-readable format for display.
        char time_str[20];
        struct tm *tm_info = localtime(&report.timestamp);
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

        // Severity as a label instead of a number
        const char *severity_label;
        switch (report.severity) {
            case 1: severity_label = "minor"; break;
            case 2: severity_label = "moderate"; break;
            case 3: severity_label = "critical"; break;
            default: severity_label = "unknown"; break;
        }

        printf("\t|%-3d| %-16s| %-10.6f| %-11.6f| %-15s| %-9s| %-20s|\n",
                count, report.inspector, report.latitude, report.longitude,
                report.category, severity_label, time_str);
        
        //We make it pretty :D
        int desc_len = strlen(report.description);
        int padding  = 78 - desc_len;
        if (padding < 0) padding = 0;

        printf("\t|---|-----------------|-----------|------------|----------------|----------|---------------------|\n");
        printf("\t||||| Description: %s%*s|\n", report.description, padding, "");
        printf("\t|---|-----------------|-----------|------------|----------------|----------|---------------------|\n");
        
        count++;
    }

    //bytes_read == 0 means clean end of file - normal exit.
    //bytes_read < 0 means an error occurred during reading.
    if (bytes_read < 0) {
        perror("cmd_list: error reading reports.dat");
        close(fd);
        return;
    } else {
        printf("\t|===|=================|===========|============|================|==========|=====================|\n");
        printf("\n\n\n\n===--------===--------===--------===--------=== End of reports (total: %d) ===--------===--------===--------===--------===\n\n", count);
        close(fd);
    }

    log_action(district, role, user, "listed_reports");
}

// ===========================================================================
//                                CMD_VIEW
// ===========================================================================

void cmd_view(const char *role, const char *user, const char *district, int report_id) {
    ensure_district_exists(district, role, user);
    
    char reports_path[512];
    BUILD_PATH(reports_path, district, "reports.dat");

    //Both roles can read reports.dat, but we can check the bits explicitly to be sure,
    //since the spec requires a stat() check before every operation.
    if(!check_permission(reports_path, role, 1, 0)) {
        return; // Permission check failed — error message already printed.
    }

    // Make sure the report ID is not negative before we do any math with it.
    if (report_id < 0) {
        fprintf(stderr, "cmd_view_error: report ID cannot be negative\n");
        return;
    }

    int fd = open(reports_path, O_RDONLY);
    if (fd < 0) {
        perror("cmd_view_error: could not open reports.dat");
        return;
    }
    
    // lseek(fd, offset, SEEK_SET) moves the file cursor to an exact byte position.
    // Since every record is sizeof(Report) bytes, record N starts at byte offset N
    // Therefore, we can jump directly to the desired report without reading the whole file.
    off_t offset = (off_t)report_id * sizeof(Report);
    if(lseek(fd, offset, SEEK_SET) < 0) {
        perror("cmd_view_error: lseek failed");
        close(fd);
        return;
    }

    //Now read exactly one record from wherever we just seeked out.
    Report report;
    ssize_t bytes_read = read(fd, &report, sizeof(Report));
    close(fd);

    if (bytes_read == 0) {
        // read() returned 0 meaning the offset was at or past the end of the file.
        // This means the report ID doesn't exist.
        fprintf(stderr, "cmd_view_error: report ID %d does not exist (end of file reached)\n", report_id);
        return;
    } else if (bytes_read < 0) {
        perror("cmd_view_error: error reading reports.dat");
        return;
    } else if (bytes_read != (ssize_t)sizeof(Report)) {
        fprintf(stderr, "cmd_view_error: partial read occurred, expected %zu bytes but got %zd\n", sizeof(Report), bytes_read);
        return;
    }

    // Format the timestamp into a human-readable string
    char time_str[20];
    struct tm *tm_info = localtime(&report.timestamp);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%S", tm_info);

    const char *severity_label;
    switch (report.severity) {
        case 1: severity_label = "minor"; break;
        case 2: severity_label = "moderate"; break;
        case 3: severity_label = "critical"; break;
        default: severity_label = "unknown"; break;
    }

    // Print the full report - all fields, not just a summary.
    printf("\n|======| District: %s | Report ID: %d |======|\n", district, report.id);
    printf("\tInspector: %s\n", report.inspector);
    printf("\tLatitude %.6f, Longitude %.6f\n", report.latitude, report.longitude);
    printf("\tCategory: %s\n", report.category);
    printf("\tSeverity: %s\n", severity_label);
    printf("\tTimestamp: %s\n", time_str);
    printf("\tDescription: %s\n\n", report.description);

    char action_desc[256];
    snprintf(action_desc, sizeof(action_desc), "viewed_report_id=%d", report_id);
    log_action(district, role, user, action_desc);  
}

// ===========================================================================
//                             CMD_REMOVE_REPORT
// ===========================================================================

void cmd_remove_report(const char *role, const char *user, const char *district, int report_id) {
    ensure_district_exists(district, role, user);
    
    //Manager role only — check write permission on reports.dat - inspectors can't remove reports.
    if(strcmp(role, "manager") != 0) {
        fprintf(stderr, "Permission denied: only managers can remove reports\n");
        return;
    }

    char reports_path[512];
    BUILD_PATH(reports_path, district, "reports.dat");

    //Manager maps to owner bits - check that owner has write access.
    if(!check_permission(reports_path, role, 0, 1)) {
        return; // Permission check failed — error message already printed.
    }

    if(report_id < 0) {
        fprintf(stderr, "cmd_remove_report_error: report ID cannot be negative\n");
        return;
    }

    // We need the total number of reports to know how many records to read and rewrite when we remove one.
    int total_reports = get_next_id(district);
    if (total_reports < 0) {
        fprintf(stderr, "cmd_remove_report_error: could not determine total number of reports\n");
        return;
    }

    if(report_id >= total_reports) {
        fprintf(stderr, "cmd_remove_report_error: report ID %d does not exist (only %d reports in file)\n", report_id, total_reports);
        return;
    }

    // Open the file for readind AND writing without truncating it.
    // O_RDWR allows us to read existing records and write changes back to earlier positions.
    int fd = open(reports_path, O_RDWR);
    if (fd < 0) {
        perror("cmd_remove_report_error: could not open reports.dat");
        return;
    }

    // --- Shift every record after report_id one position earlier ---
    // We read record N+1 and write it back to position N, effectively overwriting the record we want to remove.
    Report buffer;
    for (int i = report_id; i < total_reports - 1; i++) {

        // Seek to record N+1
        off_t read_offset = (off_t)(i+1) * sizeof(Report);
        if (lseek(fd, read_offset, SEEK_SET) < 0) {
            perror("cmd_remove_report_error: lseek for read failed in reports.dat");
            close(fd);
            return;
        }

        // Read the record
        if (read(fd, &buffer, sizeof(Report)) != (ssize_t)sizeof(Report)) {
            perror("cmd_remove_report_error: could not read from reports.dat");
            close(fd);
            return;
        }

        // Write the record to the previous position
        off_t write_offset = (off_t)i * sizeof(Report);
        if (lseek(fd, write_offset, SEEK_SET) < 0) {
            perror("cmd_remove_report_error: lseek for write failed in reports.dat");
            close(fd);
            return;
        }

        if (write(fd, &buffer, sizeof(Report)) != (ssize_t)sizeof(Report)) {
            perror("cmd_remove_report_error: could not write to reports.dat");
            close(fd);
            return;
        }
    }

    // --- Truncate the file to remove the now-duplicate last record ---
    // After shifting, the last record exists twice — once in its original slot
    // and once copied into the slot before it. ftruncate() cuts the file down
    // to exactly (total - 1) records, removing that duplicate cleanly.
    off_t new_size = (off_t)(total_reports - 1) * sizeof(Report);
    if (ftruncate(fd, new_size) < 0) {
        perror("cmd_remove_report_error: could not truncate reports.dat");
        close(fd);
        return;
    }

    close(fd);

    printf("|===| Report ID %d removed successfully from district '%s' |===| %d report(s) remaining |===|\n", 
            report_id, district, total_reports - 1);
    
    char action_desc[256];
    snprintf(action_desc, sizeof(action_desc), "removed_report_id=%d", report_id);
    log_action(district, role, user, action_desc);
}

// ===========================================================================
//                             CMD_UPDATE_THRESHOLD
// ===========================================================================

void cmd_update_threshold(const char *role, const char *user, const char *district, int value) {
    ensure_district_exists(district, role, user);
    printf("[TODO] update threshold to %d in district: %s\n", value, district);
}

// ===========================================================================
//                                 CMD_FILTER
// ===========================================================================

void cmd_filter(const char *role, const char *user, const char *district, int argc, char *argv[], int cond_start) {
    ensure_district_exists(district, role, user);
    printf("[TODO] filter reports in district: %s\n", district);
}