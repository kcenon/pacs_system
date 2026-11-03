import { useState, type FormEvent } from 'react';
import { Navigate } from 'react-router-dom';
import { Button } from '@/components/ui/button';
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '@/components/ui/card';
import { Textarea } from '@/components/ui/textarea';
import { ApiRequestError, apiClient } from '@/api/client';
import { setAccessToken, useAccessToken } from '@/lib/session';

function refusal(error: unknown): string {
  if (error instanceof ApiRequestError) {
    if (error.status === 401) {
      return `The server refused this token: ${error.message}`;
    }
    if (error.status === 403) {
      return 'This token is valid but holds no PACS scope (pacs.read, pacs.write or pacs.admin).';
    }
    if (error.status !== undefined) {
      return `The server could not check this token (${error.status}): ${error.message}`;
    }
  }
  return 'Could not reach the server to check this token.';
}

/**
 * The server accepts OAuth 2.0 bearer tokens from one issuer and has no accounts of its own, so
 * signing in means handing it a token from that issuer.
 */
export function SignInPage() {
  const signedIn = useAccessToken();
  const [token, setToken] = useState('');
  const [error, setError] = useState<string | null>(null);
  const [checking, setChecking] = useState(false);

  if (signedIn) {
    return <Navigate to="/" replace />;
  }

  const submit = async (event: FormEvent) => {
    event.preventDefault();
    setChecking(true);
    setError(null);
    try {
      const candidate = token.trim();
      await apiClient.checkAccessToken(candidate);
      setAccessToken(candidate);
    } catch (failure) {
      setError(refusal(failure));
    } finally {
      setChecking(false);
    }
  };

  return (
    <div className="flex h-screen items-center justify-center bg-background p-6">
      <Card className="w-full max-w-lg">
        <CardHeader>
          <CardTitle>Sign in to PACS Administration</CardTitle>
          <CardDescription>
            Paste an access token from the identity provider this server trusts.
          </CardDescription>
        </CardHeader>
        <CardContent>
          <form onSubmit={submit} className="space-y-4">
            <div className="space-y-2">
              <label htmlFor="access-token" className="text-sm font-medium">
                Access token
              </label>
              <Textarea
                id="access-token"
                rows={6}
                autoComplete="off"
                spellCheck={false}
                value={token}
                onChange={(event) => setToken(event.target.value)}
              />
            </div>
            {error && (
              <div role="alert" className="text-sm text-destructive">
                {error}
              </div>
            )}
            <Button type="submit" className="w-full" disabled={checking || token.trim() === ''}>
              {checking ? 'Checking...' : 'Sign in'}
            </Button>
          </form>
        </CardContent>
      </Card>
    </div>
  );
}
