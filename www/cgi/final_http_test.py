#!/usr/bin/env python3

import os
import sys
import http.client
import socket
import re
import threading
import sys

query = os.environ.get("QUERY_STRING", "")
method = os.environ.get("REQUEST_METHOD", "")
body = sys.stdin.read()

print("Content-Type: text/html")
print()

print("<html>")
print("<body>")
print("<h1>Python CGI worked</h1>")
print("<p>Method: " + method + "</p>")
print("<p>Query: " + query + "</p>")
print("<p>Body: " + body + "</p>")
print("</body>")
print("</html>")

HOST = "127.0.0.1"
PORT = 8080
PORT2 = 8081

passed = 0
failed = 0

def header_get(headers, key):
    key = key.lower()
    for k, v in headers.items():
        if k.lower() == key:
            return v
    return ""

def request(method, path, body=None, headers=None, port=PORT):
    conn = http.client.HTTPConnection(HOST, port, timeout=5)
    conn.request(method, path, body=body, headers=headers or {})
    resp = conn.getresponse()
    data = resp.read()
    headers_dict = dict(resp.getheaders())
    conn.close()
    return resp.status, resp.reason, headers_dict, data

def raw_request(raw, port=PORT):
    s = socket.create_connection((HOST, port), timeout=5)
    s.sendall(raw.encode("iso-8859-1"))
    s.settimeout(5)

    chunks = []
    try:
        while True:
            part = s.recv(4096)
            if not part:
                break
            chunks.append(part)
    except socket.timeout:
        pass

    s.close()
    return b"".join(chunks).decode("iso-8859-1", errors="replace")

def check(name, condition, detail=""):
    global passed, failed

    if condition:
        print("[PASS]", name)
        passed += 1
    else:
        print("[FAIL]", name)
        if detail:
            print("       " + detail)
        failed += 1

def status_line_has(raw, code):
    return ("HTTP/1.1 " + str(code) + " ") in raw or ("HTTP/1.0 " + str(code) + " ") in raw

def test_static():
    status, reason, headers, body = request("GET", "/")
    check("GET / -> 200", status == 200, "got %d %s" % (status, reason))
    check("GET / body contains Webserv", b"Webserv" in body, "body did not contain Webserv")

    status, reason, headers, body = request("GET", "/about.html")
    check("GET /about.html -> 200", status == 200, "got %d %s" % (status, reason))

    status, reason, headers, body = request("GET", "/missing-file.html")
    check("GET missing file -> 404", status == 404, "got %d %s" % (status, reason))

def test_redirect():
    status, reason, headers, body = request("GET", "/old")
    location = header_get(headers, "Location")

    check("GET /old -> 301 or 302", status in (301, 302), "got %d %s" % (status, reason))
    check("Redirect has Location header", location != "", "Location header missing")

    if location:
        status2, reason2, headers2, body2 = request("GET", location)
        check("Redirect target opens -> 200", status2 == 200, "target %s got %d %s" % (location, status2, reason2))

def test_methods():
    status, reason, headers, body = request("GET", "/upload")
    check("GET /upload -> 405", status == 405, "got %d %s" % (status, reason))

    allow = header_get(headers, "Allow")
    check("405 has Allow header", allow != "", "Allow header missing")

def test_upload_get_delete():
    boundary = "----WEBSERVTESTBOUNDARY"
    file_payload = b"THIS_IS_TEST_IMAGE_CONTENT"
    body = (
        b"--" + boundary.encode() + b"\r\n"
        b"Content-Disposition: form-data; name=\"file\"; filename=\"smoke.png\"\r\n"
        b"Content-Type: image/png\r\n"
        b"\r\n" +
        file_payload +
        b"\r\n--" + boundary.encode() + b"--\r\n"
    )

    headers = {
        "Content-Type": "multipart/form-data; boundary=" + boundary,
        "Content-Length": str(len(body))
    }

    status, reason, resp_headers, resp_body = request("POST", "/upload", body=body, headers=headers)
    text = resp_body.decode("utf-8", errors="replace")

    check("POST multipart /upload -> 201", status == 201, "got %d %s\n%s" % (status, reason, text))

    filename = None
    m = re.search(r"Saved as:\s*([^<\s]+)", text)
    if m:
        filename = m.group(1)

    check("Upload response contains saved filename", filename is not None, text)

    if not filename:
        return

    uploaded_path = "/uploads/" + filename

    status, reason, headers, body = request("GET", uploaded_path)
    check("GET uploaded file -> 200", status == 200, "got %d %s for %s" % (status, reason, uploaded_path))

    ctype = header_get(headers, "Content-Type")
    check("Uploaded file Content-Type is image/png", ctype.startswith("image/png"), "got " + ctype)

    check("Uploaded file content is clean", body == file_payload, "uploaded content differs; multipart may not be parsed cleanly")

    status, reason, headers, body = request("DELETE", uploaded_path)
    check("DELETE uploaded file -> 204", status == 204, "got %d %s" % (status, reason))

