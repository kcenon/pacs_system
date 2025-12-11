import { BrowserRouter, Routes, Route } from 'react-router-dom';
import { QueryClient, QueryClientProvider } from '@tanstack/react-query';
import { Layout } from '@/components/layout/Layout';
import { DashboardPage } from '@/pages/DashboardPage';
import { PatientsPage } from '@/pages/PatientsPage';
import { WorklistPage } from '@/pages/WorklistPage';
import { AuditPage } from '@/pages/AuditPage';
import { ConfigPage } from '@/pages/ConfigPage';
import { AssociationsPage } from '@/pages/AssociationsPage';

const queryClient = new QueryClient({
  defaultOptions: {
    queries: {
      staleTime: 1000 * 60, // 1 minute
      retry: 1,
      refetchOnWindowFocus: false,
    },
  },
});

function App() {
  return (
    <QueryClientProvider client={queryClient}>
      <BrowserRouter>
        <Routes>
          <Route path="/" element={<Layout />}>
            <Route index element={<DashboardPage />} />
            <Route path="patients" element={<PatientsPage />} />
            <Route path="worklist" element={<WorklistPage />} />
            <Route path="audit" element={<AuditPage />} />
            <Route path="config" element={<ConfigPage />} />
            <Route path="associations" element={<AssociationsPage />} />
          </Route>
        </Routes>
      </BrowserRouter>
    </QueryClientProvider>
  );
}

export default App;
