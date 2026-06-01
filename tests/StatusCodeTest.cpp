#include <iostream>
#include "StatusCode.hpp"

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

int main()
{
    printResult("200 reason phrase",
        StatusCode::getReasonPhrase(200) == "OK");

    printResult("404 reason phrase",
        StatusCode::getReasonPhrase(404) == "Not Found");

    printResult("500 reason phrase",
        StatusCode::getReasonPhrase(500) == "Internal Server Error");

    printResult("204 body not allowed",
        StatusCode::isBodyAllowed(204) == false);

    printResult("200 body allowed",
        StatusCode::isBodyAllowed(200) == true);

    printResult("301 is redirect",
        StatusCode::isRedirect(301) == true);

    printResult("404 is not redirect",
        StatusCode::isRedirect(404) == false);

    printResult("404 is error",
        StatusCode::isError(404) == true);

    printResult("200 is not error",
        StatusCode::isError(200) == false);

    std::cout << "----------------------------------------" << std::endl;
    std::cout << "Passed: " << g_passed << std::endl;
    std::cout << "Failed: " << g_failed << std::endl;

    if (g_failed != 0)
        return 1;

    return 0;
}