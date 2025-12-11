import { useState } from 'react';
import { useQuery } from '@tanstack/react-query';
import { Search, ChevronRight, User } from 'lucide-react';
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '@/components/ui/card';
import { Input } from '@/components/ui/input';
import { Button } from '@/components/ui/button';
import { Badge } from '@/components/ui/badge';
import {
  Table,
  TableBody,
  TableCell,
  TableHead,
  TableHeader,
  TableRow,
} from '@/components/ui/table';
import { apiClient } from '@/api/client';
import { formatDate } from '@/lib/utils';
import type { Patient, Study, PaginatedResponse } from '@/types/api';

function PatientDetail({ patient, onClose }: { patient: Patient; onClose: () => void }) {
  const { data: studiesData } = useQuery({
    queryKey: ['patient-studies', patient.patient_id],
    queryFn: () => apiClient.getPatientStudies(patient.patient_id),
  });

  const studies: Study[] = studiesData?.data || [];

  return (
    <Card>
      <CardHeader className="flex flex-row items-start justify-between">
        <div>
          <CardTitle className="flex items-center">
            <User className="mr-2 h-5 w-5" />
            {patient.patient_name || 'Unknown'}
          </CardTitle>
          <CardDescription>Patient ID: {patient.patient_id}</CardDescription>
        </div>
        <Button variant="ghost" size="sm" onClick={onClose}>
          Close
        </Button>
      </CardHeader>
      <CardContent>
        <div className="grid gap-4 md:grid-cols-2">
          <div className="space-y-2">
            <div>
              <span className="text-sm text-muted-foreground">Birth Date:</span>
              <span className="ml-2 font-medium">{formatDate(patient.birth_date)}</span>
            </div>
            <div>
              <span className="text-sm text-muted-foreground">Sex:</span>
              <span className="ml-2 font-medium">{patient.sex || '-'}</span>
            </div>
            <div>
              <span className="text-sm text-muted-foreground">Ethnic Group:</span>
              <span className="ml-2 font-medium">{patient.ethnic_group || '-'}</span>
            </div>
          </div>
          <div className="space-y-2">
            <div>
              <span className="text-sm text-muted-foreground">Other IDs:</span>
              <span className="ml-2 font-medium">{patient.other_ids || '-'}</span>
            </div>
            <div>
              <span className="text-sm text-muted-foreground">Comments:</span>
              <span className="ml-2 font-medium">{patient.comments || '-'}</span>
            </div>
          </div>
        </div>

        <div className="mt-6">
          <h4 className="mb-4 font-semibold">Studies ({studies.length})</h4>
          {studies.length === 0 ? (
            <p className="text-sm text-muted-foreground">No studies found for this patient.</p>
          ) : (
            <Table>
              <TableHeader>
                <TableRow>
                  <TableHead>Date</TableHead>
                  <TableHead>Description</TableHead>
                  <TableHead>Modality</TableHead>
                  <TableHead>Series</TableHead>
                  <TableHead>Instances</TableHead>
                </TableRow>
              </TableHeader>
              <TableBody>
                {studies.map((study) => (
                  <TableRow key={study.pk}>
                    <TableCell>{formatDate(study.study_date)}</TableCell>
                    <TableCell>{study.study_description || '-'}</TableCell>
                    <TableCell>
                      <Badge variant="secondary">{study.modalities_in_study || '-'}</Badge>
                    </TableCell>
                    <TableCell>{study.num_series}</TableCell>
                    <TableCell>{study.num_instances}</TableCell>
                  </TableRow>
                ))}
              </TableBody>
            </Table>
          )}
        </div>
      </CardContent>
    </Card>
  );
}

export function PatientsPage() {
  const [search, setSearch] = useState('');
  const [selectedPatient, setSelectedPatient] = useState<Patient | null>(null);
  const [page, setPage] = useState(0);
  const pageSize = 20;

  const { data, isLoading, error } = useQuery<PaginatedResponse<Patient>>({
    queryKey: ['patients', search, page],
    queryFn: () =>
      apiClient.getPatients({
        patient_name: search || undefined,
        limit: pageSize,
        offset: page * pageSize,
      }),
  });

  const patients = data?.data || [];
  const total = data?.pagination?.total || 0;
  const totalPages = Math.ceil(total / pageSize);

  return (
    <div className="space-y-6">
      <div>
        <h2 className="text-3xl font-bold tracking-tight">Patients</h2>
        <p className="text-muted-foreground">Browse and search patient records</p>
      </div>

      {selectedPatient ? (
        <PatientDetail patient={selectedPatient} onClose={() => setSelectedPatient(null)} />
      ) : (
        <Card>
          <CardHeader>
            <CardTitle>Patient List</CardTitle>
            <CardDescription>
              Total: {total} patients
            </CardDescription>
          </CardHeader>
          <CardContent>
            <div className="mb-4 flex items-center space-x-2">
              <div className="relative flex-1">
                <Search className="absolute left-2.5 top-2.5 h-4 w-4 text-muted-foreground" />
                <Input
                  placeholder="Search by patient name..."
                  value={search}
                  onChange={(e) => {
                    setSearch(e.target.value);
                    setPage(0);
                  }}
                  className="pl-8"
                />
              </div>
            </div>

            {isLoading ? (
              <div className="py-8 text-center text-muted-foreground">Loading patients...</div>
            ) : error ? (
              <div className="py-8 text-center text-destructive">
                Error loading patients. Make sure the API server is running.
              </div>
            ) : patients.length === 0 ? (
              <div className="py-8 text-center text-muted-foreground">
                No patients found.
              </div>
            ) : (
              <>
                <Table>
                  <TableHeader>
                    <TableRow>
                      <TableHead>Patient ID</TableHead>
                      <TableHead>Name</TableHead>
                      <TableHead>Birth Date</TableHead>
                      <TableHead>Sex</TableHead>
                      <TableHead></TableHead>
                    </TableRow>
                  </TableHeader>
                  <TableBody>
                    {patients.map((patient) => (
                      <TableRow
                        key={patient.pk}
                        className="cursor-pointer"
                        onClick={() => setSelectedPatient(patient)}
                      >
                        <TableCell className="font-medium">{patient.patient_id}</TableCell>
                        <TableCell>{patient.patient_name || '-'}</TableCell>
                        <TableCell>{formatDate(patient.birth_date)}</TableCell>
                        <TableCell>{patient.sex || '-'}</TableCell>
                        <TableCell>
                          <ChevronRight className="h-4 w-4 text-muted-foreground" />
                        </TableCell>
                      </TableRow>
                    ))}
                  </TableBody>
                </Table>

                {totalPages > 1 && (
                  <div className="mt-4 flex items-center justify-between">
                    <div className="text-sm text-muted-foreground">
                      Page {page + 1} of {totalPages}
                    </div>
                    <div className="space-x-2">
                      <Button
                        variant="outline"
                        size="sm"
                        onClick={() => setPage((p) => Math.max(0, p - 1))}
                        disabled={page === 0}
                      >
                        Previous
                      </Button>
                      <Button
                        variant="outline"
                        size="sm"
                        onClick={() => setPage((p) => Math.min(totalPages - 1, p + 1))}
                        disabled={page >= totalPages - 1}
                      >
                        Next
                      </Button>
                    </div>
                  </div>
                )}
              </>
            )}
          </CardContent>
        </Card>
      )}
    </div>
  );
}
