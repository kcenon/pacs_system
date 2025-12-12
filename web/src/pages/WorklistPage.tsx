import { useState } from 'react';
import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';
import { Search, Plus, Calendar, Pencil, Trash2, RefreshCw } from 'lucide-react';
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
import { WorklistForm, DeleteConfirmDialog, StatusUpdateDialog } from '@/components/worklist';
import { apiClient } from '@/api/client';
import { formatDateTime } from '@/lib/utils';
import type { WorklistItem, WorklistItemInput, PaginatedResponse } from '@/types/api';

type WorklistStatus = 'SCHEDULED' | 'IN_PROGRESS' | 'COMPLETED' | 'DISCONTINUED';

function getStatusBadge(status: WorklistStatus) {
  const variants: Record<WorklistStatus, 'default' | 'secondary' | 'success' | 'warning' | 'destructive'> = {
    SCHEDULED: 'default',
    IN_PROGRESS: 'warning',
    COMPLETED: 'success',
    DISCONTINUED: 'destructive',
  };
  const labels: Record<WorklistStatus, string> = {
    SCHEDULED: 'Scheduled',
    IN_PROGRESS: 'In Progress',
    COMPLETED: 'Completed',
    DISCONTINUED: 'Discontinued',
  };
  return <Badge variant={variants[status] || 'secondary'}>{labels[status] || status}</Badge>;
}

