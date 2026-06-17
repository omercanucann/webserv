#include "AutoIndex.hpp"

AutoIndex::AutoIndex()
{

}

std::string AutoIndex::_escapeHtml(const std::string &str) const
{
    std::string out;
    size_t i = 0;

    while (i < str.length())
    {
        if (str[i] == '&')
            out += "&amp;";
        else if (str[i] == '<')
            out += "&lt;";
        else if (str[i] == '>')
            out += "&gt;";
        else if (str[i] == '"')
            out += "&quot;";
        else
            out += str[i];
        i++;
    }
    return out;
}

std::string AutoIndex::_ensureSlashEnd(const std::string &path) const
{
    if(path.empty())
        return "/";
    if (path[path.length() - 1] == '/')
        return path;
    return path + "/";
}

HttpResponse AutoIndex::generate(const std::string &directoryPath, const std::string &requestPath) const
{
    HttpResponse response;
    DIR *dir;
    struct dirent *entry;
    std::string baseUrl;
    std::ostringstream body;

    dir = opendir(directoryPath.c_str());
    if (dir == NULL)
        return HttpResponse::makeErrorResponse(403);
    
    baseUrl = _ensureSlashEnd(requestPath);

    body    << "<html><head><title>Index of "
            << _escapeHtml(baseUrl)
            << "</title></head><body>\n"

            << "<h1>Index of " << _escapeHtml(baseUrl) << "</h1>\n"
            << "<ul>\n";
    
    while ((entry = readdir(dir)) != NULL)
    {
        std::string name = entry->d_name;

        if (name == "." || name == "..")
            continue;
        
        body    << "<li><a href=\""
                << _escapeHtml(baseUrl + name)
                << "\">"
                << _escapeHtml(name)
                << "</a></li>\n";
    }

    body    << "</ul>\n"
            << "</body></html>\n";
    
    closedir(dir);
    response.setStatus(200);
    response.setHeader("Content-Type", "text/html");
    response.setBody(body.str());
    return response;
}
