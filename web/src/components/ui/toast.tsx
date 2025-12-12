import * as React from 'react';
import { X } from 'lucide-react';
import { cn } from '@/lib/utils';

export interface Toast {
  id: string;
  title?: string;
  description?: string;
  variant?: 'default' | 'success' | 'destructive';
  duration?: number;
}

interface ToastContextValue {
  toasts: Toast[];
  addToast: (toast: Omit<Toast, 'id'>) => void;
  removeToast: (id: string) => void;
}

const ToastContext = React.createContext<ToastContextValue | undefined>(undefined);

export function useToast() {
  const context = React.useContext(ToastContext);
  if (!context) {
    throw new Error('useToast must be used within a ToastProvider');
  }
  return context;
}

export function ToastProvider({ children }: { children: React.ReactNode }) {
  const [toasts, setToasts] = React.useState<Toast[]>([]);

  const addToast = React.useCallback((toast: Omit<Toast, 'id'>) => {
    const id = Math.random().toString(36).substring(2, 9);
    const duration = toast.duration ?? 5000;

    setToasts((prev) => [...prev, { ...toast, id }]);

    if (duration > 0) {
      setTimeout(() => {
        setToasts((prev) => prev.filter((t) => t.id !== id));
      }, duration);
    }
  }, []);

  const removeToast = React.useCallback((id: string) => {
    setToasts((prev) => prev.filter((t) => t.id !== id));
  }, []);

  return (
    <ToastContext.Provider value={{ toasts, addToast, removeToast }}>
      {children}
      <ToastContainer />
    </ToastContext.Provider>
  );
}

function ToastContainer() {
  const { toasts, removeToast } = useToast();

  return (
    <div className="fixed bottom-4 right-4 z-50 flex flex-col gap-2">
      {toasts.map((toast) => (
        <ToastItem key={toast.id} toast={toast} onClose={() => removeToast(toast.id)} />
      ))}
    </div>
  );
}

interface ToastItemProps {
  toast: Toast;
  onClose: () => void;
}

function ToastItem({ toast, onClose }: ToastItemProps) {
  const variants = {
    default: 'bg-background border',
    success: 'bg-green-50 border-green-200 dark:bg-green-950 dark:border-green-800',
    destructive: 'bg-red-50 border-red-200 dark:bg-red-950 dark:border-red-800',
  };

  const titleVariants = {
    default: 'text-foreground',
    success: 'text-green-800 dark:text-green-200',
    destructive: 'text-red-800 dark:text-red-200',
  };

  const descVariants = {
    default: 'text-muted-foreground',
    success: 'text-green-700 dark:text-green-300',
    destructive: 'text-red-700 dark:text-red-300',
  };

  const variant = toast.variant ?? 'default';

  return (
    <div
      className={cn(
        'pointer-events-auto w-80 rounded-lg p-4 shadow-lg transition-all',
        'animate-in slide-in-from-right-full',
        variants[variant]
      )}
    >
      <div className="flex items-start gap-3">
        <div className="flex-1">
          {toast.title && (
            <p className={cn('text-sm font-semibold', titleVariants[variant])}>{toast.title}</p>
          )}
          {toast.description && (
            <p className={cn('text-sm', descVariants[variant])}>{toast.description}</p>
          )}
        </div>
        <button
          onClick={onClose}
          className="rounded-md p-1 opacity-70 hover:opacity-100 focus:outline-none focus:ring-2"
        >
          <X className="h-4 w-4" />
        </button>
      </div>
    </div>
  );
}

export { ToastContext };
