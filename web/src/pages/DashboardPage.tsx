import { useQuery } from '@tanstack/react-query';
import {
  Activity,
  Database,
  HardDrive,
  Users,
  AlertCircle,
  CheckCircle2,
  Clock,
  RefreshCw,
  Server,
  Cpu,
  MemoryStick,
  Zap,
} from 'lucide-react';
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '@/components/ui/card';
import { Badge } from '@/components/ui/badge';
import { Button } from '@/components/ui/button';
import { ApiErrorNotice } from '@/components/ui/api-error';
import { apiClient } from '@/api/client';
import { formatDateTime, formatBytes } from '@/lib/utils';
import type { SystemStatus, SystemMetrics, DashboardStats, AuditRecord, PaginatedResponse } from '@/types/api';

// Polling intervals (in ms)
const FAST_POLL_INTERVAL = 10000;  // 10 seconds for real-time data
const SLOW_POLL_INTERVAL = 30000; // 30 seconds for less frequent data

function StatusBadge({ status }: { status: string }) {
  const variants: Record<string, 'success' | 'warning' | 'destructive' | 'secondary'> = {
    healthy: 'success',
    degraded: 'warning',
    unhealthy: 'destructive',
    unknown: 'secondary',
  };

  const icons: Record<string, React.ReactNode> = {
    healthy: <CheckCircle2 className="mr-1 h-3 w-3" />,
    degraded: <AlertCircle className="mr-1 h-3 w-3" />,
    unhealthy: <AlertCircle className="mr-1 h-3 w-3" />,
    unknown: <Clock className="mr-1 h-3 w-3" />,
  };

  return (
    <Badge variant={variants[status] || 'secondary'} className="flex items-center">
      {icons[status]}
      {status.charAt(0).toUpperCase() + status.slice(1)}
    </Badge>
  );
}

function StatCard({
  title,
  value,
  description,
  icon: Icon,
  loading,
  trend,
}: {
  title: string;
  value: string | number;
  description?: string;
  icon: React.ComponentType<{ className?: string }>;
  loading?: boolean;
  trend?: { value: number; direction: 'up' | 'down' | 'neutral' };
}) {
  return (
    <Card>
      <CardHeader className="flex flex-row items-center justify-between space-y-0 pb-2">
        <CardTitle className="text-sm font-medium">{title}</CardTitle>
        <Icon className="h-4 w-4 text-muted-foreground" />
      </CardHeader>
      <CardContent>
        {loading ? (
          <div className="h-8 w-20 animate-pulse rounded bg-muted" />
        ) : (
          <>
            <div className="text-2xl font-bold">{value}</div>
            {description && (
              <p className="text-xs text-muted-foreground">{description}</p>
            )}
            {trend && (
              <p className={`text-xs ${trend.direction === 'up' ? 'text-green-600' : trend.direction === 'down' ? 'text-red-600' : 'text-muted-foreground'}`}>
                {trend.direction === 'up' ? '↑' : trend.direction === 'down' ? '↓' : '→'} {trend.value}%
              </p>
            )}
          </>
        )}
      </CardContent>
    </Card>
  );
}

function HealthIndicator({
  name,
  status,
  latency,
}: {
  name: string;
  status: 'healthy' | 'degraded' | 'unhealthy' | 'unknown';
  latency?: number;
}) {
  const statusColors: Record<string, string> = {
    healthy: 'bg-green-500',
    degraded: 'bg-yellow-500',
    unhealthy: 'bg-red-500',
    unknown: 'bg-gray-400',
  };

  const icons: Record<string, React.ComponentType<{ className?: string }>> = {
    database: Database,
    storage: HardDrive,
    server: Server,
    cpu: Cpu,
    memory: MemoryStick,
  };

  const Icon = icons[name.toLowerCase()] || Zap;

  return (
    <div className="flex items-center justify-between py-2">
      <div className="flex items-center gap-2">
        <div className={`h-2.5 w-2.5 rounded-full ${statusColors[status]}`} />
        <Icon className="h-4 w-4 text-muted-foreground" />
        <span className="text-sm font-medium capitalize">{name}</span>
      </div>
      <div className="flex items-center gap-2 text-sm text-muted-foreground">
        {latency !== undefined && <span>{latency}ms</span>}
        <StatusBadge status={status} />
      </div>
    </div>
  );
}

