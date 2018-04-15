#include <cstdio>    // BUFSIZ
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <vector>
#include <sys/stat.h>
#include <fstream>
#include <ftw.h>
#include <libgen.h>

char *destination_directory;
bool rewrite_all = false;
bool cancel = false;

bool file_exists(const char *filename) {
    std::ifstream ifile(filename);
    return (bool) ifile;
}

bool is_readable(const char *filename) {
    struct stat sb;
    if (!file_exists(filename)) {
        write(2, "Err: file ", 10);
        write(2, filename, strlen(filename));
        write(2, " does not exist\n", 15);
        return false;
    }
    stat(filename, &sb);
    if (S_ISDIR(sb.st_mode)) {
        write(2, "Err: ", 5);
        write(2, filename, strlen(filename));
        write(2, " is not a readable object\n", 26);
        return false;
    }
    return true;
}

void print_help() {
    write(1, "mycp [-h|--help] [-f] <file1> <file2>\n"
            "\tCopy from file1 to file2\n", 64);
    write(1, "mycp [-h|--help] [-f] <file1> <file2> <file3>... <dir>\n"
            "\tCopy all files to directory dir\n", 88);
    write(1,
          "mycp -R [-h|--help] [-f] [-x|--one-file-system] [-P|--no-dereference] <dir_or_file_1> <dir_or_file_2> <dir_or_file_3>... <dir>\n"
                  "\tRecursive copy all files to directory dir\n\n"
                  "[-h|--help]            -- show help\n"
                  "[-f]                   -- do not ask anything\n"
                  "[-x|--one-file-system] -- stay in one file system\n"
                  "[-P|--no-dereference]  -- do not use symbol links\n\n",
          354);
}

int find_option(int argc, char *argv[], bool &help, bool &recursive, bool &symbolic_links, bool &single_fs,
                std::vector<char *> &files) {
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            help = true;
        } else if (!strcmp(argv[i], "-f")) {
            rewrite_all = true;
        } else if (!strcmp(argv[i], "-R")) {
            recursive = true;
        } else if (!strcmp(argv[i], "-P") || !strcmp(argv[i], "--no-dereference")) {
            symbolic_links = false;
        } else if (!strcmp(argv[i], "-x") || !strcmp(argv[i], "--one-file-system")) {
            single_fs = false;
        } else {
            files.push_back(argv[i]);
        }
    }
    return 0;
}

int get_rewrite_options(const char *from) {
    char input[10];
    std::cin >> input;
    if (!strcasecmp(input, "N") || !strcasecmp(input, "No")) {
        return 0;
    } else if (!strcasecmp(input, "A") || !strcasecmp(input, "All")) {
        return rewrite_all = true;
    } else if (!strcasecmp(input, "C") || !strcasecmp(input, "Cancel")) {
        return cancel = true;
    } else if (!strcasecmp(input, "Y") || !strcasecmp(input, "Yes")) {
        return 1;
    } else {
        write(1, "Select option from list: Y[es]/N[o]/A[ll]/C[ancel]\n", 51);
        return get_rewrite_options(from);
    }
}

int copy_to_file(const char *from, const char *to) {
    char buf[BUFSIZ];
    size_t size;
    if (!rewrite_all && is_readable(to)) {
        write(2, "Rewrite file ", 13);
        write(2, from, strlen(from));
        write(2, "? Y[es]/N[o]/A[ll]/C[ancel]\n", 28);
        if (get_rewrite_options(from) == 0) return 0;
    }
    if (cancel) return 0;
    int source = open(from, O_RDONLY, 0);
    int dest = open(to, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    while (size = read(source, buf, BUFSIZ)) {
        write(dest, buf, size);
    }
    close(dest);
    close(source);
    return 0;
}

int multiple_copy(std::vector<char *> files) {
    destination_directory = files[files.size() - 1];
    files.pop_back();
    for (char *file: files) {
        if (!is_readable(file)) continue;
        if (cancel) return 0;
        char temp[sizeof(file) + sizeof(destination_directory)] = "";
        strcat(temp, destination_directory);
        strcat(temp, basename(file));
        if (copy_to_file(file, temp) < 0)return -1;
    }
    return 0;
}

int display_info(const char *fpath, const struct stat *sb, int tflag, struct FTW *ftwbuf) {
    char x[BUFSIZ] = "";
    char temp[sizeof(fpath) + sizeof(destination_directory)] = "";
    strcpy(x, fpath);
    strcat(temp, destination_directory);
    strcat(temp, x);
    if (S_ISREG(sb->st_mode)) {
        if (copy_to_file(fpath, temp) < 0)return -1;
    } else if (S_ISDIR(sb->st_mode)) {
        mkdir(temp, 0777);
    }
    return 0;
}

int recursive_copy(std::vector<char *> directories, bool symbolic_links, bool single_fs) {
    destination_directory = directories[directories.size() - 1];
    directories.pop_back();
    int flags = 0;
    if (!symbolic_links) flags |= FTW_PHYS;
    if (!single_fs) flags |= FTW_MOUNT;
    for (char *directory: directories) {
        if (nftw(directory, display_info, 50, flags) == -1) {
            write(2, "Err while copying\n", 18);
            return -1;
        }
    }
    return 0;
}

int main(int argc, char *argv[]) {
    std::vector<char *> arguments;
    bool show_help = false, recursive = false, symbolic_links = true, single_fs = true;;
    int options = find_option(argc, argv, show_help, recursive, symbolic_links, single_fs, arguments);
    if (options == 1) return 1;
    if (show_help) {
        print_help();
    }
    if (recursive) {
        if (recursive_copy(arguments, symbolic_links, single_fs) < 0) return -1;
    } else if (arguments.size() == 2) {
        copy_to_file(arguments[0], arguments[1]);
    } else if (arguments.size() > 2) {
        if (multiple_copy(arguments) < 0) return -1;
    }
    return 0;
}