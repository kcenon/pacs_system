import { useQuery } from '@tanstack/react-query';
import { Activity, Plug, RefreshCw } from 'lucide-react';
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '@/components/ui/card';
import { Button } from '@/components/ui/button';
import { Badge } from '@/components/ui/badge';
import {
  Table,
  TableBody,
  TableCell,
  TableHead,
  TableHeader,
  TableRow,
} from '@/components/ui/table';
import { apiClient } from '@/api/client';
import { formatDateTime } from '@/lib/utils';
import type { Association } from '@/types/api';

function getStateBadge(state: string) {
  const variants: Record<string, 'success' | 'warning' | 'secondary'> = {
    ESTABLISHED: 'success',
    PENDING: 'warning',
    CLOSED: 'secondary',
  };
  return <Badge variant={variants[state] || 'secondary'}>{state}</Badge>;
}

function getRoleBadge(role: string) {
  return (
    <Badge variant={role === 'SCP' ? 'default' : 'outline'}>
      {role}
    </Badge>
  );
}

export function AssociationsPage() {
  const { data, isLoading, error, refetch, isRefetching } = useQuery<{ data: Association[] }>({
    queryKey: ['associations'],
    queryFn: () => apiClient.getAssociations(),
    refetchInterval: 10000, // Refresh every 10 seconds
  });

  const associations = data?.data || [];
  const activeCount = associations.filter((a) => a.state === 'ESTABLISHED').length;

  return (
    <div className="space-y-6">
      <div className="flex items-center justify-between">
        <div>
          <h2 className="text-3xl font-bold tracking-tight">DICOM Associations</h2>
          <p className="text-muted-foreground">Monitor active network connections</p>
        </div>
        <Button variant="outline" onClick={() => refetch()} disabled={isRefetching}>
          <RefreshCw className={`mr-2 h-4 w-4 ${isRefetching ? 'animate-spin' : ''}`} />
          Refresh
        </Button>
      </div>

      {/* Stats */}
      <div className="grid gap-4 md:grid-cols-3">
        <Card>
          <CardHeader className="flex flex-row items-center justify-between space-y-0 pb-2">
            <CardTitle className="text-sm font-medium">Active Connections</CardTitle>
            <Activity className="h-4 w-4 text-muted-foreground" />
          </CardHeader>
          <CardContent>
            <div className="text-2xl font-bold">{activeCount}</div>
            <p className="text-xs text-muted-foreground">Currently established</p>
          </CardContent>
        </Card>
        <Card>
          <CardHeader className="flex flex-row items-center justify-between space-y-0 pb-2">
            <CardTitle className="text-sm font-medium">Total Associations</CardTitle>
            <Plug className="h-4 w-4 text-muted-foreground" />
          </CardHeader>
          <CardContent>
            <div className="text-2xl font-bold">{associations.length}</div>
            <p className="text-xs text-muted-foreground">Including closed</p>
          </CardContent>
        </Card>
        <Card>
          <CardHeader className="flex flex-row items-center justify-between space-y-0 pb-2">
            <CardTitle className="text-sm font-medium">SCP Role</CardTitle>
            <Activity className="h-4 w-4 text-muted-foreground" />
          </CardHeader>
          <CardContent>
            <div className="text-2xl font-bold">
              {associations.filter((a) => a.role === 'SCP' && a.state === 'ESTABLISHED').length}
            </div>
            <p className="text-xs text-muted-foreground">Incoming connections</p>
          </CardContent>
        </Card>
      </div>

      {/* Associations Table */}
      <Card>
        <CardHeader>
          <CardTitle>Association List</CardTitle>
          <CardDescription>All DICOM network associations</CardDescription>
        </CardHeader>
        <CardContent>
          {isLoading ? (
            <div className="py-8 text-center text-muted-foreground">Loading associations...</div>
          ) : error ? (
            <div className="py-8 text-center text-destructive">
              Error loading associations. Make sure the API server is running.
            </div>
          ) : associations.length === 0 ? (
            <div className="py-8 text-center text-muted-foreground">
              <Plug className="mx-auto mb-4 h-12 w-12 text-muted-foreground/50" />
              <p>No DICOM associations found.</p>
              <p className="text-sm">Connections will appear here when DICOM clients connect.</p>
            </div>
          ) : (
            <Table>
              <TableHeader>
                <TableRow>
                  <TableHead>ID</TableHead>
                  <TableHead>Remote AE</TableHead>
                  <TableHead>Remote Host</TableHead>
                  <TableHead>Port</TableHead>
                  <TableHead>Role</TableHead>
                  <TableHead>State</TableHead>
                  <TableHead>Started</TableHead>
                  <TableHead>Last Activity</TableHead>
                </TableRow>
              </TableHeader>
              <TableBody>
                {associations.map((assoc) => (
                  <TableRow key={assoc.id}>
                    <TableCell className="font-mono text-sm">{assoc.id}</TableCell>
                    <TableCell className="font-medium">{assoc.remote_ae}</TableCell>
                    <TableCell>{assoc.remote_host}</TableCell>
                    <TableCell>{assoc.remote_port}</TableCell>
                    <TableCell>{getRoleBadge(assoc.role)}</TableCell>
                    <TableCell>{getStateBadge(assoc.state)}</TableCell>
                    <TableCell className="whitespace-nowrap">
                      {formatDateTime(assoc.start_time)}
                    </TableCell>
                    <TableCell className="whitespace-nowrap">
                      {formatDateTime(assoc.last_activity)}
                    </TableCell>
                  </TableRow>
                ))}
              </TableBody>
            </Table>
          )}
        </CardContent>
      </Card>
    </div>
  );
}
