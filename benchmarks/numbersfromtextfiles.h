#ifndef BITMAPSFROMTEXTFILES_H_
#define BITMAPSFROMTEXTFILES_H_
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <dirent.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*********************************/
/********************************
 * General functions to load up bitmaps from text files.
 * Except format: comma-separated integers.
 *******************************/
/*********************************/

/**
 * Read the content of a file to a char array. Caller is
 * responsible for memory de-allocation.
 * Returns NULL on error.
 *
 * (If the individual files are small, this function is
 * a good idea.)
 */
static char *read_file(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        printf("Could not open file %s\n", filename);
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    size_t size = (size_t)ftell(fp);
    rewind(fp);
    char *answer = (char *)malloc(size + 1);
    if (!answer) {
        fclose(fp);
        return NULL;
    }
    if (fread(answer, size, 1, fp) != 1) {
        free(answer);
        return NULL;
    }
    answer[size] = '\0';
    fclose(fp);
    return answer;
}

/**
 * Given a file made of comma-separated integers,
 * read it all and generate an array of integers.
 * The caller is responsible for memory de-allocation.
 */
static uint32_t *read_integer_file(const char *filename, size_t *howmany) {
    char *buffer = read_file(filename);
    if (buffer == NULL) return NULL;

    size_t howmanyints = 1;
    size_t i1 = 0;
    for (; buffer[i1] != '\0'; i1++) {
        if (buffer[i1] == ',') ++howmanyints;
    }

    uint32_t *answer = (uint32_t *)malloc(howmanyints * sizeof(uint32_t));
    if (answer == NULL) return NULL;
    size_t pos = 0;
    for (size_t i = 0; (i < i1) && (buffer[i] != '\0'); i++) {
        uint32_t currentint;
        while ((buffer[i] < '0') || (buffer[i] > '9')) {
            i++;
            if (buffer[i] == '\0') goto END;
        }
        currentint = (uint32_t)(buffer[i] - '0');
        i++;
        for (; (buffer[i] >= '0') && (buffer[i] <= '9'); i++)
            currentint = currentint * 10 + (uint32_t)(buffer[i] - '0');
        answer[pos++] = currentint;
    }
END:
    if (pos != howmanyints) {
        printf("unexpected number of integers! %d %d \n", (int)pos,
               (int)howmanyints);
    }
    *howmany = pos;
    free(buffer);
    return answer;
}

/**
 * Does the file filename ends with the given extension.
 */
static bool hasExtension(const char *filename, const char *extension) {
    const char *ext = strrchr(filename, '.');
    return (ext && !strcmp(ext, extension));
}

/**
 * read all (count) integer files in a directory. Caller is responsible
 * for memory de-allocation. In case of error, a NULL is returned.
 */
static uint32_t **read_all_integer_files(const char *dirname,
                                         const char *extension,
                                         size_t **howmany, size_t *count) {
    struct dirent **entry_list;

    int c = scandir(dirname, &entry_list, 0, alphasort);
    if (c < 0) return NULL;
    size_t truec = 0;
    for (int i = 0; i < c; i++) {
        if (hasExtension(entry_list[i]->d_name, extension)) ++truec;
    }
    *count = truec;
    *howmany = (size_t *)malloc(sizeof(size_t) * (*count));
    uint32_t **answer = (uint32_t **)malloc(sizeof(uint32_t *) * (*count));
    size_t dirlen = strlen(dirname);
    char *modifdirname = (char *)dirname;
    if (modifdirname[dirlen - 1] != '/') {
        modifdirname = (char *)malloc(dirlen + 2);
        strcpy(modifdirname, dirname);
        modifdirname[dirlen] = '/';
        modifdirname[dirlen + 1] = '\0';
        dirlen++;
    }
    for (size_t i = 0, pos = 0; i < (size_t)c;
         i++) { /* formerly looped while i < *count */
        if (!hasExtension(entry_list[i]->d_name, extension)) continue;
        size_t filelen = strlen(entry_list[i]->d_name);
        char *fullpath = (char *)malloc(dirlen + filelen + 1);
        strcpy(fullpath, modifdirname);
        strcpy(fullpath + dirlen, entry_list[i]->d_name);
        answer[pos] = read_integer_file(fullpath, &((*howmany)[pos]));
        pos++;
        free(fullpath);
    }
    if (modifdirname != dirname) {
        free(modifdirname);
    }
    for (int i = 0; i < c; ++i) free(entry_list[i]);
    free(entry_list);
    return answer;
}

#endif /* BITMAPSFROMTEXTFILES_H_ */