def test_body_limit():
    big_body = b"a" * 1000001
    headers = {
        "Content-Type": "text/plain",
        "Content-Length": str(len(big_body))
    }

    status, reason, resp_headers, resp_body = request("POST", "/upload", body=big_body, headers=headers)
    check("POST body over client_max_body_size -> 413", status == 413, "got %d %s" % (status, reason))

def test_cgi():
    status, reason, headers, body = request("GET", "/cgi/echo.py?name=webserv&method=get")
    text = body.decode("utf-8", errors="replace")

    check("CGI GET -> 200", status == 200, "got %d %s" % (status, reason))
    check("CGI GET method", "method=GET" in text, text)
    check("CGI GET query", "query=name=webserv&method=get" in text, text)
    check("CGI GET empty body", "body=" in text, text)

    post_body = b"name=webserv&mode=post"
    headers = {
        "Content-Type": "application/x-www-form-urlencoded",
        "Content-Length": str(len(post_body))
    }

    status, reason, headers, body = request("POST", "/cgi/echo.py", body=post_body, headers=headers)
    text = body.decode("utf-8", errors="replace")

    check("CGI POST -> 200", status == 200, "got %d %s" % (status, reason))
    check("CGI POST method", "method=POST" in text, text)
    check("CGI POST body", "body=name=webserv&mode=post" in text, text)

def test_invalid_raw_requests():
    raw = raw_request("BADREQUEST\r\n\r\n")
    check("Invalid request line -> 400", status_line_has(raw, 400), raw.split("\r\n")[0])

    raw = raw_request("GET / HTTP/9.9\r\nHost: localhost:8080\r\n\r\n")
    check("Invalid HTTP version -> 505", status_line_has(raw, 505), raw.split("\r\n")[0])

    raw = raw_request("PUT / HTTP/1.1\r\nHost: localhost:8080\r\n\r\n")
    check("Unsupported method PUT -> 501 or 405", status_line_has(raw, 501) or status_line_has(raw, 405), raw.split("\r\n")[0])

def test_second_port():
    try:
        status, reason, headers, body = request("GET", "/", port=PORT2)
        check("GET / on port 8081 -> 200", status == 200, "got %d %s" % (status, reason))
    except Exception as e:
        check("Port 8081 reachable", False, str(e))

def test_stress():
    results = []
    lock = threading.Lock()

    def worker():
        try:
            status, reason, headers, body = request("GET", "/")
            ok = (status == 200)
        except Exception:
            ok = False

        with lock:
            results.append(ok)

    threads = []
    for i in range(50):
        t = threading.Thread(target=worker)
        threads.append(t)
        t.start()

    for t in threads:
        t.join()

    check("Stress: 50 parallel GET /", len(results) == 50 and all(results), "passed %d/50" % sum(1 for x in results if x))

def main():
    print("Running webserv final tests against http://%s:%d" % (HOST, PORT))
    print("Make sure ./webserv configs/default.conf is already running.\n")

    test_static()
    test_redirect()
    test_methods()
    test_upload_get_delete()
    test_body_limit()
    test_cgi()
    test_invalid_raw_requests()
    test_second_port()
    test_stress()

    print("\nPassed:", passed)
    print("Failed:", failed)

    if failed != 0:
        sys.exit(1)

if __name__ == "__main__":
    main()