#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>

typedef struct Node {
    char *url1;
    char *content1;
    struct Node *next;
} Node;

char *current_line;
char *buffer;
char *url;
int buffer_size;
regex_t reegex; // Variable to create regex
int content_length;
int save_payload;
Node *save_node;

// validation booleans:
int first_buffer;   // true: this is the first call of evaluate_buffer
int not_empty;      // chars between the last line end and current buffer index are not empty
int not_empty_line; // there was a not empty line since the last empty line/HTTP request
int last_line_empty;
int first_line;
int new_request;
Node *head;

// booleans for the header
int is_get_request;
int is_put_request;
int is_delete_request;
int header_is_valid;

// booleans for url validation
int is_content;
int is_foo;
int is_bar;
int is_baz;
int is_dynamic;

// reset all booleans
void reset_bools() {
    is_get_request = 0;
    is_put_request = 0;
    is_delete_request = 0;
    is_content = 0;
    is_foo = 0;
    is_bar = 0;
    is_baz = 0;
    is_dynamic = 0;

    header_is_valid = 0;
    not_empty = 0;
    first_line = 1;
    new_request = 1;
}

// initializes the linked list
void init_list() {
    head = malloc(sizeof(Node));
    head->url1 = NULL;
    head->content1 = NULL;
    head->next = NULL;
}

// adds a new node to the linked list
Node *add_node(char *current_url, char *current_content) {
    Node *new_node = malloc(sizeof(Node));
    new_node->url1 = current_url;
    new_node->content1 = current_content;
    new_node->next = head;
    head = new_node;
    return new_node;
}

