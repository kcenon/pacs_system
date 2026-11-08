import { ApiRequestError } from '@/api/client';

function describe(error: unknown, what: string): string {
  if (error instanceof ApiRequestError) {
    if (error.status === 403) {
      return `Your access token does not allow viewing ${what}.`;
    }
    if (error.status === undefined) {
      return `Could not reach the server to load ${what}.`;
    }
    return `The server could not load ${what} (${error.status}): ${error.message}`;
  }
  return `Could not load ${what}.`;
}

/** Shows why a request failed, with the words the server gave. */
export function ApiErrorNotice({ error, what }: { error: unknown; what: string }) {
  return (
    <div role="alert" className="py-8 text-center text-destructive">
      {describe(error, what)}
    </div>
  );
}
