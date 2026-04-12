#define _POSIX_C_SOURCE 200809L

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

typedef struct {
    char *path;
} Task;

typedef struct {
    pid_t pid;
    size_t task_index;
} RunningTask;

static void die(const char *message) {
    perror(message);
    exit(EXIT_FAILURE);
}

static int has_extension(const char *filename, const char *extension) {
    if (extension == NULL || *extension == '\0') {
        return 1;
    }

    const char *dot = strrchr(filename, '.');
    if (dot == NULL) {
        return 0;
    }

    return strcasecmp(dot, extension) == 0;
}

static int is_regular_file(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return 0;
    }
    return S_ISREG(st.st_mode);
}

static void append_task(Task **tasks, size_t *count, size_t *capacity, const char *path) {
    if (*count == *capacity) {
        size_t new_capacity = (*capacity == 0) ? 16 : (*capacity * 2);
        Task *new_tasks = realloc(*tasks, new_capacity * sizeof(Task));
        if (new_tasks == NULL) {
            die("realloc");
        }
        *tasks = new_tasks;
        *capacity = new_capacity;
    }

    (*tasks)[*count].path = strdup(path);
    if ((*tasks)[*count].path == NULL) {
        die("strdup");
    }

    (*count)++;
}

static int collect_tasks(const char *input_dir, const char *extension, Task **tasks, size_t *task_count) {
    DIR *dir = opendir(input_dir);
    if (dir == NULL) {
        return -1;
    }

    struct dirent *entry;
    size_t count = 0;
    size_t capacity = 0;
    Task *buffer = NULL;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        if (!has_extension(entry->d_name, extension)) {
            continue;
        }

        char full_path[PATH_MAX];
        int written = snprintf(full_path, sizeof(full_path), "%s/%s", input_dir, entry->d_name);
        if (written < 0 || (size_t)written >= sizeof(full_path)) {
            continue;
        }

        if (!is_regular_file(full_path)) {
            continue;
        }

        append_task(&buffer, &count, &capacity, full_path);
    }

    closedir(dir);
    *tasks = buffer;
    *task_count = count;
    return 0;
}

static pid_t spawn_worker(const char *worker_script, const char *input_file, const char *output_dir) {
    pid_t pid = fork();
    if (pid < 0) {
        return -1;
    }

    if (pid == 0) {
        char *const argv[] = {
            (char *)worker_script,
            (char *)input_file,
            (char *)output_dir,
            NULL
        };

        execve(worker_script, argv, environ);
        perror("execve worker");
        _exit(127);
    }

    return pid;
}

static int run_finalizer(const char *final_script, const char *output_dir, const char *result_file) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork finalizer");
        return -1;
    }

    if (pid == 0) {
        char *const argv[] = {
            (char *)final_script,
            (char *)output_dir,
            (char *)result_file,
            NULL
        };

        execve(final_script, argv, environ);
        perror("execve finalizer");
        _exit(127);
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid finalizer");
        return -1;
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "Final script failed with status %d\n", WEXITSTATUS(status));
        return -1;
    }

    return 0;
}

static void free_tasks(Task *tasks, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        free(tasks[i].path);
    }
    free(tasks);
}

static int find_running_index(const RunningTask *running, size_t running_count, pid_t pid) {
    for (size_t i = 0; i < running_count; ++i) {
        if (running[i].pid == pid) {
            return (int)i;
        }
    }
    return -1;
}

int main(int argc, char **argv) {
    if (argc < 7 || argc > 8) {
        fprintf(stderr,
                "Usage: %s <input_dir> <worker_script> <final_script> <parallelism> <output_dir> <result_file> [extension]\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    const char *input_dir = argv[1];
    const char *worker_script = argv[2];
    const char *final_script = argv[3];
    int parallelism = atoi(argv[4]);
    const char *output_dir = argv[5];
    const char *result_file = argv[6];
    const char *extension = (argc == 8) ? argv[7] : ".tiff";

    if (parallelism <= 0) {
        fprintf(stderr, "Parallelism must be > 0\n");
        return EXIT_FAILURE;
    }

    Task *tasks = NULL;
    size_t task_count = 0;

    if (collect_tasks(input_dir, extension, &tasks, &task_count) != 0) {
        perror("collect_tasks");
        return EXIT_FAILURE;
    }

    if (task_count == 0) {
        fprintf(stderr, "No input files found in %s with extension %s\n", input_dir, extension);
        free_tasks(tasks, task_count);
        return EXIT_FAILURE;
    }

    RunningTask *running = calloc((size_t)parallelism, sizeof(RunningTask));
    if (running == NULL) {
        die("calloc");
    }

    size_t next_task = 0;
    size_t running_count = 0;
    int worker_failures = 0;

    while (next_task < task_count && running_count < (size_t)parallelism) {
        pid_t pid = spawn_worker(worker_script, tasks[next_task].path, output_dir);
        if (pid < 0) {
            perror("fork worker");
            break;
        }

        running[running_count].pid = pid;
        running[running_count].task_index = next_task;
        fprintf(stdout, "[START] pid=%d file=%s\n", (int)pid, tasks[next_task].path);
        fflush(stdout);

        running_count++;
        next_task++;
    }

    while (running_count > 0) {
        int status = 0;
        pid_t done_pid = wait(&status);
        if (done_pid < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("wait");
            break;
        }

        int idx = find_running_index(running, running_count, done_pid);
        if (idx >= 0) {
            size_t task_idx = running[idx].task_index;
            int ok = WIFEXITED(status) && WEXITSTATUS(status) == 0;
            if (!ok) {
                worker_failures++;
            }

            fprintf(stdout,
                    "[DONE] pid=%d status=%s file=%s\n",
                    (int)done_pid,
                    ok ? "OK" : "FAIL",
                    tasks[task_idx].path);
            fflush(stdout);

            running[idx] = running[running_count - 1];
            running_count--;
        }

        while (next_task < task_count && running_count < (size_t)parallelism) {
            pid_t pid = spawn_worker(worker_script, tasks[next_task].path, output_dir);
            if (pid < 0) {
                perror("fork worker");
                break;
            }

            running[running_count].pid = pid;
            running[running_count].task_index = next_task;
            fprintf(stdout, "[START] pid=%d file=%s\n", (int)pid, tasks[next_task].path);
            fflush(stdout);

            running_count++;
            next_task++;
        }
    }

    free(running);

    if (worker_failures > 0) {
        fprintf(stderr, "Workers failed: %d\n", worker_failures);
    }

    int final_rc = run_finalizer(final_script, output_dir, result_file);

    free_tasks(tasks, task_count);

    return (final_rc == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
