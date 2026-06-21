#!/bin/bash

# ════════════════════════════════════════════════════════════════
# WebServ Stress Test Suite
# ════════════════════════════════════════════════════════════════

HOST="http://localhost"
PORT_1="8080"
PORT_2="8081"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

STATS_FILE="/tmp/webserv_stress_results.txt"
> "$STATS_FILE"

echo -e "${BLUE}════════════════════════════════════════════════════════════════${NC}"
echo -e "${BLUE}         WEBSERV STREss TEST SUITE${NC}"
echo -e "${BLUE}════════════════════════════════════════════════════════════════${NC}"

# Function to format number with thousands separator
format_num() {
    printf "%'d" "$1"
}

# ════════════════════════════════════════════════════════════════
# TEST 1: Simple GET Requests
# ════════════════════════════════════════════════════════════════
echo -e "\n${YELLOW}[TEST 1] Simple GET Requests (1000 istekler)${NC}"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" | tee -a "$STATS_FILE"

START_TIME=$(date +%s%N)
SUCCESS=0
FAILED=0

for i in $(seq 1 1000); do
    STATUS=$(curl -s -o /dev/null -w "%{http_code}" "$HOST:$PORT_1/")
    if [ "$STATUS" = "200" ]; then
        ((SUCCESS++))
    else
        ((FAILED++))
        echo -e "${RED}  ✗ Request $i failed with status: $STATUS${NC}"
    fi
    
    # Progress indicator
    if [ $((i % 100)) -eq 0 ]; then
        echo -ne "\r  ✓ Processed: $i/1000 requests"
    fi
done

END_TIME=$(date +%s%N)
DURATION_MS=$(( (END_TIME - START_TIME) / 1000000 ))
DURATION_SEC=$(awk "BEGIN {printf \"%.2f\", $DURATION_MS / 1000}")
AVG_TIME=$(awk "BEGIN {printf \"%.2f\", $DURATION_MS / 1000}")

echo -e "\n${GREEN}  ✓ Tamamlandı!${NC}"
echo -e "  Başarılı: ${GREEN}$(format_num $SUCCESS)${NC} / Başarısız: ${RED}$(format_num $FAILED)${NC}"
echo -e "  Toplam süre: ${BLUE}${DURATION_SEC}s${NC}"
echo -e "  Ortalama istek süresi: ${BLUE}${AVG_TIME}ms${NC}" | tee -a "$STATS_FILE"
echo -e "  İstek/saniye: ${BLUE}$(awk "BEGIN {printf \"%.0f\", 1000 / $AVG_TIME}")${NC}" | tee -a "$STATS_FILE"

# ════════════════════════════════════════════════════════════════
# TEST 2: Concurrent Connections
# ════════════════════════════════════════════════════════════════
echo -e "\n${YELLOW}[TEST 2] Eşzamanlı Bağlantılar (50 concurrent)${NC}"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" | tee -a "$STATS_FILE"

START_TIME=$(date +%s%N)
SUCCESS=0
FAILED=0

# Run 200 requests with 50 parallel connections
for i in $(seq 1 200); do
    (
        STATUS=$(curl -s -o /dev/null -w "%{http_code}" "$HOST:$PORT_1/index.html")
        if [ "$STATUS" = "200" ]; then
            echo "OK"
        else
            echo "FAIL:$STATUS"
        fi
    ) &
    
    # Keep maximum 50 parallel processes
    if [ $((i % 50)) -eq 0 ]; then
        wait
    fi
done

wait

END_TIME=$(date +%s%N)
DURATION_MS=$(( (END_TIME - START_TIME) / 1000000 ))
DURATION_SEC=$(awk "BEGIN {printf \"%.2f\", $DURATION_MS / 1000}")

echo -e "\n${GREEN}  ✓ Tamamlandı!${NC}"
echo -e "  Toplam süre: ${BLUE}${DURATION_SEC}s${NC}" | tee -a "$STATS_FILE"

