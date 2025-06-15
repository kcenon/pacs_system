# PACS System Monitoring Guide

## Overview

The PACS System includes comprehensive real-time monitoring capabilities for tracking system health, performance, and operational metrics.

## Architecture

### Components

1. **Monitoring Service**: Core metric collection and storage
2. **Dashboard Service**: Web-based visualization
3. **Alert Manager**: Threshold-based alerting
4. **Health Checks**: Service availability monitoring

### Metric Types

- **Counter**: Monotonically increasing values (e.g., total requests)
- **Gauge**: Values that go up/down (e.g., CPU usage)
- **Histogram**: Distribution of values (e.g., response times)
- **Timer**: Duration measurements with percentiles

## Configuration

### Basic Configuration

```json
{
  "monitoring": {
    "enabled": true,
    "metricsInterval": 60,
    "healthCheckInterval": 30,
    "collectSystemMetrics": true,
    "enablePrometheus": true,
    "prometheusPort": 9090,
    "dashboard": {
      "enabled": true,
      "port": 8081,
      "requireAuth": true
    }
  }
}
```

### Enabling Monitoring

```cpp
#include "common/monitoring/monitoring_service.h"

// Initialize monitoring
MonitoringConfig config;
config.enabled = true;
config.collectSystemMetrics = true;
config.enablePrometheus = true;

MonitoringManager::getInstance().initialize(config);
auto* monitoring = MonitoringManager::getInstance().getService();

// Start monitoring
auto result = monitoring->start();
```

## Using Metrics

### Recording Metrics

```cpp
// Increment counter
MONITOR_COUNTER("pacs_dicom_operations_total", 1);

// Set gauge value
MONITOR_GAUGE("pacs_active_connections", connectionCount);

// Record histogram sample
MONITOR_HISTOGRAM("pacs_query_size_bytes", querySize);

// Time an operation
{
    MONITOR_TIMING("pacs_database_query_ms");
    // Perform database query
    auto result = database->executeQuery(sql);
}
```

### Custom Metrics

```cpp
// Register custom metric
monitoring->registerMetric({
    "my_custom_metric",
    MetricType::Gauge,
    "Description of my metric",
    "unit",
    {{"label1", "value1"}}
});

// Update metric
monitoring->setGauge("my_custom_metric", 42.0);
```

### Metric Labels

```cpp
// Add labels for better categorization
monitoring->incrementCounter(
    "pacs_api_requests_total", 
    1.0,
    {{"method", "GET"}, {"endpoint", "/studies"}, {"status", "200"}}
);
```

## Health Checks

### Registering Health Checks

```cpp
// Database health check
monitoring->registerHealthCheck("database", []() {
    HealthCheckResult result;
    
    try {
        auto db = DatabaseManager::getInstance();
        auto queryResult = db->executeQuery("SELECT 1");
        
        result.healthy = queryResult.isOk();
        result.message = result.healthy ? "Database is healthy" : "Database query failed";
        result.details["connection_pool_size"] = std::to_string(db->getPoolSize());
    } catch (const std::exception& e) {
        result.healthy = false;
        result.message = std::string("Exception: ") + e.what();
    }
    
    return result;
});

// DICOM service health check
monitoring->registerHealthCheck("dicom_service", []() {
    HealthCheckResult result;
    
    auto echoResult = performDicomEcho("localhost", 11112);
    result.healthy = echoResult.isOk();
    result.message = result.healthy ? "DICOM service responding" : "DICOM echo failed";
    
    return result;
});
```

### Running Health Checks

```cpp
// Run all health checks
auto healthResults = monitoring->runHealthChecks();

for (const auto& [name, result] : healthResults) {
    logger::logInfo("Health check '{}': {} - {}", 
                    name, 
                    result.healthy ? "HEALTHY" : "UNHEALTHY",
                    result.message);
}
```

## Monitoring Dashboard

### Accessing the Dashboard

Navigate to: `http://localhost:8081/dashboard`

### Available Dashboards

1. **System Overview**
   - CPU and memory usage
   - System uptime
   - Thread count
   - Overall health status

2. **DICOM Operations**
   - Operation counts (C-STORE, C-FIND, etc.)
   - Response times
   - Active connections
   - Error rates

3. **Database Performance**
   - Query counts and timing
   - Connection pool status
   - Transaction rates

### Creating Custom Dashboards

```cpp
DashboardLayout customLayout = {
    "my_custom_dashboard",
    "My Custom Dashboard",
    {
        // Add widgets
        {
            "my_gauge",
            "Custom Metric",
            WidgetType::Gauge,
            {"my_custom_metric"},
            std::chrono::seconds(5),
            1, 1,  // Width, height in grid units
            {{"max", "100"}, {"unit", "items"}}
        },
        {
            "my_chart",
            "Metric History",
            WidgetType::LineChart,
            {"my_custom_metric"},
            std::chrono::seconds(5),
            2, 2,
            {{"timeRange", "300"}}  // 5 minutes
        }
    },
    4  // Grid columns
};

dashboard->addLayout(customLayout);
```

## Alerts

### Configuring Alerts

