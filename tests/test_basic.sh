#!/bin/bash

HOST="http://localhost:8080"
PASS=0
FAIL=0

GREEN="\033[0;32m"
RED="\033[0;31m"
YELLOW="\033[1;33m"
RESET="\033[0m"

print_result()
{
    if [ "$1" = "OK" ]; then
        echo "${GREEN}[PASS]${RESET} $2"
        PASS=$((PASS + 1))
    else
        echo "${RED}[FAIL]${RESET} $2"
        FAIL=$((FAIL + 1))
    fi
}

check_status()
{
    NAME="$1"
    CMD="$2"
    EXPECTED="$3"

    STATUS=$(eval "$CMD" 2>/dev/null | head -n 1 | awk '{print $2}')

    if [ "$STATUS" = "$EXPECTED" ]; then
        print_result "OK" "$NAME -> expected $EXPECTED, got $STATUS"
    else
        print_result "FAIL" "$NAME -> expected $EXPECTED, got $STATUS"
    fi
}

check_body_contains()
{
    NAME="$1"
    CMD="$2"
    TEXT="$3"

    BODY=$(eval "$CMD" 2>/dev/null)

    echo "$BODY" | grep -q "$TEXT"
    if [ $? -eq 0 ]; then
        print_result "OK" "$NAME -> body contains '$TEXT'"
    else
        print_result "FAIL" "$NAME -> body does not contain '$TEXT'"
    fi
}

echo "${YELLOW}Preparing test files...${RESET}"

mkdir -p www
mkdir -p www/folder
mkdir -p www/noindex
mkdir -p www/errors
mkdir -p www/uploads

echo "<h1>Hello static</h1>" > www/index.html
echo "<h1>About page</h1>" > www/about.html
echo "<h1>Folder index</h1>" > www/folder/index.html
echo "<h1>Custom 404 Page</h1>" > www/errors/404.html
echo "<h1>Custom 403 Page</h1>" > www/errors/403.html
echo "<h1>Custom 413 Page</h1>" > www/errors/413.html
echo "delete me" > www/uploads/delete_test.bin

echo ""
echo "${YELLOW}Running tests...${RESET}"
echo ""

check_status "GET /" \
    "curl -i $HOST/" \
    "200"

check_body_contains "GET / body" \
    "curl -i $HOST/" \
    "Hello static"

check_status "GET /about.html" \
    "curl -i $HOST/about.html" \
    "200"

check_body_contains "GET /about.html body" \
    "curl -i $HOST/about.html" \
    "About page"

check_status "GET folder index /folder/" \
    "curl -i $HOST/folder/" \
    "200"

check_body_contains "GET folder body" \
    "curl -i $HOST/folder/" \
    "Folder index"

check_status "GET no index folder" \
    "curl -i $HOST/noindex/" \
    "403"

check_status "GET not found" \
    "curl -i $HOST/nope.html" \
    "404"

check_body_contains "Custom 404 page" \
    "curl -i $HOST/nope.html" \
    "Custom 404 Page"

check_status "Path traversal GET" \
    "curl --path-as-is -i $HOST/../../Makefile" \
    "403"

check_body_contains "Custom 403 page" \
    "curl --path-as-is -i $HOST/../../Makefile" \
    "Custom 403 Page"

check_status "Unsupported method PUT" \
    "curl -i -X PUT $HOST/" \
    "405"

UPLOAD_RESPONSE=$(curl -i -X POST "$HOST/upload" --data-binary "hello uploaded file" 2>/dev/null)
UPLOAD_STATUS=$(echo "$UPLOAD_RESPONSE" | head -n 1 | awk '{print $2}')
UPLOAD_FILE=$(echo "$UPLOAD_RESPONSE" | grep -o "upload_[0-9][0-9]*_[0-9][0-9]*.bin" | head -n 1)

if [ "$UPLOAD_STATUS" = "201" ] && [ -n "$UPLOAD_FILE" ]; then
    print_result "OK" "POST /upload -> created $UPLOAD_FILE"
else
    print_result "FAIL" "POST /upload -> expected 201 and upload filename"
fi

if [ -f "www/uploads/$UPLOAD_FILE" ]; then
    print_result "OK" "Uploaded file exists on disk"
else
    print_result "FAIL" "Uploaded file does not exist on disk"
fi

check_status "GET uploaded file" \
    "curl -i $HOST/uploads/$UPLOAD_FILE" \
    "200"

check_body_contains "GET uploaded file body" \
    "curl -i $HOST/uploads/$UPLOAD_FILE" \
    "hello uploaded file"

check_status "DELETE uploaded file" \
    "curl -i -X DELETE $HOST/uploads/$UPLOAD_FILE" \
    "204"

check_status "GET deleted uploaded file" \
    "curl -i $HOST/uploads/$UPLOAD_FILE" \
    "404"

check_status "DELETE protected file /about.html" \
    "curl -i -X DELETE $HOST/about.html" \
    "403"

if [ -f "www/about.html" ]; then
    print_result "OK" "Protected file still exists after DELETE attempt"
else
    print_result "FAIL" "Protected file was deleted"
fi

check_status "DELETE manual upload file" \
    "curl -i -X DELETE $HOST/uploads/delete_test.bin" \
    "204"

echo ""
echo "${YELLOW}Testing 413 Payload Too Large...${RESET}"

dd if=/dev/zero of=tests/big_test.bin bs=1024 count=2048 >/dev/null 2>&1

check_status "POST big body" \
    "curl -i -X POST $HOST/upload --data-binary @tests/big_test.bin" \
    "413"

check_body_contains "Custom 413 page" \
    "curl -i -X POST $HOST/upload --data-binary @tests/big_test.bin" \
    "Custom 413 Page"

rm -f tests/big_test.bin

echo ""
echo "================================"
echo "${GREEN}PASS:${RESET} $PASS"
echo "${RED}FAIL:${RESET} $FAIL"
echo "================================"

if [ "$FAIL" -eq 0 ]; then
    echo "${GREEN}All tests passed.${RESET}"
    exit 0
else
    echo "${RED}Some tests failed.${RESET}"
    exit 1
fi