import { useState } from 'react';
import { RefreshCw } from 'lucide-react';
import { Button } from '@/components/ui/button';
import { Select } from '@/components/ui/select';
import {
  Dialog,
  DialogContent,
  DialogHeader,
  DialogFooter,
  DialogTitle,
  DialogDescription,
} from '@/components/ui/dialog';
import type { WorklistItem } from '@/types/api';

interface StatusUpdateDialogProps {
  open: boolean;
  onOpenChange: (open: boolean) => void;
  item: WorklistItem | null;
  onConfirm: (newStatus: string) => Promise<void>;
  isLoading?: boolean;
}

const STATUS_OPTIONS = [
  { value: 'SCHEDULED', label: 'Scheduled' },
  { value: 'IN_PROGRESS', label: 'In Progress' },
  { value: 'COMPLETED', label: 'Completed' },
  { value: 'DISCONTINUED', label: 'Discontinued' },
];

const STATUS_DESCRIPTIONS: Record<string, string> = {
  SCHEDULED: 'The procedure is scheduled and waiting to be performed.',
  IN_PROGRESS: 'The procedure is currently being performed.',
  COMPLETED: 'The procedure has been completed successfully.',
  DISCONTINUED: 'The procedure has been cancelled or discontinued.',
};

export function StatusUpdateDialog({
  open,
  onOpenChange,
  item,
  onConfirm,
  isLoading = false,
}: StatusUpdateDialogProps) {
  const [selectedStatus, setSelectedStatus] = useState<string>('');

  const handleOpenChange = (newOpen: boolean) => {
    if (newOpen && item) {
      setSelectedStatus(item.step_status);
    }
    onOpenChange(newOpen);
  };

  if (!item) return null;

  const handleConfirm = async () => {
    if (selectedStatus && selectedStatus !== item.step_status) {
      await onConfirm(selectedStatus);
    }
  };

  const isChanged = selectedStatus !== item.step_status;

  return (
    <Dialog open={open} onOpenChange={handleOpenChange}>
      <DialogContent className="max-w-md">
        <DialogHeader>
          <div className="flex items-center gap-3">
            <div className="flex h-10 w-10 items-center justify-center rounded-full bg-primary/10">
              <RefreshCw className="h-5 w-5 text-primary" />
            </div>
            <div>
              <DialogTitle>Update Status</DialogTitle>
              <DialogDescription>Change the procedure status</DialogDescription>
            </div>
          </div>
        </DialogHeader>

        <div className="py-4 space-y-4">
          <div className="rounded-md border bg-muted/50 p-3 text-sm">
            <div className="grid grid-cols-2 gap-2">
              <div>
                <span className="text-muted-foreground">Step ID:</span>
                <span className="ml-2 font-medium">{item.step_id}</span>
              </div>
              <div>
                <span className="text-muted-foreground">Patient:</span>
                <span className="ml-2 font-medium">
                  {item.patient_name || item.patient_id}
                </span>
              </div>
            </div>
          </div>

          <div className="space-y-2">
            <label className="text-sm font-medium">New Status</label>
            <Select
              value={selectedStatus}
              onChange={(e) => setSelectedStatus(e.target.value)}
              options={STATUS_OPTIONS}
            />
            {selectedStatus && (
              <p className="text-xs text-muted-foreground">
                {STATUS_DESCRIPTIONS[selectedStatus]}
              </p>
            )}
          </div>

          {isChanged && (
            <div className="rounded-md bg-amber-50 dark:bg-amber-950/20 border border-amber-200 dark:border-amber-800 p-3 text-sm">
              <p className="text-amber-800 dark:text-amber-200">
                Status will change from{' '}
                <span className="font-semibold">{item.step_status}</span> to{' '}
                <span className="font-semibold">{selectedStatus}</span>
              </p>
            </div>
          )}
        </div>

        <DialogFooter>
          <Button
            type="button"
            variant="outline"
            onClick={() => onOpenChange(false)}
            disabled={isLoading}
          >
            Cancel
          </Button>
          <Button
            type="button"
            onClick={handleConfirm}
            disabled={isLoading || !isChanged}
          >
            {isLoading ? 'Updating...' : 'Update Status'}
          </Button>
        </DialogFooter>
      </DialogContent>
    </Dialog>
  );
}
