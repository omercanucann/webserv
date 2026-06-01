#include <iostream>
#include <string>
#include "MimeTypes.hpp"

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

static void expectMimeType(const std::string& testName,
                           const std::string& path,
                           const std::string& expected)
{
    MimeTypes mimeTypes;
    std::string result;

    result = mimeTypes.getMimeType(path);

    printResult(testName, result == expected);

    if (result != expected)
    {
        std::cout << "       Path: " << path << std::endl;
        std::cout << "       Expected: " << expected << std::endl;
        std::cout << "       Got: " << result << std::endl;
    }
}

int main()
{
    expectMimeType("HTML file", "/www/index.html", "text/html");
    expectMimeType("CSS file", "/www/style.css", "text/css");
    expectMimeType("JS file", "/www/app.js", "application/javascript");
    expectMimeType("PNG file", "/www/image.png", "image/png");
    expectMimeType("JPG file", "/www/photo.jpg", "image/jpeg");
    expectMimeType("Uppercase extension", "/www/INDEX.HTML", "text/html");
    expectMimeType("Unknown extension", "/www/file.unknown", "application/octet-stream");
    expectMimeType("No extension", "/www/README", "application/octet-stream");
    expectMimeType("Dot in folder name", "/www/site.v1/index.html", "text/html");

    std::cout << "----------------------------------------" << std::endl;
    std::cout << "Passed: " << g_passed << std::endl;
    std::cout << "Failed: " << g_failed << std::endl;

    if (g_failed != 0)
        return 1;

    return 0;
}