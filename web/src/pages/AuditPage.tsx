import { useState } from 'react';
import { useQuery } from '@tanstack/react-query';
import { Search, Download, FileText } from 'lucide-react';
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
import { formatDateTime } from '@/lib/utils';
import type { AuditRecord, PaginatedResponse } from '@/types/api';

function getOutcomeBadge(outcome: string) {
  const variants: Record<string, 'success' | 'destructive' | 'secondary'> = {
    Success: 'success',
    Failure: 'destructive',
  };
  return <Badge variant={variants[outcome] || 'secondary'}>{outcome}</Badge>;
}

function AuditDetail({ record, onClose }: { record: AuditRecord; onClose: () => void }) {
  return (
    <Card>
      <CardHeader className="flex flex-row items-start justify-between">
        <div>
          <CardTitle className="flex items-center">
            <FileText className="mr-2 h-5 w-5" />
            Audit Log Detail
          </CardTitle>
          <CardDescription>Event ID: {record.pk}</CardDescription>
        </div>
        <Button variant="ghost" size="sm" onClick={onClose}>
          Close
        </Button>
      </CardHeader>
      <CardContent>
        <div className="grid gap-4 md:grid-cols-2">
          <div className="space-y-3">
            <div>
              <span className="text-sm text-muted-foreground">Event Type:</span>
              <span className="ml-2 font-medium">{record.event_type}</span>
            </div>
            <div>
              <span className="text-sm text-muted-foreground">Outcome:</span>
              <span className="ml-2">{getOutcomeBadge(record.outcome)}</span>
            </div>
            <div>
              <span className="text-sm text-muted-foreground">Timestamp:</span>
              <span className="ml-2 font-medium">{formatDateTime(record.timestamp)}</span>
            </div>
            <div>
              <span className="text-sm text-muted-foreground">User ID:</span>
              <span className="ml-2 font-medium">{record.user_id || '-'}</span>
            </div>
          </div>
          <div className="space-y-3">
            <div>
              <span className="text-sm text-muted-foreground">Source AE:</span>
              <span className="ml-2 font-medium">{record.source_ae || '-'}</span>
            </div>
            <div>
              <span className="text-sm text-muted-foreground">Target AE:</span>
              <span className="ml-2 font-medium">{record.target_ae || '-'}</span>
            </div>
            <div>
              <span className="text-sm text-muted-foreground">Source IP:</span>
              <span className="ml-2 font-medium">{record.source_ip || '-'}</span>
            </div>
            <div>
              <span className="text-sm text-muted-foreground">Patient ID:</span>
              <span className="ml-2 font-medium">{record.patient_id || '-'}</span>
            </div>
          </div>
        </div>
        <div className="mt-4 space-y-3">
          <div>
            <span className="text-sm text-muted-foreground">Study UID:</span>
            <span className="ml-2 break-all font-mono text-sm">{record.study_uid || '-'}</span>
          </div>
          <div>
            <span className="text-sm text-muted-foreground">Message:</span>
            <p className="mt-1 rounded bg-muted p-2 text-sm">{record.message || '-'}</p>
          </div>
          {record.details && (
            <div>
              <span className="text-sm text-muted-foreground">Details:</span>
              <p className="mt-1 rounded bg-muted p-2 font-mono text-sm">{record.details}</p>
            </div>
          )}
        </div>
      </CardContent>
    </Card>
  );
}