function ActivityFeed({ activities, loading }: { activities: AuditRecord[]; loading: boolean }) {
  const getEventIcon = (eventType: string) => {
    if (eventType.includes('STORE')) return <HardDrive className="h-4 w-4 text-blue-500" />;
    if (eventType.includes('QUERY')) return <Database className="h-4 w-4 text-green-500" />;
    if (eventType.includes('RETRIEVE')) return <Activity className="h-4 w-4 text-purple-500" />;
    if (eventType.includes('LOGIN') || eventType.includes('AUTH')) return <Users className="h-4 w-4 text-orange-500" />;
    return <Zap className="h-4 w-4 text-gray-500" />;
  };

  const getOutcomeBadge = (outcome: string) => {
    const lower = outcome.toLowerCase();
    if (lower === 'success' || lower === 'ok') return <Badge variant="success">Success</Badge>;
    if (lower === 'failure' || lower === 'error') return <Badge variant="destructive">Failed</Badge>;
    return <Badge variant="secondary">{outcome}</Badge>;
  };

  if (loading) {
    return (
      <div className="space-y-3">
        {[...Array(5)].map((_, i) => (
          <div key={i} className="flex items-start gap-3 animate-pulse">
            <div className="h-8 w-8 rounded-full bg-muted" />
            <div className="flex-1 space-y-2">
              <div className="h-4 w-3/4 rounded bg-muted" />
              <div className="h-3 w-1/2 rounded bg-muted" />
            </div>
          </div>
        ))}
      </div>
    );
  }

  if (activities.length === 0) {
    return (
      <div className="py-8 text-center text-muted-foreground">
        <Activity className="mx-auto mb-2 h-8 w-8" />
        <p>No recent activity</p>
        <p className="text-sm">Activities will appear here as DICOM operations occur</p>
      </div>
    );
  }

  return (
    <div className="space-y-4 max-h-80 overflow-y-auto">
      {activities.map((activity) => (
        <div key={activity.pk} className="flex items-start gap-3 border-b pb-3 last:border-0">
          <div className="flex h-8 w-8 items-center justify-center rounded-full bg-muted">
            {getEventIcon(activity.event_type)}
          </div>
          <div className="flex-1 min-w-0">
            <div className="flex items-center justify-between gap-2">
              <p className="font-medium text-sm truncate">{activity.event_type}</p>
              {getOutcomeBadge(activity.outcome)}
            </div>
            <p className="text-xs text-muted-foreground truncate">
              {activity.message || `${activity.source_ae} → ${activity.target_ae}`}
            </p>
            <p className="text-xs text-muted-foreground">
              {formatDateTime(activity.timestamp)}
              {activity.patient_id && ` • Patient: ${activity.patient_id}`}
            </p>
          </div>
        </div>
      ))}
    </div>
  );
}

