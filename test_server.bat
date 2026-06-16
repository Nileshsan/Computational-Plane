@echo off
setlocal

echo Testing C++ Server Health Endpoint...
curl http://127.0.0.1:8888/health

echo.
echo Testing Analysis Endpoint...
curl -X POST http://127.0.0.1:8888/api/analyze-payment-behavior echo   -H "Content-Type: application/json" echo   -d "{\"company_id\": 1, \"company_name\": \"Test\", \"clients\": [{\"client_name\": \"Client A\", \"payment_days\": [8,9,7], \"on_time_count\": 3, \"outstanding_amount\": 100000, \"transaction_count\": 3}]}"
