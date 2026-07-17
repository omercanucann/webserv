#!/usr/bin/env python3
"""Black-box HTTP integration tests for webserv.

By default this script starts ./webserv with configs/default.conf, performs the
tests against ports 8080 and 8081, and stops the server afterwards.

Usage:
    python3 tests/http_test.py
    python3 tests/http_test.py --external
    python3 tests/http_test.py --host 127.0.0.1 --port 8080
"""

import argparse
import concurrent.futures
import http.client
import os
import re
import signal
import socket
import subprocess
import sys
import tempfile
import time


ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MINIMUM_TEST_COUNT = 100


class Response(object):
    def __init__(self, status, reason, version, headers, body):
        self.status = status
        self.reason = reason
        self.version = version
        self.headers = headers
        self.body = body

    def header(self, name):
        wanted = name.lower()
        for key, value in self.headers:
            if key.lower() == wanted:
                return value
        return ""


class TestRunner(object):
    def __init__(self, verbose_failures=False):
        self.passed = 0
        self.failed = 0
        self.verbose_failures = verbose_failures

    def expect(self, name, condition, detail=""):
        if condition:
            self.passed += 1
            print("[PASS] " + name)
            return

        self.failed += 1
        print("[FAIL] " + name)
        if detail and self.verbose_failures:
            print("       " + str(detail).replace("\n", "\n       "))

    @property
    def total(self):
        return self.passed + self.failed


class HttpClient(object):
    def __init__(self, host, port, timeout):
        self.host = host
        self.port = port
        self.timeout = timeout

    def request(self, method, path, body=None, headers=None, port=None):
        target_port = self.port if port is None else port
        connection = http.client.HTTPConnection(
            self.host, target_port, timeout=self.timeout)
        try:
            connection.request(method, path, body=body, headers=headers or {})
            incoming = connection.getresponse()
            payload = incoming.read()
            return Response(
                incoming.status,
                incoming.reason,
                incoming.version,
                incoming.getheaders(),
                payload)
        finally:
            connection.close()

    def raw(self, request_bytes, port=None):
        target_port = self.port if port is None else port
        sock = socket.create_connection(
            (self.host, target_port), timeout=self.timeout)
        chunks = []
        try:
            sock.settimeout(self.timeout)
            sock.sendall(request_bytes)
            while True:
                part = sock.recv(65536)
                if not part:
                    break
                chunks.append(part)
        except socket.timeout:
            pass
        finally:
            sock.close()
        return b"".join(chunks)


def status_from_raw(raw_response):
    match = re.search(br"HTTP/\d\.\d\s+(\d{3})\b", raw_response)
    if match is None:
        return None
    return int(match.group(1))


def first_line(raw_response):
    return raw_response.split(b"\r\n", 1)[0].decode(
        "iso-8859-1", errors="replace")


def content_length_matches(response):
    value = response.header("Content-Length")
    return value.isdigit() and int(value) == len(response.body)


class ServerProcess(object):
    def __init__(self, executable, config, host, ports, timeout, external):
        self.executable = executable
        self.config = config
        self.host = host
        self.ports = ports
        self.timeout = timeout
        self.external = external
        self.process = None
        self.log = None

    def _port_open(self, port):
        try:
            sock = socket.create_connection((self.host, port), timeout=0.2)
            sock.close()
            return True
        except OSError:
            return False

    def start(self):
        if self.external:
            missing = [port for port in self.ports if not self._port_open(port)]
            if missing:
                raise RuntimeError(
                    "--external kullanıldı ama portlar açık değil: %s"
                    % ", ".join(str(port) for port in missing))
            return

        occupied = [port for port in self.ports if self._port_open(port)]
        if occupied:
            raise RuntimeError(
                "Port zaten kullanımda: %s. Çalışan sunucuyu sınamak için "
                "--external kullanın."
                % ", ".join(str(port) for port in occupied))
        if not os.path.isfile(self.executable):
            raise RuntimeError(
                "Sunucu binary'si bulunamadı: %s (önce make çalıştırın)"
                % self.executable)
        if not os.path.isfile(self.config):
            raise RuntimeError("Config bulunamadı: " + self.config)

        self.log = tempfile.TemporaryFile(mode="w+")
        self.process = subprocess.Popen(
            [self.executable, self.config],
            cwd=ROOT,
            stdout=self.log,
            stderr=subprocess.STDOUT)

        deadline = time.time() + self.timeout
        while time.time() < deadline:
            if self.process.poll() is not None:
                self.log.seek(0)
                raise RuntimeError(
                    "Sunucu test başlamadan kapandı:\n" + self.log.read())
            if all(self._port_open(port) for port in self.ports):
                return
            time.sleep(0.05)

        raise RuntimeError("Sunucu portları zamanında açılmadı")

    def stop(self):
        if self.process is not None and self.process.poll() is None:
            self.process.send_signal(signal.SIGINT)
            try:
                self.process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                self.process.terminate()
                try:
                    self.process.wait(timeout=2)
                except subprocess.TimeoutExpired:
                    self.process.kill()
                    self.process.wait()
        if self.log is not None:
            self.log.close()


