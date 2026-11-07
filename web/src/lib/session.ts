import { useSyncExternalStore } from 'react';

// Kept for the tab only: closing it ends the session. A token in storage is readable by any
// script on this origin, which is why the page loads no third-party script.
const STORAGE_KEY = 'pacs.accessToken';

const listeners = new Set<() => void>();

function notify() {
  listeners.forEach((listener) => listener());
}

function subscribe(listener: () => void) {
  listeners.add(listener);
  return () => {
    listeners.delete(listener);
  };
}

export function getAccessToken(): string | null {
  return sessionStorage.getItem(STORAGE_KEY);
}

export function setAccessToken(token: string) {
  sessionStorage.setItem(STORAGE_KEY, token);
  notify();
}

export function clearAccessToken() {
  sessionStorage.removeItem(STORAGE_KEY);
  notify();
}

export function useAccessToken(): string | null {
  return useSyncExternalStore(subscribe, getAccessToken);
}
