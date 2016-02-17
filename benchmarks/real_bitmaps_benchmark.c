#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>

#include "roaring.h"


/**
 * Read the content of a file to a char array. Caller is
 * responsible for memory de-allocation.
 * Returns NULL on error.
 *
 * (If the individual files are small, this function is
 * a good idea.)
 */
static char * read_file(const char * filename) {
    FILE * fp = fopen (filename , "r" );
    if( !fp ) {
        printf("Could not open file %s\n",filename);
        return NULL;
    }

    fseek( fp , 0 , SEEK_END);
    size_t size = (size_t) ftell( fp );
    rewind( fp );
    char * answer = malloc(size + 1);
    if(! answer) {
        fclose(fp);
        return NULL;
    }
    if( fread( answer , size, 1 , fp) != 1) {
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
static uint32_t * read_integer_file(char * filename, size_t * howmany) {
    char * buffer = read_file(filename);
    if(buffer == NULL) return NULL;

    size_t howmanyints = 1;
    for (int i=0; buffer[i] != '\0'; i++) {
        if(buffer[i] == ',') ++ howmanyints;
    }

    uint32_t * answer = malloc(howmanyints * sizeof(uint32_t));
    if(answer == NULL) return NULL;
    size_t pos = 0;
    for (int i=0; buffer[i] != '\0'; i++) {
        uint32_t currentint;
        while((buffer[i] < '0') || (buffer[i] > '9')) {
            i++;
            if(buffer[i] == '\0') goto END;
        }
        currentint =  (uint32_t) ( buffer[i] - '0');
        i++;
        for ( ; (buffer[i] >= '0') && (buffer[i] <= '9'); i++)
            currentint = currentint * 10 + (uint32_t) (buffer[i] - '0');
        answer[pos++] = currentint;
    }
END:
    if(pos != howmanyints) {
        printf("unexpected number of integers! %d %d \n",(int)pos,(int)howmanyints);
    }
    *howmany = pos;
    free(buffer);
    return answer;
}

static bool hasExtension(char * filename, char * extension) {
    char *ext = strrchr(filename, '.');
    return (ext && !strcmp(ext, extension));
}

/**
 * read all (count) integer files in a directory. Caller is responsible
 * for memory de-allocation. In case of error, a NULL is returned.
 */
static uint32_t ** read_all_integer_files(char * dirname, char * extension, size_t ** howmany, size_t * count) {
    struct dirent **entry_list;

    int c = scandir(dirname, &entry_list, 0, alphasort);
    if(c < 0) return NULL;
    size_t truec = 0;
    for (int i = 0; i < c; i++) {
        if (hasExtension(entry_list[i]->d_name, extension)) ++ truec;
    }
    *count = truec;
    *howmany = malloc(sizeof(size_t) * (*count));
    uint32_t ** answer = malloc(sizeof(uint32_t *) * (*count));
    size_t dirlen = strlen(dirname);
    char * modifdirname = dirname;
    if(modifdirname[dirlen - 1] != '/') {
        modifdirname = malloc(dirlen+2);
        strcpy(modifdirname,dirname);
        modifdirname[dirlen] ='/';
        modifdirname[dirlen+1] ='\0';
        dirlen ++;
    }
    for (size_t i = 0, pos = 0; i < (size_t) c; i++) { /* formerly looped while i < *count */
        if (!hasExtension(entry_list[i]->d_name, extension)) continue;
        size_t filelen = strlen(entry_list[i]->d_name);
        char * fullpath = malloc(dirlen + filelen + 1);
        strcpy(fullpath,modifdirname);
        strcpy(fullpath+dirlen,entry_list[i]->d_name);
        answer[pos] = read_integer_file(fullpath,&((*howmany)[pos]));
        pos ++;
        free(fullpath);
    }
    if(modifdirname != dirname) {
        free(modifdirname);
    }
    for (int i=0; i < c; ++i)
            free(entry_list[i]);
    free(entry_list);
    return answer;
}

static roaring_bitmap_t ** read_all_bitmaps(char * dirname, char * extension, size_t * count) {
    size_t * howmany = NULL;
    uint32_t ** numbers = read_all_integer_files(dirname, extension, &howmany, count);
    if(numbers == NULL) return NULL;

    printf("reading %d files, now constructing bitmaps.\n",(int)*count);
    roaring_bitmap_t ** answer = malloc(sizeof(roaring_bitmap_t *) * (*count));
    for (size_t i = 0; i < *count; i++) {
        printf("loading %d integers in bitmap %d \n",(int)howmany[i],(int)i+1);
        answer[i] = roaring_bitmap_of_ptr(howmany[i],numbers[i]);
        free(numbers[i]);
        numbers[i] = NULL;
    }
    free(howmany);
    free(numbers);
    return answer;
}

static void printusage(char * command) {
    printf(" Try %s directory \n where directory could be benchmarks/realdata/census1881\n",command);;
}

int main(int argc, char **argv) {
    int c;
    char * extension = ".txt";
    while ((c = getopt(argc, argv, "e:h")) != -1)
        switch (c) {
        case 'e':
            extension = optarg;
            break;
        case 'h':
            printusage(argv[0]);
            return 0;
        default:
            abort();
        }
    if (optind  >= argc) {
        printusage(argv[0]);
        return -1;
    }
    char * dirname = argv[optind];
    size_t count;
    roaring_bitmap_t ** bitmaps = read_all_bitmaps(dirname,extension, &count);
    if(bitmaps == NULL) return -1;
    printf("Loaded %d bitmaps from directory %s \n",(int)count,dirname);

    // try ANDing together consecutive pairs
    for (int i=0; i < (int) count-1; ++i) {
            roaring_bitmap_t *temp = roaring_bitmap_and( bitmaps[i], bitmaps[i+1]);
            printf("AND number %d has card %d\n",i,(int) roaring_bitmap_get_cardinality(temp));
            roaring_bitmap_free(temp);
    }

    // then mangle them with inplace
    for (int i=0; i < (int) count-1; i+=2) {
            roaring_bitmap_and_inplace( bitmaps[i], bitmaps[i+1]);
            printf("inplace AND number %d has card %d\n",i,(int) roaring_bitmap_get_cardinality(bitmaps[i]));
    }



    for (int i=0; i < (int) count; ++i)
            roaring_bitmap_free(bitmaps[i]);
    free(bitmaps);
    return 0;
}