export function AuditPage() {
  const [search, setSearch] = useState('');
  const [eventType, setEventType] = useState('');
  const [outcome, setOutcome] = useState('');
  const [selectedRecord, setSelectedRecord] = useState<AuditRecord | null>(null);
  const [page, setPage] = useState(0);
  const pageSize = 20;

  const { data, isLoading, error } = useQuery<PaginatedResponse<AuditRecord>>({
    queryKey: ['audit', search, eventType, outcome, page],
    queryFn: () =>
      apiClient.getAuditLogs({
        user_id: search || undefined,
        event_type: eventType || undefined,
        outcome: outcome || undefined,
        limit: pageSize,
        offset: page * pageSize,
      }),
  });

  const records = data?.data || [];
  const total = data?.pagination?.total || 0;
  const totalPages = Math.ceil(total / pageSize);

  const handleExport = async (format: 'json' | 'csv') => {
    try {
      const data = await apiClient.exportAuditLogs(
        {
          user_id: search || undefined,
          event_type: eventType || undefined,
          outcome: outcome || undefined,
        },
        format
      );

      const blob = format === 'csv' ? data : new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' });
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = `audit_logs.${format}`;
      document.body.appendChild(a);
      a.click();
      document.body.removeChild(a);
      URL.revokeObjectURL(url);
    } catch (err) {
      console.error('Export failed:', err);
    }
  };

  return (
    <div className="space-y-6">
      <div className="flex items-center justify-between">
        <div>
          <h2 className="text-3xl font-bold tracking-tight">Audit Logs</h2>
          <p className="text-muted-foreground">View and export system audit logs</p>
        </div>
        <div className="space-x-2">
          <Button variant="outline" onClick={() => handleExport('csv')}>
            <Download className="mr-2 h-4 w-4" />
            Export CSV
          </Button>
          <Button variant="outline" onClick={() => handleExport('json')}>
            <Download className="mr-2 h-4 w-4" />
            Export JSON
          </Button>
        </div>
      </div>

      {selectedRecord ? (
        <AuditDetail record={selectedRecord} onClose={() => setSelectedRecord(null)} />
      ) : (
        <Card>
          <CardHeader>
            <CardTitle>Audit Log Entries</CardTitle>
            <CardDescription>Total: {total} entries</CardDescription>
          </CardHeader>
          <CardContent>
            <div className="mb-4 flex flex-wrap items-center gap-2">
              <div className="relative flex-1 min-w-[200px]">
                <Search className="absolute left-2.5 top-2.5 h-4 w-4 text-muted-foreground" />
                <Input
                  placeholder="Search by user ID..."
                  value={search}
                  onChange={(e) => {
                    setSearch(e.target.value);
                    setPage(0);
                  }}
                  className="pl-8"
                />
              </div>
              <select
                value={eventType}
                onChange={(e) => {
                  setEventType(e.target.value);
                  setPage(0);
                }}
                className="h-10 rounded-md border border-input bg-background px-3 py-2 text-sm"
              >
                <option value="">All Events</option>
                <option value="STORE">Store</option>
                <option value="QUERY">Query</option>
                <option value="RETRIEVE">Retrieve</option>
                <option value="ASSOCIATION">Association</option>
                <option value="LOGIN">Login</option>
              </select>
              <select
                value={outcome}
                onChange={(e) => {
                  setOutcome(e.target.value);
                  setPage(0);
                }}
                className="h-10 rounded-md border border-input bg-background px-3 py-2 text-sm"
              >
                <option value="">All Outcomes</option>
                <option value="Success">Success</option>
                <option value="Failure">Failure</option>
              </select>
            </div>

            {isLoading ? (
              <div className="py-8 text-center text-muted-foreground">Loading audit logs...</div>
            ) : error ? (
              <div className="py-8 text-center text-destructive">
                Error loading audit logs. Make sure the API server is running.
              </div>
            ) : records.length === 0 ? (
              <div className="py-8 text-center text-muted-foreground">
                <FileText className="mx-auto mb-4 h-12 w-12 text-muted-foreground/50" />
                <p>No audit log entries found.</p>
              </div>
            ) : (
              <>
                <Table>
                  <TableHeader>
                    <TableRow>
                      <TableHead>Timestamp</TableHead>
                      <TableHead>Event Type</TableHead>
                      <TableHead>User</TableHead>
                      <TableHead>Source AE</TableHead>
                      <TableHead>Message</TableHead>
                      <TableHead>Outcome</TableHead>
                    </TableRow>
                  </TableHeader>
                  <TableBody>
                    {records.map((record) => (
                      <TableRow
                        key={record.pk}
                        className="cursor-pointer"
                        onClick={() => setSelectedRecord(record)}
                      >
                        <TableCell className="whitespace-nowrap">
                          {formatDateTime(record.timestamp)}
                        </TableCell>
                        <TableCell>
                          <Badge variant="outline">{record.event_type}</Badge>
                        </TableCell>
                        <TableCell>{record.user_id || '-'}</TableCell>
                        <TableCell>{record.source_ae || '-'}</TableCell>
                        <TableCell className="max-w-xs truncate">{record.message || '-'}</TableCell>
                        <TableCell>{getOutcomeBadge(record.outcome)}</TableCell>
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
      )}
    </div>
  );
}
