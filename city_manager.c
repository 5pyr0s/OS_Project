#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>        // open(), O_RDONLY, O_WRONLY, etc.
#include <unistd.h>       // read(), write(), close(), lseek(), symlink(), unlink()
#include <sys/stat.h>     // stat(), lstat(), chmod(), mkdir(), struct stat
#include <sys/types.h>    // mode_t, off_t, etc.
#include <time.h>         // time_t, time()

// Fixed field sizes — changing these later will corrupt existing binary files!
#define NAME_LEN      64
#define CATEGORY_LEN  32
#define DESC_LEN      256



typedef struct {
    int    id;                     // 4 bytes
    char   inspector[NAME_LEN];    // 64 bytes
    double latitude;               // 8 bytes
    double longitude;              // 8 bytes
    char   category[CATEGORY_LEN]; // 32 bytes
    int    severity;               // 4 bytes  (1=minor, 2=moderate, 3=critical)
    time_t timestamp;              // 8 bytes  (Unix timestamp)
    char   description[DESC_LEN];  // 256 bytes
} Report;
// Total: roughly 384 bytes per record (exact value: sizeof(Report))



// --- Forward declarations ---
void cmd_add(const char *role, const char *user, const char *district);
void cmd_list(const char *role, const char *user, const char *district);
void cmd_view(const char *role, const char *user, const char *district, int report_id);
void cmd_remove_report(const char *role, const char *user, const char *district, int report_id);
void cmd_update_threshold(const char *role, const char *user, const char *district, int value);
void cmd_filter(const char *role, const char *user, const char *district, int argc, char *argv[], int cond_start);



int main(int argc, char *argv[]) {
    char *role     = NULL;
    char *user     = NULL;
    char *command  = NULL;
    char *district = NULL;

    // We'll need these for specific commands
    int   report_id = -1;
    int   threshold = -1;
    int   filter_conditions_start = -1; // index in argv where conditions begin

    // --- Parse all arguments ---
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
            filter_conditions_start = i + 1; // conditions start after district
        }
    }

    // --- Validate required arguments ---
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

    // --- Dispatch to the right function ---
    if      (strcmp(command, "add")              == 0) cmd_add(role, user, district);
    else if (strcmp(command, "list")             == 0) cmd_list(role, user, district);
    else if (strcmp(command, "view")             == 0) cmd_view(role, user, district, report_id);
    else if (strcmp(command, "remove_report")    == 0) cmd_remove_report(role, user, district, report_id);
    else if (strcmp(command, "update_threshold") == 0) cmd_update_threshold(role, user, district, threshold);
    else if (strcmp(command, "filter")           == 0) cmd_filter(role, user, district, argc, argv, filter_conditions_start);

    return 0;
}

void cmd_add(const char *role, const char *user, const char *district) {
    printf("[TODO] add report to district: %s\n", district);
}
void cmd_list(const char *role, const char *user, const char *district) {
    printf("[TODO] list reports in district: %s\n", district);
}
void cmd_view(const char *role, const char *user, const char *district, int report_id) {
    printf("[TODO] view report %d in district: %s\n", report_id, district);
}
void cmd_remove_report(const char *role, const char *user, const char *district, int report_id) {
    printf("[TODO] remove report %d from district: %s\n", report_id, district);
}
void cmd_update_threshold(const char *role, const char *user, const char *district, int value) {
    printf("[TODO] update threshold to %d in district: %s\n", value, district);
}
void cmd_filter(const char *role, const char *user, const char *district, int argc, char *argv[], int cond_start) {
    printf("[TODO] filter reports in district: %s\n", district);
}