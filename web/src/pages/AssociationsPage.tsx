import { useQuery } from '@tanstack/react-query';
import { Activity, Ban, MessageSquare, Plug, RefreshCw } from 'lucide-react';
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '@/components/ui/card';
import { Button } from '@/components/ui/button';
import { Badge } from '@/components/ui/badge';
import { ApiErrorNotice } from '@/components/ui/api-error';
import { apiClient } from '@/api/client';
import { formatBytes } from '@/lib/utils';
import type { AssociationStatistics } from '@/types/api';

function StatCard({
  title,
  value,
  description,
  icon: Icon,
}: {
  title: string;
  value: string;
  description: string;
  icon: typeof Activity;
}) {
  return (
    <Card>
      <CardHeader className="flex flex-row items-center justify-between space-y-0 pb-2">
        <CardTitle className="text-sm font-medium">{title}</CardTitle>
        <Icon className="h-4 w-4 text-muted-foreground" />
      </CardHeader>
      <CardContent>
        <div className="text-2xl font-bold" data-testid={`association-${title.toLowerCase().replace(/ /g, '-')}`}>
          {value}
        </div>
        <p className="text-xs text-muted-foreground">{description}</p>
      </CardContent>
    </Card>
  );
}

export function AssociationsPage() {
  const { data, isLoading, error, refetch, isRefetching } = useQuery<AssociationStatistics>({
    queryKey: ['associations'],
    queryFn: () => apiClient.getAssociations(),
    refetchInterval: 10000, // Refresh every 10 seconds
  });

  return (
    <div className="space-y-6">
      <div className="flex items-center justify-between">
        <div>
          <h2 className="text-3xl font-bold tracking-tight">DICOM Associations</h2>
          <p className="text-muted-foreground">Totals from the DICOM server since it started</p>
        </div>
        <Button variant="outline" onClick={() => refetch()} disabled={isRefetching}>
          <RefreshCw className={`mr-2 h-4 w-4 ${isRefetching ? 'animate-spin' : ''}`} />
          Refresh
        </Button>
      </div>

      {isLoading ? (
        <div className="py-8 text-center text-muted-foreground">Loading associations...</div>
      ) : error ? (
        <ApiErrorNotice error={error} what="associations" />
      ) : data ? (
        <>
          <div className="grid gap-4 md:grid-cols-3">
            <StatCard
              title="Active Connections"
              value={data.active_count.toLocaleString()}
              description="Open right now"
              icon={Activity}
            />
            <StatCard
              title="Total Associations"
              value={data.total_associations.toLocaleString()}
              description="Accepted since start"
              icon={Plug}
            />
            <StatCard
              title="Rejected Associations"
              value={data.rejected_associations.toLocaleString()}
              description="Refused since start"
              icon={Ban}
            />
          </div>
          <Card>
            <CardHeader>
              <CardTitle className="flex items-center justify-between">
                DICOM Server
                <Badge variant={data.server_running ? 'success' : 'destructive'}>
                  {data.server_running ? 'Running' : 'Stopped'}
                </Badge>
              </CardTitle>
              <CardDescription>Traffic since the server started</CardDescription>
            </CardHeader>
            <CardContent>
              <div className="flex flex-wrap gap-6 text-sm text-muted-foreground">
                <span className="flex items-center gap-1">
                  <MessageSquare className="h-4 w-4" />
                  Messages: <strong>{data.messages_processed.toLocaleString()}</strong>
                </span>
                <span>
                  Received: <strong>{formatBytes(data.bytes_received)}</strong>
                </span>
                <span>
                  Sent: <strong>{formatBytes(data.bytes_sent)}</strong>
                </span>
                <span>
                  Uptime: <strong>{data.uptime_seconds.toLocaleString()} s</strong>
                </span>
              </div>
            </CardContent>
          </Card>
        </>
      ) : null}
    </div>
  );
}
