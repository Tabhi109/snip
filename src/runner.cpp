#include "snip/runner.hpp"

#include <unistd.h>
#include <sys/wait.h>
#include <poll.h>
#include <array>
#include <stdexcept>
#include <vector>

namespace snip {

CommandResult ProcessRunner::execute(const std::vector<std::string>& args) {
    if (args.empty()) {
        return {-1, "", "Error: No command provided to snip.\n"};
    }

    // 1. Create two unidirectional pipes:
    // pipefd[0] = read end (parent reads from this)
    // pipefd[1] = write end (child writes to this)
    int out_pipe[2];
    int err_pipe[2];

    if (pipe(out_pipe) < 0 || pipe(err_pipe) < 0) {
        throw std::runtime_error("snip: failed to create OS pipes");
    }

    // 2. Fork into parent and child
    pid_t pid = fork();
    if (pid < 0) {
        throw std::runtime_error("snip: fork failed");
    }

    if (pid == 0) {
        // --- CHILD PROCESS ---
        // Close the read ends in the child (child never reads)
        close(out_pipe[0]);
        close(err_pipe[0]);

        // Redirect child stdout -> out_pipe[1]
        dup2(out_pipe[1], STDOUT_FILENO);
        close(out_pipe[1]);

        // Redirect child stderr -> err_pipe[1]
        dup2(err_pipe[1], STDERR_FILENO);
        close(err_pipe[1]);

        // Prepare NULL-terminated array of char* for execvp
        std::vector<char*> c_args;
        c_args.reserve(args.size() + 1);
        for (const auto& arg : args) {
            c_args.push_back(const_cast<char*>(arg.c_str()));
        }
        c_args.push_back(nullptr);

        // execvp replaces the current process image with the target command
        execvp(c_args[0], c_args.data());

        // If execvp reaches here, it failed to launch
        _exit(127);
    }

    // --- PARENT PROCESS ---
    // Close the write ends in parent (parent never writes)
    close(out_pipe[1]);
    close(err_pipe[1]);

    CommandResult result;
    std::array<char, 4096> buffer;

    // Use poll() to multiplex reads from both stdout and stderr pipes
    // This completely prevents deadlocks if one pipe fills up
    std::array<pollfd, 2> fds{};
    fds[0] = { out_pipe[0], POLLIN, 0 };
    fds[1] = { err_pipe[0], POLLIN, 0 };

    int active_pipes = 2;

    while (active_pipes > 0) {
        int poll_res = poll(fds.data(), static_cast<nfds_t>(fds.size()), -1);
        if (poll_res < 0) {
            break; // Interrupted by signal or error
        }

        for (size_t i = 0; i < 2; ++i) {
            if (fds[i].fd == -1) continue;

            if (fds[i].revents & POLLIN) {
                ssize_t bytes_read = read(fds[i].fd, buffer.data(), buffer.size());
                if (bytes_read > 0) {
                    if (i == 0) {
                        result.stdout_output.append(buffer.data(), static_cast<size_t>(bytes_read));
                    } else {
                        result.stderr_output.append(buffer.data(), static_cast<size_t>(bytes_read));
                    }
                } else {
                    // EOF reached on this pipe
                    close(fds[i].fd);
                    fds[i].fd = -1;
                    --active_pipes;
                }
            } else if (fds[i].revents & (POLLHUP | POLLERR)) {
                // Child closed its end or error occurred
                close(fds[i].fd);
                fds[i].fd = -1;
                --active_pipes;
            }
        }
    }

    // 3. Wait for child to exit and capture its exit status code
    int status = 0;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status)) {
        result.exit_code = WEXITSTATUS(status);
    } else {
        result.exit_code = -1;
    }

    return result;
}

} // namespace snip