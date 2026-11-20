#include "mel_bridge.h"
#include <errno.h>
#include <spawn.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>

extern char **environ;

int process_recording_with_python(const char *recording_wav_path)
{
    char *python_arguments[] = {
        "python3",
        "melSpec/gemSpec.py",
        (char *)recording_wav_path,
        NULL
    };

    pid_t python_process_id;
    int python_wait_status;
    int python_start_error = posix_spawnp(
        &python_process_id, python_arguments[0], NULL, NULL,
        python_arguments, environ
    );

    if (python_start_error != 0) {
        fprintf(stderr, "Kunne ikke starte Python: %s\n", strerror(python_start_error));
        return 1;
    }

    while (waitpid(python_process_id, &python_wait_status, 0) == -1) {
        if (errno == EINTR) continue;

        perror("Kunne ikke vente på Python");
        return 1;
    }

    if (!WIFEXITED(python_wait_status))
        return 1;

    return WEXITSTATUS(python_wait_status);
}
