/**
 * @file monitoring_dashboard.h
 * @brief Real-time monitoring dashboard for PACS System
 *
 * Copyright (c) 2024 PACS System
 * All rights reserved.
 */

#pragma once

#include <string>
#include <memory>
#include <functional>
#include <vector>

#include "monitoring_service.h"
#include "core/result.h"

namespace pacs {

/**
 * @brief Dashboard widget types
 */
enum class WidgetType {
    LineChart,      // Time series data
    Gauge,          // Single value with min/max
    Counter,        // Numeric counter
    Table,          // Tabular data
    HealthStatus,   // Health check status
    LogViewer       // Recent log entries
};

/**
 * @brief Dashboard widget configuration
 */
struct DashboardWidget {
    std::string id;
    std::string title;
    WidgetType type;
    std::vector<std::string> metrics;  // Metric names to display
    std::chrono::seconds refreshInterval{5};
    int width = 1;  // Grid width (1-4)
    int height = 1; // Grid height (1-4)
    std::unordered_map<std::string, std::string> options;
};

/**
 * @brief Dashboard layout
 */
struct DashboardLayout {
    std::string name;
    std::string description;
    std::vector<DashboardWidget> widgets;
    int gridColumns = 4;
};

/**
 * @brief Alert severity levels
 */
enum class AlertSeverity {
    Info,
    Warning,
    Error,
    Critical
};

/**
 * @brief Alert configuration
 */
struct AlertConfig {
    std::string name;
    std::string metric;
    std::string condition;  // e.g., "> 90", "< 10", "== 0"
    double threshold;
    AlertSeverity severity;
    std::chrono::seconds duration{60};  // How long condition must be true
    std::string message;
    std::vector<std::string> notificationChannels;
};

/**
 * @brief Alert state
 */
struct AlertState {
    std::string alertName;
    bool active = false;
    std::chrono::system_clock::time_point triggeredAt;
    std::chrono::system_clock::time_point resolvedAt;
    std::string currentValue;
    std::string message;
};

/**
 * @brief Monitoring dashboard configuration
 */
struct DashboardConfig {
    bool enabled = true;
    uint16_t httpPort = 8081;
    std::string bindAddress = "0.0.0.0";
    std::string basePath = "/dashboard";
    bool requireAuth = true;
    std::chrono::seconds dataRetention{3600};  // 1 hour
};

/**
 * @brief Monitoring dashboard service
 */
class MonitoringDashboard {
public:
    MonitoringDashboard(MonitoringService& monitoringService,
                        const DashboardConfig& config);
    ~MonitoringDashboard();
    
    /**
     * @brief Start dashboard service
     */
    core::Result<void> start();
    
    /**
     * @brief Stop dashboard service
     */
    void stop();
    
    /**
     * @brief Add dashboard layout
     */
    void addLayout(const DashboardLayout& layout);
    
    /**
     * @brief Get available layouts
     */
    std::vector<DashboardLayout> getLayouts() const;
    
    /**
     * @brief Configure alert
     */
    void configureAlert(const AlertConfig& alert);
    
    /**
     * @brief Get active alerts
     */
    std::vector<AlertState> getActiveAlerts() const;
    
    /**
     * @brief Get dashboard data as JSON
     */
    std::string getDashboardDataJson(const std::string& layoutName) const;
    
    /**
     * @brief Get metric history
     */
    std::vector<std::pair<std::chrono::system_clock::time_point, double>>
    getMetricHistory(const std::string& metric, std::chrono::seconds duration) const;
    
    /**
     * @brief Register custom widget renderer
     */
    using WidgetRenderer = std::function<std::string(const DashboardWidget&,
                                                      const std::unordered_map<std::string, MetricValue>&)>;
    void registerWidgetRenderer(WidgetType type, WidgetRenderer renderer);
    
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    MonitoringService& monitoringService_;
    DashboardConfig config_;
    
    /**
     * @brief Create default layouts
     */
    void createDefaultLayouts();
    
    /**
     * @brief Check alerts
     */
    void checkAlerts();
    
