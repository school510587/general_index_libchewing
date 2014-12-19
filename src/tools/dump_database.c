#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "chewing-private.h"
#include "global-private.h"
#include "private.h"

char *dict = NULL;
TreeType * root = NULL;
const char USAGE[] =
    "Usage: %s <dictionary.dat> <index_tree.dat>\n"
    "This program dumps the entire index structure to stdout.\n";

void dump(uint32_t node_pos, uint32_t indent)
{
    uint16_t key = 0;
    uint32_t i;

    for (i = 0; i < indent; i++)
        fputs("    ", stdout);

    key = root[node_pos].key;
    if (key != 0) {
        uint32_t beg = root[node_pos].child.begin;
        uint32_t end = root[node_pos].child.end;

        printf("key=%u begin=%u end=%u\n", key, beg, end);
        for (i = beg; i < end; i++)
            dump(i, indent + 1);
    }
    else {
        uint32_t pos = root[node_pos].phrase.pos;
        uint32_t freq = root[node_pos].phrase.freq;

        printf("phrase=%s freq=%u\n", &dict[pos], freq);
    }
}

char *read_input(const char *filename)
{
    char *buf = NULL;
    FILE *fp;
    size_t filesize;

    assert(filename);

    fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "Error opening the file %s\n", filename);
        exit(-1);
    }

    fseek(fp, 0, SEEK_END);
    filesize = ftell(fp);
    rewind(fp);

    buf = ALC(char, filesize);
    assert(buf);
    fread(buf, filesize, 1, fp);

    fclose(fp);

    return buf;
}

int main(int argc, char *argv[])
{

    if (argc != 3) {
        printf(USAGE, argv[0]);
        return -1;
    }

    dict = read_input(argv[1]);
    root = (TreeType*)read_input(argv[2]);

    dump(0, 0);

    free(dict);
    free(root);

    return 0;
}
