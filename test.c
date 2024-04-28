#include <stdio.h>

int main() {
    const char test_buffer[] = "user-agent: wallflowser.exe\r\nsexy-marinara: none\r\n\r\n";
    char input_line[100];

    sscanf(test_buffer, "%10[^-]", input_line);

    printf("Read line: %s\n", input_line);

    return 0;
}