# ════════════════════════════════════════════════════════════════
# TEST 3: Large File Downloads
# ════════════════════════════════════════════════════════════════
echo -e "\n${YELLOW}[TEST 3] Büyük Dosya İndirme Testi${NC}"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" | tee -a "$STATS_FILE"

# First, create a large test file
echo "  Büyük test dosyası oluşturuluyor..."
dd if=/dev/zero of="/tmp/large_test_file.bin" bs=1M count=10 2>/dev/null
cp "/tmp/large_test_file.bin" "/mnt/c/Users/Gaming/Desktop/Masaüstü/webserv/www/large_test.bin"

START_TIME=$(date +%s%N)
SUCCESS=0
FAILED=0

for i in $(seq 1 50); do
    STATUS=$(curl -s -o /dev/null -w "%{http_code}" "$HOST:$PORT_1/large_test.bin")
    if [ "$STATUS" = "200" ]; then
        ((SUCCESS++))
    else
        ((FAILED++))
    fi
    
    if [ $((i % 10)) -eq 0 ]; then
        echo -ne "\r  ✓ İndirilen: $i/50 dosya"
    fi
done

END_TIME=$(date +%s%N)
DURATION_MS=$(( (END_TIME - START_TIME) / 1000000 ))
DURATION_SEC=$(awk "BEGIN {printf \"%.2f\", $DURATION_MS / 1000}")
THROUGHPUT=$(awk "BEGIN {printf \"%.2f\", (10 * 50) / ($DURATION_MS / 1000 / 1024)}")

echo -e "\n${GREEN}  ✓ Tamamlandı!${NC}"
echo -e "  Başarılı: ${GREEN}$(format_num $SUCCESS)${NC} / Başarısız: ${RED}$(format_num $FAILED)${NC}"
echo -e "  Toplam süre: ${BLUE}${DURATION_SEC}s${NC}"
echo -e "  İşlenmiş veriler: ${BLUE}500MB${NC}"
echo -e "  Throughput: ${BLUE}${THROUGHPUT}MB/s${NC}" | tee -a "$STATS_FILE"

# ════════════════════════════════════════════════════════════════
# TEST 4: POST Requests (Upload Simulation)
# ════════════════════════════════════════════════════════════════
echo -e "\n${YELLOW}[TEST 4] POST İstekleri (Upload Simulasyonu)${NC}"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" | tee -a "$STATS_FILE"

# Create test file for upload
echo "Test upload data" > "/tmp/test_upload.txt"

START_TIME=$(date +%s%N)
SUCCESS=0
FAILED=0

for i in $(seq 1 100); do
    STATUS=$(curl -s -o /dev/null -w "%{http_code}" -F "file=@/tmp/test_upload.txt" "$HOST:$PORT_1/upload")
    if [ "$STATUS" = "200" ] || [ "$STATUS" = "201" ] || [ "$STATUS" = "204" ]; then
        ((SUCCESS++))
    else
        ((FAILED++))
    fi
    
    if [ $((i % 20)) -eq 0 ]; then
        echo -ne "\r  ✓ Upload yapılan: $i/100"
    fi
done

END_TIME=$(date +%s%N)
DURATION_MS=$(( (END_TIME - START_TIME) / 1000000 ))
DURATION_SEC=$(awk "BEGIN {printf \"%.2f\", $DURATION_MS / 1000}")

echo -e "\n${GREEN}  ✓ Tamamlandı!${NC}"
echo -e "  Başarılı: ${GREEN}$(format_num $SUCCESS)${NC} / Başarısız: ${RED}$(format_num $FAILED)${NC}"
echo -e "  Toplam süre: ${BLUE}${DURATION_SEC}s${NC}" | tee -a "$STATS_FILE"

# ════════════════════════════════════════════════════════════════
# TEST 5: CGI Execution
# ════════════════════════════════════════════════════════════════
echo -e "\n${YELLOW}[TEST 5] CGI Script Yürütme Testi${NC}"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" | tee -a "$STATS_FILE"