    /**
     * @brief Render widget HTML
     */
    std::string renderWidget(const DashboardWidget& widget) const;
};

/**
 * @brief Pre-defined dashboard layouts
 */
class DashboardLayouts {
public:
    /**
     * @brief System overview dashboard
     */
    static DashboardLayout getSystemOverview() {
        return {
            "system_overview",
            "System Overview Dashboard",
            {
                // CPU usage gauge
                {
                    "cpu_gauge",
                    "CPU Usage",
                    WidgetType::Gauge,
                    {"pacs_system_cpu_usage_percent"},
                    std::chrono::seconds(5),
                    1, 1,
                    {{"max", "100"}, {"unit", "%"}}
                },
                // Memory usage gauge
                {
                    "memory_gauge",
                    "Memory Usage",
                    WidgetType::Gauge,
                    {"pacs_system_memory_usage_percent"},
                    std::chrono::seconds(5),
                    1, 1,
                    {{"max", "100"}, {"unit", "%"}}
                },
                // Uptime counter
                {
                    "uptime_counter",
                    "System Uptime",
                    WidgetType::Counter,
                    {"pacs_system_uptime_seconds"},
                    std::chrono::seconds(60),
                    1, 1,
                    {{"format", "duration"}}
                },
                // Thread count
                {
                    "thread_counter",
                    "Active Threads",
                    WidgetType::Counter,
                    {"pacs_system_threads"},
                    std::chrono::seconds(10),
                    1, 1,
                    {}
                },
                // Health status
                {
                    "health_status",
                    "System Health",
                    WidgetType::HealthStatus,
                    {},
                    std::chrono::seconds(30),
                    4, 1,
                    {}
                },
                // CPU history chart
                {
                    "cpu_history",
                    "CPU Usage History",
                    WidgetType::LineChart,
                    {"pacs_system_cpu_usage_percent"},
                    std::chrono::seconds(5),
                    2, 2,
                    {{"timeRange", "300"}}  // 5 minutes
                },
                // Memory history chart
                {
                    "memory_history",
                    "Memory Usage History",
                    WidgetType::LineChart,
                    {"pacs_system_memory_usage_bytes"},
                    std::chrono::seconds(5),
                    2, 2,
                    {{"timeRange", "300"}}
                }
            },
            4  // 4 column grid
        };
    }
    
    /**
     * @brief DICOM operations dashboard
     */
    static DashboardLayout getDicomOperations() {
        return {
            "dicom_operations",
            "DICOM Operations Dashboard",
            {
                // Operation counters
                {
                    "store_counter",
                    "C-STORE Operations",
                    WidgetType::Counter,
                    {"pacs_dicom_store_total"},
                    std::chrono::seconds(5),
                    1, 1,
                    {}
                },
                {
                    "find_counter",
                    "C-FIND Operations",
                    WidgetType::Counter,
                    {"pacs_dicom_find_total"},
                    std::chrono::seconds(5),
                    1, 1,
                    {}
                },
                {
                    "move_counter",
                    "C-MOVE Operations",
                    WidgetType::Counter,
                    {"pacs_dicom_move_total"},
                    std::chrono::seconds(5),
                    1, 1,
                    {}
                },
                {
                    "echo_counter",
                    "C-ECHO Operations",
                    WidgetType::Counter,
                    {"pacs_dicom_echo_total"},
                    std::chrono::seconds(5),
                    1, 1,
                    {}
                },
                // Operation timing
                {
                    "operation_timing",
                    "Operation Response Time",
                    WidgetType::LineChart,
                    {"pacs_dicom_operation_duration_ms"},
                    std::chrono::seconds(5),
                    4, 2,
                    {{"timeRange", "600"}}  // 10 minutes
                },
                // Active connections
                {
                    "active_connections",
                    "Active DICOM Connections",
                    WidgetType::Gauge,
                    {"pacs_dicom_active_connections"},
                    std::chrono::seconds(5),
                    2, 1,
                    {{"max", "100"}}
                },
                // Error rate
                {
                    "error_rate",
                    "DICOM Error Rate",
                    WidgetType::LineChart,
                    {"pacs_dicom_errors_total"},
                    std::chrono::seconds(10),
                    2, 1,
                    {{"timeRange", "300"}}
                }
            },
            4
        };
    }
    
    /**
     * @brief Database performance dashboard
     */
    static DashboardLayout getDatabasePerformance() {
        return {
            "database_performance",
            "Database Performance Dashboard",
            {
                // Query counters
                {
                    "query_counter",
                    "Total Queries",
                    WidgetType::Counter,
                    {"pacs_database_queries_total"},
                    std::chrono::seconds(5),
                    1, 1,
                    {}
                },
                // Query timing
                {
                    "query_timing",
                    "Query Response Time",
                    WidgetType::LineChart,
                    {"pacs_database_query_duration_ms"},
                    std::chrono::seconds(5),
                    3, 2,
                    {{"timeRange", "300"}}
                },
                // Connection pool
                {
                    "db_connections",
                    "Database Connections",
                    WidgetType::Table,
                    {"pacs_database_pool_active", "pacs_database_pool_available"},
                    std::chrono::seconds(10),
                    2, 1,
                    {}
                },
                // Transaction rate
                {
                    "transaction_rate",
                    "Transactions/sec",
                    WidgetType::LineChart,
                    {"pacs_database_transactions_per_second"},
                    std::chrono::seconds(5),
                    2, 1,
                    {{"timeRange", "300"}}
                }
            },
            4
        };
    }
};

} // namespace pacs