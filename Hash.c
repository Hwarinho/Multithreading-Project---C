//Code for linked list obtained from: https://www.linuxjournal.com/content/hash-tables%E2%80%94theory-and-practice
//THE MORE USEFUL HASHTABLE: https://medium.com/@bartobri/hash-crash-the-basics-of-hash-tables-bef82a8ea550
//XML TUTORIAL: http://xmlsoft.org/tutorial/apc.html
//more XML STUFF: https://www.linuxquestions.org/questions/programming-9/creating-an-xml-file-using-libxml-745532/

#include 
#include 
#include 
#include 

#define TABLESIZE 20

// Linked List
typedef struct node
{
  char *data;
  struct node *next;
} node;

// A Hash Function: the returned hash value will be the 
// ASCII value of the first character of the string
// modulo the size of the table.
unsigned int hash(const char *str, int tablesize)
{
    int value;

    // Get the first letter of the string
    value = toupper(str[0]) - 'A';

    return value % tablesize;
}

static int lookup(node *table[], const char *key)
{
    unsigned index = hash(key, TABLESIZE);
    const node *it = table[index];

    // Try to find if a matching key in the list exists
    while(it != NULL && strcmp(it->data, key) != 0)
    {
        it = it->next;
    }
    return it != NULL;
}

int insert(node *table[], char *key)
{
    if( !lookup(table, key) )
    {
        // Find the desired linked list
        unsigned index = hash(key, TABLESIZE);
        node *new_node = malloc(sizeof *new_node);

        if(new_node == NULL)
            return 0;

        new_node->data = malloc(strlen(key)+1);

        if(new_node->data == NULL)
            return 0;

        // Add the new key and link to the front of the list
        strcpy(new_node->data, key);
        new_node->next = table[index];
        table[index] = new_node;
        return 1;
    }
    return 0;
}

// Populate Hash Table
// First parameter: The hash table variable
// Second parameter: The name of the text file with the words
int populate_hash(node *table[], FILE *file)
{
    char word[50];
    char c;

    do {
        c = fscanf(file, "%s", word);
        // IMPORTANT: remove newline character
        size_t ln = strlen(word) - 1;
        if (word[ln] == '\n')
            word[ln] = '\0';

        insert(table, word);
    } while (c != EOF);

    return 1;
}

int main(int argc, char **argv)
{
    char word[50];
    char c;
    int found = 0;

    // Initialize the hash table
    node *table[TABLESIZE] = {0};

    FILE *INPUT;
    INPUT = fopen("INPUT", "r");
    // Populate hash table
    populate_hash(table, INPUT);
    fclose(INPUT);
    printf("The hash table is ready!\n");

    int line = 0;
    FILE *CHECK;
    CHECK = fopen("CHECK", "r");

    do {
        c = fscanf(CHECK, "%s", word);

        // IMPORTANT: remove newline character
        size_t ln = strlen(word) - 1;
        if (word[ln] == '\n')
            word[ln] = '\0';

        line++;
        if( lookup(table, word) )
        {
            found++;
        }
    } while (c != EOF);

    printf("Found %d words in the hash table!\n", found);

    fclose(CHECK);
    return 0;
}
