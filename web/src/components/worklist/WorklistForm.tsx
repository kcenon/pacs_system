import { useState, useMemo } from 'react';
import { Input } from '@/components/ui/input';
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
import type { WorklistItem, WorklistItemInput } from '@/types/api';

interface WorklistFormProps {
  open: boolean;
  onOpenChange: (open: boolean) => void;
  item?: WorklistItem | null;
  onSubmit: (data: WorklistItemInput) => Promise<void>;
  isLoading?: boolean;
}

const MODALITY_OPTIONS = [
  { value: 'CR', label: 'CR - Computed Radiography' },
  { value: 'CT', label: 'CT - Computed Tomography' },
  { value: 'DX', label: 'DX - Digital Radiography' },
  { value: 'MR', label: 'MR - Magnetic Resonance' },
  { value: 'NM', label: 'NM - Nuclear Medicine' },
  { value: 'PT', label: 'PT - PET' },
  { value: 'RF', label: 'RF - Radiofluoroscopy' },
  { value: 'US', label: 'US - Ultrasound' },
  { value: 'XA', label: 'XA - X-Ray Angiography' },
];

const SEX_OPTIONS = [
  { value: 'M', label: 'Male' },
  { value: 'F', label: 'Female' },
  { value: 'O', label: 'Other' },
];

function generateStepId(): string {
  const timestamp = Date.now().toString(36);
  const random = Math.random().toString(36).substring(2, 8);
  return `SPS-${timestamp}-${random}`.toUpperCase();
}

function generateStudyUid(): string {
  const root = '1.2.826.0.1.3680043.8.498';
  const timestamp = Date.now();
  const random = Math.floor(Math.random() * 1000000);
  return `${root}.${timestamp}.${random}`;
}

function formatDateTimeLocal(isoString: string): string {
  if (!isoString) return '';
  try {
    const date = new Date(isoString);
    const offset = date.getTimezoneOffset();
    const localDate = new Date(date.getTime() - offset * 60 * 1000);
    return localDate.toISOString().slice(0, 16);
  } catch {
    return '';
  }
}

function formatToIso(dateTimeLocal: string): string {
  if (!dateTimeLocal) return '';
  try {
    const date = new Date(dateTimeLocal);
    return date.toISOString();
  } catch {
    return '';
  }
}

function getInitialFormData(item?: WorklistItem | null): WorklistItemInput {
  if (item) {
    return {
      step_id: item.step_id,
      patient_id: item.patient_id,
      patient_name: item.patient_name || '',
      birth_date: item.birth_date || '',
      sex: item.sex || '',
      accession_no: item.accession_no || '',
      requested_proc_id: item.requested_proc_id || '',
      study_uid: item.study_uid || '',
      scheduled_datetime: formatDateTimeLocal(item.scheduled_datetime),
      station_ae: item.station_ae || '',
      station_name: item.station_name || '',
      modality: item.modality,
      procedure_desc: item.procedure_desc || '',
      protocol_code: item.protocol_code || '',
      referring_phys: item.referring_phys || '',
      referring_phys_id: item.referring_phys_id || '',
    };
  }

  const now = new Date();
  now.setMinutes(now.getMinutes() + 30);
  return {
    step_id: generateStepId(),
    patient_id: '',
    patient_name: '',
    birth_date: '',
    sex: '',
    accession_no: '',
    requested_proc_id: '',
    study_uid: generateStudyUid(),
    scheduled_datetime: now.toISOString().slice(0, 16),
    station_ae: '',
    station_name: '',
    modality: '',
    procedure_desc: '',
    protocol_code: '',
    referring_phys: '',
    referring_phys_id: '',
  };
}

interface WorklistFormContentProps {
  item?: WorklistItem | null;
  onSubmit: (data: WorklistItemInput) => Promise<void>;
  onCancel: () => void;
  isLoading?: boolean;
}

