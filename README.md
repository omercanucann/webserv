*This project has been created as part of the 42 curriculum by oonal, agurses, oucan.*

# Webserv

## Description

Webserv is a C++98 HTTP server developed for the 42 curriculum. The goal of the project is to understand how HTTP servers work by implementing the core server logic ourselves: socket creation, non-blocking I/O, request parsing, response generation, static file serving, file upload, DELETE handling, configuration-based routing, error pages, and CGI execution.

The server is designed to work with real web browsers and command-line HTTP clients such as `curl`. It uses a configuration file inspired by the `server` block style of NGINX. Each configured server can listen on a different interface and port, serve different static content, define route-specific rules, and enable features such as upload, autoindex, redirection, and CGI.

## Main Features

- HTTP server written in C++98
- Non-blocking socket handling with `poll()`
- Multiple listening ports from the configuration file
- HTTP request parsing for request line, headers, query string, body, `Content-Length`, and chunked transfer decoding
- HTTP response generation with accurate status codes
- Static file serving with MIME type detection
- Default index file handling for directories
- Custom and default error pages
- Route-based accepted methods
- HTTP redirection through configuration
- Directory listing through autoindex when enabled
- File upload support through configured upload routes
- DELETE support for removable resources
- CGI execution based on configured file extension and interpreter
- Basic test flow with browser, `curl`, and custom test scripts

## Project Structure

The exact structure may change during development, but the project is organized around these main modules:

```text
.
├── Config/              # Configuration parser and config data structures
├── Http/                # HTTP parser, request/response classes, static handler, CGI handler
├── Network_Server/      # Socket binding, poll reactor, connection slots, bridge layer
├── Router/              # Route matching between requests and config locations
├── configs/             # Example/default configuration files
├── www/                 # Default static website files
├── tests/               # Optional test scripts
├── Makefile
└── README.md
```

## Requirements

- A Unix-like environment
- `c++` compiler
- C++98 support
- `make`
- `curl` for manual tests
- A browser for visual testing
- A CGI interpreter such as Python or PHP if CGI routes are enabled in the configuration

The project must be compiled with:

```bash
-Wall -Wextra -Werror -std=c++98
```

No external libraries or Boost libraries are used.

## Compilation

From the repository root, run:

```bash
make
```

To fully rebuild:

```bash
make re
```

To remove object files:

```bash
make clean
```

To remove object files and the executable:

```bash
make fclean
```

The executable name is:

```bash
webserv
```

## Execution

Run the server with a configuration file:

```bash
./webserv configs/default.conf
```

If no configuration file is provided, the server may use the default configuration path defined in the program:

```bash
./webserv
```

Example expected output:

```text
[main] Sunucu hazir. Port(lar) dinleniyor...
[main] Durdurmak icin Ctrl+C
```

Stop the server with:

```bash
Ctrl + C
```

## Configuration File Overview

The configuration file is inspired by NGINX-style server blocks. A typical configuration may look like this:

```nginx
server {
    listen 127.0.0.1:8080;
    root www/site1;
    index index.html;
    client_max_body_size 1000000;
    error_page 404 www/errors/404.html;
    error_page 403 www/errors/403.html;

    location / {
        allow_methods GET;
        root www/site1;
        index index.html;
        autoindex off;
    }

    location /upload {
        allow_methods POST;
        upload_enable on;
        upload_path www/uploads;
    }

    location /uploads {
        allow_methods GET DELETE;
        root www;
        autoindex on;
    }

    location /redirect {
        return 301 /;
    }

    location /cgi-bin {
        allow_methods GET POST;
        root www/cgi-bin;
        cgi_extension .py;
        cgi_interpreter /usr/bin/python3;
    }
}
```

Supported configuration concepts include:

- `listen`: interface and port pair
- `root`: root directory for requested files
- `index`: default file for directory requests
- `client_max_body_size`: maximum accepted request body size
- `error_page`: custom error page for a status code
- `location`: route-specific configuration block
- `allow_methods`: accepted HTTP methods for a route
- `return`: HTTP redirection
- `autoindex`: directory listing on or off
- `upload_enable`: enable upload for a route
- `upload_path`: target directory for uploaded files
- `cgi_extension`: CGI file extension
- `cgi_interpreter`: interpreter used to execute CGI scripts

## Manual Testing

Start the server first:

```bash
make re
./webserv configs/default.conf
```

Then use another terminal for the following tests.

### Static GET

```bash
curl -i http://localhost:8080/
```

Expected result:

```text
HTTP/1.1 200 OK
```

### GET a specific file

```bash
curl -i http://localhost:8080/index.html
```

### 404 Not Found

```bash
curl -i http://localhost:8080/nope.html
```

Expected result:

```text
HTTP/1.1 404 Not Found
```

### Method Not Allowed

```bash
curl -i -X PUT http://localhost:8080/
```

Expected result:

```text
HTTP/1.1 405 Method Not Allowed
```