export function DashboardPage() {
  // System status query
  const {
    data: status,
    isLoading: statusLoading,
    error: statusError,
    refetch: refetchStatus,
  } = useQuery<SystemStatus>({
    queryKey: ['system-status'],
    queryFn: () => apiClient.getSystemStatus(),
    refetchInterval: SLOW_POLL_INTERVAL,
  });

  // System metrics query
  const { data: metrics } = useQuery<SystemMetrics>({
    queryKey: ['system-metrics'],
    queryFn: () => apiClient.getSystemMetrics(),
    refetchInterval: SLOW_POLL_INTERVAL,
  });

  // Dashboard stats query (aggregates associations, patients, studies)
  const {
    data: stats,
    isLoading: statsLoading,
    error: statsError,
    refetch: refetchStats,
    isRefetching: isStatsRefetching,
  } = useQuery<DashboardStats>({
    queryKey: ['dashboard-stats'],
    queryFn: () => apiClient.getDashboardStats(),
    refetchInterval: FAST_POLL_INTERVAL,
  });

  // Recent activity query
  const { data: recentActivity, isLoading: activityLoading, error: activityError } = useQuery<PaginatedResponse<AuditRecord>>({
    queryKey: ['recent-activity'],
    queryFn: () => apiClient.getRecentActivity(10),
    refetchInterval: FAST_POLL_INTERVAL,
  });

  const handleRefresh = () => {
    refetchStatus();
    refetchStats();
  };

  // Only what the server reported: a page that assumes health hides the failure it exists to show.
  const componentHealth: Record<string, { status: 'healthy' | 'unhealthy'; latency_ms?: number }> = {};
  if (status?.database) {
    componentHealth.database = {
      status: status.database.connected ? 'healthy' : 'unhealthy',
      latency_ms: status.database.response_time_ms,
    };
  }
  if (status?.storage) {
    componentHealth.storage = {
      status: status.storage.writable && status.storage.readable ? 'healthy' : 'unhealthy',
    };
  }

  return (
    <div className="space-y-6">
      {/* Header with Refresh Button */}
      <div className="flex items-center justify-between">
        <div>
          <h2 className="text-3xl font-bold tracking-tight">Dashboard</h2>
          <p className="text-muted-foreground">
            Overview of your PACS system status and metrics
          </p>
        </div>
        <Button
          variant="outline"
          onClick={handleRefresh}
          disabled={isStatsRefetching}
        >
          <RefreshCw className={`mr-2 h-4 w-4 ${isStatsRefetching ? 'animate-spin' : ''}`} />
          Refresh
        </Button>
      </div>

      {/* System Status Card */}
      <Card>
        <CardHeader>
          <CardTitle className="flex items-center justify-between">
            System Status
            {statusLoading ? (
              <Badge variant="secondary">Loading...</Badge>
            ) : statusError ? (
              <Badge variant="destructive">Error</Badge>
            ) : (
              <StatusBadge status={status?.status || 'unknown'} />
            )}
          </CardTitle>
          <CardDescription>
            {statusError
              ? statusError.message
              : status?.message || (statusLoading ? 'Checking system status...' : 'No message from the server')}
          </CardDescription>
        </CardHeader>
        <CardContent>
          <div className="flex flex-wrap gap-4 text-sm text-muted-foreground">
            <span>Version: <strong>{status?.version || '-'}</strong></span>
            {metrics?.uptime_seconds && (
              <span>
                Uptime: <strong>
                  {Math.floor(metrics.uptime_seconds / 86400)}d{' '}
                  {Math.floor((metrics.uptime_seconds % 86400) / 3600)}h{' '}
                  {Math.floor((metrics.uptime_seconds % 3600) / 60)}m
                </strong>
              </span>
            )}
            {!!metrics?.requests_total && (
              <span>Total Requests: <strong>{metrics.requests_total.toLocaleString()}</strong></span>
            )}
          </div>
        </CardContent>
      </Card>

      {statsError && <ApiErrorNotice error={statsError} what="the counts" />}

      {/* Quick Stats Grid */}
      <div className="grid gap-4 md:grid-cols-2 lg:grid-cols-4">
        <StatCard
          title="Active Associations"
          value={stats?.active_associations ?? '-'}
          description="Current DICOM connections"
          icon={Activity}
          loading={statsLoading}
        />
        <StatCard
          title="Total Patients"
          value={stats?.total_patients?.toLocaleString() ?? '-'}
          description="In database"
          icon={Users}
          loading={statsLoading}
        />
        <StatCard
          title="Total Studies"
          value={stats?.total_studies?.toLocaleString() ?? '-'}
          description="Stored in system"
          icon={Database}
          loading={statsLoading}
        />
        <StatCard
          title="Storage Used"
          value={status?.storage ? formatBytes(status.storage.used_bytes) : '-'}
          description={status?.storage ? `of ${formatBytes(status.storage.total_bytes)}` : 'Not reported'}
          icon={HardDrive}
          loading={statusLoading}
        />
      </div>

      {/* Detailed Sections */}
      <div className="grid gap-4">
        {/* Component Health */}
        <Card>
          <CardHeader>
            <CardTitle className="text-lg">System Health</CardTitle>
            <CardDescription>Component status indicators</CardDescription>
          </CardHeader>
          <CardContent>
            <div className="space-y-1">
              {Object.entries(componentHealth).map(([name, health]) => (
                <HealthIndicator
                  key={name}
                  name={name}
                  status={(health as { status: 'healthy' | 'degraded' | 'unhealthy' | 'unknown' }).status}
                  latency={(health as { latency_ms?: number }).latency_ms}
                />
              ))}
              {Object.keys(componentHealth).length === 0 && (
                <p className="text-sm text-muted-foreground py-4 text-center">
                  Health data unavailable
                </p>
              )}
            </div>
          </CardContent>
        </Card>

      </div>

      {/* Recent Activity */}
      <Card>
        <CardHeader>
          <CardTitle className="text-lg">Recent Activity</CardTitle>
          <CardDescription>Latest system events and DICOM operations</CardDescription>
        </CardHeader>
        <CardContent>
          {activityError ? (
            <ApiErrorNotice error={activityError} what="recent activity" />
          ) : (
            <ActivityFeed activities={recentActivity?.data || []} loading={activityLoading} />
          )}
        </CardContent>
      </Card>

      {/* Footer Info */}
      <div className="text-center text-xs text-muted-foreground">
        <p>
          Dashboard auto-refreshes every {FAST_POLL_INTERVAL / 1000} seconds •{' '}
          Last updated: {new Date().toLocaleTimeString()}
        </p>
      </div>
    </div>
  );
}
