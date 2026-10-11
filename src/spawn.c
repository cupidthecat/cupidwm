/* Command and pipeline process creation. */

void spawn(const char * const *argv)
{
	if (!argv || !argv[0])
		return;

	int argc = 0;
	while (argv[argc])
		argc++;

	int cmd_count = 1;
	for (int i = 0; i < argc; i++) {
		if (strcmp(argv[i], "|") == 0)
			cmd_count++;
	}

	const char ***commands = malloc(cmd_count * sizeof(char **)); /* *** bruh */
	if (!commands) {
		perror("malloc commands");
		return;
	}

	/* initialize all command pointers to NULL for safe cleanup */
	for (int i = 0; i < cmd_count; i++)
		commands[i] = NULL;

	int cmd_idx = 0;
	int arg_start = 0;
	for (int i = 0; i <= argc; i++) {
		if (!argv[i] || strcmp(argv[i], "|") == 0) {
			int len = i - arg_start;
			const char **cmd_args = malloc((len + 1) * sizeof(char *));

			if (!cmd_args) {
				perror("malloc cmd_args");

				for (int j = 0; j < cmd_idx; j++)
					free(commands[j]);

				free(commands);
				return;
			}

			for (int j = 0; j < len; j++)
				cmd_args[j] = argv[arg_start + j];

			cmd_args[len] = NULL;
			if (cmd_idx >= cmd_count) {
				free(cmd_args);
				for (int j = 0; j < cmd_idx; j++)
					free(commands[j]);
				free(commands);
				fprintf(stderr, "cupidwm: internal command parser bounds error\n");
				return;
			}
			commands[cmd_idx++] = cmd_args;
			arg_start = i + 1;
		}
	}

	int (*pipes)[2] = NULL;
	if (cmd_count > 1)
		pipes = malloc(sizeof(*pipes) * (size_t)(cmd_count - 1));
	if (cmd_count > 1 && !pipes) {
		perror("malloc pipes");

		for (int j = 0; j < cmd_count; j++)
			free(commands[j]);

		free(commands);
		return;
	}

	for (int i = 0; i < cmd_count - 1; i++) {
		if (pipe(pipes[i]) == -1) {
			perror("pipe");
			for (int j = 0; j < i; j++) {
				close(pipes[j][0]);
				close(pipes[j][1]);
			}

			for (int j = 0; j < cmd_count; j++)
				free(commands[j]);

			free(commands);
			free(pipes);
			return;
		}
	}

	for (int i = 0; i < cmd_count; i++) {
		if (!commands[i] || !commands[i][0])
			continue;

		pid_t pid = fork();
		if (pid < 0) {
			perror("fork");

			for (int k = 0; k < cmd_count - 1; k++) {
				close(pipes[k][0]);
				close(pipes[k][1]);
			}

			for (int j = 0; j < cmd_count; j++)
				free(commands[j]);

			free(commands);
			free(pipes);
			return;
		}
		if (pid == 0) {
			close(ConnectionNumber(dpy));

			if (i > 0)
				dup2(pipes[i - 1][0], STDIN_FILENO);

			if (i < cmd_count - 1)
				dup2(pipes[i][1], STDOUT_FILENO);

			for (int k = 0; k < cmd_count - 1; k++) {
				close(pipes[k][0]);
				close(pipes[k][1]);
			}

			execvp(commands[i][0], (char* const*)(void*)commands[i]);
			fprintf(stderr, "cupidwm: execvp '%s' failed\n", commands[i][0]);
			exit(EXIT_FAILURE);
		}
	}

	for (int i = 0; i < cmd_count - 1; i++) {
		close(pipes[i][0]);
		close(pipes[i][1]);
	}

	for (int i = 0; i < cmd_count; i++)
		free(commands[i]);

	free(commands);
	free(pipes);
}

