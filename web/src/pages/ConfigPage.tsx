import { useQuery } from '@tanstack/react-query';
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '@/components/ui/card';
import { Badge } from '@/components/ui/badge';
import { ApiErrorNotice } from '@/components/ui/api-error';
import { apiClient } from '@/api/client';
import { formatBytes } from '@/lib/utils';
import type { SystemConfig, SystemVersion } from '@/types/api';

function Setting({ name, children }: { name: string; children: React.ReactNode }) {
  return (
    <div className="flex items-center justify-between border-b py-2 text-sm last:border-b-0">
      <dt className="text-muted-foreground">{name}</dt>
      <dd className="font-medium">{children}</dd>
    </div>
  );
}

/**
 * Shows what the running server reports. The server reads its settings when it starts and has no
 * way to change them while running, so this page offers none: an edit here could only pretend.
 */
export function ConfigPage() {
  const { data: config, isLoading, error } = useQuery<SystemConfig>({
    queryKey: ['system-config'],
    queryFn: () => apiClient.getSystemConfig(),
  });

  const { data: version } = useQuery<SystemVersion>({
    queryKey: ['system-version'],
    queryFn: () => apiClient.getSystemVersion(),
  });

  return (
    <div className="space-y-6">
      <div>
        <h2 className="text-3xl font-bold tracking-tight">Configuration</h2>
        <p className="text-muted-foreground">
          Settings of the running server. To change one, restart the server with new options.
        </p>
      </div>

      <Card>
        <CardHeader>
          <CardTitle>HTTP API</CardTitle>
          <CardDescription>The settings this page was served through</CardDescription>
        </CardHeader>
        <CardContent>
          {isLoading ? (
            <div className="py-8 text-center text-muted-foreground">Loading settings...</div>
          ) : error ? (
            <ApiErrorNotice error={error} what="the settings" />
          ) : config ? (
            <dl>
              <Setting name="Bind address">{config.bind_address}</Setting>
              <Setting name="Port">{config.port}</Setting>
              <Setting name="Worker threads">{config.concurrency}</Setting>
              <Setting name="Request timeout">{config.request_timeout_seconds} s</Setting>
              <Setting name="Largest request body">{formatBytes(config.max_body_size)}</Setting>
              <Setting name="Allowed web origin">{config.cors_allowed_origins || 'None'}</Setting>
              <Setting name="TLS">
                <Badge variant={config.enable_tls ? 'success' : 'warning'}>
                  {config.enable_tls ? 'On' : 'Off'}
                </Badge>
              </Setting>
            </dl>
          ) : null}
        </CardContent>
      </Card>

      {version && (
        <p className="text-center text-xs text-muted-foreground">
          PACS {version.pacs_version}, API {version.api_version}
        </p>
      )}
    </div>
  );
}
