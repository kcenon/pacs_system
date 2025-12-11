import { useQuery } from '@tanstack/react-query';
import {
  Activity,
  Database,
  HardDrive,
  Users,
  AlertCircle,
  CheckCircle2,
  Clock,
} from 'lucide-react';
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '@/components/ui/card';
import { Badge } from '@/components/ui/badge';
import { apiClient } from '@/api/client';
import type { SystemStatus, SystemMetrics } from '@/types/api';

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
}: {
  title: string;
  value: string | number;
  description?: string;
  icon: React.ComponentType<{ className?: string }>;
}) {
  return (
    <Card>
      <CardHeader className="flex flex-row items-center justify-between space-y-0 pb-2">
        <CardTitle className="text-sm font-medium">{title}</CardTitle>
        <Icon className="h-4 w-4 text-muted-foreground" />
      </CardHeader>
      <CardContent>
        <div className="text-2xl font-bold">{value}</div>
        {description && (
          <p className="text-xs text-muted-foreground">{description}</p>
        )}
      </CardContent>
    </Card>
  );
}

export function DashboardPage() {
  const { data: status, isLoading: statusLoading, error: statusError } = useQuery<SystemStatus>({
    queryKey: ['system-status'],
    queryFn: () => apiClient.getSystemStatus(),
    refetchInterval: 30000, // Refresh every 30 seconds
  });

  const { data: metrics } = useQuery<SystemMetrics>({
    queryKey: ['system-metrics'],
    queryFn: () => apiClient.getSystemMetrics(),
    refetchInterval: 30000,
  });

  return (
    <div className="space-y-6">
      <div>
        <h2 className="text-3xl font-bold tracking-tight">Dashboard</h2>
        <p className="text-muted-foreground">
          Overview of your PACS system status and metrics
        </p>
      </div>

      {/* System Status */}
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
            {status?.message || 'Checking system status...'}
          </CardDescription>
        </CardHeader>
        <CardContent>
          <div className="text-sm text-muted-foreground">
            Version: {status?.version || '-'}
          </div>
        </CardContent>
      </Card>

      {/* Quick Stats */}
      <div className="grid gap-4 md:grid-cols-2 lg:grid-cols-4">
        <StatCard
          title="Active Associations"
          value={0}
          description="Current DICOM connections"
          icon={Activity}
        />
        <StatCard
          title="Total Patients"
          value="-"
          description="In database"
          icon={Users}
        />
        <StatCard
          title="Total Studies"
          value="-"
          description="Stored in system"
          icon={Database}
        />
        <StatCard
          title="Storage Used"
          value="-"
          description="Of allocated space"
          icon={HardDrive}
        />
      </div>

      {/* Metrics */}
      <div className="grid gap-4 md:grid-cols-2">
        <Card>
          <CardHeader>
            <CardTitle>System Metrics</CardTitle>
            <CardDescription>Performance and usage statistics</CardDescription>
          </CardHeader>
          <CardContent>
            <div className="space-y-4">
              <div className="flex justify-between">
                <span className="text-muted-foreground">Uptime</span>
                <span className="font-medium">
                  {metrics?.uptime_seconds
                    ? `${Math.floor(metrics.uptime_seconds / 3600)}h ${Math.floor((metrics.uptime_seconds % 3600) / 60)}m`
                    : '-'}
                </span>
              </div>
              <div className="flex justify-between">
                <span className="text-muted-foreground">Total Requests</span>
                <span className="font-medium">{metrics?.requests_total ?? '-'}</span>
              </div>
              <div className="flex justify-between">
                <span className="text-muted-foreground">Status</span>
                <span className="font-medium">{metrics?.message || '-'}</span>
              </div>
            </div>
          </CardContent>
        </Card>

        <Card>
          <CardHeader>
            <CardTitle>Recent Activity</CardTitle>
            <CardDescription>Latest system events</CardDescription>
          </CardHeader>
          <CardContent>
            <div className="space-y-4 text-sm text-muted-foreground">
              <p>No recent activity to display.</p>
              <p>Connect to the PACS server to see real-time events.</p>
            </div>
          </CardContent>
        </Card>
      </div>
    </div>
  );
}
