#include <stdio.h>

int main() {
    FILE * manifest = fopen("manifest.txt", "r");
    char string[100];
    while(EOF != fscanf_s(manifest, "%[^\r\n] ", string, sizeof(string))){
        printf("%s\n", string);
    }
    

    return 0;
}