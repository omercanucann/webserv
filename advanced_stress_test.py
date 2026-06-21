#!/usr/bin/env python3

import requests
import time
import threading
import json
import sys
from urllib.parse import urljoin
from statistics import mean, median, stdev
from datetime import datetime
import subprocess

class WebServStressTest:
    def __init__(self, host="http://localhost", port_1=8080, port_2=8081):
        self.host = host
        self.port_1 = port_1
        self.port_2 = port_2
        self.base_url_1 = f"{host}:{port_1}"
        self.base_url_2 = f"{host}:{port_2}"
        self.results = {}
        self.lock = threading.Lock()
        
    def colored(self, text, color):
        colors = {
            'green': '\033[92m',
            'red': '\033[91m',
            'yellow': '\033[93m',
            'blue': '\033[94m',
            'reset': '\033[0m'
        }
        return f"{colors.get(color, '')}{text}{colors['reset']}"
    
    def print_header(self, title):
        print(f"\n{self.colored('═' * 70, 'blue')}")
        print(f"{self.colored(f'  {title}', 'blue')}")
        print(f"{self.colored('═' * 70, 'blue')}\n")
    
    def test_response_times(self, num_requests=100):
        """Test response times for GET requests"""
        self.print_header("TEST 1: Response Time Analysis")
        
        response_times = []
        errors = 0
        
        print(f"Sending {num_requests} sequential GET requests...")
        
        for i in range(num_requests):
            try:
                start = time.time()
                response = requests.get(f"{self.base_url_1}/", timeout=10)
                elapsed = (time.time() - start) * 1000
                
                response_times.append(elapsed)
                
                if i % 20 == 0:
                    print(f"  ✓ {i}/{num_requests} requests completed")
                    
            except Exception as e:
                errors += 1
        
        if response_times:
            print(f"\n{self.colored('Results:', 'green')}")
            print(f"  Total requests: {num_requests}")
            print(f"  Successful: {self.colored(num_requests - errors, 'green')}")
            print(f"  Failed: {self.colored(errors, 'red')}")
            print(f"  Min response time: {self.colored(f'{min(response_times):.2f}ms', 'blue')}")
            print(f"  Max response time: {self.colored(f'{max(response_times):.2f}ms', 'blue')}")
            print(f"  Avg response time: {self.colored(f'{mean(response_times):.2f}ms', 'blue')}")
            print(f"  Median response time: {self.colored(f'{median(response_times):.2f}ms', 'blue')}")
            if len(response_times) > 1:
                print(f"  Std deviation: {self.colored(f'{stdev(response_times):.2f}ms', 'blue')}")
            
            self.results['response_times'] = {
                'min': min(response_times),
                'max': max(response_times),
                'avg': mean(response_times),
                'median': median(response_times),
                'total': num_requests,
                'successful': num_requests - errors
            }
    
    def test_concurrent_connections(self, num_concurrent=50, num_requests=5):
        """Test concurrent connections"""
        self.print_header(f"TEST 2: Concurrent Connections ({num_concurrent} parallel)")
        
        response_times = []
        errors = 0
        total = num_concurrent * num_requests
        
        def make_request():
            try:
                start = time.time()
                response = requests.get(f"{self.base_url_1}/index.html", timeout=10)
                elapsed = (time.time() - start) * 1000
                with self.lock:
                    response_times.append(elapsed)
            except Exception as e:
                with self.lock:
                    errors += 1
        
        print(f"Sending {total} requests with {num_concurrent} concurrent connections...")
        
        start_time = time.time()
        threads = []
        
        for batch in range(num_requests):
            for i in range(num_concurrent):
                t = threading.Thread(target=make_request)
                t.start()
                threads.append(t)
            
            for t in threads:
                t.join()
            
            threads = []
            print(f"  ✓ Batch {batch + 1}/{num_requests} completed")
        
        total_time = time.time() - start_time
        
        print(f"\n{self.colored('Results:', 'green')}")
        print(f"  Total requests: {total}")
        print(f"  Successful: {self.colored(total - errors, 'green')}")
        print(f"  Failed: {self.colored(errors, 'red')}")
        print(f"  Total time: {self.colored(f'{total_time:.2f}s', 'blue')}")
        print(f"  Requests/sec: {self.colored(f'{total/total_time:.0f}', 'blue')}")
        
        if response_times:
            print(f"  Avg response time: {self.colored(f'{mean(response_times):.2f}ms', 'blue')}")
        
        self.results['concurrent'] = {
            'total': total,
            'successful': total - errors,
            'total_time': total_time,
            'rps': total / total_time
        }
    
    def test_connection_limits(self, max_connections=100):
        """Test maximum concurrent connections"""
        self.print_header(f"TEST 3: Connection Limit Test ({max_connections} connections)")
        
        threads = []
        successful = 0
        failed = 0
        connection_times = []
        
        def maintain_connection():
            nonlocal successful, failed
            try:
                start = time.time()
                response = requests.get(f"{self.base_url_1}/", timeout=5)
                elapsed = time.time() - start
                with self.lock:
                    successful += 1
                    connection_times.append(elapsed * 1000)
            except Exception as e:
                with self.lock:
                    failed += 1
        
        print(f"Attempting to establish {max_connections} connections...")
        
        for i in range(max_connections):
            t = threading.Thread(target=maintain_connection)
            t.start()
            threads.append(t)
            
            if i % 20 == 0:
                print(f"  ✓ {i}/{max_connections} connections attempted")
        
        for t in threads:
            t.join()
        
        print(f"\n{self.colored('Results:', 'green')}")
        print(f"  Successful connections: {self.colored(successful, 'green')}")
        print(f"  Failed connections: {self.colored(failed, 'red')}")
        print(f"  Success rate: {self.colored(f'{(successful/max_connections)*100:.1f}%', 'blue')}")
        
        if connection_times:
            print(f"  Avg connection time: {self.colored(f'{mean(connection_times):.2f}ms', 'blue')}")
        
        self.results['connections'] = {
            'successful': successful,
            'failed': failed,
            'success_rate': (successful / max_connections) * 100
        }
    
    def test_error_handling(self, num_requests=200):
        """Test error handling (404, 405, etc)"""
        self.print_header("TEST 4: Error Handling")
        
        status_codes = {}
        
        print(f"Testing error handling with {num_requests} requests...")
        
        # 404 errors
        for i in range(num_requests // 2):
            try:
                response = requests.get(f"{self.base_url_1}/nonexistent_{i}.html", timeout=5)
                status = response.status_code
                status_codes[status] = status_codes.get(status, 0) + 1
            except:
                pass
            
            if i % 50 == 0:
                print(f"  ✓ {i}/{num_requests // 2} error requests processed")
        
        # 405 errors (POST on restricted location)
        for i in range(num_requests // 2):
            try:
                response = requests.post(f"{self.base_url_1}/", timeout=5)
                status = response.status_code
                status_codes[status] = status_codes.get(status, 0) + 1
            except:
                pass
        
        print(f"\n{self.colored('Results:', 'green')}")
        for status, count in sorted(status_codes.items()):
            print(f"  Status {status}: {self.colored(count, 'blue')} responses")
        
        self.results['errors'] = status_codes
    
    def test_resource_limits(self):
        """Test resource usage"""
        self.print_header("TEST 5: System Resource Usage")
        
        try:
            # Get process info
            pid = subprocess.check_output(['pgrep', 'webserv']).decode().strip().split('\n')[0]
            
            # Get CPU and memory usage
            ps_output = subprocess.check_output(
                ['ps', 'aux'],
            ).decode()
            
            for line in ps_output.split('\n'):
                if 'webserv' in line and 'grep' not in line:
                    parts = line.split()
                    if len(parts) >= 6:
                        cpu = float(parts[2])
                        mem = float(parts[3])
                        
                        print(f"  CPU Usage: {self.colored(f'{cpu}%', 'blue')}")
                        print(f"  Memory Usage: {self.colored(f'{mem}%', 'blue')}")
                        
                        self.results['resources'] = {
                            'cpu': cpu,
                            'memory': mem
                        }
                        break
            
            # Check open files
            try:
                lsof_output = subprocess.check_output(
                    ['lsof', '-p', pid]
                ).decode()
                open_sockets = lsof_output.count('TCP') + lsof_output.count('UDP')
                print(f"  Open sockets: {self.colored(open_sockets, 'blue')}")
            except:
                print("  Open sockets: N/A")
                
        except Exception as e:
            print(f"  {self.colored(f'Could not retrieve system info: {e}', 'red')}")
    
    def generate_html_report(self, filename='/tmp/webserv_stress_report.html'):
        """Generate HTML report"""
        self.print_header("Generating HTML Report")
        
        html_content = f"""
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>WebServ Stress Test Report</title>
    <style>
        * {{ margin: 0; padding: 0; box-sizing: border-box; }}
        body {{
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            padding: 20px;
        }}
        .container {{
            max-width: 1200px;
            margin: 0 auto;
            background: white;
            border-radius: 10px;
            box-shadow: 0 10px 40px rgba(0, 0, 0, 0.3);
            overflow: hidden;
        }}
        .header {{
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 40px;
            text-align: center;
        }}
        .header h1 {{
            font-size: 2.5em;
            margin-bottom: 10px;
        }}
        .header p {{
            font-size: 1.1em;
            opacity: 0.9;
        }}
        .content {{
            padding: 40px;
        }}
        .test-section {{
            margin-bottom: 40px;
            border-left: 4px solid #667eea;
            padding-left: 20px;
        }}
        .test-section h2 {{
            color: #333;
            margin-bottom: 15px;
            font-size: 1.5em;
        }}
        .metric {{
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
            gap: 20px;
            margin-bottom: 20px;
        }}
        .metric-card {{
            background: #f8f9fa;
            padding: 20px;
            border-radius: 5px;
            border: 1px solid #dee2e6;
        }}
        .metric-label {{
            color: #666;
            font-size: 0.9em;
            margin-bottom: 5px;
        }}
        .metric-value {{
            color: #667eea;
            font-size: 1.8em;
            font-weight: bold;
        }}
        .success {{ color: #28a745; }}
        .error {{ color: #dc3545; }}
        .warning {{ color: #ffc107; }}
        .info {{ color: #17a2b8; }}
        table {{
            width: 100%;
            border-collapse: collapse;
            margin-top: 20px;
        }}
        table thead {{
            background: #667eea;
            color: white;
        }}
        table th {{
            padding: 12px;
            text-align: left;
        }}
        table td {{
            padding: 12px;
            border-bottom: 1px solid #dee2e6;
        }}
        table tr:hover {{
            background: #f8f9fa;
        }}
        .footer {{
            background: #f8f9fa;
            padding: 20px;
            text-align: center;
            color: #666;
            border-top: 1px solid #dee2e6;
        }}
        .summary {{
            background: #e7f3ff;
            border-left: 4px solid #2196F3;
            padding: 20px;
            margin-bottom: 20px;
            border-radius: 5px;
        }}
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>🚀 WebServ Stress Test Report</h1>
            <p>Comprehensive Performance Analysis</p>
            <p style="font-size: 0.9em; margin-top: 10px;">{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}</p>
        </div>
        
        <div class="content">
            <div class="summary">
                <h3>📊 Executive Summary</h3>
                <p>This stress test suite evaluated the performance of the WebServ HTTP server across multiple scenarios including sequential requests, concurrent connections, error handling, and resource utilization.</p>
            </div>
"""
        
        # Test 1: Response Times
        if 'response_times' in self.results:
            rt = self.results['response_times']
            html_content += f"""
            <div class="test-section">
                <h2>Test 1: Response Time Analysis</h2>
                <p>Sequential GET requests to measure response times and consistency.</p>
                <div class="metric">
                    <div class="metric-card">
                        <div class="metric-label">Total Requests</div>
                        <div class="metric-value">{rt['total']}</div>
                    </div>
                    <div class="metric-card">
                        <div class="metric-label">Successful</div>
                        <div class="metric-value success">{rt['successful']}</div>
                    </div>
                    <div class="metric-card">
                        <div class="metric-label">Min Response</div>
                        <div class="metric-value">{rt['min']:.2f}ms</div>
                    </div>
                    <div class="metric-card">
                        <div class="metric-label">Max Response</div>
                        <div class="metric-value">{rt['max']:.2f}ms</div>
                    </div>
                    <div class="metric-card">
                        <div class="metric-label">Avg Response</div>
                        <div class="metric-value info">{rt['avg']:.2f}ms</div>
                    </div>
                    <div class="metric-card">
                        <div class="metric-label">Median Response</div>
                        <div class="metric-value">{rt['median']:.2f}ms</div>
                    </div>
                </div>
            </div>
"""
        
        # Test 2: Concurrent
        if 'concurrent' in self.results:
            cc = self.results['concurrent']
            html_content += f"""
            <div class="test-section">
                <h2>Test 2: Concurrent Connections</h2>
                <p>Multiple simultaneous connections to test concurrency handling.</p>
                <div class="metric">
                    <div class="metric-card">
                        <div class="metric-label">Total Requests</div>
                        <div class="metric-value">{cc['total']}</div>
                    </div>
                    <div class="metric-card">
                        <div class="metric-label">Successful</div>
                        <div class="metric-value success">{cc['successful']}</div>
                    </div>
                    <div class="metric-card">
                        <div class="metric-label">Total Time</div>
                        <div class="metric-value">{cc['total_time']:.2f}s</div>
                    </div>
                    <div class="metric-card">
                        <div class="metric-label">Requests/sec</div>
                        <div class="metric-value info">{cc['rps']:.0f}</div>
                    </div>
                </div>
            </div>
"""
        
        # Test 3: Connections
        if 'connections' in self.results:
            cn = self.results['connections']
            html_content += f"""
            <div class="test-section">
                <h2>Test 3: Connection Limits</h2>
                <p>Testing maximum concurrent connection handling capacity.</p>
                <div class="metric">
                    <div class="metric-card">
                        <div class="metric-label">Successful</div>
                        <div class="metric-value success">{cn['successful']}</div>
                    </div>
                    <div class="metric-card">
                        <div class="metric-label">Failed</div>
                        <div class="metric-value error">{cn['failed']}</div>
                    </div>
                    <div class="metric-card">
                        <div class="metric-label">Success Rate</div>
                        <div class="metric-value">{cn['success_rate']:.1f}%</div>
                    </div>
                </div>
            </div>
"""
        
        # Test 4: Errors
        if 'errors' in self.results:
            errors = self.results['errors']
            html_content += """
            <div class="test-section">
                <h2>Test 4: Error Handling</h2>
                <p>Testing proper HTTP error response handling.</p>
                <table>
                    <thead>
                        <tr>
                            <th>Status Code</th>
                            <th>Count</th>
                        </tr>
                    </thead>
                    <tbody>
"""
            for status, count in sorted(errors.items()):
                html_content += f"<tr><td>{status}</td><td>{count}</td></tr>"
            html_content += """
                    </tbody>
                </table>
            </div>
"""
        
        # Test 5: Resources
        if 'resources' in self.results:
            res = self.results['resources']
            html_content += f"""
            <div class="test-section">
                <h2>Test 5: System Resources</h2>
                <p>Resource consumption during stress testing.</p>
                <div class="metric">
                    <div class="metric-card">
                        <div class="metric-label">CPU Usage</div>
                        <div class="metric-value">{res['cpu']:.2f}%</div>
                    </div>
                    <div class="metric-card">
                        <div class="metric-label">Memory Usage</div>
                        <div class="metric-value">{res['memory']:.2f}%</div>
                    </div>
                </div>
            </div>
"""
        
        html_content += """
        </div>
        
        <div class="footer">
            <p>WebServ Stress Test Report | Generated automatically</p>
        </div>
    </div>
</body>
</html>
"""
        
        with open(filename, 'w') as f:
            f.write(html_content)
        
        print(f"  ✓ HTML report saved to: {self.colored(filename, 'green')}")
        
        # Also save JSON
        json_file = filename.replace('.html', '.json')
        with open(json_file, 'w') as f:
            json.dump(self.results, f, indent=2, default=str)
        
        print(f"  ✓ JSON report saved to: {self.colored(json_file, 'green')}")
    
    def run_all_tests(self):
        """Run all stress tests"""
        print(f"\n{self.colored('╔' + '═' * 68 + '╗', 'blue')}")
        print(f"{self.colored('║' + ' ' * 10 + 'WebServ Advanced Stress Test Suite' + ' ' * 24 + '║', 'blue')}")
        print(f"{self.colored('╚' + '═' * 68 + '╝', 'blue')}\n")
        
        try:
            self.test_response_times(100)
            self.test_concurrent_connections(50, 5)
            self.test_connection_limits(100)
            self.test_error_handling(200)
            self.test_resource_limits()
            self.generate_html_report()
            
            print(f"\n{self.colored('✓ All stress tests completed successfully!', 'green')}\n")
            
        except KeyboardInterrupt:
            print(f"\n{self.colored('✗ Test interrupted by user', 'red')}\n")
        except Exception as e:
            print(f"\n{self.colored(f'✗ Error during testing: {e}', 'red')}\n")

if __name__ == '__main__':
    tester = WebServStressTest()
    tester.run_all_tests()