def test_static_files(client, runner):
    print("\n== Static files and response framing ==")
    root = client.request("GET", "/")
    runner.expect("GET / returns 200", root.status == 200, root.status)
    runner.expect("GET / reason is OK", root.reason == "OK", root.reason)
    runner.expect("GET / uses HTTP/1.1", root.version == 11, root.version)
    runner.expect("GET / serves the webserv page", b"Webserv Test Page" in root.body)
    runner.expect("GET / is text/html", root.header("Content-Type") == "text/html")
    runner.expect("GET / Content-Length is exact", content_length_matches(root))
    runner.expect("GET / closes the connection", root.header("Connection").lower() == "close")

    index = client.request("GET", "/index.html")
    runner.expect("GET /index.html returns 200", index.status == 200, index.status)
    runner.expect("/ and /index.html have identical bodies", index.body == root.body)
    runner.expect("index.html has an exact Content-Length", content_length_matches(index))
    runner.expect("index.html MIME type is HTML", index.header("Content-Type") == "text/html")

    about = client.request("GET", "/about.html")
    runner.expect("GET /about.html returns 200", about.status == 200, about.status)
    runner.expect("about.html body is correct", b"About page" in about.body)
    runner.expect("about.html MIME type is HTML", about.header("Content-Type") == "text/html")
    runner.expect("about.html Content-Length is exact", content_length_matches(about))

    new_page = client.request("GET", "/new.html")
    runner.expect("GET /new.html returns 200", new_page.status == 200, new_page.status)
    runner.expect("new.html has redirect target content", b"Redirect worked successfully" in new_page.body)
    runner.expect("new.html Content-Length is exact", content_length_matches(new_page))

    empty = client.request("GET", "/empty.html")
    runner.expect("GET empty file returns 200", empty.status == 200, empty.status)
    runner.expect("Empty file response body is empty", empty.body == b"", len(empty.body))
    runner.expect("Empty file Content-Length is zero", empty.header("Content-Length") == "0")
    runner.expect("Empty HTML file keeps HTML MIME", empty.header("Content-Type") == "text/html")

    note = client.request("GET", "/images/note.txt")
    runner.expect("GET text file returns 200", note.status == 200, note.status)
    runner.expect("Text file body is preserved", note.body == b"<h1>Hello Static</h1>")
    runner.expect("Text file MIME is text/plain", note.header("Content-Type") == "text/plain")
    runner.expect("Text file Content-Length is exact", content_length_matches(note))

    image = client.request("GET", "/images/berzerk.webp")
    runner.expect("GET binary image returns 200", image.status == 200, image.status)
    runner.expect("Binary image is not empty", len(image.body) > 100000, len(image.body))
    runner.expect("Unknown webp MIME uses octet-stream", image.header("Content-Type") == "application/octet-stream")
    runner.expect("Binary Content-Length is exact", content_length_matches(image))

    query = client.request("GET", "/about.html?name=omer&page=2")
    runner.expect("Static path with query returns 200", query.status == 200, query.status)
    runner.expect("Query does not change static body", query.body == about.body)
    runner.expect("Query response Content-Length is exact", content_length_matches(query))

    last_missing = None
    for index_number in range(1, 11):
        last_missing = client.request(
            "GET", "/definitely-missing-%02d.txt" % index_number)
        runner.expect(
            "Missing resource %02d returns 404" % index_number,
            last_missing.status == 404,
            last_missing.status)

    runner.expect("404 uses configured custom page", b"Custom 404 Page" in last_missing.body)
    runner.expect("404 page MIME is HTML", last_missing.header("Content-Type") == "text/html")
    runner.expect("404 Content-Length is exact", content_length_matches(last_missing))