START_TIME=$(date +%s%N)
SUCCESS=0
FAILED=0

for i in $(seq 1 100); do
    STATUS=$(curl -s -o /dev/null -w "%{http_code}" "$HOST:$PORT_1/cgi/echo.py?test=value")
    if [ "$STATUS" = "200" ]; then
        ((SUCCESS++))
    else
        ((FAILED++))
    fi
    
    if [ $((i % 20)) -eq 0 ]; then
        echo -ne "\r  ✓ CGI istekleri: $i/100"
    fi
done

END_TIME=$(date +%s%N)
DURATION_MS=$(( (END_TIME - START_TIME) / 1000000 ))
DURATION_SEC=$(awk "BEGIN {printf \"%.2f\", $DURATION_MS / 1000}")
AVG_TIME=$(awk "BEGIN {printf \"%.0f\", $DURATION_MS / 100}")

echo -e "\n${GREEN}  ✓ Tamamlandı!${NC}"
echo -e "  Başarılı: ${GREEN}$(format_num $SUCCESS)${NC} / Başarısız: ${RED}$(format_num $FAILED)${NC}"
echo -e "  Toplam süre: ${BLUE}${DURATION_SEC}s${NC}"
echo -e "  Ortalama CGI yürütme: ${BLUE}${AVG_TIME}ms${NC}" | tee -a "$STATS_FILE"

# ════════════════════════════════════════════════════════════════
# TEST 6: Error Handling (404 Requests)
# ════════════════════════════════════════════════════════════════
echo -e "\n${YELLOW}[TEST 6] Hata Yönetimi (404 İstekleri)${NC}"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" | tee -a "$STATS_FILE"

START_TIME=$(date +%s%N)
CORRECT_404=0
TOTAL=500

for i in $(seq 1 $TOTAL); do
    STATUS=$(curl -s -o /dev/null -w "%{http_code}" "$HOST:$PORT_1/nonexistent_$i.html")
    if [ "$STATUS" = "404" ]; then
        ((CORRECT_404++))
    fi
    
    if [ $((i % 100)) -eq 0 ]; then
        echo -ne "\r  ✓ İşlenen: $i/$TOTAL istekler"
    fi
done

END_TIME=$(date +%s%N)
DURATION_MS=$(( (END_TIME - START_TIME) / 1000000 ))
DURATION_SEC=$(awk "BEGIN {printf \"%.2f\", $DURATION_MS / 1000}")

echo -e "\n${GREEN}  ✓ Tamamlandı!${NC}"
echo -e "  Doğru 404 cevapları: ${GREEN}$(format_num $CORRECT_404)${NC}/$TOTAL"
echo -e "  Toplam süre: ${BLUE}${DURATION_SEC}s${NC}" | tee -a "$STATS_FILE"

# ════════════════════════════════════════════════════════════════
# TEST 7: Slow Client Simulation
# ════════════════════════════════════════════════════════════════
echo -e "\n${YELLOW}[TEST 7] Yavaş Client Simulasyonu${NC}"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" | tee -a "$STATS_FILE"

echo "  50 eşzamanlı yavaş istek yapılıyor..."

START_TIME=$(date +%s%N)
SUCCESS=0
FAILED=0

for i in $(seq 1 50); do
    (
        timeout 30s curl -s -o /dev/null -w "%{http_code}" \
            -m 30 "$HOST:$PORT_1/large_test.bin" && echo "OK" || echo "TIMEOUT"
    ) &
    
    if [ $((i % 10)) -eq 0 ]; then
        echo -ne "\r  ✓ Başlatılan: $i/50 istek"
    fi
done

wait

END_TIME=$(date +%s%N)
DURATION_MS=$(( (END_TIME - START_TIME) / 1000000 ))
DURATION_SEC=$(awk "BEGIN {printf \"%.2f\", $DURATION_MS / 1000}")

