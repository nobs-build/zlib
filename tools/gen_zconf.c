#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  include <direct.h>
#  define mkdir_one(path) _mkdir(path)
#  define PATH_SEPARATOR '\\'
#else
#  include <sys/stat.h>
#  include <sys/types.h>
#  define mkdir_one(path) mkdir(path, 0777)
#  define PATH_SEPARATOR '/'
#endif

#define LINE_BUFFER_SIZE 4096

static int path_is_separator(char ch)
{
    return ch == '/' || ch == '\\';
}

static int make_parent_directories(const char *file_path)
{
    char *path;
    char *cursor;
    size_t length;

    length = strlen(file_path);
    path = (char *)malloc(length + 1);
    if (path == NULL) {
        fprintf(stderr, "gen_zconf: out of memory\n");
        return 0;
    }

    memcpy(path, file_path, length + 1);
    for (cursor = path + 1; *cursor != '\0'; ++cursor) {
        if (!path_is_separator(*cursor))
            continue;

        if (cursor == path + 2 && path[1] == ':')
            continue;

        *cursor = '\0';
        if (path[0] != '\0' && mkdir_one(path) != 0 && errno != EEXIST) {
            fprintf(stderr, "gen_zconf: cannot create directory '%s': %s\n",
                    path, strerror(errno));
            free(path);
            return 0;
        }
        *cursor = PATH_SEPARATOR;
    }

    free(path);
    return 1;
}

static int starts_with(const char *text, const char *prefix)
{
    return strncmp(text, prefix, strlen(prefix)) == 0;
}

static int write_configured_line(FILE *output, const char *line,
                                 int have_unistd, int have_stdarg)
{
    if (starts_with(line, "#if HAVE_UNISTD_H-0")) {
        if (have_unistd)
            return fputs("#if 1                 /* was set by gen_zconf */\n",
                         output) != EOF;
    } else if (starts_with(line, "#if HAVE_STDARG_H-0")) {
        if (have_stdarg)
            return fputs("#if 1                 /* was set by gen_zconf */\n",
                         output) != EOF;
    }

    return fputs(line, output) != EOF;
}

static int generate(const char *source_path, const char *output_path,
                    const char *platform)
{
    FILE *source;
    FILE *output;
    char line[LINE_BUFFER_SIZE];
    int have_unistd;
    int have_stdarg;
    int success;

    if (strcmp(platform, "windows") == 0) {
        have_unistd = 0;
        have_stdarg = 1;
    } else if (strcmp(platform, "unix") == 0) {
        have_unistd = 1;
        have_stdarg = 1;
    } else {
        fprintf(stderr, "gen_zconf: unsupported target platform '%s'\n",
                platform);
        return 0;
    }

    source = fopen(source_path, "rb");
    if (source == NULL) {
        fprintf(stderr, "gen_zconf: cannot open '%s': %s\n",
                source_path, strerror(errno));
        return 0;
    }

    if (!make_parent_directories(output_path)) {
        fclose(source);
        return 0;
    }

    output = fopen(output_path, "wb");
    if (output == NULL) {
        fprintf(stderr, "gen_zconf: cannot create '%s': %s\n",
                output_path, strerror(errno));
        fclose(source);
        return 0;
    }

    success = 1;
    while (fgets(line, sizeof(line), source) != NULL) {
        if (!write_configured_line(output, line, have_unistd, have_stdarg)) {
            success = 0;
            break;
        }
        if (strchr(line, '\n') == NULL && !feof(source)) {
            fprintf(stderr, "gen_zconf: input line exceeds %d bytes\n",
                    LINE_BUFFER_SIZE - 1);
            success = 0;
            break;
        }
    }

    if (ferror(source)) {
        fprintf(stderr, "gen_zconf: cannot read '%s': %s\n",
                source_path, strerror(errno));
        success = 0;
    }
    if (fclose(output) != 0) {
        fprintf(stderr, "gen_zconf: cannot finish '%s': %s\n",
                output_path, strerror(errno));
        success = 0;
    }
    fclose(source);

    if (!success)
        remove(output_path);
    return success;
}

int main(int argc, char **argv)
{
    if (argc != 4) {
        fprintf(stderr,
                "usage: gen_zconf <source-zconf.h> <output-zconf.h> "
                "<windows|unix>\n");
        return EXIT_FAILURE;
    }

    return generate(argv[1], argv[2], argv[3])
               ? EXIT_SUCCESS
               : EXIT_FAILURE;
}
