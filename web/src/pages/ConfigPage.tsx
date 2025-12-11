import { useState } from 'react';
import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';
import { Save, RefreshCw, Shield, Server, Globe } from 'lucide-react';
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '@/components/ui/card';
import { Input } from '@/components/ui/input';
import { Button } from '@/components/ui/button';
import { Badge } from '@/components/ui/badge';
import { apiClient } from '@/api/client';
import type { SystemConfig, SystemVersion } from '@/types/api';

export function ConfigPage() {
  const queryClient = useQueryClient();
  const [editedConfig, setEditedConfig] = useState<Partial<SystemConfig>>({});
  const [hasChanges, setHasChanges] = useState(false);

  const { data: config, isLoading: configLoading } = useQuery<SystemConfig>({
    queryKey: ['system-config'],
    queryFn: () => apiClient.getSystemConfig(),
  });

  const { data: version } = useQuery<SystemVersion>({
    queryKey: ['system-version'],
    queryFn: () => apiClient.getSystemVersion(),
  });

  const updateMutation = useMutation({
    mutationFn: (newConfig: Partial<SystemConfig>) => apiClient.updateSystemConfig(newConfig),
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['system-config'] });
      setHasChanges(false);
      setEditedConfig({});
    },
  });

  const handleInputChange = (field: keyof SystemConfig, value: string | number | boolean) => {
    setEditedConfig((prev) => ({ ...prev, [field]: value }));
    setHasChanges(true);
  };

  const handleSave = () => {
    updateMutation.mutate(editedConfig);
  };

  const handleReset = () => {
    setEditedConfig({});
    setHasChanges(false);
  };

  const currentConfig = { ...config, ...editedConfig };

  return (
    <div className="space-y-6">
      <div className="flex items-center justify-between">
        <div>
          <h2 className="text-3xl font-bold tracking-tight">Configuration</h2>
          <p className="text-muted-foreground">Manage PACS system settings</p>
        </div>
        <div className="space-x-2">
          <Button variant="outline" onClick={handleReset} disabled={!hasChanges}>
            <RefreshCw className="mr-2 h-4 w-4" />
            Reset
          </Button>
          <Button onClick={handleSave} disabled={!hasChanges || updateMutation.isPending}>
            <Save className="mr-2 h-4 w-4" />
            {updateMutation.isPending ? 'Saving...' : 'Save Changes'}
          </Button>
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

      {configLoading ? (
        <div className="py-8 text-center text-muted-foreground">Loading configuration...</div>
      ) : (
        <>
          {/* Server Settings */}
          <Card>
            <CardHeader>
              <CardTitle className="flex items-center">
                <Server className="mr-2 h-5 w-5" />
                Server Settings
              </CardTitle>
              <CardDescription>REST API server configuration</CardDescription>
            </CardHeader>
            <CardContent>
              <div className="grid gap-4 md:grid-cols-2">
                <div className="space-y-2">
                  <label className="text-sm font-medium">Bind Address</label>
                  <Input
                    value={currentConfig.bind_address || ''}
                    onChange={(e) => handleInputChange('bind_address', e.target.value)}
                  />
                </div>
                <div className="space-y-2">
                  <label className="text-sm font-medium">Port</label>
                  <Input
                    type="number"
                    value={currentConfig.port || ''}
                    onChange={(e) => handleInputChange('port', parseInt(e.target.value))}
                  />
                </div>
                <div className="space-y-2">
                  <label className="text-sm font-medium">Concurrency</label>
                  <Input
                    type="number"
                    value={currentConfig.concurrency || ''}
                    onChange={(e) => handleInputChange('concurrency', parseInt(e.target.value))}
                  />
                </div>
                <div className="space-y-2">
                  <label className="text-sm font-medium">Request Timeout (seconds)</label>
                  <Input
                    type="number"
                    value={currentConfig.request_timeout_seconds || ''}
                    onChange={(e) =>
                      handleInputChange('request_timeout_seconds', parseInt(e.target.value))
                    }
                  />
                </div>
                <div className="space-y-2">
                  <label className="text-sm font-medium">Max Body Size (bytes)</label>
                  <Input
                    type="number"
                    value={currentConfig.max_body_size || ''}
                    onChange={(e) => handleInputChange('max_body_size', parseInt(e.target.value))}
                  />
                </div>
              </div>
            </CardContent>
          </Card>

          {/* CORS Settings */}
          <Card>
            <CardHeader>
              <CardTitle className="flex items-center">
                <Globe className="mr-2 h-5 w-5" />
                CORS Settings
              </CardTitle>
              <CardDescription>Cross-Origin Resource Sharing configuration</CardDescription>
            </CardHeader>
            <CardContent>
              <div className="space-y-4">
                <div className="flex items-center justify-between">
                  <div>
                    <span className="font-medium">Enable CORS</span>
                    <p className="text-sm text-muted-foreground">
                      Allow cross-origin requests from web browsers
                    </p>
                  </div>
                  <Badge variant={currentConfig.enable_cors ? 'success' : 'secondary'}>
                    {currentConfig.enable_cors ? 'Enabled' : 'Disabled'}
                  </Badge>
                </div>
                <div className="space-y-2">
                  <label className="text-sm font-medium">Allowed Origins</label>
                  <Input
                    value={currentConfig.cors_allowed_origins || ''}
                    onChange={(e) => handleInputChange('cors_allowed_origins', e.target.value)}
                    placeholder="* or specific origins"
                  />
                </div>
              </div>
            </CardContent>
          </Card>

          {/* Security Settings */}
          <Card>
            <CardHeader>
              <CardTitle className="flex items-center">
                <Shield className="mr-2 h-5 w-5" />
                Security Settings
              </CardTitle>
              <CardDescription>TLS and encryption configuration</CardDescription>
            </CardHeader>
            <CardContent>
              <div className="flex items-center justify-between">
                <div>
                  <span className="font-medium">Enable TLS</span>
                  <p className="text-sm text-muted-foreground">
                    Encrypt connections using TLS/SSL
                  </p>
                </div>
                <Badge variant={currentConfig.enable_tls ? 'success' : 'secondary'}>
                  {currentConfig.enable_tls ? 'Enabled' : 'Disabled'}
                </Badge>
              </div>
            </CardContent>
          </Card>
        </>
      )}

      {updateMutation.isError && (
        <div className="rounded-md bg-destructive/10 p-4 text-destructive">
          Error saving configuration. Please try again.
        </div>
      )}

      {updateMutation.isSuccess && (
        <div className="rounded-md bg-green-500/10 p-4 text-green-600">
          Configuration saved successfully.
        </div>
      )}
    </div>
  );
}