def test_routing(client, runner):
    print("\n== Routing, autoindex and redirect ==")
    listing = client.request("GET", "/images")
    runner.expect("GET /images returns autoindex", listing.status == 200, listing.status)
    runner.expect("Autoindex title contains request path", b"Index of /images/" in listing.body)
    runner.expect("Autoindex lists note.txt", b"note.txt" in listing.body)
    runner.expect("Autoindex lists berzerk.webp", b"berzerk.webp" in listing.body)
    runner.expect("Autoindex hides current-directory entry", b'href="/images/."' not in listing.body)
    runner.expect("Autoindex hides parent-directory entry", b'href="/images/.."' not in listing.body)
    runner.expect("Autoindex MIME is HTML", listing.header("Content-Type") == "text/html")
    runner.expect("Autoindex Content-Length is exact", content_length_matches(listing))

    slash_listing = client.request("GET", "/images/")
    runner.expect("GET /images/ also returns autoindex", slash_listing.status == 200)
    runner.expect("Autoindex slash form contains note.txt", b"note.txt" in slash_listing.body)

    folder = client.request("GET", "/folder")
    runner.expect("Directory with index returns 200", folder.status == 200, folder.status)
    runner.expect("Directory index file is served", b"folder" in folder.body.lower())

    redirect = client.request("GET", "/old")
    runner.expect("Configured redirect returns 301", redirect.status == 301, redirect.status)
    runner.expect("Redirect reason is Moved Permanently", redirect.reason == "Moved Permanently")
    runner.expect("Redirect Location is /new.html", redirect.header("Location") == "/new.html")
    runner.expect("Redirect response has HTML body", b"Redirecting to /new.html" in redirect.body)
    runner.expect("Redirect Content-Length is exact", content_length_matches(redirect))
    target = client.request("GET", redirect.header("Location") or "/new.html")
    runner.expect("Redirect target returns 200", target.status == 200, target.status)

    boundary = client.request("GET", "/imagesXYZ")
    runner.expect("Location prefix requires path boundary", boundary.status == 404, boundary.status)
    runner.expect("Boundary mismatch does not expose autoindex", b"Index of" not in boundary.body)


def test_method_rules(client, runner):
    print("\n== Method rules ==")
    cases = [
        ("GET", "/upload", 405, "POST"),
        ("POST", "/", 405, "GET"),
        ("DELETE", "/", 405, "GET"),
        ("POST", "/images", 405, "GET"),
        ("DELETE", "/images", 405, "GET"),
        ("GET", "/post_body", 405, "POST"),
        ("DELETE", "/cgi/echo.py", 405, "GET, POST"),
        ("PUT", "/", 405, "GET"),
        ("PATCH", "/", 405, "GET"),
        ("OPTIONS", "/", 405, "GET"),
    ]
    for method, path, expected, allowed in cases:
        response = client.request(method, path)
        label = "%s %s" % (method, path)
        runner.expect(label + " returns 405", response.status == expected, response.status)
        runner.expect(label + " exposes the Allow header", response.header("Allow") == allowed,
                      response.header("Allow"))


