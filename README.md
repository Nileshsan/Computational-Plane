# PBS Finsight C++ Processing Server

A high-performance gRPC-based processing server that analyzes financial transaction data and generates insights.

## Features

- **Payment Pattern Analysis**: Analyzes party payment behavior using FIFO matching algorithm
- **Fixed Expense Detection**: Identifies recurring expenses and payment patterns
- **Cashflow Prediction**: Predicts future cashflow with confidence intervals
- **Client Categorization**: Categorizes clients (VIP, Regular, At-Risk, Inactive) and suggests discounts
- **gRPC Service**: Fast, language-agnostic communication with Django backend

## Architecture

### Core Components

```
src/
  ├── main.cpp                 # Server entry point and gRPC setup
  ├── service_impl.cpp        # ProcessingService RPC handler implementation
  ├── payment_analyzer.cpp    # Payment pattern analysis engine
  ├── expense_analyzer.cpp    # Fixed expense detection engine
  ├── cashflow_predictor.cpp  # Cashflow prediction engine
  └── client_categorizer.cpp  # Client classification and discount logic

include/
  ├── data_structures.h       # Transaction, Pattern, Expense, Prediction data models
  ├── payment_analyzer.h      # PaymentAnalyzer class definition
  ├── expense_analyzer.h      # ExpenseAnalyzer class definition
  ├── cashflow_predictor.h    # CashflowPredictor class definition
  ├── client_categorizer.h    # ClientCategorizer class definition
  └── service_impl.h          # ProcessingServiceImpl class definition

grpc/
  └── protos/
      └── processing.proto    # gRPC service and message definitions
```

### Data Flow

```
Django Agent (sends Tally data)
    ↓
Django Backend (ingest_tally_data, receive_closing_balances)
    ↓
Processing Trigger (trigger_processing_on_new_data)
    ↓
gRPC Client (grpc_client.py)
    ↓
C++ ProcessingService (port 50051)
    ├─→ PaymentAnalyzer (payment patterns)
    ├─→ ExpenseAnalyzer (fixed expenses)
    ├─→ CashflowPredictor (predictions)
    └─→ ClientCategorizer (client segments + discounts)
    ↓
ComputeResponse (JSON results)
    ↓
gRPC Result Mapper (grpc_result_mapper.py)
    ↓
Django ComputationResult (database)
    ↓
Mobile App (reads pre-computed insights)
```

## Building the Server

### Prerequisites

```bash
# Ubuntu/Debian
sudo apt-get install build-essential cmake git

# macOS
brew install cmake

# Install gRPC and protobuf (recommended: use vcpkg or system package manager)
# For Windows: vcpkg install grpc:x64-windows protobuf:x64-windows
```

### Build Steps

```bash
cd Backend/C++_server
mkdir build
cd build
cmake ..
make

# Run tests (optional)
./processing_tests
```

### Run Server

```bash
./processing_server
# Output: ProcessingService listening on 0.0.0.0:50051
```

## API Reference

### ExecuteStep RPC

Execute a single computation step on the C++ server.

**Request:**
```json
{
  "job_id": "uuid-correlation-id",
  "company_id": "123",
  "step": "payment-patterns",  // One of: data-loading, payment-patterns, fixed-expenses, cashflow-setup, full-pipeline
  "input": {
    "transactions": [...],
    "since_days": 30
  }
}
```

**Response:**
```json
{
  "success": true,
  "result": {
    "patterns": [...],
    "total_parties": 15,
    "avg_confidence": 0.72
  },
  "error_message": "",
  "execution_time_ms": 145,
  "logs": [
    {"level": "info", "message": "Analysis started"}
  ]
}
```

### ExecutePipeline RPC

Execute the full analysis pipeline.

**Request:**
```json
{
  "job_id": "uuid",
  "company_id": "123",
  "step": "full-pipeline",
  "input": {
    "transactions": [...],
    "bank_balance": 50000.00
  }
}
```

**Response:**
```json
{
  "success": true,
  "result": {
    "pipeline_status": "completed",
    "results": {
      "payment_patterns": {...},
      "fixed_expenses": {...},
      "cashflow": {...},
      "client_categories": {...}
    },
    "steps_completed": 5
  },
  "execution_time_ms": 450
}
```

## Key Algorithms

### 1. Payment Pattern Analysis (FIFO Matching)

**Algorithm:**
1. Group transactions by party name
2. Separate into sales and receipts
3. Sort each group by date
4. Match sales to receipts using FIFO (First-In-First-Out)
5. Calculate weighted average payment delay
6. Compute standard deviation and confidence score

**Output:**
- `avg_payment_days`: Average delay between invoice and payment
- `delay_std_deviation`: Variability in payment timing
- `pattern_consistency`: Score 0-1 indicating reliability
- `confidence_score`: Confidence in the analysis based on sample size

