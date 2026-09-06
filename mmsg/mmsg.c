#define _GNU_SOURCE
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "mango/common/log.h"

static void usage(void) {
	printf("Usage: mmsg <command> [args...]\n\n");
	printf("One-shot queries (get):\n");
	printf(
		"  get version                              Show compositor version\n");
	printf("  get cursorpos                            Show pointer position + "
		   "monitor\n");
	printf("  get keymode                              Show current keymode\n");
	printf("  get keyboardlayout                       Show current keyboard "
		   "layout\n");
	printf("  get last_open_surface [monitor]          Show last open surface "
		   "(default focused monitor)\n");
	printf("  get monitor <name>                       Show monitor details\n");
	printf("  get focusing-client                      Show focused client "
		   "details\n");
	printf("  get client <id>                          Show client details by "
		   "ID\n");
	printf("  get tag <monitor> <index>                Show tag details "
		   "(1‑based index)\n");
	printf("  get all-clients                          List all clients\n");
	printf("  get all-monitors                         List all monitors\n");
	printf("  get all-devices                          List physical input "
		   "devices\n");
	printf("  get all-tags                             List all tags (all "
		   "monitors)\n");
	printf("  get layouts                              List all available "
		   "layouts\n");
	printf(
		"  get tags <monitor>                       List tags for a monitor\n");
	printf("  dispatch <func>[,arg...] [client,<id>]   Call a compositor "
		   "function\n");
	printf("     <func> and arguments are separated by commas.\n");
	printf("     Add 'client,<id>' at the beginning or end to target a "
		   "specific client.\n");
	printf("     Examples:\n");
	printf("       dispatch togglefloating\n");
	printf("       dispatch movewin,10,100\n");
	printf("       dispatch movewin,10,100 client,4\n");
	printf("Persistent streams (watch):\n");
	printf(
		"  watch monitor <name>                     Stream monitor changes\n");
	printf("  watch focusing-client                    Stream focused client "
		   "changes\n");
	printf(
		"  watch client <id>                        Stream client changes\n");
	printf("  watch tags <monitor>                     Stream tag changes for "
		   "a monitor\n");
	printf("  watch all-monitors                       Stream all monitors "
		   "changes\n");
	printf(
		"  watch all-tags                           Stream all tags changes\n");
	printf("  watch all-clients                        Stream all clients "
		   "changes\n");
	printf("  watch all-devices                        Stream the last input "
		   "device that triggered an event\n");
	printf(
		"  watch keymode                            Stream keymode changes\n");
	printf("  watch keyboardlayout                     Stream keyboard layout "
		   "changes\n");
	printf("  watch last_open_surface [monitor]        Stream last open "
		   "surface changes\n\n");
	printf("Environment:\n");
	printf("  MANGO_INSTANCE_SIGNATURE  IPC socket path (set by the "
		   "compositor)\n\n");
	printf("Run 'mmsg --help', '-h' or 'help' to see this message.\n");
}

int main(int argc, char *argv[]) {
	if (argc >= 2 &&
		(strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0 ||
		 strcmp(argv[1], "help") == 0)) {
		usage();
		return EXIT_SUCCESS;
	}

	if (argc < 2) {
		mango_error(false, WLR_INFO,
					"Usage: mmsg <command> [args...]\n"
					"  get <type> ...      one-shot request\n"
					"  watch <type> ...    persistent stream\n");
		return EXIT_FAILURE;
	}

	const char *socket_path = getenv("MANGO_INSTANCE_SIGNATURE");
	if (!socket_path) {
		mango_error(false, WLR_ERROR,
					"MANGO_INSTANCE_SIGNATURE is not set. Did you run "
					"'mmsg' in mango?\n");
		return EXIT_FAILURE;
	}

	int sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock < 0) {
		perror("socket");
		return EXIT_FAILURE;
	}

	struct sockaddr_un addr = {.sun_family = AF_UNIX};
	strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

	if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("connect");
		close(sock);
		return EXIT_FAILURE;
	}

	char cmd[4096] = {0};
	int offset = 0;
	for (int i = 1; i < argc; i++) {
		int n = snprintf(cmd + offset, sizeof(cmd) - offset, "%s%s", argv[i],
						 (i == argc - 1) ? "" : " ");
		if (n < 0 || n >= (int)(sizeof(cmd) - offset)) {
			mango_error(false, WLR_ERROR, "command too long.\n");
			close(sock);
			return EXIT_FAILURE;
		}
		offset += n;
	}

	int n = snprintf(cmd + offset, sizeof(cmd) - offset, "\n");
	if (n < 0 || n >= (int)(sizeof(cmd) - offset)) {
		mango_error(false, WLR_ERROR, "command too long to append newline.\n");
		close(sock);
		return EXIT_FAILURE;
	}

	if (send(sock, cmd, strlen(cmd), MSG_NOSIGNAL) < 0) {
		perror("send");
		close(sock);
		return EXIT_FAILURE;
	}

	FILE *stream = fdopen(sock, "r");
	if (!stream) {
		perror("fdopen");
		close(sock);
		return EXIT_FAILURE;
	}

	char *line = NULL;
	size_t len = 0;
	while (getline(&line, &len, stream) != -1) {
		printf("%s", line);
		fflush(stdout);
	}

	if (ferror(stream)) {
		perror("recv");
		free(line);
		fclose(stream);
		return EXIT_FAILURE;
	}

	free(line);
	fclose(stream);
	return EXIT_SUCCESS;
}