def test_raw_protocol(client, runner, port):
    print("\n== Raw HTTP parsing and protocol errors ==")

    valid_requests = [
        ("HTTP/1.0 request without Host is accepted",
         b"GET /about.html HTTP/1.0\r\n\r\n", 200),
        ("Lowercase host header is accepted",
         b"GET /about.html HTTP/1.1\r\nhost: localhost:8080\r\n\r\n", 200),
        ("Uppercase HOST header is accepted",
         b"GET /about.html HTTP/1.1\r\nHOST: localhost:8080\r\n\r\n", 200),
        ("OWS around Host value is accepted",
         b"GET /about.html HTTP/1.1\r\nHost:\t localhost:8080 \t\r\n\r\n", 200),
        ("Query target is accepted by raw parser",
         b"GET /about.html?a=1&b=two HTTP/1.1\r\nHost: localhost:8080\r\n\r\n", 200),
    ]
    for name, raw_request, expected in valid_requests:
        raw_response = client.raw(raw_request, port)
        runner.expect(name, status_from_raw(raw_response) == expected,
                      first_line(raw_response))

    malformed_requests = [
        ("Missing Host in HTTP/1.1 returns 400",
         b"GET / HTTP/1.1\r\n\r\n", 400),
        ("Malformed request line returns 400",
         b"GET_ONLY\r\nHost: localhost:8080\r\n\r\n", 400),
        ("Request line with extra part returns 400",
         b"GET / HTTP/1.1 EXTRA\r\nHost: localhost:8080\r\n\r\n", 400),
        ("Empty request line returns 400",
         b"\r\nHost: localhost:8080\r\n\r\n", 400),
        ("Target without leading slash returns 400",
         b"GET index.html HTTP/1.1\r\nHost: localhost:8080\r\n\r\n", 400),
        ("Invalid method token returns 400",
         b"GE@T / HTTP/1.1\r\nHost: localhost:8080\r\n\r\n", 400),
        ("Malformed header without colon returns 400",
         b"GET / HTTP/1.1\r\nHost localhost:8080\r\n\r\n", 400),
        ("Header name containing spaces returns 400",
         b"GET / HTTP/1.1\r\nHost: localhost:8080\r\nBad Header: x\r\n\r\n", 400),
        ("Empty header name returns 400",
         b"GET / HTTP/1.1\r\nHost: localhost:8080\r\n: value\r\n\r\n", 400),
        ("HTTP/2.0 request returns 505",
         b"GET / HTTP/2.0\r\nHost: localhost:8080\r\n\r\n", 505),
        ("Unknown HTTP version returns 505",
         b"GET / HTTP/9.9\r\nHost: localhost:8080\r\n\r\n", 505),
        ("Path traversal is forbidden",
         b"GET /../Config/ConfigParser.cpp HTTP/1.1\r\nHost: localhost:8080\r\n\r\n", 403),
    ]
    for name, raw_request, expected in malformed_requests:
        raw_response = client.raw(raw_request, port)
        runner.expect(name, status_from_raw(raw_response) == expected,
                      first_line(raw_response))


def test_request_bodies(client, runner, port):
    print("\n== Content-Length and chunked bodies ==")
    payload = b"hello-body"
    response = client.request(
        "POST", "/post_body", body=payload,
        headers={"Content-Type": "text/plain", "Content-Length": str(len(payload))})
    runner.expect("POST body route returns 200", response.status == 200, response.status)
    runner.expect("POST body route echoes bytes exactly", response.body == payload, response.body)
    runner.expect("POST echo MIME is text/plain", response.header("Content-Type") == "text/plain")
    runner.expect("POST echo Content-Length is exact", content_length_matches(response))

    empty = client.request("POST", "/post_body", body=b"", headers={"Content-Length": "0"})
    runner.expect("Zero-length POST is accepted", empty.status == 200, empty.status)
    runner.expect("Zero-length POST returns empty body", empty.body == b"")

    exact_limit_body = b"x" * 100
    exact = client.request("POST", "/post_body", body=exact_limit_body,
                           headers={"Content-Length": "100"})
    runner.expect("Body exactly at location limit is accepted", exact.status == 200, exact.status)
    runner.expect("Body at limit is preserved", exact.body == exact_limit_body)

    over_limit = client.request("POST", "/post_body", body=b"x" * 101,
                                headers={"Content-Length": "101"})
    runner.expect("Body over location limit returns 413", over_limit.status == 413, over_limit.status)
    runner.expect("413 response identifies Payload Too Large", b"Payload Too Large" in over_limit.body)

    chunked = client.raw(
        b"POST /post_body HTTP/1.1\r\n"
        b"Host: localhost:8080\r\nTransfer-Encoding: chunked\r\n\r\n"
        b"5\r\nHello\r\n6\r\n World\r\n0\r\n\r\n", port)
    runner.expect("Chunked POST is accepted", status_from_raw(chunked) == 200, first_line(chunked))
    runner.expect("Chunked POST is decoded", chunked.endswith(b"Hello World"))

    chunk_extension = client.raw(
        b"POST /post_body HTTP/1.1\r\n"
        b"Host: localhost:8080\r\nTransfer-Encoding: chunked\r\n\r\n"
        b"4;name=value\r\nWiki\r\n0\r\n\r\n", port)
    runner.expect("Chunk extension syntax is accepted", status_from_raw(chunk_extension) == 200,
                  first_line(chunk_extension))
    runner.expect("Chunk extension does not alter data", chunk_extension.endswith(b"Wiki"))

    body_errors = [
        ("Content-Length and chunked together return 400",
         b"POST /post_body HTTP/1.1\r\nHost: localhost:8080\r\n"
         b"Content-Length: 4\r\nTransfer-Encoding: chunked\r\n\r\n"
         b"4\r\nTest\r\n0\r\n\r\n", 400),
        ("Non-numeric Content-Length returns 400",
         b"POST /post_body HTTP/1.1\r\nHost: localhost:8080\r\n"
         b"Content-Length: abc\r\n\r\n", 400),
        ("Malformed chunk framing returns 400",
         b"POST /post_body HTTP/1.1\r\nHost: localhost:8080\r\n"
         b"Transfer-Encoding: chunked\r\n\r\n"
         b"5\r\nHell\r\n0\r\n\r\n", 400),
        ("POST body without length returns 411",
         b"POST /post_body HTTP/1.1\r\nHost: localhost:8080\r\n\r\ndata", 411),
    ]
    for name, raw_request, expected in body_errors:
        raw_response = client.raw(raw_request, port)
        runner.expect(name, status_from_raw(raw_response) == expected,
                      first_line(raw_response))

    get_with_body = client.raw(
        b"GET /about.html HTTP/1.1\r\nHost: localhost:8080\r\n"
        b"Content-Length: 4\r\n\r\ndata", port)
    runner.expect("GET with framed body remains a valid GET",
                  status_from_raw(get_with_body) == 200, first_line(get_with_body))


