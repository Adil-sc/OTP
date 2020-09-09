#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

//Function: generateKey
//--------------------------
//Description: generates a key of random chars depending on the length specified by the user
//Returns: A string of random chars of length keyLength
char *generateKey(int keyLength, char alphabetSet[]){

//    char key[27] = {0};
    char *key = calloc(keyLength, sizeof(char));

    //Generate a random index position and associate it with a char in the alphaSet
    for (int i = 0; i < keyLength; i++){

        int randIndex = (rand() % (26 - 0 + 1)) + 0;
        key[i] = alphabetSet[randIndex];
    }

    //All output must end with a new line for the grading script
    key[keyLength] = '\n';

    return key;

}


int main(int argc, char *argv[]) {

    char alphabetSet[27] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ ";
    int keyLength = 0;

    //If the user fails to provide the length of the key they want, throw an error
    if(!argv[1]){
        fprintf(stderr,"Error: Keylength not provided\n");
        exit(1);
    }

    srand(time(0));
    sscanf(argv[1], "%d", &keyLength);

    //Generate the key and sent it to stdout
    char *key = generateKey(keyLength, alphabetSet);
    fprintf(stdout, "%s", key);

    return 0;
}

