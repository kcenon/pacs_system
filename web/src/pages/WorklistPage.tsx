import { useState } from 'react';
import { useQuery } from '@tanstack/react-query';
import { Search, Plus, Calendar } from 'lucide-react';
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '@/components/ui/card';
import { Input } from '@/components/ui/input';
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
import { formatDate, formatTime } from '@/lib/utils';
import type { WorklistItem, PaginatedResponse } from '@/types/api';

function getStatusBadge(status: string) {
  const variants: Record<string, 'default' | 'secondary' | 'success' | 'warning' | 'destructive'> = {
    SCHEDULED: 'default',
    IN_PROGRESS: 'warning',
    COMPLETED: 'success',
    DISCONTINUED: 'destructive',
  };
  return <Badge variant={variants[status] || 'secondary'}>{status}</Badge>;
}

export function WorklistPage() {
  const [search, setSearch] = useState('');
  const [statusFilter, setStatusFilter] = useState<string>('');
  const [page, setPage] = useState(0);
  const pageSize = 20;

  const { data, isLoading, error } = useQuery<PaginatedResponse<WorklistItem>>({
    queryKey: ['worklist', search, statusFilter, page],
    queryFn: () =>
      apiClient.getWorklist({
        patient_id: search || undefined,
        status: statusFilter || undefined,
        limit: pageSize,
        offset: page * pageSize,
      }),
  });

  const items = data?.data || [];
  const total = data?.pagination?.total || 0;
  const totalPages = Math.ceil(total / pageSize);

  return (
    <div className="space-y-6">
      <div className="flex items-center justify-between">
        <div>
          <h2 className="text-3xl font-bold tracking-tight">Worklist</h2>
          <p className="text-muted-foreground">Manage scheduled procedures</p>
        </div>
        <Button>
          <Plus className="mr-2 h-4 w-4" />
          New Item
        </Button>
      </div>

      <Card>
        <CardHeader>
          <CardTitle>Scheduled Procedures</CardTitle>
          <CardDescription>Total: {total} items</CardDescription>
        </CardHeader>
        <CardContent>
          <div className="mb-4 flex items-center space-x-2">
            <div className="relative flex-1">
              <Search className="absolute left-2.5 top-2.5 h-4 w-4 text-muted-foreground" />
              <Input
                placeholder="Search by patient ID..."
                value={search}
                onChange={(e) => {
                  setSearch(e.target.value);
                  setPage(0);
                }}
                className="pl-8"
              />
            </div>
            <select
              value={statusFilter}
              onChange={(e) => {
                setStatusFilter(e.target.value);
                setPage(0);
              }}
              className="h-10 rounded-md border border-input bg-background px-3 py-2 text-sm"
            >
              <option value="">All Status</option>
              <option value="SCHEDULED">Scheduled</option>
              <option value="IN_PROGRESS">In Progress</option>
              <option value="COMPLETED">Completed</option>
              <option value="DISCONTINUED">Discontinued</option>
            </select>
          </div>

          {isLoading ? (
            <div className="py-8 text-center text-muted-foreground">Loading worklist...</div>
          ) : error ? (
            <div className="py-8 text-center text-destructive">
              Error loading worklist. Make sure the API server is running.
            </div>
          ) : items.length === 0 ? (
            <div className="py-8 text-center text-muted-foreground">
              <Calendar className="mx-auto mb-4 h-12 w-12 text-muted-foreground/50" />
              <p>No scheduled procedures found.</p>
            </div>
          ) : (
            <>
              <Table>
                <TableHeader>
                  <TableRow>
                    <TableHead>Date/Time</TableHead>
                    <TableHead>Patient</TableHead>
                    <TableHead>Procedure</TableHead>
                    <TableHead>Modality</TableHead>
                    <TableHead>Station AE</TableHead>
                    <TableHead>Status</TableHead>
                  </TableRow>
                </TableHeader>
                <TableBody>
                  {items.map((item) => (
                    <TableRow key={item.pk}>
                      <TableCell>
                        <div className="font-medium">
                          {formatDate(item.scheduled_procedure_step_start_date)}
                        </div>
                        <div className="text-sm text-muted-foreground">
                          {formatTime(item.scheduled_procedure_step_start_time)}
                        </div>
                      </TableCell>
                      <TableCell>
                        <div className="font-medium">{item.patient_name || '-'}</div>
                        <div className="text-sm text-muted-foreground">{item.patient_id}</div>
                      </TableCell>
                      <TableCell>
                        <div>{item.scheduled_procedure_step_description || '-'}</div>
                        <div className="text-sm text-muted-foreground">
                          {item.requested_procedure_id}
                        </div>
                      </TableCell>
                      <TableCell>
                        <Badge variant="outline">{item.modality}</Badge>
                      </TableCell>
                      <TableCell>{item.scheduled_station_ae || '-'}</TableCell>
                      <TableCell>{getStatusBadge(item.status)}</TableCell>
                    </TableRow>
                  ))}
                </TableBody>
              </Table>

              {totalPages > 1 && (
                <div className="mt-4 flex items-center justify-between">
                  <div className="text-sm text-muted-foreground">
                    Page {page + 1} of {totalPages}
                  </div>
                  <div className="space-x-2">
                    <Button
                      variant="outline"
                      size="sm"
                      onClick={() => setPage((p) => Math.max(0, p - 1))}
                      disabled={page === 0}
                    >
                      Previous
                    </Button>
                    <Button
                      variant="outline"
                      size="sm"
                      onClick={() => setPage((p) => Math.min(totalPages - 1, p + 1))}
                      disabled={page >= totalPages - 1}
                    >
                      Next
                    </Button>
                  </div>
                </div>
              )}
            </>
          )}
        </CardContent>
      </Card>
    </div>
  );
}
