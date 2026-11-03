import { BrowserRouter, Navigate, Outlet, Routes, Route } from 'react-router-dom';
import { QueryClient, QueryClientProvider } from '@tanstack/react-query';
import { Layout } from '@/components/layout/Layout';
import { ToastProvider } from '@/components/ui/toast';
import { ApiRequestError } from '@/api/client';
import { useAccessToken } from '@/lib/session';
import { DashboardPage } from '@/pages/DashboardPage';
import { PatientsPage } from '@/pages/PatientsPage';
import { WorklistPage } from '@/pages/WorklistPage';
import { AuditPage } from '@/pages/AuditPage';
import { ConfigPage } from '@/pages/ConfigPage';
import { AssociationsPage } from '@/pages/AssociationsPage';
import { SignInPage } from '@/pages/SignInPage';

const queryClient = new QueryClient({
  defaultOptions: {
    queries: {
      staleTime: 1000 * 60, // 1 minute
      // A refusal will not change by asking again; a failure on the way might.
      retry: (failures, error) =>
        failures < 1 &&
        !(error instanceof ApiRequestError && error.status !== undefined && error.status < 500),
      refetchOnWindowFocus: false,
    },
  },
});

function RequireSession() {
  const token = useAccessToken();
  return token ? <Outlet /> : <Navigate to="/sign-in" replace />;
}

function App() {
  return (
    <QueryClientProvider client={queryClient}>
      <ToastProvider>
        <BrowserRouter>
          <Routes>
            <Route path="/sign-in" element={<SignInPage />} />
            <Route element={<RequireSession />}>
              <Route path="/" element={<Layout />}>
                <Route index element={<DashboardPage />} />
                <Route path="patients" element={<PatientsPage />} />
                <Route path="worklist" element={<WorklistPage />} />
                <Route path="audit" element={<AuditPage />} />
                <Route path="config" element={<ConfigPage />} />
                <Route path="associations" element={<AssociationsPage />} />
              </Route>
            </Route>
          </Routes>
        </BrowserRouter>
      </ToastProvider>
    </QueryClientProvider>
  );
}

export default App;