export function WorklistPage() {
  const queryClient = useQueryClient();
  const [search, setSearch] = useState('');
  const [statusFilter, setStatusFilter] = useState<string>('');
  const [page, setPage] = useState(0);
  const pageSize = 20;

  const [isFormOpen, setIsFormOpen] = useState(false);
  const [editingItem, setEditingItem] = useState<WorklistItem | null>(null);
  const [deletingItem, setDeletingItem] = useState<WorklistItem | null>(null);
  const [statusUpdateItem, setStatusUpdateItem] = useState<WorklistItem | null>(null);
  const [expandedRow, setExpandedRow] = useState<number | null>(null);

  const { data, isLoading, error } = useQuery<PaginatedResponse<WorklistItem>>({
    queryKey: ['worklist', search, statusFilter, page],
    queryFn: () =>
      apiClient.getWorklist({
        patient_id: search || undefined,
        include_all_status: statusFilter ? undefined : true,
        limit: pageSize,
        offset: page * pageSize,
      }),
  });

  const createMutation = useMutation({
    mutationFn: (item: WorklistItemInput) => apiClient.createWorklistItem(item),
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['worklist'] });
      setIsFormOpen(false);
    },
  });

  const updateStatusMutation = useMutation({
    mutationFn: ({ pk, status }: { pk: number; status: string }) =>
      apiClient.updateWorklistItem(pk, { step_status: status }),
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['worklist'] });
      setStatusUpdateItem(null);
    },
  });

  const deleteMutation = useMutation({
    mutationFn: (pk: number) => apiClient.deleteWorklistItem(pk),
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['worklist'] });
      setDeletingItem(null);
    },
  });

  const items = data?.data || [];
  const total = data?.pagination?.total || 0;
  const totalPages = Math.ceil(total / pageSize);

  const handleCreate = () => {
    setEditingItem(null);
    setIsFormOpen(true);
  };

  const handleEdit = (item: WorklistItem) => {
    setEditingItem(item);
    setIsFormOpen(true);
  };

  const handleFormSubmit = async (formData: WorklistItemInput) => {
    if (editingItem) {
      await updateStatusMutation.mutateAsync({
        pk: editingItem.pk,
        status: formData.step_status || editingItem.step_status,
      });
      setIsFormOpen(false);
      setEditingItem(null);
    } else {
      await createMutation.mutateAsync(formData);
    }
  };

  const handleStatusUpdate = async (newStatus: string) => {
    if (statusUpdateItem) {
      await updateStatusMutation.mutateAsync({
        pk: statusUpdateItem.pk,
        status: newStatus,
      });
    }
  };

  const handleDelete = async () => {
    if (deletingItem) {
      await deleteMutation.mutateAsync(deletingItem.pk);
    }
  };

  const toggleRowExpand = (pk: number) => {
    setExpandedRow(expandedRow === pk ? null : pk);
  };

  return (
    <div className="space-y-6">
      <div className="flex items-center justify-between">
        <div>
          <h2 className="text-3xl font-bold tracking-tight">Worklist</h2>
          <p className="text-muted-foreground">Manage scheduled procedures</p>
        </div>
        <Button onClick={handleCreate}>
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
              <Button variant="outline" className="mt-4" onClick={handleCreate}>
                <Plus className="mr-2 h-4 w-4" />
                Create First Item
              </Button>
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
                    <TableHead className="w-[100px]">Actions</TableHead>
                  </TableRow>
                </TableHeader>
                <TableBody>
                  {items.map((item) => (
                    <>
                      <TableRow
                        key={item.pk}
                        className="cursor-pointer hover:bg-muted/50"
                        onClick={() => toggleRowExpand(item.pk)}
                      >
                        <TableCell>
                          <div className="font-medium">
                            {formatDateTime(item.scheduled_datetime)}
                          </div>
                        </TableCell>
                        <TableCell>
                          <div className="font-medium">{item.patient_name || '-'}</div>
                          <div className="text-sm text-muted-foreground">{item.patient_id}</div>
                        </TableCell>
                        <TableCell>
                          <div>{item.procedure_desc || '-'}</div>
                          <div className="text-sm text-muted-foreground">
                            {item.requested_proc_id || item.step_id}
                          </div>
                        </TableCell>
                        <TableCell>
                          <Badge variant="outline">{item.modality}</Badge>
                        </TableCell>
                        <TableCell>{item.station_ae || '-'}</TableCell>
                        <TableCell>{getStatusBadge(item.step_status)}</TableCell>
                        <TableCell>
                          <div className="flex items-center gap-1" onClick={(e) => e.stopPropagation()}>
                            <Button
                              variant="ghost"
                              size="sm"
                              className="h-8 w-8 p-0"
                              onClick={() => setStatusUpdateItem(item)}
                              title="Update Status"
                            >
                              <RefreshCw className="h-4 w-4" />
                            </Button>
                            <Button
                              variant="ghost"
                              size="sm"
                              className="h-8 w-8 p-0"
                              onClick={() => handleEdit(item)}
                              title="Edit"
                            >
                              <Pencil className="h-4 w-4" />
                            </Button>
                            <Button
                              variant="ghost"
                              size="sm"
                              className="h-8 w-8 p-0 text-destructive hover:text-destructive"
                              onClick={() => setDeletingItem(item)}
                              title="Delete"
                            >
                              <Trash2 className="h-4 w-4" />
                            </Button>
                          </div>
                        </TableCell>
                      </TableRow>
                      {expandedRow === item.pk && (
                        <TableRow key={`${item.pk}-expanded`}>
                          <TableCell colSpan={7} className="bg-muted/30">
                            <div className="p-4 grid grid-cols-2 md:grid-cols-4 gap-4 text-sm">
                              <div>
                                <span className="text-muted-foreground">Step ID:</span>
                                <span className="ml-2 font-medium">{item.step_id}</span>
                              </div>
                              <div>
                                <span className="text-muted-foreground">Accession No:</span>
                                <span className="ml-2 font-medium">{item.accession_no || '-'}</span>
                              </div>
                              <div>
                                <span className="text-muted-foreground">Birth Date:</span>
                                <span className="ml-2 font-medium">{item.birth_date || '-'}</span>
                              </div>
                              <div>
                                <span className="text-muted-foreground">Sex:</span>
                                <span className="ml-2 font-medium">{item.sex || '-'}</span>
                              </div>
                              <div>
                                <span className="text-muted-foreground">Station Name:</span>
                                <span className="ml-2 font-medium">{item.station_name || '-'}</span>
                              </div>
                              <div>
                                <span className="text-muted-foreground">Referring Physician:</span>
                                <span className="ml-2 font-medium">{item.referring_phys || '-'}</span>
                              </div>
                              <div>
                                <span className="text-muted-foreground">Protocol:</span>
                                <span className="ml-2 font-medium">{item.protocol_code || '-'}</span>
                              </div>
                              <div>
                                <span className="text-muted-foreground">Study UID:</span>
                                <span className="ml-2 font-medium text-xs">{item.study_uid || '-'}</span>
                              </div>
                              <div className="col-span-2">
                                <span className="text-muted-foreground">Created:</span>
                                <span className="ml-2 font-medium">{formatDateTime(item.created_at)}</span>
                              </div>
                              <div className="col-span-2">
                                <span className="text-muted-foreground">Updated:</span>
                                <span className="ml-2 font-medium">{formatDateTime(item.updated_at)}</span>
                              </div>
                            </div>
                          </TableCell>
                        </TableRow>
                      )}
                    </>
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

      <WorklistForm
        open={isFormOpen}
        onOpenChange={setIsFormOpen}
        item={editingItem}
        onSubmit={handleFormSubmit}
        isLoading={createMutation.isPending || updateStatusMutation.isPending}
      />

      <DeleteConfirmDialog
        open={Boolean(deletingItem)}
        onOpenChange={(open) => !open && setDeletingItem(null)}
        item={deletingItem}
        onConfirm={handleDelete}
        isLoading={deleteMutation.isPending}
      />

      <StatusUpdateDialog
        open={Boolean(statusUpdateItem)}
        onOpenChange={(open) => !open && setStatusUpdateItem(null)}
        item={statusUpdateItem}
        onConfirm={handleStatusUpdate}
        isLoading={updateStatusMutation.isPending}
      />
    </div>
  );
}