def test_cgi(client, runner):
    print("\n== CGI ==")
    get_response = client.request("GET", "/cgi/echo.py?name=webserv&value=42")
    runner.expect("CGI GET returns 200", get_response.status == 200, get_response.status)
    runner.expect("CGI GET exposes method", b"method=GET" in get_response.body)
    runner.expect("CGI GET exposes query", b"query=name=webserv&value=42" in get_response.body)
    runner.expect("CGI GET has empty input body", b"body=\n" in get_response.body)
    runner.expect("CGI GET MIME is text/plain", get_response.header("Content-Type") == "text/plain")
    runner.expect("CGI GET Content-Length is exact", content_length_matches(get_response))

    payload = b"name=omer&message=hello"
    post_response = client.request(
        "POST", "/cgi/echo.py", body=payload,
        headers={"Content-Type": "application/x-www-form-urlencoded",
                 "Content-Length": str(len(payload))})
    runner.expect("CGI POST returns 200", post_response.status == 200, post_response.status)
    runner.expect("CGI POST exposes method", b"method=POST" in post_response.body)
    runner.expect("CGI POST receives body exactly", b"body=" + payload in post_response.body)
    runner.expect("CGI POST Content-Length is exact", content_length_matches(post_response))


def test_upload_lifecycle(client, runner, cleanup_paths):
    print("\n== Upload, download and delete ==")
    original_name = "webserv_test_%d_%d.txt" % (os.getpid(), int(time.time()))
    payload = b"webserv integration upload\x00with binary byte\n"
    boundary = "----WebservBoundary%d" % os.getpid()
    multipart = (
        ("--%s\r\n" % boundary).encode("ascii")
        + ("Content-Disposition: form-data; name=\"file\"; filename=\"%s\"\r\n"
           % original_name).encode("ascii")
        + b"Content-Type: text/plain\r\n\r\n"
        + payload
        + ("\r\n--%s--\r\n" % boundary).encode("ascii"))
    upload = client.request(
        "POST", "/upload", body=multipart,
        headers={"Content-Type": "multipart/form-data; boundary=" + boundary,
                 "Content-Length": str(len(multipart))})
    runner.expect("Multipart upload returns 201", upload.status == 201, upload.status)
    runner.expect("Upload reason is Created", upload.reason == "Created", upload.reason)
    runner.expect("Upload response reports success", b"Upload successful" in upload.body)
    match = re.search(br"Saved as:\s*([^<\s]+)", upload.body)
    runner.expect("Upload response contains saved filename", match is not None, upload.body)

    saved_name = match.group(1).decode("ascii") if match is not None else ""
    if saved_name:
        cleanup_paths.append(os.path.join(ROOT, "uploads", saved_name))
    public_path = "/uploads/" + saved_name
    downloaded = client.request("GET", public_path) if saved_name else Response(0, "", 0, [], b"")
    runner.expect("Uploaded file can be downloaded", downloaded.status == 200, downloaded.status)
    runner.expect("Downloaded upload matches binary payload", downloaded.body == payload)
    runner.expect("Uploaded .txt file has text MIME", downloaded.header("Content-Type") == "text/plain")

    deleted = client.request("DELETE", public_path) if saved_name else Response(0, "", 0, [], b"")
    runner.expect("Uploaded file can be deleted", deleted.status == 204, deleted.status)
    runner.expect("204 delete response has no body", deleted.body == b"")
    runner.expect("Deleted upload is no longer accessible",
                  saved_name != "" and client.request("GET", public_path).status == 404)