function WorklistFormContent({
  item,
  onSubmit,
  onCancel,
  isLoading = false,
}: WorklistFormContentProps) {
  const isEdit = Boolean(item);

  const initialData = useMemo(() => getInitialFormData(item), [item]);
  const [formData, setFormData] = useState<WorklistItemInput>(initialData);
  const [errors, setErrors] = useState<Record<string, string>>({});

  const handleChange = (field: keyof WorklistItemInput, value: string) => {
    setFormData((prev) => ({ ...prev, [field]: value }));
    if (errors[field]) {
      setErrors((prev) => {
        const next = { ...prev };
        delete next[field];
        return next;
      });
    }
  };

  const validate = (): boolean => {
    const newErrors: Record<string, string> = {};

    if (!formData.step_id.trim()) {
      newErrors.step_id = 'Step ID is required';
    }
    if (!formData.patient_id.trim()) {
      newErrors.patient_id = 'Patient ID is required';
    }
    if (!formData.modality) {
      newErrors.modality = 'Modality is required';
    }
    if (!formData.scheduled_datetime) {
      newErrors.scheduled_datetime = 'Scheduled date/time is required';
    }

    setErrors(newErrors);
    return Object.keys(newErrors).length === 0;
  };

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!validate()) return;

    const submitData: WorklistItemInput = {
      ...formData,
      scheduled_datetime: formatToIso(formData.scheduled_datetime),
    };

    await onSubmit(submitData);
  };

  return (
    <form onSubmit={handleSubmit} className="space-y-4">
      <div className="grid grid-cols-2 gap-4">
        <div className="space-y-2">
          <label className="text-sm font-medium">
            Step ID <span className="text-destructive">*</span>
          </label>
          <Input
            value={formData.step_id}
            onChange={(e) => handleChange('step_id', e.target.value)}
            placeholder="SPS-..."
            disabled={isEdit}
          />
          {errors.step_id && <p className="text-xs text-destructive">{errors.step_id}</p>}
        </div>

        <div className="space-y-2">
          <label className="text-sm font-medium">
            Modality <span className="text-destructive">*</span>
          </label>
          <Select
            value={formData.modality}
            onChange={(e) => handleChange('modality', e.target.value)}
            options={MODALITY_OPTIONS}
            placeholder="Select modality"
          />
          {errors.modality && <p className="text-xs text-destructive">{errors.modality}</p>}
        </div>
      </div>

      <div className="grid grid-cols-2 gap-4">
        <div className="space-y-2">
          <label className="text-sm font-medium">
            Scheduled Date/Time <span className="text-destructive">*</span>
          </label>
          <Input
            type="datetime-local"
            value={formData.scheduled_datetime}
            onChange={(e) => handleChange('scheduled_datetime', e.target.value)}
          />
          {errors.scheduled_datetime && (
            <p className="text-xs text-destructive">{errors.scheduled_datetime}</p>
          )}
        </div>

        <div className="space-y-2">
          <label className="text-sm font-medium">Accession Number</label>
          <Input
            value={formData.accession_no}
            onChange={(e) => handleChange('accession_no', e.target.value)}
            placeholder="e.g., ACC001"
          />
        </div>
      </div>

      <div className="border-t pt-4">
        <h4 className="text-sm font-semibold mb-3">Patient Information</h4>
        <div className="grid grid-cols-2 gap-4">
          <div className="space-y-2">
            <label className="text-sm font-medium">
              Patient ID <span className="text-destructive">*</span>
            </label>
            <Input
              value={formData.patient_id}
              onChange={(e) => handleChange('patient_id', e.target.value)}
              placeholder="e.g., PAT001"
            />
            {errors.patient_id && (
              <p className="text-xs text-destructive">{errors.patient_id}</p>
            )}
          </div>

          <div className="space-y-2">
            <label className="text-sm font-medium">Patient Name</label>
            <Input
              value={formData.patient_name}
              onChange={(e) => handleChange('patient_name', e.target.value)}
              placeholder="e.g., Doe^John"
            />
          </div>
        </div>

        <div className="grid grid-cols-2 gap-4 mt-4">
          <div className="space-y-2">
            <label className="text-sm font-medium">Birth Date</label>
            <Input
              type="date"
              value={formData.birth_date}
              onChange={(e) => handleChange('birth_date', e.target.value)}
            />
          </div>

          <div className="space-y-2">
            <label className="text-sm font-medium">Sex</label>
            <Select
              value={formData.sex}
              onChange={(e) => handleChange('sex', e.target.value)}
              options={SEX_OPTIONS}
              placeholder="Select sex"
            />
          </div>
        </div>
      </div>

      <div className="border-t pt-4">
        <h4 className="text-sm font-semibold mb-3">Procedure Details</h4>
        <div className="grid grid-cols-2 gap-4">
          <div className="space-y-2">
            <label className="text-sm font-medium">Procedure Description</label>
            <Input
              value={formData.procedure_desc}
              onChange={(e) => handleChange('procedure_desc', e.target.value)}
              placeholder="e.g., Chest X-Ray"
            />
          </div>

          <div className="space-y-2">
            <label className="text-sm font-medium">Protocol Code</label>
            <Input
              value={formData.protocol_code}
              onChange={(e) => handleChange('protocol_code', e.target.value)}
              placeholder="e.g., CHEST_PA"
            />
          </div>
        </div>

        <div className="grid grid-cols-2 gap-4 mt-4">
          <div className="space-y-2">
            <label className="text-sm font-medium">Requested Procedure ID</label>
            <Input
              value={formData.requested_proc_id}
              onChange={(e) => handleChange('requested_proc_id', e.target.value)}
              placeholder="e.g., REQ001"
            />
          </div>

          <div className="space-y-2">
            <label className="text-sm font-medium">Study Instance UID</label>
            <Input
              value={formData.study_uid}
              onChange={(e) => handleChange('study_uid', e.target.value)}
              placeholder="Auto-generated"
              disabled={isEdit}
            />
          </div>
        </div>
      </div>

      <div className="border-t pt-4">
        <h4 className="text-sm font-semibold mb-3">Station & Physician</h4>
        <div className="grid grid-cols-2 gap-4">
          <div className="space-y-2">
            <label className="text-sm font-medium">Station AE Title</label>
            <Input
              value={formData.station_ae}
              onChange={(e) => handleChange('station_ae', e.target.value)}
              placeholder="e.g., CT_SCANNER_1"
            />
          </div>

          <div className="space-y-2">
            <label className="text-sm font-medium">Station Name</label>
            <Input
              value={formData.station_name}
              onChange={(e) => handleChange('station_name', e.target.value)}
              placeholder="e.g., CT Room 1"
            />
          </div>
        </div>

        <div className="grid grid-cols-2 gap-4 mt-4">
          <div className="space-y-2">
            <label className="text-sm font-medium">Referring Physician</label>
            <Input
              value={formData.referring_phys}
              onChange={(e) => handleChange('referring_phys', e.target.value)}
              placeholder="e.g., Dr. Smith"
            />
          </div>

          <div className="space-y-2">
            <label className="text-sm font-medium">Referring Physician ID</label>
            <Input
              value={formData.referring_phys_id}
              onChange={(e) => handleChange('referring_phys_id', e.target.value)}
              placeholder="e.g., PHY001"
            />
          </div>
        </div>
      </div>

      <DialogFooter className="pt-4">
        <Button
          type="button"
          variant="outline"
          onClick={onCancel}
          disabled={isLoading}
        >
          Cancel
        </Button>
        <Button type="submit" disabled={isLoading}>
          {isLoading ? 'Saving...' : isEdit ? 'Update' : 'Create'}
        </Button>
      </DialogFooter>
    </form>
  );
}

export function WorklistForm({
  open,
  onOpenChange,
  item,
  onSubmit,
  isLoading = false,
}: WorklistFormProps) {
  const isEdit = Boolean(item);
  const formKey = item?.pk ?? 'new';

  return (
    <Dialog open={open} onOpenChange={onOpenChange}>
      <DialogContent className="max-w-2xl max-h-[90vh] overflow-y-auto">
        <DialogHeader>
          <DialogTitle>{isEdit ? 'Edit Worklist Item' : 'Create New Worklist Item'}</DialogTitle>
          <DialogDescription>
            {isEdit
              ? 'Update the scheduled procedure information below.'
              : 'Fill in the details to schedule a new procedure.'}
          </DialogDescription>
        </DialogHeader>

        {open && (
          <WorklistFormContent
            key={formKey}
            item={item}
            onSubmit={onSubmit}
            onCancel={() => onOpenChange(false)}
            isLoading={isLoading}
          />
        )}
      </DialogContent>
    </Dialog>
  );
}
