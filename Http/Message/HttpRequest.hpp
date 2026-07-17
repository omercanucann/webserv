#ifndef HTTPREQUEST_HPP
#define HTTPREQUEST_HPP

#include <string>
#include <map>

class HttpRequest
{
	private:
		std::string	_method;
		std::string	_path;
		std::string	_query;
		std::string	_version;
		std::map<std::string, std::string> _headers;
		std::string _body;
		std::string _bodyFilePath;
		size_t      _bodySize;
		bool        _bodyStoredInFile;

	public:
		HttpRequest();

		void		setMethod(const std::string& method);
		void		setPath(const std::string& path);
		void		setQuery(const std::string& query);
		void		setVersion(const std::string& version);
		void		setHeader(const std::string& key, const std::string& value);
		void		setBody(const std::string& body);
		void		setBodyFile(const std::string& path, size_t bodySize);

		const		std::string& getMethod() const;
		const		std::string& getPath() const;
		const		std::string& getQuery() const;
		const		std::string& getVersion() const;
		const		std::string& getBody() const;
		const		std::string& getBodyFilePath() const;
		size_t		getBodySize() const;
		bool		hasBodyFile() const;

		std::string	getHeader(const std::string &key) const;
		bool		hasHeader(const std::string &key) const;
};

#endif
