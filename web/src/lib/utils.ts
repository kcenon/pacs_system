import { clsx, type ClassValue } from 'clsx';
import { twMerge } from 'tailwind-merge';

export function cn(...inputs: ClassValue[]) {
  return twMerge(clsx(inputs));
}

export function formatDate(dateStr: string): string {
  if (!dateStr) return '-';
  // DICOM date format: YYYYMMDD
  if (dateStr.length === 8 && !dateStr.includes('-')) {
    const year = dateStr.substring(0, 4);
    const month = dateStr.substring(4, 6);
    const day = dateStr.substring(6, 8);
    return `${year}-${month}-${day}`;
  }
  return dateStr;
}

export function formatTime(timeStr: string): string {
  if (!timeStr) return '-';
  // DICOM time format: HHMMSS or HHMMSS.ffffff
  if (timeStr.length >= 6 && !timeStr.includes(':')) {
    const hour = timeStr.substring(0, 2);
    const minute = timeStr.substring(2, 4);
    const second = timeStr.substring(4, 6);
    return `${hour}:${minute}:${second}`;
  }
  return timeStr;
}

export function formatDateTime(dateTimeStr: string): string {
  if (!dateTimeStr) return '-';
  try {
    const date = new Date(dateTimeStr);
    return date.toLocaleString();
  } catch {
    return dateTimeStr;
  }
}

export function formatBytes(bytes: number): string {
  if (bytes === 0) return '0 B';
  const k = 1024;
  const sizes = ['B', 'KB', 'MB', 'GB', 'TB'];
  const i = Math.floor(Math.log(bytes) / Math.log(k));
  return `${parseFloat((bytes / Math.pow(k, i)).toFixed(2))} ${sizes[i]}`;
}

export function truncate(str: string, maxLength: number): string {
  if (!str || str.length <= maxLength) return str;
  return `${str.substring(0, maxLength)}...`;
}