def test_second_server(client, runner, second_port):
    print("\n== Second listen port ==")
    root = client.request("GET", "/", port=second_port)
    runner.expect("Second server port accepts connections", root.status == 200, root.status)
    runner.expect("Second server serves index page", b"Webserv Test Page" in root.body)
    runner.expect("Second server Content-Length is exact", content_length_matches(root))
    missing = client.request("GET", "/missing-on-second-port", port=second_port)
    runner.expect("Second server returns 404 for missing file", missing.status == 404, missing.status)
    directory = client.request("GET", "/images", port=second_port)
    runner.expect("Second server keeps autoindex disabled", directory.status == 404, directory.status)
    wrong_method = client.request("POST", "/", port=second_port)
    runner.expect("Second server enforces GET-only root", wrong_method.status == 405, wrong_method.status)


def test_concurrency(client, runner):
    print("\n== Concurrent connections ==")

    def fetch(index_number):
        path = "/about.html?parallel=%d" % index_number
        response = client.request("GET", path)
        return response.status == 200 and b"About page" in response.body

    with concurrent.futures.ThreadPoolExecutor(max_workers=20) as pool:
        results = list(pool.map(fetch, range(40)))
    runner.expect("40 concurrent requests all complete", len(results) == 40)
    runner.expect("40 concurrent requests all return correct data", all(results),
                  "%d/40 successful" % sum(1 for result in results if result))


def parse_args():
    parser = argparse.ArgumentParser(description="Run black-box HTTP tests against webserv")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8080)
    parser.add_argument("--second-port", type=int, default=8081)
    parser.add_argument("--timeout", type=float, default=5.0)
    parser.add_argument("--server", default=os.path.join(ROOT, "webserv"))
    parser.add_argument("--config", default=os.path.join(ROOT, "configs", "default.conf"))
    parser.add_argument("--external", action="store_true",
                        help="Do not start webserv; test an already running server")
    parser.add_argument("--verbose-failures", action="store_true",
                        help="Print response details for failed checks")
    return parser.parse_args()


def main():
    args = parse_args()
    runner = TestRunner(args.verbose_failures)
    client = HttpClient(args.host, args.port, args.timeout)
    server = ServerProcess(
        os.path.abspath(args.server), os.path.abspath(args.config), args.host,
        [args.port, args.second_port], args.timeout, args.external)
    cleanup_paths = []

    try:
        server.start()
        print("Testing http://%s:%d and port %d" %
              (args.host, args.port, args.second_port))
        test_static_files(client, runner)
        test_routing(client, runner)
        test_method_rules(client, runner)
        test_raw_protocol(client, runner, args.port)
        test_request_bodies(client, runner, args.port)
        test_cgi(client, runner)
        test_upload_lifecycle(client, runner, cleanup_paths)
        test_second_server(client, runner, args.second_port)
        test_concurrency(client, runner)
    except (OSError, RuntimeError, http.client.HTTPException) as error:
        print("\n[SETUP ERROR] " + str(error), file=sys.stderr)
        return 2
    finally:
        server.stop()
        for path in cleanup_paths:
            if os.path.isfile(path):
                os.remove(path)

    print("\n" + "=" * 60)
    print("Total: %d  Passed: %d  Failed: %d" %
          (runner.total, runner.passed, runner.failed))
    if runner.total < MINIMUM_TEST_COUNT:
        print("[INTERNAL ERROR] Only %d checks ran; at least %d are required."
              % (runner.total, MINIMUM_TEST_COUNT))
        return 2
    if runner.failed:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
