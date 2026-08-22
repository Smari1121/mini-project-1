#include "frecency.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pwd.h>

#define FRECENCY_FILE ".cshell_frecency"

static void get_frecency_path(char *buf, size_t size) {
    struct passwd *pw = getpwuid(getuid());
    if (pw) {
        snprintf(buf, size, "%s/%s", pw->pw_dir, FRECENCY_FILE);
    } else {
        buf[0] = '\0';
    }
}

typedef struct {
    char path[1024];
    int rank;
    time_t last_access;
} frecency_entry_t;

void frecency_update(const char *path) {
    char db_path[1024];
    get_frecency_path(db_path, sizeof(db_path));
    if (!db_path[0]) return;

    FILE *f = fopen(db_path, "r");
    frecency_entry_t entries[512];
    int count = 0;
    int found = 0;

    if (f) {
        char line[2048];
        while (fgets(line, sizeof(line), f) && count < 512) {
            line[strcspn(line, "\n")] = '\0';
            char *sep1 = strrchr(line, '|');
            if (!sep1) continue;
            *sep1 = '\0';
            char *sep2 = strrchr(line, '|');
            if (!sep2) continue;
            *sep2 = '\0';
            
            strncpy(entries[count].path, line, sizeof(entries[count].path));
            entries[count].rank = atoi(sep2 + 1);
            entries[count].last_access = (time_t)atol(sep1 + 1);
            
            if (strcmp(entries[count].path, path) == 0) {
                entries[count].rank++;
                entries[count].last_access = time(NULL);
                found = 1;
            }
            count++;
        }
        fclose(f);
    }

    if (!found && count < 512) {
        strncpy(entries[count].path, path, sizeof(entries[count].path));
        entries[count].rank = 1;
        entries[count].last_access = time(NULL);
        count++;
    }

    f = fopen(db_path, "w");
    if (f) {
        for (int i = 0; i < count; i++) {
            fprintf(f, "%s|%d|%ld\n", entries[i].path, entries[i].rank, (long)entries[i].last_access);
        }
        fclose(f);
    }
}

char *frecency_lookup(const char *name) {
    char db_path[1024];
    get_frecency_path(db_path, sizeof(db_path));
    if (!db_path[0]) return NULL;

    FILE *f = fopen(db_path, "r");
    if (!f) return NULL;

    char line[2048];
    char best_path[1024] = {0};
    double best_score = -1.0;
    time_t best_time = 0;

    time_t now = time(NULL);

    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        char *sep1 = strrchr(line, '|');
        if (!sep1) continue;
        *sep1 = '\0';
        char *sep2 = strrchr(line, '|');
        if (!sep2) continue;
        *sep2 = '\0';

        char *path = line;
        if (!strstr(path, name)) continue;
        if (access(path, F_OK) != 0) continue;

        int rank = atoi(sep2 + 1);
        time_t last_access = (time_t)atol(sep1 + 1);
        
        double age = difftime(now, last_access);
        double factor = 0.25;
        if (age < 3600) factor = 4.0;
        else if (age < 86400) factor = 2.0;
        else if (age < 604800) factor = 0.5;

        double score = rank * factor;
        
        if (score > best_score || (score == best_score && last_access > best_time) || (score == best_score && last_access == best_time && strcmp(path, best_path) < 0)) {
            best_score = score;
            best_time = last_access;
            strncpy(best_path, path, sizeof(best_path));
        }
    }
    fclose(f);

    if (best_path[0]) return strdup(best_path);
    return NULL;
}
