#include <iostream>
#include <string>
#include "HttpResponse.hpp"

static int g_passed = 0;
static int g_failed = 0;

static void printResult(const std::string& testName, bool ok)
{
    if (ok)
    {
        std::cout << "[OK]   " << testName << std::endl;
        g_passed++;
    }
    else
    {
        std::cout << "[FAIL] " << testName << std::endl;
        g_failed++;
    }
}

static bool contains(const std::string& text, const std::string& part)
{
    return text.find(part) != std::string::npos;
}

static void testBasic200Response()
{
    HttpResponse response;
    std::string raw;

    response.setStatus(200);
    response.setHeader("Content-Type", "text/html");
    response.setBody("<h1>Hello</h1>");

    raw = response.toString();

    printResult("200 status line",
        contains(raw, "HTTP/1.1 200 OK\r\n"));

    printResult("Content-Type header",
        contains(raw, "Content-Type: text/html\r\n"));

    printResult("Content-Length header",
        contains(raw, "Content-Length: 14\r\n"));

    printResult("Connection close header",
        contains(raw, "Connection: close\r\n"));

    printResult("Header body separator",
        contains(raw, "\r\n\r\n"));

    printResult("Body exists",
        contains(raw, "<h1>Hello</h1>"));
}

static void test404ErrorResponse()
{
    HttpResponse response;
    std::string raw;

    response = HttpResponse::makeErrorResponse(404);
    raw = response.toString();

    printResult("404 status line",
        contains(raw, "HTTP/1.1 404 Not Found\r\n"));

    printResult("404 content type",
        contains(raw, "Content-Type: text/html\r\n"));

    printResult("404 body contains Not Found",
        contains(raw, "<h1>404 Not Found</h1>"));
}

static void test201CreatedResponse()
{
    HttpResponse response;
    std::string raw;

    response.setStatus(201);
    response.setHeader("Content-Type", "text/plain");
    response.setBody("File uploaded");

    raw = response.toString();

    printResult("201 status line",
        contains(raw, "HTTP/1.1 201 Created\r\n"));

    printResult("201 content length",
        contains(raw, "Content-Length: 13\r\n"));

    printResult("201 body",
        contains(raw, "File uploaded"));
}

static void testCustomHeader()
{
    HttpResponse response;
    std::string raw;

    response.setStatus(200);
    response.setHeader("X-Test", "hello");
    response.setBody("OK");

    raw = response.toString();

    printResult("Custom header exists",
        contains(raw, "X-Test: hello\r\n"));
}

static void test204NoContentResponse()
{
    HttpResponse response;
    std::string raw;

    response.setStatus(204);
    response.setHeader("Content-Type", "text/plain");
    response.setBody("This body should not be sent");

    raw = response.toString();

    printResult("204 status line",
        contains(raw, "HTTP/1.1 204 No Content\r\n"));

    printResult("204 content length zero",
        contains(raw, "Content-Length: 0\r\n"));

    printResult("204 body is not sent",
        !contains(raw, "This body should not be sent"));
}

static void test301RedirectResponse()
{
    HttpResponse response;
    std::string raw;

    response = HttpResponse::makeRedirectResponse(301, "/new-page");
    raw = response.toString();

    printResult("301 status line",
        contains(raw, "HTTP/1.1 301 Moved Permanently\r\n"));

    printResult("301 location header",
        contains(raw, "Location: /new-page\r\n"));

    printResult("301 content type",
        contains(raw, "Content-Type: text/html\r\n"));

    printResult("301 body contains location",
        contains(raw, "Redirecting to /new-page"));
}

static void test405MethodNotAllowedResponse()
{
    HttpResponse response;
    std::string raw;

    response = HttpResponse::makeMethodNotAllowedResponse("GET, POST");
    raw = response.toString();

    printResult("405 status line",
        contains(raw, "HTTP/1.1 405 Method Not Allowed\r\n"));

    printResult("405 allow header",
        contains(raw, "Allow: GET, POST\r\n"));

    printResult("405 content type",
        contains(raw, "Content-Type: text/html\r\n"));

    printResult("405 body",
        contains(raw, "<h1>405 Method Not Allowed</h1>"));
}

int main()
{
    testBasic200Response();
    test404ErrorResponse();
    test201CreatedResponse();
    testCustomHeader();
    test204NoContentResponse();
    test301RedirectResponse();
    test405MethodNotAllowedResponse();

    std::cout << "----------------------------------------" << std::endl;
    std::cout << "Passed: " << g_passed << std::endl;
    std::cout << "Failed: " << g_failed << std::endl;

    if (g_failed != 0)
        return 1;

    return 0;
}