**Formula for Confidence:**
```
confidence = min(sample_size / 10, 1.0) * pattern_consistency
```

### 2. Fixed Expense Detection

**Algorithm:**
1. Filter for outgoing payments (payment register type)
2. Cluster transactions by rounded amount (to nearest 10)
3. For each cluster with ≥3 occurrences:
   - Calculate average amount and interval between payments
   - Compute standard deviation of intervals
   - Assess pattern consistency
4. If `std_interval < avg_interval * 0.2`, classify as fixed expense

**Output:**
- `description`: Expense name
- `amount`: Average amount paid
- `interval_days`: Average days between payments
- `frequency`: "monthly", "quarterly", "yearly"
- `next_date`: Predicted next payment date
- `pattern_consistency`: 0-1 score

### 3. Cashflow Prediction

**Algorithm:**
1. Get current bank balance
2. For each day N in prediction range (e.g., next 30 days):
   - Calculate expected receipts:
     - For each outstanding sale by party P:
       - Expected date = sale_date + avg_payment_days[P]
       - Amount = sale amount
       - Confidence = pattern_confidence[P]
   - Calculate expected expenses:
     - For each fixed expense, check if due on day N
     - Add amount if due
   - Update balance: `balance[N] = balance[N-1] + receipts - expenses`
   - Apply confidence intervals (min/max bounds)
3. Return array of daily predictions

**Confidence Bands:**
```
uncertainty = Σ(std_dev[i] / (avg_days[i] + 1))
band = |predicted_balance| × 0.1 × uncertainty
min_balance = predicted_balance - band
max_balance = predicted_balance + band
```

### 4. Client Categorization

**Algorithm:**
1. Calculate metrics for each party:
   - Total transaction value
   - Transaction count
   - Days since last transaction
   - On-time vs late payment ratio
   - Average transaction value
2. Assign category:
   - **VIP**: < 30 days inactive, reliability > 0.9, avg transaction > 5000
   - **Regular**: Normal activity, reliability 0.6-0.9
   - **At-Risk**: > 90 days inactive OR reliability < 0.6
   - **Inactive**: > 180 days no activity
3. Calculate discount eligibility:
   - **VIP**: 5-10% discount (based on reliability)
   - **Regular (high reliability)**: 2-3% discount
   - **At-Risk**: 1% early-payment discount
   - **Inactive**: No discount

## Configuration

Set environment variables to customize gRPC client behavior:

```bash
# Django side (grpc_client.py)
export CPP_SERVICE_HOST=localhost        # C++ server host
export CPP_SERVICE_PORT=50051            # C++ server port
export CPP_SERVICE_TIMEOUT=300           # RPC timeout in seconds
export CPP_SERVICE_MAX_RETRIES=3         # Retry attempts on failure
```

## Testing

### Unit Tests

```bash
cd build
./processing_tests
```

Tests cover:
- Payment delay calculation
- Pattern consistency scoring
- Expense clustering
- Cashflow daily calculations
- Client categorization logic

### Integration Testing

Test the full flow:

```bash
# Terminal 1: Start C++ server
./processing_server

# Terminal 2: Run Django management command
python manage.py test transactions.tests.TestGrpcIntegration

# Or trigger via API
curl -X POST http://localhost:8000/api/transactions/ingest-tally \
  -H "Authorization: Token YOUR_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"type": "vouchers", "vouchers": [...]}'
```

## Performance Tuning

### Memory Management
- Uses smart pointers for automatic cleanup
- Transaction data stored in vectors for efficient iteration
- Pattern cache for repeated lookups

### Optimization Opportunities
- Multi-threaded processing for large datasets (future)
- SIMD for statistical calculations (future)
- Caching of analysis results in Redis (future)

## Troubleshooting

### Connection Refused
```
Error: Connection to C++ service at localhost:50051 failed
```
**Solution:** Ensure C++ server is running: `./processing_server`

### Timeout
```
Error: gRPC deadline exceeded
```
**Solution:** Increase timeout or optimize algorithm:
- Reduce `since_days` parameter
- Check server logs for bottlenecks
- Increase `CPP_SERVICE_TIMEOUT`

### Deadlock in Payment Matching
```
Deadlock found when trying to get lock; try restarting transaction
```
**Solution:** Retry logic is built-in (max 3 attempts with exponential backoff)

## Future Enhancements

1. **Streaming Results**: Stream large result sets to avoid memory bloat
2. **Async Processing**: Use thread pool for parallel analysis
3. **ML Models**: Integrate ML for payment delay prediction
4. **Discounting Engine**: Advanced discount optimization
5. **Export APIs**: CSV, PDF report generation
6. **Webhooks**: Real-time notifications on cash shortfalls
7. **Multi-language Support**: Generate stubs for Go, Node.js, Python

## License

© PBS Finsight 2026