### Upload with POST

```bash
curl -i -X POST http://localhost:8080/upload --data-binary "hello uploaded file"
```

Expected result:

```text
HTTP/1.1 201 Created
```

### Read uploaded file

Use the filename returned by the upload response:

```bash
curl -i http://localhost:8080/uploads/<uploaded_file_name>
```

### DELETE uploaded file

```bash
curl -i -X DELETE http://localhost:8080/uploads/<uploaded_file_name>
```

Expected result:

```text
HTTP/1.1 204 No Content
```

### Path traversal protection

```bash
curl --path-as-is -i http://localhost:8080/../../Makefile
```

Expected result:

```text
HTTP/1.1 403 Forbidden
```

### Large body test

```bash
dd if=/dev/zero of=big.bin bs=1024 count=2048
curl -i -X POST http://localhost:8080/upload --data-binary @big.bin
rm -f big.bin
```

Expected result if the body exceeds `client_max_body_size`:

```text
HTTP/1.1 413 Payload Too Large
```

### Multiple port test

If the configuration defines multiple servers on different ports, test each one:

```bash
curl -i http://localhost:8080/
curl -i http://localhost:8081/
```

The two ports should be able to serve different content depending on the configuration.

## CGI Testing

A CGI route should be configured with a file extension and interpreter. Example:

```nginx
location /cgi-bin {
    allow_methods GET POST;
    root www/cgi-bin;
    cgi_extension .py;
    cgi_interpreter /usr/bin/python3;
}
```

Example GET test:

```bash
curl -i http://localhost:8080/cgi-bin/test.py
```

Example POST test:

```bash
curl -i -X POST http://localhost:8080/cgi-bin/test.py --data-binary "name=webserv"
```

The CGI program should receive the request data through its environment and standard input according to the server-CGI communication rules.

## Browser Testing

Open the configured server in a browser:

```text
http://localhost:8080/
```

The server should be able to serve a static website containing HTML, CSS, JavaScript, and images.

## Stress Testing

The server should remain available under repeated or simultaneous requests. Example simple test:

```bash
for i in $(seq 1 100); do
    curl -s http://localhost:8080/ > /dev/null &
done
wait
```

You can also compare behaviour with NGINX when checking response headers and status codes.

## Implementation Notes

### Network Layer

The network layer opens listening sockets, sets file descriptors to non-blocking mode, accepts clients, and uses `poll()` to monitor socket readiness. The reactor is responsible for read and write readiness and for closing client connections safely.

### HTTP Layer

The HTTP parser converts raw request bytes into an `HttpRequest` object. The response layer builds valid HTTP responses with status line, headers, content length, content type, and body.

### Routing and Static Handling

The router matches the request path with the best location from the active server configuration. The static handler uses the matched route to serve files, check allowed methods, perform redirects, handle uploads, generate autoindex pages, and delete files when allowed.

### CGI Handling

The CGI handler executes configured scripts using the configured interpreter. It sends the request body to the CGI process, reads the CGI output, parses CGI headers, and converts the result into an HTTP response.

## Resources

- RFC 2616: Hypertext Transfer Protocol -- HTTP/1.1
- RFC 7230: HTTP/1.1 Message Syntax and Routing
- RFC 7231: HTTP/1.1 Semantics and Content
- MDN Web Docs: HTTP Overview
- MDN Web Docs: HTTP Response Status Codes
- MDN Web Docs: MIME Types
- NGINX documentation: server and location blocks
- The Open Group manual pages for `socket`, `bind`, `listen`, `accept`, `poll`, `read`, `write`, `fcntl`, `fork`, `execve`, `pipe`, `waitpid`, `stat`, `opendir`, `readdir`, and `closedir`

## Use of AI

AI tools were used as a learning and productivity aid during development. They were used to:

- Explain HTTP server concepts in simpler terms
- Plan the project division between team members
- Design test cases for request parsing, response generation, upload, DELETE, and error pages
- Review possible edge cases such as path traversal, body size limits, and incomplete requests
- Help draft documentation and README structure

All AI-generated suggestions were reviewed, tested, and adapted by the team. The final implementation decisions and submitted code are the responsibility of the project members.

## Team Responsibilities

A possible division of the work:

- Network layer: sockets, non-blocking mode, `poll()`, client lifecycle, read/write buffers
- HTTP layer: parser, request model, response model, status codes, static files, upload, DELETE, error responses
- Configuration and CGI layer: config parser, server/location rules, CGI environment, process execution, CGI output handling

## Known Notes Before Evaluation

Before evaluation, verify that:

- The project compiles with `-Wall -Wextra -Werror -std=c++98`
- The executable is named `webserv`
- A valid default configuration file exists
- Static files and error pages exist for the demo
- Upload and DELETE routes are configured correctly
- CGI scripts are executable and their interpreter path is correct
- Multiple ports, if configured, serve different content
- No forbidden external libraries are used
- README first line contains the real 42 logins of all teammates