// find url in linked list
Node *find_node(char *current_url) {
    Node *current = head;
    while (current != NULL) {
        if (current->url1 != NULL && strcmp(current->url1, current_url) == 0) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}


void free1(Node *node) {
    if (node == NULL) {
        return;
    }
    free(node->url1);
    free(node->content1);
    free(node);
}

// remove url from linked list
void remove_node(char *current_url) {
    Node *current = head;
    Node *previous = NULL;
    while (current != NULL) {
        if (current->url1 != NULL && strcmp(current->url1, current_url) == 0) {
            if (previous == NULL) {
                head = current->next;
            } else {
                previous->next = current->next;
            }
            free1(current);
            return;
        }
        previous = current;
        current = current->next;
    }
}


void check_content_length(char *line) {
    char *nr = calloc(1, sizeof(char));
    char content_pattern[] = "^((Content)|(content))-((Length)|(length)): [0-9]*\r\n";
    if (regcomp(&reegex, content_pattern, REG_EXTENDED) != 0) {
        perror("Regex could not be created.");
        exit(1);
    }
    int val = regexec(&reegex, line, 0, NULL, 0);

    if (val == 0) {
        is_content = 1;
        int i = 16;
        while (line[i] != '\r') {
            nr = realloc(nr, strlen(nr) + 2);
            strncat(nr, &line[i], 1);
            i++;
        }
        content_length = atoi(nr);
    }
    free(nr);
}

// saves the url to url
void get_url(char *line) {
    int i = 0;
    // iterate to url
    while (line[i] != ' ') {
        i++;
    }
    i++;

    url = calloc(1, sizeof(char));
    while (line[i] != ' ') {
        url = realloc(url, (strlen(url) + 1 + 1));
        strncat(url, &line[i], 1);
        i++;
    }
}

// checks if the url is valid
void check_req() {
    char *foo = "/static/foo";
    char *bar = "/static/bar";
    char *baz = "/static/baz";
    char *dynamic = "/dynamic/";

    if (strcmp(url, foo) == 0)
        is_foo = 1;
    else if (strcmp(url, bar) == 0)
        is_bar = 1;
    else if (strcmp(url, baz) == 0)
        is_baz = 1;
    else if (strncmp(url, dynamic, 9) == 0)
        is_dynamic = 1;
}

void respond_get(int socket) {
    if (is_foo) {
        char *response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 3\r\n\r\nFoo";
        if (send(socket, response, strlen(response), 0) < 0) {
            perror("send");
            exit(1);
        }
    } else if (is_bar) {
        char *response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 3\r\n\r\nBar";
        if (send(socket, response, strlen(response), 0) < 0) {
            perror("send");
            exit(1);
        }
    } else if (is_baz) {
        char *response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 3\r\n\r\nBaz";
        if (send(socket, response, strlen(response), 0) < 0) {
            perror("send");
            exit(1);
        }
    } else if (is_dynamic) {
        Node *tmp = find_node(url);
        if (tmp == NULL) {
            char *response = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
            if (send(socket, response, strlen(response), 0) < 0) {
                perror("send");
                exit(1);
            }
        } else {
            char *response_header = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: ";
            char *response = calloc(strlen(response_header) + 1, sizeof(char));
            strcpy(response, response_header);
            char content_length_str[128] = "";
            sprintf(content_length_str, "%lu", strlen(tmp->content1));
            response = realloc(response, strlen(response) + 1 + strlen(content_length_str));
            strcat(response, content_length_str);
            char *token = "\r\n\r\n";
            response = realloc(response, strlen(response) + 1 + strlen(token));
            strcat(response, token);
            response = realloc(response, strlen(response) + 1 + strlen(tmp->content1));
            strcat(response, tmp->content1);
            if (send(socket, response, strlen(response), 0) < 0) {
                perror("send");
                exit(1);
            }
        }
    } else {
        char *response = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
        if (send(socket, response, strlen(response), 0) < 0) {
            perror("send");
            exit(1);
        }
    }
}

void respond_put(int socket) {
    if (!is_dynamic) {
        char *response = "HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\n\r\n";
        if (send(socket, response, strlen(response), 0) < 0) {
            perror("send");
            exit(1);
        }
    } else {
        Node *found = find_node(url);
        save_payload = 1;
        if (found == NULL) {
            save_node = add_node(url, calloc(1, sizeof(char)));
            char *response = "HTTP/1.1 201 Created\r\nContent-Length: 0\r\n\r\n";
            if (send(socket, response, strlen(response), 0) < 0) {
                perror("send");
                exit(1);
            }
        } else {
            save_node = found;
            free(found->content1);
            save_node->content1 = calloc(1, sizeof(char));
            char *response = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
            if (send(socket, response, strlen(response), 0) < 0) {
                perror("send");
                exit(1);
            }
        }
    }
}

void respond_del(int socket) {
    if (!is_dynamic) {
        char *response = "HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\n\r\n";
        if (send(socket, response, strlen(response), 0) < 0) {
            perror("send");
            exit(1);
        }
    } else {
        Node *found = find_node(url);
        if (found == NULL) {
            char *response = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
            if (send(socket, response, strlen(response), 0) < 0) {
                perror("send");
                exit(1);
            }
        } else {
            remove_node(url);
            char *response = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
            if (send(socket, response, strlen(response), 0) < 0) {
                perror("send");
                exit(1);
            }
        }
    }
}

void send_response(int socket) {
    if (header_is_valid) {
        if (is_put_request)
            respond_put(socket);
        else if (is_get_request)
            respond_get(socket);
        else if (is_delete_request)
            respond_del(socket);
        else {
            char *response = "HTTP/1.1 501 IDK\r\nContent-Length: 0\r\n\r\n";
            if (send(socket, response, strlen(response), 0) < 0) {
                perror("send");
                exit(1);
            }
        }
    } else {
        char *response = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
        if (send(socket, response, strlen(response), 0) < 0) {
            perror("send");
            exit(1);
        }
    }
}

int http_header(char *line) {
    char header_pattern[] = "^((GET)|(POST)|(DELETE)|(HEAD)|(PUT)|(CONNECT)|(OPTIONS)|(TRACE)|(PATCH)) [^[:space:]]* HTTP/[0-9][.]{0,1}[0-9]{0,1}\r\n";
    if (regcomp(&reegex, header_pattern, REG_EXTENDED) != 0) {
        perror("Regex could not be created.");
        exit(1);
    }
    int val = regexec(&reegex, line, 0, NULL, 0);

    if (val == 0) { // pattern was matched, header is correct
        get_url(line);
        check_req();
        // set is_get_request
        char get_pattern[] = "^(GET) ";
        if (regcomp(&reegex, get_pattern, REG_EXTENDED) != 0) {
            perror("Regex could not be created");
            exit(1);
        }
        val = regexec(&reegex, line, 0, NULL, 0);
        if (val == 0) {
            is_get_request = 1;
        } else if (val == REG_NOMATCH) {
            is_get_request = 0;
        } else {
            perror("Regex matching did not work.");
            exit(1);
        }

        // set is_put_request
        char put_pattern[] = "^(PUT) ";
        if (regcomp(&reegex, put_pattern, REG_EXTENDED) != 0) {
            perror("Regex could not be created");
            exit(1);
        }
        val = regexec(&reegex, line, 0, NULL, 0);
        if (val == 0) {
            is_put_request = 1;
        } else if (val == REG_NOMATCH) {
            is_put_request = 0;
        } else {
            perror("Regex matching did not work.");
            exit(1);
        }

        // set is_delete_request
        char del_pattern[] = "^(DELETE) ";
        if (regcomp(&reegex, del_pattern, REG_EXTENDED) != 0) {
            perror("Regex could not be created");
            exit(1);
        }
        val = regexec(&reegex, line, 0, NULL, 0);
        if (val == 0) {
            is_delete_request = 1;
        } else if (val == REG_NOMATCH) {
            is_delete_request = 0;
        } else {
            perror("Regex matching did not work.");
            exit(1);
        }

        return 1;
    } else if (val == REG_NOMATCH) { // pattern was not matched, head is incorrect
        is_get_request = 0;
        return 0;
    } else { // somethin failed
        perror("Regex matching did not work.");
        exit(1);
    }
}

// removes line from current_line. Everything until new_line_start is removed
void remove_line(int new_line_start) {
    if (current_line == NULL)
        return;

    int new_line_len = strlen(current_line) - new_line_start;

    if (new_line_len <= 0) { // new line was at the end of current line
        free(current_line);
        current_line = calloc(1, sizeof(char));
        current_line[0] = '\0';
    } else {
        char *tmp_line = calloc(new_line_len + 1, sizeof(char));
        strcpy(tmp_line, &current_line[new_line_start]);
        free(current_line);
        current_line = tmp_line;
    }
}

/*"*/

void save_payload_n(int save_len) {
    if (save_node == NULL)
        exit(1);
    save_node->content1 = realloc(save_node->content1, strlen(save_node->content1) + 1 + save_len);
    strncat(save_node->content1, current_line, save_len);
}


void process_payload() {
    // remove payload from current line
    if (content_length > 0) { // there is payload to be removed
        int new_line_len = strlen(current_line) - content_length;

        if (new_line_len <= 0) { // payload is bigger/equal than current_line
            if (save_payload)
                save_payload_n(strlen(current_line));
            content_length -= strlen(current_line);
            free(current_line);
            current_line = calloc(1, sizeof(char));
        } else {
            if (save_payload)
                save_payload_n(content_length);
            char *tmp_line = calloc(new_line_len + 1, sizeof(char));
            strcpy(tmp_line, &current_line[content_length]);
            free(current_line);
            current_line = tmp_line;
            content_length = 0;
        }

        if (!content_length) {
            save_payload = 0;
            save_node = NULL;
        }
    }
}

void evaluate_request(int socket) {
    current_line = realloc(current_line, strlen(current_line) + 1 + strlen(buffer));
    strcat(current_line, buffer);

    for (int i = 0; i < buffer_size; i++)
        buffer[i] = '\0';

    if (strlen(current_line) <= 0)
        return;

    // remove payload from current line if request is finished
    if (new_request)
        process_payload();

    int new_line_start = 0;
    int currently_empty = 1;
    int str_len = strlen(current_line);
    int i = 0;
    while (i < str_len) {
        if (strstr(&current_line[i], "\r\n") == &current_line[i]) { // CRLF token found, full line in current_line
            if (first_line) { // check header until we passed the first line
                header_is_valid = http_header(current_line);
                first_line = 0;
            }

            // set new_line_start
            new_line_start = i + strlen("\r\n");

            if (currently_empty) { // found an empty line
                // request is complete
                send_response(socket);
                remove_line(new_line_start);
                process_payload();

                reset_bools();

                i = -1;
                str_len = strlen(current_line);
            } else {
                // found a non-empty line
                check_content_length(current_line);
                remove_line(new_line_start);
                i = -1;
                str_len = strlen(current_line);
            }
            currently_empty = 1;
        } else {
            currently_empty = 0;
        }

        i++;
    }
}

int main(int argc, char **argv) {
    // Start here :)
    // get port
    if (argc < 2) {
        perror("Type in the desired port number!\n");
        exit(1);
    }
    unsigned short port = (unsigned short) atoi(argv[1]);

    int s, c;
    struct sockaddr_in s_addr;
    buffer_size = 1024;
    buffer = calloc(buffer_size + 1, sizeof(char));
    url = calloc(1, sizeof(char));
    current_line = calloc(1, sizeof(char));
    not_empty = 0;
    not_empty_line = 0;
    first_buffer = 1;
    header_is_valid = 0;

    content_length = 0;
    save_payload = 0;
    save_node = NULL;

    reset_bools();
    init_list();

    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        perror("Generating of socket failed.\n");
        exit(1);
    }

    s_addr.sin_family = AF_INET;
    s_addr.sin_port = htons(port);
    s_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(s, (struct sockaddr *) &s_addr, sizeof(s_addr)) < 0) {
        perror("Binding failed.\n");
        exit(1);
    }
    if (listen(s, 10) != 0) {
        perror("Listening failed.\n");
        exit(1);
    }

    while (1) {
        if ((c = accept(s, (struct sockaddr *) NULL, NULL)) == -1) {
            perror("Could not accept connection.\n");
            exit(1);
        }

        while (1) {
            if (recv(c, buffer, buffer_size, 0) == -1) {
                perror("Receiving packet failed.\n");
                exit(1);
            }
            if (strlen(buffer) == 0)
                break;
            evaluate_request(c);
        }
    }

    free(buffer);

    return EXIT_SUCCESS;
}