```cpp
// CPU usage alert
AlertConfig cpuAlert = {
    "high_cpu_usage",
    "pacs_system_cpu_usage_percent",
    ">",
    90.0,  // Threshold
    AlertSeverity::Warning,
    std::chrono::seconds(300),  // Must be true for 5 minutes
    "CPU usage is above 90%",
    {"email", "slack"}  // Notification channels
};

dashboard->configureAlert(cpuAlert);

// Memory usage alert
AlertConfig memoryAlert = {
    "high_memory_usage",
    "pacs_system_memory_usage_percent",
    ">",
    85.0,
    AlertSeverity::Critical,
    std::chrono::seconds(60),
    "Memory usage is critically high",
    {"email", "pagerduty"}
};

dashboard->configureAlert(memoryAlert);
```

### Alert Conditions

Supported conditions:
- `>` Greater than
- `<` Less than
- `==` Equal to
- `!=` Not equal to
- `>=` Greater than or equal
- `<=` Less than or equal

### Getting Active Alerts

```cpp
auto activeAlerts = dashboard->getActiveAlerts();

for (const auto& alert : activeAlerts) {
    logger::logWarning("Active alert: {} - {}", 
                       alert.alertName, 
                       alert.message);
}
```

## Prometheus Integration

### Metrics Endpoint

Access Prometheus-formatted metrics at: `http://localhost:9090/metrics`

### Prometheus Configuration

```yaml
# prometheus.yml
scrape_configs:
  - job_name: 'pacs_system'
    static_configs:
      - targets: ['localhost:9090']
    scrape_interval: 30s
```

### Example Queries

```promql
# CPU usage over time
pacs_system_cpu_usage_percent

# Request rate (5m average)
rate(pacs_api_requests_total[5m])

# 95th percentile response time
histogram_quantile(0.95, pacs_api_request_duration_ms)

# Error rate
rate(pacs_dicom_errors_total[5m]) / rate(pacs_dicom_operations_total[5m])
```

## Grafana Integration

### Import Dashboard

1. Add Prometheus data source in Grafana
2. Import provided dashboard JSON files from `dashboards/grafana/`
3. Configure variables and time ranges

### Custom Panels

```json
{
  "targets": [
    {
      "expr": "rate(pacs_dicom_operations_total[5m])",
      "legendFormat": "{{operation}}"
    }
  ],
  "title": "DICOM Operations Rate",
  "type": "graph"
}
```

## Performance Impact

### Overhead

- CPU: < 1% for metric collection
- Memory: ~10MB for 1000 metrics with 1-hour retention
- Network: Minimal (metrics are in-memory)

### Optimization

```cpp
// Disable expensive metrics
config.collectSystemMetrics = false;

// Increase collection interval
config.metricsInterval = std::chrono::seconds(300);  // 5 minutes

// Limit metric retention
config.maxMetricAge = 1800;  // 30 minutes
```

## Troubleshooting

### Missing Metrics

1. Check if monitoring is enabled
2. Verify metric is registered
3. Check metric name spelling
4. Ensure metric is being updated

```cpp
// Debug metrics
auto allMetrics = monitoring->getAllMetrics();
for (const auto& [name, value] : allMetrics) {
    logger::logDebug("Metric: {} = {}", name, value.getValue());
}
```

### High Memory Usage

- Reduce metric retention time
- Disable histogram metrics if not needed
- Increase aggregation intervals

### Dashboard Not Loading

1. Check dashboard service is running
2. Verify port is not blocked
3. Check authentication if enabled
4. Review browser console for errors

## Best Practices

1. **Metric Naming**
   - Use consistent prefixes (e.g., `pacs_`)
   - Include unit in name (e.g., `_bytes`, `_ms`)
   - Use labels for variations

2. **Collection Frequency**
   - System metrics: 30-60 seconds
   - Application metrics: 5-10 seconds
   - Health checks: 30-60 seconds

3. **Label Usage**
   - Keep cardinality low
   - Use consistent label names
   - Avoid high-variability values

4. **Alert Configuration**
   - Set appropriate thresholds
   - Use duration to avoid flapping
   - Test alerts before production

5. **Dashboard Design**
   - Group related metrics
   - Use appropriate visualizations
   - Include context and units
   - Limit widgets per dashboard

## Integration Examples

### With DICOM Operations

```cpp
class MonitoredDicomService : public DicomService {
    void handleCStore(DcmDataset* dataset) override {
        MONITOR_TIMING("pacs_dicom_cstore_duration_ms");
        MONITOR_COUNTER("pacs_dicom_operations_total", 1);
        
        try {
            // Process C-STORE
            processStore(dataset);
            
            MONITOR_COUNTER("pacs_dicom_cstore_success_total", 1);
        } catch (const std::exception& e) {
            MONITOR_COUNTER("pacs_dicom_cstore_error_total", 1);
            throw;
        }
    }
};
```

### With Database Operations

```cpp
class MonitoredDatabase : public Database {
    Result<QueryResult> executeQuery(const std::string& sql) override {
        MONITOR_TIMING("pacs_database_query_duration_ms");
        
        auto result = Database::executeQuery(sql);
        
        if (result.isOk()) {
            MONITOR_COUNTER("pacs_database_queries_success_total", 1);
            MONITOR_HISTOGRAM("pacs_database_rows_returned", 
                              result.getValue().rowCount());
        } else {
            MONITOR_COUNTER("pacs_database_queries_error_total", 1);
        }
        
        return result;
    }
};
```