echo -e "\n${GREEN}  ✓ Tamamlandı!${NC}"
echo -e "  Toplam süre: ${BLUE}${DURATION_SEC}s${NC}" | tee -a "$STATS_FILE"

# ════════════════════════════════════════════════════════════════
# TEST 8: Mixed Load Test
# ════════════════════════════════════════════════════════════════
echo -e "\n${YELLOW}[TEST 8] Karışık Yük Testi (30s)${NC}"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" | tee -a "$STATS_FILE"

START_TIME=$(date +%s%N)
REQUEST_COUNT=0
SUCCESS_COUNT=0

echo "  Farklı türde istekler gönderiliyor..."

for i in {1..30}; do
    # GET request
    (curl -s -o /dev/null "$HOST:$PORT_1/" && echo "OK" || echo "FAIL") > /dev/null &
    ((REQUEST_COUNT++))
    
    # GET with query
    (curl -s -o /dev/null "$HOST:$PORT_1/cgi/echo.py?test=$i" && echo "OK" || echo "FAIL") > /dev/null &
    ((REQUEST_COUNT++))
    
    # POST request
    (curl -s -o /dev/null -F "file=@/tmp/test_upload.txt" "$HOST:$PORT_1/upload" && echo "OK" || echo "FAIL") > /dev/null &
    ((REQUEST_COUNT++))
    
    if [ $((i % 5)) -eq 0 ]; then
        echo -ne "\r  ✓ Gönderilen istek grupları: $i/30"
    fi
done

wait

END_TIME=$(date +%s%N)
DURATION_MS=$(( (END_TIME - START_TIME) / 1000000 ))
DURATION_SEC=$(awk "BEGIN {printf \"%.2f\", $DURATION_MS / 1000}")

echo -e "\n${GREEN}  ✓ Tamamlandı!${NC}"
echo -e "  Toplam istekler: ${BLUE}$(format_num $REQUEST_COUNT)${NC}"
echo -e "  Toplam süre: ${BLUE}${DURATION_SEC}s${NC}"
echo -e "  İstek/saniye: ${BLUE}$(awk "BEGIN {printf \"%.0f\", $REQUEST_COUNT / ($DURATION_MS / 1000)}")${NC}" | tee -a "$STATS_FILE"

# ════════════════════════════════════════════════════════════════
# Summary and System Info
# ════════════════════════════════════════════════════════════════
echo -e "\n${BLUE}════════════════════════════════════════════════════════════════${NC}"
echo -e "${BLUE}         SİSTEM BİLGİSİ VE ÖZET${NC}"
echo -e "${BLUE}════════════════════════════════════════════════════════════════${NC}"

echo -e "\n${YELLOW}İşlemci Kullanımı:${NC}"
ps aux | grep webserv | grep -v grep | awk '{print "  CPU: " $3 "% | MEM: " $4 "%"}'

echo -e "\n${YELLOW}Bellek Bilgisi:${NC}"
free -h | awk 'NR==2 {print "  Toplam: " $2 " | Kullanılan: " $3 " | Boş: " $4}'

echo -e "\n${YELLOW}Ağ Bağlantıları:${NC}"
netstat -an 2>/dev/null | grep ESTABLISHED | grep -E ":(8080|8081)" | wc -l | awk '{print "  Aktif bağlantılar: " $1}'

echo -e "\n${YELLOW}Açık Dosyalar:${NC}"
lsof -p $(pgrep webserv) 2>/dev/null | grep -E "TCP|UDP" | wc -l | awk '{print "  Açık soketler: " $1}'

echo -e "\n${BLUE}════════════════════════════════════════════════════════════════${NC}"
echo -e "${GREEN}✓ Stress test tamamlandı!${NC}"
echo -e "${BLUE}════════════════════════════════════════════════════════════════${NC}\n"

# Display full results
echo -e "\n${YELLOW}Detaylı Sonuçlar:${NC}"
cat "$STATS_FILE"
