import { useState } from 'react';
import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';
import { useForm } from 'react-hook-form';
import { zodResolver } from '@hookform/resolvers/zod';
import { z } from 'zod';
import {
  Save,
  RefreshCw,
  Shield,
  Server,
  Globe,
  HardDrive,
  FileText,
  Network,
  AlertTriangle,
} from 'lucide-react';
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '@/components/ui/card';
import { Input } from '@/components/ui/input';
import { Button } from '@/components/ui/button';
import { Badge } from '@/components/ui/badge';
import { Switch } from '@/components/ui/switch';
import { Select } from '@/components/ui/select';
import {
  Dialog,
  DialogContent,
  DialogDescription,
  DialogFooter,
  DialogHeader,
  DialogTitle,
} from '@/components/ui/dialog';
import { useToast } from '@/components/ui/toast';
import { apiClient } from '@/api/client';
import type {
  SystemConfig,
  SystemVersion,
  DicomServerConfig,
  StorageConfig,
  LoggingConfig,
} from '@/types/api';

// Validation schemas
const restServerSchema = z.object({
  bind_address: z.string().min(1, 'Bind address is required'),
  port: z.number().min(1).max(65535, 'Port must be between 1 and 65535'),
  concurrency: z.number().min(1).max(100, 'Concurrency must be between 1 and 100'),
  enable_cors: z.boolean(),
  cors_allowed_origins: z.string(),
  enable_tls: z.boolean(),
  tls_cert_path: z.string().optional(),
  tls_key_path: z.string().optional(),
  request_timeout_seconds: z.number().min(1).max(3600, 'Timeout must be between 1 and 3600'),
  max_body_size: z.number().min(1024, 'Minimum body size is 1KB'),
});

const dicomServerSchema = z.object({
  ae_title: z
    .string()
    .min(1, 'AE Title is required')
    .max(16, 'AE Title cannot exceed 16 characters')
    .regex(/^[A-Z0-9_]+$/, 'AE Title must contain only uppercase letters, numbers, and underscores'),
  port: z.number().min(1).max(65535, 'Port must be between 1 and 65535'),
  max_associations: z.number().min(1).max(100, 'Max associations must be between 1 and 100'),
  max_pdu_size: z.number().min(4096).max(1048576, 'PDU size must be between 4KB and 1MB'),
  idle_timeout_seconds: z.number().min(0).max(3600, 'Idle timeout must be between 0 and 3600'),
  association_timeout_seconds: z.number().min(1).max(300, 'Association timeout must be between 1 and 300'),
  ae_whitelist: z.array(z.string()),
  accept_unknown_calling_ae: z.boolean(),
});

const storageSchema = z.object({
  storage_path: z.string().min(1, 'Storage path is required'),
  storage_type: z.enum(['file', 's3', 'azure', 'hsm']),
  max_storage_size_gb: z.number().optional(),
  archive_path: z.string().optional(),
  enable_compression: z.boolean().optional(),
  compression_level: z.number().min(1).max(9).optional(),
});

const loggingSchema = z.object({
  log_directory: z.string().min(1, 'Log directory is required'),
  log_level: z.enum(['trace', 'debug', 'info', 'warn', 'error', 'fatal', 'off']),
  enable_console: z.boolean(),
  enable_file: z.boolean(),
  enable_audit_log: z.boolean(),
  max_file_size_mb: z.number().min(1).max(1000, 'File size must be between 1 and 1000 MB'),
  max_files: z.number().min(1).max(100, 'Max files must be between 1 and 100'),
  audit_log_format: z.enum(['json', 'syslog']),
  async_mode: z.boolean(),
});

type RestServerFormData = z.infer<typeof restServerSchema>;
type DicomServerFormData = z.infer<typeof dicomServerSchema>;
type StorageFormData = z.infer<typeof storageSchema>;
type LoggingFormData = z.infer<typeof loggingSchema>;

// Default values for configs that don't have backend API yet
const defaultDicomConfig: DicomServerConfig = {
  ae_title: 'PACS_SCP',
  port: 11112,
  max_associations: 20,
  max_pdu_size: 16384,
  idle_timeout_seconds: 300,
  association_timeout_seconds: 30,
  ae_whitelist: [],
  accept_unknown_calling_ae: false,
};

