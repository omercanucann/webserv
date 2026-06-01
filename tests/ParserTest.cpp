#include <iostream>
#include <string>
#include "HttpParser.hpp"

static void printSeparator()
{
    std::cout << "----------------------------------------" << std::endl;
}

static void expectOk(const std::string& testName, const std::string& rawRequest)
{
    HttpParser parser(1024 * 1024);

    std::cout << "[TEST] " << testName << std::endl;

    try
    {
        HttpRequest request = parser.parse(rawRequest);

        std::cout << "RESULT: OK" << std::endl;
        std::cout << "Method: " << request.getMethod() << std::endl;
        std::cout << "Path: " << request.getPath() << std::endl;
        std::cout << "Query: " << request.getQuery() << std::endl;
        std::cout << "Version: " << request.getVersion() << std::endl;
        std::cout << "Host: " << request.getHeader("Host") << std::endl;
        std::cout << "Body: " << request.getBody() << std::endl;
    }
    catch (const HttpParser::ParseException& e)
    {
        std::cout << "RESULT: FAIL" << std::endl;
        std::cout << "Unexpected error: " << e.getStatusCode()
                  << " " << e.what() << std::endl;
    }

    printSeparator();
}

static void expectError(const std::string& testName,
                        const std::string& rawRequest,
                        int expectedStatus)
{
    HttpParser parser(1024 * 1024);

    std::cout << "[TEST] " << testName << std::endl;

    try
    {
        parser.parse(rawRequest);

        std::cout << "RESULT: FAIL" << std::endl;
        std::cout << "Expected error status: " << expectedStatus << std::endl;
        std::cout << "But parser returned OK" << std::endl;
    }
    catch (const HttpParser::ParseException& e)
    {
        if (e.getStatusCode() == expectedStatus)
        {
            std::cout << "RESULT: OK" << std::endl;
            std::cout << "Status: " << e.getStatusCode() << std::endl;
            std::cout << "Message: " << e.what() << std::endl;
        }
        else
        {
            std::cout << "RESULT: FAIL" << std::endl;
            std::cout << "Expected: " << expectedStatus << std::endl;
            std::cout << "Got: " << e.getStatusCode() << std::endl;
            std::cout << "Message: " << e.what() << std::endl;
        }
    }

    printSeparator();
}

int main()
{
    expectOk(
        "Valid GET request",
        "GET /index.html HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "\r\n"
    );

    expectOk(
        "Valid GET request with query",
        "GET /search?q=cat&page=2 HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "\r\n"
    );

    expectOk(
        "Valid POST request with Content-Length",
        "POST /login HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "Content-Length: 26\r\n"
        "\r\n"
        "username=omer&password=123"
    );

    expectOk(
        "Valid DELETE request",
        "DELETE /uploads/test.txt HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "\r\n"
    );

    expectOk(
        "Valid chunked POST request",
        "POST /chunk HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "5\r\n"
        "Hello\r\n"
        "6\r\n"
        " World\r\n"
        "0\r\n"
        "\r\n"
    );

    expectError(
        "Missing Host header in HTTP/1.1",
        "GET /index.html HTTP/1.1\r\n"
        "\r\n",
        400
    );

    expectError(
        "Unsupported method",
        "PUT /index.html HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "\r\n",
        501
    );

    expectError(
        "Malformed request line",
        "GET /index.html\r\n"
        "Host: localhost:8080\r\n"
        "\r\n",
        400
    );

    expectError(
        "Too many request line parts",
        "GET /index.html HTTP/1.1 EXTRA\r\n"
        "Host: localhost:8080\r\n"
        "\r\n",
        400
    );

    expectError(
        "Invalid HTTP version",
        "GET /index.html HTTP/2.0\r\n"
        "Host: localhost:8080\r\n"
        "\r\n",
        505
    );

    expectError(
        "Malformed header",
        "GET /index.html HTTP/1.1\r\n"
        "Host localhost:8080\r\n"
        "\r\n",
        400
    );

    expectError(
        "Invalid Content-Length",
        "POST /login HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "Content-Length: abc\r\n"
        "\r\n"
        "hello",
        400
    );

    expectError(
        "Incomplete body",
        "POST /login HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "Content-Length: 20\r\n"
        "\r\n"
        "short",
        400
    );

    expectError(
        "Broken chunked body",
        "POST /chunk HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "5\r\n"
        "Hello"
        "0\r\n"
        "\r\n",
        400
    );

    return 0;
}