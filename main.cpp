#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>// POSIX系统调用和系统限制定义
#include <sys/wait.h>// 等待系统调用
#include <fcntl.h> // 文件控制选项
#include <sstream>
#include <algorithm>

// Shell类定义了一个简单的shell环境
class Shell {
public:
    void run() {
        std::string line;
        while (true) {
            std::cout << "myshell> ";
            if (!std::getline(std::cin, line)) {
                break;
            }
            if (line.empty()) {
                continue;
            }
            // 解析命令行，分割命令和参数
            std::vector<std::string> tokens = parseCommand(line);
            if (tokens.empty()) {
                continue;
            }
            // 根据命令执行不同的操作
            if (tokens[0] == "exit") {
                break;
            } else if (tokens[0] == "cd") {
                changeDirectory(tokens);
            } else {
                execute(line);
            }
        }
    }

private:
// 执行命令，支持管道操作
    void execute(const std::string& line) {
        //解析命令行，创建管道，fork子进程，执行命令的过程
        std::vector<std::string> commands;
        std::istringstream stream(line);
        std::string command;
        while (std::getline(stream, command, '|')) {
            commands.push_back(command);
        }

        int numCommands = commands.size();
        int in_fd = -1;
        for (int i = 0; i < numCommands; ++i) {
            int pipe_fd[2];
            if (i < numCommands - 1) {
                if (pipe(pipe_fd) == -1) {
                    perror("pipe");
                    return;
                }
            }

            pid_t pid = fork();
            if (pid == -1) {
                perror("fork");
                return;
            } else if (pid == 0) {
                if (in_fd != -1) {
                    dup2(in_fd, STDIN_FILENO);
                    close(in_fd);
                }
                if (i < numCommands - 1) {
                    close(pipe_fd[0]);
                    dup2(pipe_fd[1], STDOUT_FILENO);
                    close(pipe_fd[1]);
                }

                std::vector<std::string> args = parseCommand(commands[i]);
                std::vector<char*> c_args;
                for (const auto& arg : args) {
                    c_args.push_back(const_cast<char*>(arg.c_str()));
                }
                c_args.push_back(nullptr);

                executeCommand(c_args);
            } else {
                if (in_fd != -1) {
                    close(in_fd);
                }
                if (i < numCommands - 1) {
                    close(pipe_fd[1]);
                    in_fd = pipe_fd[0];
                }
                waitpid(pid, nullptr, 0);
            }
        }
    }
// 解析命令行，返回命令和参数的列表
    std::vector<std::string> parseCommand(const std::string& command) {
        std::istringstream stream(command);
        std::vector<std::string> args;
        std::string arg;
        while (stream >> arg) {
            args.push_back(arg);
        }
        return args;
    }
    
    // 执行命令，处理输入输出重定向
    void executeCommand(const std::vector<char*>& args) {
        std::string inputFile, outputFile;
        bool inputRedirect = false, outputRedirect = false;
        std::vector<char*> execArgs;

        for (size_t i = 0; i < args.size() && args[i] != nullptr; ++i) {
            std::string arg(args[i]);
            if (arg == "<") {
                inputFile = args[++i];
                inputRedirect = true;
            } else if (arg == ">") {
                outputFile = args[++i];
                outputRedirect = true;
            } else {
                execArgs.push_back(args[i]);
            }
        }
        execArgs.push_back(nullptr);

        if (inputRedirect) {
            int fd = open(inputFile.c_str(), O_RDONLY);
            if (fd == -1) {
                perror("open input file");
                exit(EXIT_FAILURE);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }

        if (outputRedirect) {
            int fd = open(outputFile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd == -1) {
                perror("open output file");
                exit(EXIT_FAILURE);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }

        execvp(execArgs[0], execArgs.data());
        perror("execvp");
        exit(EXIT_FAILURE);
    }
    //调用chdir来改变工作目录
    void changeDirectory(const std::vector<std::string>& args) {
        if (args.size() < 2) {
            std::cerr << "cd: missing argument\n";
            return;
        }
        if (chdir(args[1].c_str()) != 0) {
            perror("cd");
        }
    }
};
// 主函数，创建Shell对象并运行
int main() {
    Shell shell;
    shell.run();
    return 0;
}
