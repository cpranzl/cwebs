#include <stdio.h>
#include <string.h>
#include <time.h>
#include <microhttpd.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

#define PORT 8080

int handle_request(void *cls, struct MHD_Connection *connection,
                   const char *url, const char *method,
                   const char *version, const char *upload_data,
                   size_t *upload_data_size, void **con_cls) {
    if (strcmp(method, "GET") != 0) {
        return MHD_NO; // Nur GET-Anfragen werden unterstützt
    }

    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) {
        perror("pipe");
        return MHD_NO;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return MHD_NO;
    }

    if (pid == 0) { // Kindprozess
        close(pipe_fd[0]); // Leseende schließen

        // Aktuelle Zeit ermitteln
        time_t rawtime;
        struct tm *timeinfo;
        char buffer[80];

        time(&rawtime);
        timeinfo = localtime(&rawtime);

        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
        char response[100];
        snprintf(response, sizeof(response), "{\"time\": \"%s\"}", buffer);

        // Antwort in die Pipe schreiben
        write(pipe_fd[1], response, strlen(response) + 1);
        close(pipe_fd[1]);

        exit(0); // Kindprozess beenden
    } else { // Elternprozess
        close(pipe_fd[1]); // Schreibende schließen

        // Warten auf die Antwort des Kindprozesses
        char response[100];
        read(pipe_fd[0], response, sizeof(response));
        close(pipe_fd[0]);

        // Antwort erstellen
        struct MHD_Response *http_response;
        http_response = MHD_create_response_from_buffer(strlen(response),
                                                        (void *)response,
                                                        MHD_RESPMEM_MUST_COPY);

        MHD_add_response_header(http_response, "Content-Type", "application/json");
        int ret = MHD_queue_response(connection, MHD_HTTP_OK, http_response);
        MHD_destroy_response(http_response);

        return ret;
    }
}

int main() {
    struct MHD_Daemon *daemon;

    daemon = MHD_start_daemon(MHD_USE_INTERNAL_POLLING_THREAD, PORT, NULL, NULL,
                              &handle_request, NULL, MHD_OPTION_END);
    if (NULL == daemon) {
        fprintf(stderr, "Failed to start server\n");
        return 1;
    }

    printf("Server running on http://localhost:%d\n", PORT);

    // Server läuft, bis ein Tastendruck erfolgt
    getchar();

    MHD_stop_daemon(daemon);
    return 0;
}