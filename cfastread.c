#include <ctype.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#define WPM 400
// should be 1/2 the size of the longest string
// don't want to read in the whole thing first.
// if the string is longer than 2x the offset, 
// too bad.
#define OFFSET 20
#define BUFSIZE 1024
#define COLOR "\x1b[0m"
// "\x1b[37;1m"
#define DEFAULT_CENTERCOLOR "\x1b[31;1m"
#define RESETCOLOR "\x1b[0m"

// Color options for -c flag
typedef struct {
    const char *name;
    const char *code;
} ColorOption;

static const ColorOption colors[] = {
    {"red",     "\x1b[31;1m"},   // default - bright red
    {"green",   "\x1b[32;1m"},   // bright green
    {"yellow",  "\x1b[33;1m"},   // bright yellow
    {"blue",    "\x1b[34;1m"},   // bright blue
    {"magenta", "\x1b[35;1m"},   // bright magenta
    {"cyan",    "\x1b[36;1m"},   // bright cyan
    {"white",   "\x1b[37;1m"},   // bright white
    {"orange",  "\x1b[38;5;208m"}, // 256-color orange (good visibility)
    {NULL, NULL}
};

// Global for selected center color
static const char *centercolor = DEFAULT_CENTERCOLOR;

// Look up color by name, returns NULL if not found
const char *get_color_code(const char *name) {
    for (int i = 0; colors[i].name != NULL; i++) {
        if (strcasecmp(colors[i].name, name) == 0) {
            return colors[i].code;
        }
    }
    return NULL;
}

// Print available colors
void print_colors() {
    fprintf(stderr, "Available colors: ");
    for (int i = 0; colors[i].name != NULL; i++) {
        fprintf(stderr, "%s%s", colors[i].name,
                colors[i+1].name ? ", " : "\n");
    }
}

// Print help message and exit
void print_help(const char *progname) {
    printf("Usage: %s [-c color] [file]\n\n", progname);
    printf("A speed reading tool that displays words one at a time with the\n");
    printf("center character highlighted to help focus your eyes.\n\n");
    printf("Options:\n");
    printf("  -c, --color <name>  Set the highlight color for the center character\n");
    printf("  -h, --help          Show this help message and exit\n\n");
    printf("Available colors:\n");
    printf("  red      Bright red (default)\n");
    printf("  green    Bright green\n");
    printf("  yellow   Bright yellow\n");
    printf("  blue     Bright blue\n");
    printf("  magenta  Bright magenta\n");
    printf("  cyan     Bright cyan\n");
    printf("  white    Bright white\n");
    printf("  orange   Orange (256-color mode)\n\n");
    printf("If no file is specified, reads from stdin.\n");
    printf("Color names are case-insensitive.\n");
    exit(0);
}

// number of spaces to prepend to string on output
int spaces(int stringsize, int offset) {
// offset marks where the middle character should go.
// so # of spaces is offset - stringsize/2
// or 0 of negative
    int result = offset - (stringsize/2); 
    return (result > 0) ? result : 0;
}

int buffer_spans(char *buffer, size_t size, char *delims) {
    int i = 0;
    int len_delims = strlen(delims);
    // for each delimiter (marking separation character)
    // check if the last character in the buffer matches.
    // If it does, we don't need to save the last token
    for (i = 0; i < len_delims; i++) {
        if (buffer[size-1] == delims[i]) {
            return 0;
        }
    }
    return 1; 
}

void printstring(char *string) {
    int n_spaces;
    int middle = (strlen(string)/2);

    n_spaces = spaces(strlen(string), OFFSET);
    printf("%*s", n_spaces,"");
    printf(COLOR "%.*s", middle, string);
    printf("%s%.*s", centercolor, 1, string+middle);
    printf(COLOR "%s" RESETCOLOR, string+middle+1);
}
    

int main(int argc, char *argv[]) {
    struct stat st_buf;
    int status, save_last, has_punct;
    int opt;
    char *filename = NULL;

    FILE *f;
    char *delims = ";\n \"";
    char buffer[BUFSIZE],*p;
    // for strings that span the buffer boundary
    char *savedstring = NULL;
    char *frankenstring = NULL;
    char *last;

    // long options
    static struct option long_options[] = {
        {"color", required_argument, 0, 'c'},
        {"help",  no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    // parse options
    while ((opt = getopt_long(argc, argv, "c:h", long_options, NULL)) != -1) {
        switch (opt) {
            case 'c': {
                const char *code = get_color_code(optarg);
                if (code == NULL) {
                    fprintf(stderr, "Unknown color: %s\n", optarg);
                    print_colors();
                    exit(1);
                }
                centercolor = code;
                break;
            }
            case 'h':
                print_help(argv[0]);
                break;
            default:
                fprintf(stderr, "Usage: %s [-c color] [file]\n", argv[0]);
                print_colors();
                exit(1);
        }
    }

    // remaining argument is the file
    if (optind < argc) {
        filename = argv[optind];
    }

    // check file
    if (filename != NULL) {
        status = stat(filename, &st_buf);
        if (status != 0 || (!S_ISREG(st_buf.st_mode))) {
            fprintf(stderr, "Error or file not found: %s\n", filename);
            return 1;
        }
        f = fopen(filename, "r");
    } else {
        f = stdin;
    }

        // open the file
    if (f == NULL) {
        fprintf(stderr, "Error opening file: %s\n", filename);
        return 1;
    }

    while(fgets(buffer, BUFSIZE, f) != NULL) {
        save_last = buffer_spans(buffer, strlen(buffer), delims);

        p=strtok(buffer, delims);
        // fgets keeps newlines, but the tokenizer splits them
        // so p can be NULL if fgets only has a newline.
        if (p == NULL) {
            usleep(4*(60.0/WPM) * 1000 * 1000);
            continue;
        }
        if (savedstring && (savedstring != NULL)) { 
            if (frankenstring && (frankenstring != NULL)) {
                free(frankenstring);
                frankenstring = NULL;
            }

            frankenstring = malloc(sizeof(char) * (strlen(p)+strlen(savedstring)) + 1);
            frankenstring[0] = '\0';
            strncat(frankenstring, savedstring, strlen(savedstring));
            strncat(frankenstring, p, strlen(p));
            p = frankenstring;
        }

        do {
            if (save_last == 1) {
                last = p;
            }
            printstring(p);

            // I actually don't know why these don't work if both are
            // puts or printf or they are swapped... shrug.
            printf("\x1b[0K");
            puts("\x1b[1A");

            // delay twice as long for punctuation
            has_punct = (ispunct(p[strlen(p)-1]) == 0) ? 1 : 2;
            usleep(has_punct*(60.0/WPM) * 1000 * 1000);

        } while ((p = strtok(NULL, delims)));

        // clean up
        if (save_last == 1) {
            if (savedstring != NULL) {
                free(savedstring);
            }
            savedstring = strdup(last);
        }
    
    }
    fclose(f);
    printf("\n");
}
