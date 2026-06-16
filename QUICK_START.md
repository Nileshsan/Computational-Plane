# C++ Server - Quick Reference

## Quick Start (5 minutes)

### 1. Build the Server

```powershell
cd C:\Users\Admin\Nilesh_Projects\PBS_Finsight\Backend\C++_server
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

### 2. Run the Server

```powershell
# From build directory
.\Release\http_server.exe

# Or use full path
C:\Users\Admin\Nilesh_Projects\PBS_Finsight\Backend\C++_server\build\Release\http_server.exe
```

### 3. Verify It's Running

```bash
curl http://127.0.0.1:9000/health
# Response: {"status": "healthy"}
```

### 4. Test with Sample Request

```bash
curl -X POST http://127.0.0.1:9000/api/analyze-payment-behavior \
  -H "Content-Type: application/json" \
  -d '{
    "company_id": 1,
    "company_name": "Test Company",
    "clients": [{
      "client_name": "Test Client",
      "payment_days": [8, 9, 7],
      "on_time_count": 3,
      "outstanding_amount": 100000,
      "transaction_count": 3
    }]
  }'
```

## Two Server Options

| Feature | HTTP Server | Worker Pool |
|---------|------------|------------|
| **Executable** | `http_server.exe` | `payment_server.exe` |
| **Port** | 9000 | Configurable |
| **Model** | Thread-per-connection | Fixed worker pool |
| **Concurrency** | 100+ simultaneous | 8 workers (configurable) |
| **Best For** | Django integration | Batch processing |
| **Queue** | Connection queue | Request queue (1000 max) |
| **Metrics** | Per-request logging | Aggregated metrics |

## Key Features

### HTTP Server (Recommended for Django)

✅ Real HTTP server with socket support
✅ Handles multiple concurrent clients
✅ JSON request/response parsing
✅ Per-request logging
✅ Error handling for malformed requests
✅ Health check endpoint
✅ Stateless design

### Worker Pool Server

✅ Multi-threaded worker pool (8 workers)
✅ Request queue for batching
✅ Performance metrics collection
✅ Backpressure handling
✅ Queue depth monitoring
✅ Timeout protection (300s)

## API Endpoints

### POST /api/analyze-payment-behavior

**Input**: Company data with clients and payment history
**Output**: Classification, risk assessment, cashflow forecast

### GET /health

**Output**: Server status

## Request Format

```json
{
  "company_id": 1,
  "company_name": "Company Name",
  "clients": [
    {
      "client_name": "Client Name",
      "payment_days": [8, 9, 7, 10, 8],
      "on_time_count": 5,
      "outstanding_amount": 500000,
      "transaction_count": 5
    }
  ]
}
```

## Response Format

```json
{
  "request_id": "REQ-1",
  "company_id": 1,
  "status": "success",
  "client_analysis": {
    "Client Name": {
      "classification": "A",
      "avg_payment_days": 8.4,
      "risk_level": "low",
      "outstanding": 500000
    }
  },
  "classifications": {"Client Name": "A"},
  "cashflow_30_days": 350000,
  "cashflow_60_days": 425000,
  "cashflow_90_days": 500000,
  "total_outstanding": 500000,
  "processed_by": "cpp_server"
}
```

## Client Classification

| Classification | Avg Payment Days | Risk Level | Description |
|---|---|---|---|
| **A** | ≤ 10 days | Low | Excellent payer |
| **B** | 11-25 days | Medium | Good payer |
| **C** | 26-35 days | High | Moderate payer |
| **D** | > 35 days | Critical | Risky payer |

## Integration Steps

1. **Start Django**: `python manage.py runserver`
2. **Start C++ Server**: `http_server.exe`
3. **Import Transactions**: Signals trigger automatically
4. **Check Results**: View in CppServerLog table or API

## Environment Variables (Django)

```python
# settings.py
CPP_SERVER_HOST = "127.0.0.1"
CPP_SERVER_PORT = 9000
CPP_SERVER_ENDPOINT = "/api/analyze-payment-behavior"
CPP_REQUEST_TIMEOUT = 300
CPP_MAX_RETRIES = 3
```

## Monitoring

### Check Server Status
```bash
curl http://127.0.0.1:9000/health
```

### View Logs
- Server logs to console (stdout/stderr)
- Django logs requests/responses
- CppServerLog table tracks all communications

### Common Log Messages
```
[SERVER] Listening on http://127.0.0.1:9000
[ACCEPT] New connection from 127.0.0.1:XXXX
[ANALYSIS_1] Company: Test Company (ID: 1)
[ANALYSIS_1]   Client: Client A - A (low)
```

## Troubleshooting

### Port 9000 Already in Use
```powershell
netstat -ano | findstr :9000
taskkill /PID <PID> /F
```

### Build Fails
```powershell
# Clean and rebuild
rmdir /s build
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

### Connection Refused
1. Check C++ server is running
2. Verify port is 9000
3. Check Windows Firewall allows local connections

### JSON Parse Errors
1. Verify payload has all required fields
2. Check Content-Type header is application/json
3. Validate JSON syntax

## Performance Tips

✓ Use HTTP server for Django integration (default)
✓ For batch: use payment_server.exe
✓ Max 8 concurrent workers (configurable)
✓ Queue size: 1000 requests
✓ Process time: ~50-100ms per client
✓ Can handle 100+ simultaneous connections

## Files Modified

| File | Changes |
|------|---------|
| `CMakeLists.txt` | Added HTTP server build target |
| `src/http_server.cpp` | NEW: Real HTTP server (1000 lines) |
| `src/payment_server.cpp` | NEW: Worker pool server (600 lines) |

## Django Integration Checklist

- [ ] C++ server running on port 9000
- [ ] Django server running on port 8000
- [ ] Transactions being imported
- [ ] Signals triggering payment analysis
- [ ] CppServerLog showing requests
- [ ] Responses returning from C++ server

## Next: Production Deployment

1. Run http_server.exe as Windows Service
2. Monitor with health check endpoint
3. Configure firewall for port 9000
4. Set up log rotation
5. Enable performance monitoring
6. Test with production data volume

## Support

See `CPP_HTTP_SERVER_INTEGRATION.md` for detailed documentation.