const defaultStorageConfig: StorageConfig = {
  storage_path: '/var/pacs/dicom',
  storage_type: 'file',
  max_storage_size_gb: 1000,
  archive_path: '/var/pacs/archive',
  enable_compression: false,
  compression_level: 6,
};

const defaultLoggingConfig: LoggingConfig = {
  log_directory: 'logs',
  log_level: 'info',
  enable_console: true,
  enable_file: true,
  enable_audit_log: true,
  max_file_size_mb: 100,
  max_files: 10,
  audit_log_format: 'json',
  async_mode: true,
};

export function ConfigPage() {
  const queryClient = useQueryClient();
  const { addToast } = useToast();
  const [confirmDialogOpen, setConfirmDialogOpen] = useState(false);
  const [pendingChanges, setPendingChanges] = useState<{
    type: 'rest' | 'dicom' | 'storage' | 'logging';
    data: unknown;
  } | null>(null);
  const [activeTab, setActiveTab] = useState<'rest' | 'dicom' | 'storage' | 'security' | 'logging'>('rest');

  // Queries
  const { data: config, isLoading: configLoading } = useQuery<SystemConfig>({
    queryKey: ['system-config'],
    queryFn: () => apiClient.getSystemConfig(),
  });

  const { data: version } = useQuery<SystemVersion>({
    queryKey: ['system-version'],
    queryFn: () => apiClient.getSystemVersion(),
  });

  // For now, use default configs since backend APIs may not exist
  const dicomConfig = defaultDicomConfig;
  const storageConfig = defaultStorageConfig;
  const loggingConfig = defaultLoggingConfig;

  // Form for REST Server settings
  const restForm = useForm<RestServerFormData>({
    resolver: zodResolver(restServerSchema),
    values: config
      ? {
          bind_address: config.bind_address,
          port: config.port,
          concurrency: config.concurrency,
          enable_cors: config.enable_cors,
          cors_allowed_origins: config.cors_allowed_origins,
          enable_tls: config.enable_tls,
          tls_cert_path: config.tls_cert_path || '',
          tls_key_path: config.tls_key_path || '',
          request_timeout_seconds: config.request_timeout_seconds,
          max_body_size: config.max_body_size,
        }
      : undefined,
  });

  // Form for DICOM Server settings
  const dicomForm = useForm<DicomServerFormData>({
    resolver: zodResolver(dicomServerSchema),
    defaultValues: dicomConfig,
  });

  // Form for Storage settings
  const storageForm = useForm<StorageFormData>({
    resolver: zodResolver(storageSchema),
    defaultValues: storageConfig,
  });

  // Form for Logging settings
  const loggingForm = useForm<LoggingFormData>({
    resolver: zodResolver(loggingSchema),
    defaultValues: loggingConfig,
  });

  // Mutations
  const updateRestMutation = useMutation({
    mutationFn: (newConfig: Partial<SystemConfig>) => apiClient.updateSystemConfig(newConfig),
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['system-config'] });
      addToast({
        title: 'Success',
        description: 'REST server configuration saved successfully.',
        variant: 'success',
      });
    },
    onError: (error: Error) => {
      addToast({
        title: 'Error',
        description: `Failed to save configuration: ${error.message}`,
        variant: 'destructive',
      });
    },
  });

  const updateDicomMutation = useMutation({
    mutationFn: (newConfig: DicomServerFormData) => apiClient.updateDicomConfig(newConfig),
    onSuccess: () => {
      addToast({
        title: 'Success',
        description: 'DICOM server configuration saved successfully.',
        variant: 'success',
      });
    },
    onError: (error: Error) => {
      addToast({
        title: 'Error',
        description: `Failed to save configuration: ${error.message}`,
        variant: 'destructive',
      });
    },
  });

  const updateStorageMutation = useMutation({
    mutationFn: (newConfig: StorageFormData) => apiClient.updateStorageConfig(newConfig),
    onSuccess: () => {
      addToast({
        title: 'Success',
        description: 'Storage configuration saved successfully.',
        variant: 'success',
      });
    },
    onError: (error: Error) => {
      addToast({
        title: 'Error',
        description: `Failed to save configuration: ${error.message}`,
        variant: 'destructive',
      });
    },
  });

  const updateLoggingMutation = useMutation({
    mutationFn: (newConfig: LoggingFormData) => apiClient.updateLoggingConfig(newConfig),
    onSuccess: () => {
      addToast({
        title: 'Success',
        description: 'Logging configuration saved successfully.',
        variant: 'success',
      });
    },
    onError: (error: Error) => {
      addToast({
        title: 'Error',
        description: `Failed to save configuration: ${error.message}`,
        variant: 'destructive',
      });
    },
  });

  // Handlers
  const handleRestSubmit = (data: RestServerFormData) => {
    if (data.enable_tls !== config?.enable_tls) {
      setPendingChanges({ type: 'rest', data });
      setConfirmDialogOpen(true);
    } else {
      updateRestMutation.mutate(data);
    }
  };

  const handleDicomSubmit = (data: DicomServerFormData) => {
    setPendingChanges({ type: 'dicom', data });
    setConfirmDialogOpen(true);
  };

  const handleStorageSubmit = (data: StorageFormData) => {
    setPendingChanges({ type: 'storage', data });
    setConfirmDialogOpen(true);
  };

  const handleLoggingSubmit = (data: LoggingFormData) => {
    updateLoggingMutation.mutate(data);
  };

  const handleConfirmSave = () => {
    if (pendingChanges) {
      switch (pendingChanges.type) {
        case 'rest':
          updateRestMutation.mutate(pendingChanges.data as RestServerFormData);
          break;
        case 'dicom':
          updateDicomMutation.mutate(pendingChanges.data as DicomServerFormData);
          break;
        case 'storage':
          updateStorageMutation.mutate(pendingChanges.data as StorageFormData);
          break;
      }
    }
    setConfirmDialogOpen(false);
    setPendingChanges(null);
  };

  const tabs = [
    { id: 'rest' as const, label: 'REST API', icon: Server },
    { id: 'dicom' as const, label: 'DICOM Server', icon: Network },
    { id: 'storage' as const, label: 'Storage', icon: HardDrive },
    { id: 'security' as const, label: 'Security', icon: Shield },
    { id: 'logging' as const, label: 'Logging', icon: FileText },
  ];

  return (
    <div className="space-y-6">
      {/* Header */}
      <div className="flex items-center justify-between">
        <div>
          <h2 className="text-3xl font-bold tracking-tight">Configuration</h2>
          <p className="text-muted-foreground">Manage PACS system settings</p>
        </div>
      </div>

      {/* Version Info */}
      <Card>
        <CardHeader>
          <CardTitle className="flex items-center">
            <Server className="mr-2 h-5 w-5" />
            System Information
          </CardTitle>
          <CardDescription>Current system version and API information</CardDescription>
        </CardHeader>
        <CardContent>
          <div className="grid gap-4 md:grid-cols-3">
            <div>
              <span className="text-sm text-muted-foreground">API Version</span>
              <p className="font-medium">{version?.api_version || '-'}</p>
            </div>
            <div>
              <span className="text-sm text-muted-foreground">PACS Version</span>
              <p className="font-medium">{version?.pacs_version || '-'}</p>
            </div>
            <div>
              <span className="text-sm text-muted-foreground">Server Version</span>
              <p className="font-medium">{version?.crow_version || '-'}</p>
            </div>
          </div>
        </CardContent>
      </Card>

      {/* Tab Navigation */}
      <div className="flex space-x-1 rounded-lg bg-muted p-1">
        {tabs.map((tab) => (
          <button
            key={tab.id}
            onClick={() => setActiveTab(tab.id)}
            className={`flex items-center space-x-2 rounded-md px-4 py-2 text-sm font-medium transition-colors ${
              activeTab === tab.id
                ? 'bg-background text-foreground shadow-sm'
                : 'text-muted-foreground hover:text-foreground'
            }`}
          >
            <tab.icon className="h-4 w-4" />
            <span>{tab.label}</span>
          </button>
        ))}
      </div>

      {configLoading ? (
        <div className="py-8 text-center text-muted-foreground">Loading configuration...</div>
      ) : (
        <>
          {/* REST API Settings */}
          {activeTab === 'rest' && (
            <form onSubmit={restForm.handleSubmit(handleRestSubmit)}>
              <Card>
                <CardHeader>
                  <CardTitle className="flex items-center">
                    <Server className="mr-2 h-5 w-5" />
                    REST API Server Settings
                  </CardTitle>
                  <CardDescription>Configure the REST API server for web interface</CardDescription>
                </CardHeader>
                <CardContent className="space-y-6">
                  <div className="grid gap-4 md:grid-cols-2">
                    <div className="space-y-2">
                      <label className="text-sm font-medium">Bind Address</label>
                      <Input {...restForm.register('bind_address')} />
                      {restForm.formState.errors.bind_address && (
                        <p className="text-sm text-destructive">
                          {restForm.formState.errors.bind_address.message}
                        </p>
                      )}
                    </div>
                    <div className="space-y-2">
                      <label className="text-sm font-medium">Port</label>
                      <Input
                        type="number"
                        {...restForm.register('port', { valueAsNumber: true })}
                      />
                      {restForm.formState.errors.port && (
                        <p className="text-sm text-destructive">
                          {restForm.formState.errors.port.message}
                        </p>
                      )}
                    </div>
                    <div className="space-y-2">
                      <label className="text-sm font-medium">Concurrency (Worker Threads)</label>
                      <Input
                        type="number"
                        {...restForm.register('concurrency', { valueAsNumber: true })}
                      />
                      {restForm.formState.errors.concurrency && (
                        <p className="text-sm text-destructive">
                          {restForm.formState.errors.concurrency.message}
                        </p>
                      )}
                    </div>
                    <div className="space-y-2">
                      <label className="text-sm font-medium">Request Timeout (seconds)</label>
                      <Input
                        type="number"
                        {...restForm.register('request_timeout_seconds', { valueAsNumber: true })}
                      />
                      {restForm.formState.errors.request_timeout_seconds && (
                        <p className="text-sm text-destructive">
                          {restForm.formState.errors.request_timeout_seconds.message}
                        </p>
                      )}
                    </div>
                    <div className="space-y-2">
                      <label className="text-sm font-medium">Max Body Size (bytes)</label>
                      <Input
                        type="number"
                        {...restForm.register('max_body_size', { valueAsNumber: true })}
                      />
                      {restForm.formState.errors.max_body_size && (
                        <p className="text-sm text-destructive">
                          {restForm.formState.errors.max_body_size.message}
                        </p>
                      )}
                    </div>
                  </div>

                  {/* CORS Settings */}
                  <div className="border-t pt-4">
                    <h4 className="mb-4 flex items-center text-sm font-semibold">
                      <Globe className="mr-2 h-4 w-4" />
                      CORS Settings
                    </h4>
                    <div className="space-y-4">
                      <div className="flex items-center justify-between">
                        <div>
                          <span className="font-medium">Enable CORS</span>
                          <p className="text-sm text-muted-foreground">
                            Allow cross-origin requests from web browsers
                          </p>
                        </div>
                        <Switch
                          checked={restForm.watch('enable_cors')}
                          onCheckedChange={(checked) => restForm.setValue('enable_cors', checked)}
                        />
                      </div>
                      {restForm.watch('enable_cors') && (
                        <div className="space-y-2">
                          <label className="text-sm font-medium">Allowed Origins</label>
                          <Input
                            {...restForm.register('cors_allowed_origins')}
                            placeholder="* or specific origins (comma-separated)"
                          />
                        </div>
                      )}
                    </div>
                  </div>

                  <div className="flex justify-end space-x-2 border-t pt-4">
                    <Button
                      type="button"
                      variant="outline"
                      onClick={() => restForm.reset()}
                      disabled={!restForm.formState.isDirty}
                    >
                      <RefreshCw className="mr-2 h-4 w-4" />
                      Reset
                    </Button>
                    <Button
                      type="submit"
                      disabled={!restForm.formState.isDirty || updateRestMutation.isPending}
                    >
                      <Save className="mr-2 h-4 w-4" />
                      {updateRestMutation.isPending ? 'Saving...' : 'Save Changes'}
                    </Button>
                  </div>
                </CardContent>
              </Card>
            </form>
          )}

          {/* DICOM Server Settings */}
          {activeTab === 'dicom' && (
            <form onSubmit={dicomForm.handleSubmit(handleDicomSubmit)}>
              <Card>
                <CardHeader>
                  <CardTitle className="flex items-center">
                    <Network className="mr-2 h-5 w-5" />
                    DICOM Server Settings
                  </CardTitle>
                  <CardDescription>
                    Configure DICOM network service provider (SCP) settings
                  </CardDescription>
                </CardHeader>
                <CardContent className="space-y-6">
                  <div className="grid gap-4 md:grid-cols-2">
                    <div className="space-y-2">
                      <label className="text-sm font-medium">AE Title</label>
                      <Input
                        {...dicomForm.register('ae_title')}
                        placeholder="PACS_SCP"
                        className="uppercase"
                      />
                      <p className="text-xs text-muted-foreground">
                        Application Entity Title (max 16 characters, uppercase)
                      </p>
                      {dicomForm.formState.errors.ae_title && (
                        <p className="text-sm text-destructive">
                          {dicomForm.formState.errors.ae_title.message}
                        </p>
                      )}
                    </div>
                    <div className="space-y-2">
                      <label className="text-sm font-medium">DICOM Port</label>
                      <Input
                        type="number"
                        {...dicomForm.register('port', { valueAsNumber: true })}
                      />
                      <p className="text-xs text-muted-foreground">
                        Standard DICOM port: 11112
                      </p>
                      {dicomForm.formState.errors.port && (
                        <p className="text-sm text-destructive">
                          {dicomForm.formState.errors.port.message}
                        </p>
                      )}
                    </div>
                    <div className="space-y-2">
                      <label className="text-sm font-medium">Max Associations</label>
                      <Input
                        type="number"
                        {...dicomForm.register('max_associations', { valueAsNumber: true })}
                      />
                      {dicomForm.formState.errors.max_associations && (
                        <p className="text-sm text-destructive">
                          {dicomForm.formState.errors.max_associations.message}
                        </p>
                      )}
                    </div>
                    <div className="space-y-2">
                      <label className="text-sm font-medium">Max PDU Size (bytes)</label>
                      <Input
                        type="number"
                        {...dicomForm.register('max_pdu_size', { valueAsNumber: true })}
                      />
                      {dicomForm.formState.errors.max_pdu_size && (
                        <p className="text-sm text-destructive">
                          {dicomForm.formState.errors.max_pdu_size.message}
                        </p>
                      )}
                    </div>
                    <div className="space-y-2">
                      <label className="text-sm font-medium">Idle Timeout (seconds)</label>
                      <Input
                        type="number"
                        {...dicomForm.register('idle_timeout_seconds', { valueAsNumber: true })}
                      />
                      <p className="text-xs text-muted-foreground">
                        0 = no timeout
                      </p>
                    </div>
                    <div className="space-y-2">
                      <label className="text-sm font-medium">Association Timeout (seconds)</label>
                      <Input
                        type="number"
                        {...dicomForm.register('association_timeout_seconds', { valueAsNumber: true })}
                      />
                    </div>
                  </div>

                  <div className="border-t pt-4">
                    <div className="flex items-center justify-between">
                      <div>
                        <span className="font-medium">Accept Unknown Calling AE</span>
                        <p className="text-sm text-muted-foreground">
                          Accept connections from AE titles not in whitelist
                        </p>
                      </div>
                      <Switch
                        checked={dicomForm.watch('accept_unknown_calling_ae')}
                        onCheckedChange={(checked) =>
                          dicomForm.setValue('accept_unknown_calling_ae', checked)
                        }
                      />
                    </div>
                  </div>

                  <div className="flex justify-end space-x-2 border-t pt-4">
                    <Button
                      type="button"
                      variant="outline"
                      onClick={() => dicomForm.reset()}
                      disabled={!dicomForm.formState.isDirty}
                    >
                      <RefreshCw className="mr-2 h-4 w-4" />
                      Reset
                    </Button>
                    <Button
                      type="submit"
                      disabled={!dicomForm.formState.isDirty || updateDicomMutation.isPending}
                    >
                      <Save className="mr-2 h-4 w-4" />
                      {updateDicomMutation.isPending ? 'Saving...' : 'Save Changes'}
                    </Button>
                  </div>
                </CardContent>
              </Card>
            </form>
          )}

          {/* Storage Settings */}
          {activeTab === 'storage' && (
            <form onSubmit={storageForm.handleSubmit(handleStorageSubmit)}>
              <Card>
                <CardHeader>
                  <CardTitle className="flex items-center">
                    <HardDrive className="mr-2 h-5 w-5" />
                    Storage Settings
                  </CardTitle>
                  <CardDescription>Configure DICOM image storage location and options</CardDescription>
                </CardHeader>
                <CardContent className="space-y-6">
                  <div className="grid gap-4 md:grid-cols-2">
                    <div className="space-y-2">
                      <label className="text-sm font-medium">Storage Path</label>
                      <Input
                        {...storageForm.register('storage_path')}
                        placeholder="/var/pacs/dicom"
                      />
                      {storageForm.formState.errors.storage_path && (
                        <p className="text-sm text-destructive">
                          {storageForm.formState.errors.storage_path.message}
                        </p>
                      )}
                    </div>
                    <div className="space-y-2">
                      <label className="text-sm font-medium">Storage Type</label>
                      <Select
                        {...storageForm.register('storage_type')}
                        options={[
                          { value: 'file', label: 'Local File System' },
                          { value: 's3', label: 'Amazon S3' },
                          { value: 'azure', label: 'Azure Blob Storage' },
                          { value: 'hsm', label: 'Hierarchical Storage (HSM)' },
                        ]}
                      />
                    </div>
                    <div className="space-y-2">
                      <label className="text-sm font-medium">Max Storage Size (GB)</label>
                      <Input
                        type="number"
                        {...storageForm.register('max_storage_size_gb', { valueAsNumber: true })}
                      />
                      <p className="text-xs text-muted-foreground">
                        Leave empty for unlimited
                      </p>
                    </div>
                    <div className="space-y-2">
                      <label className="text-sm font-medium">Archive Path</label>
                      <Input
                        {...storageForm.register('archive_path')}
                        placeholder="/var/pacs/archive"
                      />
                    </div>
                  </div>

                  <div className="border-t pt-4">
                    <div className="flex items-center justify-between">
                      <div>
                        <span className="font-medium">Enable Compression</span>
                        <p className="text-sm text-muted-foreground">
                          Compress DICOM files to save storage space
                        </p>
                      </div>
                      <Switch
                        checked={storageForm.watch('enable_compression') ?? false}
                        onCheckedChange={(checked) =>
                          storageForm.setValue('enable_compression', checked)
                        }
                      />
                    </div>
                    {storageForm.watch('enable_compression') && (
                      <div className="mt-4 space-y-2">
                        <label className="text-sm font-medium">Compression Level (1-9)</label>
                        <Input
                          type="number"
                          min={1}
                          max={9}
                          {...storageForm.register('compression_level', { valueAsNumber: true })}
                        />
                      </div>
                    )}
                  </div>

                  <div className="flex justify-end space-x-2 border-t pt-4">
                    <Button
                      type="button"
                      variant="outline"
                      onClick={() => storageForm.reset()}
                      disabled={!storageForm.formState.isDirty}
                    >
                      <RefreshCw className="mr-2 h-4 w-4" />
                      Reset
                    </Button>
                    <Button
                      type="submit"
                      disabled={!storageForm.formState.isDirty || updateStorageMutation.isPending}
                    >
                      <Save className="mr-2 h-4 w-4" />
                      {updateStorageMutation.isPending ? 'Saving...' : 'Save Changes'}
                    </Button>
                  </div>
                </CardContent>
              </Card>
            </form>
          )}

          {/* Security Settings */}
          {activeTab === 'security' && (
            <form onSubmit={restForm.handleSubmit(handleRestSubmit)}>
              <Card>
                <CardHeader>
                  <CardTitle className="flex items-center">
                    <Shield className="mr-2 h-5 w-5" />
                    Security Settings
                  </CardTitle>
                  <CardDescription>Configure TLS encryption and security options</CardDescription>
                </CardHeader>
                <CardContent className="space-y-6">
                  <div className="flex items-center justify-between">
                    <div>
                      <span className="font-medium">Enable TLS/SSL</span>
                      <p className="text-sm text-muted-foreground">
                        Encrypt connections using TLS/SSL certificates
                      </p>
                    </div>
                    <Switch
                      checked={restForm.watch('enable_tls')}
                      onCheckedChange={(checked) => restForm.setValue('enable_tls', checked)}
                    />
                  </div>

                  {restForm.watch('enable_tls') && (
                    <div className="rounded-lg border border-amber-200 bg-amber-50 p-4 dark:border-amber-800 dark:bg-amber-950">
                      <div className="flex items-start space-x-2">
                        <AlertTriangle className="mt-0.5 h-5 w-5 text-amber-600" />
                        <div>
                          <p className="text-sm font-medium text-amber-800 dark:text-amber-200">
                            TLS Configuration Required
                          </p>
                          <p className="text-sm text-amber-700 dark:text-amber-300">
                            You must provide valid certificate and key files to enable TLS.
                          </p>
                        </div>
                      </div>
                    </div>
                  )}

                  {restForm.watch('enable_tls') && (
                    <div className="grid gap-4 md:grid-cols-2">
                      <div className="space-y-2">
                        <label className="text-sm font-medium">TLS Certificate Path</label>
                        <Input
                          {...restForm.register('tls_cert_path')}
                          placeholder="/etc/pacs/certs/server.crt"
                        />
                        <p className="text-xs text-muted-foreground">
                          Path to the PEM-encoded certificate file
                        </p>
                      </div>
                      <div className="space-y-2">
                        <label className="text-sm font-medium">TLS Private Key Path</label>
                        <Input
                          {...restForm.register('tls_key_path')}
                          placeholder="/etc/pacs/certs/server.key"
                        />
                        <p className="text-xs text-muted-foreground">
                          Path to the PEM-encoded private key file
                        </p>
                      </div>
                    </div>
                  )}

                  <div className="border-t pt-4">
                    <h4 className="mb-4 text-sm font-semibold">Current Status</h4>
                    <div className="flex items-center space-x-2">
                      <Badge variant={config?.enable_tls ? 'success' : 'secondary'}>
                        {config?.enable_tls ? 'TLS Enabled' : 'TLS Disabled'}
                      </Badge>
                      {config?.enable_tls && config.tls_cert_path && (
                        <span className="text-sm text-muted-foreground">
                          Certificate: {config.tls_cert_path}
                        </span>
                      )}
                    </div>
                  </div>

                  <div className="flex justify-end space-x-2 border-t pt-4">
                    <Button
                      type="button"
                      variant="outline"
                      onClick={() => restForm.reset()}
                      disabled={!restForm.formState.isDirty}
                    >
                      <RefreshCw className="mr-2 h-4 w-4" />
                      Reset
                    </Button>
                    <Button
                      type="submit"
                      disabled={!restForm.formState.isDirty || updateRestMutation.isPending}
                    >
                      <Save className="mr-2 h-4 w-4" />
                      {updateRestMutation.isPending ? 'Saving...' : 'Save Changes'}
                    </Button>
                  </div>
                </CardContent>
              </Card>
            </form>
          )}

          {/* Logging Settings */}
          {activeTab === 'logging' && (
            <form onSubmit={loggingForm.handleSubmit(handleLoggingSubmit)}>
              <Card>
                <CardHeader>
                  <CardTitle className="flex items-center">
                    <FileText className="mr-2 h-5 w-5" />
                    Logging Settings
                  </CardTitle>
                  <CardDescription>
                    Configure application logging and audit trail settings
                  </CardDescription>
                </CardHeader>
                <CardContent className="space-y-6">
                  <div className="grid gap-4 md:grid-cols-2">
                    <div className="space-y-2">
                      <label className="text-sm font-medium">Log Directory</label>
                      <Input {...loggingForm.register('log_directory')} placeholder="logs" />
                      {loggingForm.formState.errors.log_directory && (
                        <p className="text-sm text-destructive">
                          {loggingForm.formState.errors.log_directory.message}
                        </p>
                      )}
                    </div>
                    <div className="space-y-2">
                      <label className="text-sm font-medium">Log Level</label>
                      <Select
                        {...loggingForm.register('log_level')}
                        options={[
                          { value: 'trace', label: 'Trace' },
                          { value: 'debug', label: 'Debug' },
                          { value: 'info', label: 'Info' },
                          { value: 'warn', label: 'Warning' },
                          { value: 'error', label: 'Error' },
                          { value: 'fatal', label: 'Fatal' },
                          { value: 'off', label: 'Off' },
                        ]}
                      />
                    </div>
                    <div className="space-y-2">
                      <label className="text-sm font-medium">Max File Size (MB)</label>
                      <Input
                        type="number"
                        {...loggingForm.register('max_file_size_mb', { valueAsNumber: true })}
                      />
                      {loggingForm.formState.errors.max_file_size_mb && (
                        <p className="text-sm text-destructive">
                          {loggingForm.formState.errors.max_file_size_mb.message}
                        </p>
                      )}
                    </div>
                    <div className="space-y-2">
                      <label className="text-sm font-medium">Max Log Files</label>
                      <Input
                        type="number"
                        {...loggingForm.register('max_files', { valueAsNumber: true })}
                      />
                      {loggingForm.formState.errors.max_files && (
                        <p className="text-sm text-destructive">
                          {loggingForm.formState.errors.max_files.message}
                        </p>
                      )}
                    </div>
                    <div className="space-y-2">
                      <label className="text-sm font-medium">Audit Log Format</label>
                      <Select
                        {...loggingForm.register('audit_log_format')}
                        options={[
                          { value: 'json', label: 'JSON' },
                          { value: 'syslog', label: 'Syslog' },
                        ]}
                      />
                    </div>
                  </div>

                  <div className="space-y-4 border-t pt-4">
                    <h4 className="text-sm font-semibold">Output Destinations</h4>
                    <div className="flex items-center justify-between">
                      <div>
                        <span className="font-medium">Console Output</span>
                        <p className="text-sm text-muted-foreground">Log to standard output</p>
                      </div>
                      <Switch
                        checked={loggingForm.watch('enable_console')}
                        onCheckedChange={(checked) =>
                          loggingForm.setValue('enable_console', checked)
                        }
                      />
                    </div>
                    <div className="flex items-center justify-between">
                      <div>
                        <span className="font-medium">File Output</span>
                        <p className="text-sm text-muted-foreground">Log to files with rotation</p>
                      </div>
                      <Switch
                        checked={loggingForm.watch('enable_file')}
                        onCheckedChange={(checked) => loggingForm.setValue('enable_file', checked)}
                      />
                    </div>
                    <div className="flex items-center justify-between">
                      <div>
                        <span className="font-medium">Audit Log</span>
                        <p className="text-sm text-muted-foreground">
                          Separate audit trail for HIPAA compliance
                        </p>
                      </div>
                      <Switch
                        checked={loggingForm.watch('enable_audit_log')}
                        onCheckedChange={(checked) =>
                          loggingForm.setValue('enable_audit_log', checked)
                        }
                      />
                    </div>
                    <div className="flex items-center justify-between">
                      <div>
                        <span className="font-medium">Async Mode</span>
                        <p className="text-sm text-muted-foreground">
                          Use asynchronous logging for better performance
                        </p>
                      </div>
                      <Switch
                        checked={loggingForm.watch('async_mode')}
                        onCheckedChange={(checked) => loggingForm.setValue('async_mode', checked)}
                      />
                    </div>
                  </div>

                  <div className="flex justify-end space-x-2 border-t pt-4">
                    <Button
                      type="button"
                      variant="outline"
                      onClick={() => loggingForm.reset()}
                      disabled={!loggingForm.formState.isDirty}
                    >
                      <RefreshCw className="mr-2 h-4 w-4" />
                      Reset
                    </Button>
                    <Button
                      type="submit"
                      disabled={!loggingForm.formState.isDirty || updateLoggingMutation.isPending}
                    >
                      <Save className="mr-2 h-4 w-4" />
                      {updateLoggingMutation.isPending ? 'Saving...' : 'Save Changes'}
                    </Button>
                  </div>
                </CardContent>
              </Card>
            </form>
          )}
        </>
      )}

      {/* Confirmation Dialog */}
      <Dialog open={confirmDialogOpen} onOpenChange={setConfirmDialogOpen}>
        <DialogContent>
          <DialogHeader>
            <DialogTitle className="flex items-center">
              <AlertTriangle className="mr-2 h-5 w-5 text-amber-500" />
              Confirm Configuration Change
            </DialogTitle>
            <DialogDescription>
              {pendingChanges?.type === 'rest' &&
                'Changing REST server settings may require a server restart to take effect.'}
              {pendingChanges?.type === 'dicom' &&
                'Changing DICOM server settings will affect all active and future associations.'}
              {pendingChanges?.type === 'storage' &&
                'Changing storage settings may affect data accessibility. Ensure the new path is valid and accessible.'}
            </DialogDescription>
          </DialogHeader>
          <DialogFooter>
            <Button variant="outline" onClick={() => setConfirmDialogOpen(false)}>
              Cancel
            </Button>
            <Button onClick={handleConfirmSave}>Confirm & Save</Button>
          </DialogFooter>
        </DialogContent>
      </Dialog>
    </div>
  );
}
