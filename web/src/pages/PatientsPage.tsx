import { useState, useCallback, useMemo } from 'react';
import { useQuery } from '@tanstack/react-query';
import {
  Search,
  ChevronRight,
  ChevronDown,
  User,
  Calendar,
  Filter,
  FileImage,
  Layers,
  X,
  Info,
  Folder,
  FolderOpen,
  File,
} from 'lucide-react';
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
import { formatDate, formatTime, formatBytes, cn } from '@/lib/utils';
import type { Patient, Study, Series, Instance, PaginatedResponse } from '@/types/api';

// Debounce hook for search input
function useDebounce<T>(value: T, delay: number): T {
  const [debouncedValue, setDebouncedValue] = useState<T>(value);

  useMemo(() => {
    const handler = setTimeout(() => {
      setDebouncedValue(value);
    }, delay);

    return () => {
      clearTimeout(handler);
    };
  }, [value, delay]);

  return debouncedValue;
}

// Study filters interface
interface StudyFilters {
  dateFrom: string;
  dateTo: string;
  modality: string;
}

// Tree node component for Series/Instance navigation
interface TreeNodeProps {
  label: string;
  icon: React.ReactNode;
  expandedIcon?: React.ReactNode;
  isExpanded?: boolean;
  isSelected?: boolean;
  hasChildren?: boolean;
  level?: number;
  onClick?: () => void;
  onExpand?: () => void;
  badge?: string;
}

function TreeNode({
  label,
  icon,
  expandedIcon,
  isExpanded = false,
  isSelected = false,
  hasChildren = false,
  level = 0,
  onClick,
  onExpand,
  badge,
}: TreeNodeProps) {
  return (
    <div
      className={cn(
        'flex items-center gap-2 py-1.5 px-2 rounded cursor-pointer hover:bg-muted/50 transition-colors',
        isSelected && 'bg-muted'
      )}
      style={{ paddingLeft: `${level * 16 + 8}px` }}
      onClick={onClick}
    >
      {hasChildren ? (
        <button
          className="p-0.5 hover:bg-muted rounded"
          onClick={(e) => {
            e.stopPropagation();
            onExpand?.();
          }}
        >
          {isExpanded ? (
            <ChevronDown className="h-4 w-4 text-muted-foreground" />
          ) : (
            <ChevronRight className="h-4 w-4 text-muted-foreground" />
          )}
        </button>
      ) : (
        <span className="w-5" />
      )}
      {isExpanded && expandedIcon ? expandedIcon : icon}
      <span className="flex-1 truncate text-sm">{label}</span>
      {badge && (
        <Badge variant="secondary" className="ml-auto text-xs">
          {badge}
        </Badge>
      )}
    </div>
  );
}

// Metadata viewer component
interface MetadataViewerProps {
  title: string;
  data: Record<string, string | number | null | undefined>;
  onClose?: () => void;
}

function MetadataViewer({ title, data, onClose }: MetadataViewerProps) {
  return (
    <Card className="h-full">
      <CardHeader className="flex flex-row items-center justify-between py-3">
        <CardTitle className="text-lg flex items-center gap-2">
          <Info className="h-4 w-4" />
          {title}
        </CardTitle>
        {onClose && (
          <Button variant="ghost" size="sm" onClick={onClose}>
            <X className="h-4 w-4" />
          </Button>
        )}
      </CardHeader>
      <CardContent className="space-y-2 max-h-[400px] overflow-y-auto">
        {Object.entries(data).map(([key, value]) => (
          <div key={key} className="flex justify-between text-sm border-b pb-1">
            <span className="text-muted-foreground capitalize">
              {key.replace(/_/g, ' ')}
            </span>
            <span className="font-medium truncate ml-2 max-w-[200px]" title={String(value ?? '-')}>
              {value ?? '-'}
            </span>
          </div>
        ))}
      </CardContent>
    </Card>
  );
}

// Study filters component
interface StudyFiltersProps {
  filters: StudyFilters;
  onFiltersChange: (filters: StudyFilters) => void;
  onClear: () => void;
  availableModalities: string[];
}

function StudyFiltersPanel({
  filters,
  onFiltersChange,
  onClear,
  availableModalities,
}: StudyFiltersProps) {
  const hasFilters = filters.dateFrom || filters.dateTo || filters.modality;

  return (
    <div className="flex flex-wrap items-center gap-3 p-3 bg-muted/30 rounded-lg">
      <Filter className="h-4 w-4 text-muted-foreground" />
      <div className="flex items-center gap-2">
        <label className="text-sm text-muted-foreground">Date:</label>
        <Input
          type="date"
          className="h-8 w-36"
          value={filters.dateFrom}
          onChange={(e) => onFiltersChange({ ...filters, dateFrom: e.target.value })}
          placeholder="From"
        />
        <span className="text-muted-foreground">-</span>
        <Input
          type="date"
          className="h-8 w-36"
          value={filters.dateTo}
          onChange={(e) => onFiltersChange({ ...filters, dateTo: e.target.value })}
          placeholder="To"
        />
      </div>
      <div className="flex items-center gap-2">
        <label className="text-sm text-muted-foreground">Modality:</label>
        <select
          className="h-8 px-3 rounded-md border border-input bg-background text-sm"
          value={filters.modality}
          onChange={(e) => onFiltersChange({ ...filters, modality: e.target.value })}
        >
          <option value="">All</option>
          {availableModalities.map((mod) => (
            <option key={mod} value={mod}>
              {mod}
            </option>
          ))}
        </select>
      </div>
      {hasFilters && (
        <Button variant="ghost" size="sm" onClick={onClear}>
          Clear filters
        </Button>
      )}
    </div>
  );
}

// Patient browser with full hierarchy navigation
function PatientBrowser({
  patient,
  onClose,
}: {
  patient: Patient;
  onClose: () => void;
}) {
  const [selectedStudy, setSelectedStudy] = useState<Study | null>(null);
  const [selectedSeries, setSelectedSeries] = useState<Series | null>(null);
  const [selectedInstance, setSelectedInstance] = useState<Instance | null>(null);
  const [expandedStudies, setExpandedStudies] = useState<Set<string>>(new Set());
  const [expandedSeries, setExpandedSeries] = useState<Set<string>>(new Set());
  const [studyFilters, setStudyFilters] = useState<StudyFilters>({
    dateFrom: '',
    dateTo: '',
    modality: '',
  });

  // Fetch studies for patient
  const { data: studiesData, isLoading: studiesLoading } = useQuery({
    queryKey: ['patient-studies', patient.patient_id],
    queryFn: () => apiClient.getPatientStudies(patient.patient_id),
  });

  const studies: Study[] = studiesData?.data || [];

  // Extract available modalities from studies
  const availableModalities = useMemo(() => {
    const modalities = new Set<string>();
    studies.forEach((study) => {
      if (study.modalities_in_study) {
        study.modalities_in_study.split('\\').forEach((m) => modalities.add(m.trim()));
      }
    });
    return Array.from(modalities).sort();
  }, [studies]);

  // Filter studies
  const filteredStudies = useMemo(() => {
    return studies.filter((study) => {
      if (studyFilters.dateFrom) {
        const studyDate = formatDate(study.study_date);
        if (studyDate < studyFilters.dateFrom) return false;
      }
      if (studyFilters.dateTo) {
        const studyDate = formatDate(study.study_date);
        if (studyDate > studyFilters.dateTo) return false;
      }
      if (studyFilters.modality && study.modalities_in_study) {
        if (!study.modalities_in_study.includes(studyFilters.modality)) return false;
      }
      return true;
    });
  }, [studies, studyFilters]);

  // Fetch series for selected study
  const { data: seriesData } = useQuery({
    queryKey: ['study-series', selectedStudy?.study_instance_uid],
    queryFn: () =>
      selectedStudy ? apiClient.getStudySeries(selectedStudy.study_instance_uid) : null,
    enabled: !!selectedStudy,
  });

  const seriesList: Series[] = seriesData?.data || [];

  // Fetch instances for selected series
  const { data: instancesData } = useQuery({
    queryKey: ['series-instances', selectedSeries?.series_instance_uid],
    queryFn: () =>
      selectedSeries
        ? apiClient.getSeriesInstances(selectedSeries.series_instance_uid)
        : null,
    enabled: !!selectedSeries,
  });

  const instances: Instance[] = instancesData?.data || [];

  const toggleStudyExpanded = useCallback((studyUid: string) => {
    setExpandedStudies((prev) => {
      const next = new Set(prev);
      if (next.has(studyUid)) {
        next.delete(studyUid);
      } else {
        next.add(studyUid);
      }
      return next;
    });
  }, []);

  const toggleSeriesExpanded = useCallback((seriesUid: string) => {
    setExpandedSeries((prev) => {
      const next = new Set(prev);
      if (next.has(seriesUid)) {
        next.delete(seriesUid);
      } else {
        next.add(seriesUid);
      }
      return next;
    });
  }, []);

  const handleStudySelect = useCallback((study: Study) => {
    setSelectedStudy(study);
    setSelectedSeries(null);
    setSelectedInstance(null);
    setExpandedStudies((prev) => new Set(prev).add(study.study_instance_uid));
  }, []);

  const handleSeriesSelect = useCallback((series: Series) => {
    setSelectedSeries(series);
    setSelectedInstance(null);
    setExpandedSeries((prev) => new Set(prev).add(series.series_instance_uid));
  }, []);

  const handleInstanceSelect = useCallback((instance: Instance) => {
    setSelectedInstance(instance);
  }, []);

  const clearFilters = useCallback(() => {
    setStudyFilters({ dateFrom: '', dateTo: '', modality: '' });
  }, []);

  // Build metadata for selected item
  const selectedMetadata = useMemo(() => {
    if (selectedInstance) {
      return {
        title: 'Instance Details',
        data: {
          'SOP Instance UID': selectedInstance.sop_instance_uid,
          'SOP Class UID': selectedInstance.sop_class_uid,
          'Transfer Syntax': selectedInstance.transfer_syntax,
          'Instance Number': selectedInstance.instance_number,
          'File Size': formatBytes(selectedInstance.file_size),
        },
      };
    }
    if (selectedSeries) {
      return {
        title: 'Series Details',
        data: {
          'Series Instance UID': selectedSeries.series_instance_uid,
          'Series Number': selectedSeries.series_number,
          Modality: selectedSeries.modality,
          Description: selectedSeries.series_description,
          'Body Part': selectedSeries.body_part_examined,
          'Station Name': selectedSeries.station_name,
          'Number of Instances': selectedSeries.num_instances,
        },
      };
    }
    if (selectedStudy) {
      return {
        title: 'Study Details',
        data: {
          'Study Instance UID': selectedStudy.study_instance_uid,
          'Study ID': selectedStudy.study_id,
          'Study Date': formatDate(selectedStudy.study_date),
          'Study Time': formatTime(selectedStudy.study_time),
          'Accession Number': selectedStudy.accession_number,
          'Referring Physician': selectedStudy.referring_physician,
          Description: selectedStudy.study_description,
          Modalities: selectedStudy.modalities_in_study,
          'Number of Series': selectedStudy.num_series,
          'Number of Instances': selectedStudy.num_instances,
        },
      };
    }
    return null;
  }, [selectedStudy, selectedSeries, selectedInstance]);

  return (
    <div className="space-y-4">
      {/* Patient Header */}
      <Card>
        <CardHeader className="flex flex-row items-start justify-between py-3">
          <div>
            <CardTitle className="flex items-center text-xl">
              <User className="mr-2 h-5 w-5" />
              {patient.patient_name || 'Unknown'}
            </CardTitle>
            <CardDescription className="mt-1">
              Patient ID: {patient.patient_id} | Birth Date: {formatDate(patient.birth_date)} |
              Sex: {patient.sex || '-'}
            </CardDescription>
          </div>
          <Button variant="outline" onClick={onClose}>
            Back to Patient List
          </Button>
        </CardHeader>
      </Card>

      {/* Study Filters */}
      <StudyFiltersPanel
        filters={studyFilters}
        onFiltersChange={setStudyFilters}
        onClear={clearFilters}
        availableModalities={availableModalities}
      />

      {/* Main Content Grid */}
      <div className="grid gap-4 lg:grid-cols-3">
        {/* Navigation Tree */}
        <Card className="lg:col-span-2">
          <CardHeader className="py-3">
            <CardTitle className="text-lg flex items-center gap-2">
              <Layers className="h-4 w-4" />
              Studies / Series / Instances
            </CardTitle>
            <CardDescription>
              {filteredStudies.length} studies | Click to expand and view details
            </CardDescription>
          </CardHeader>
          <CardContent className="max-h-[500px] overflow-y-auto">
            {studiesLoading ? (
              <div className="py-8 text-center text-muted-foreground">Loading studies...</div>
            ) : filteredStudies.length === 0 ? (
              <div className="py-8 text-center text-muted-foreground">
                No studies found for this patient.
              </div>
            ) : (
              <div className="space-y-1">
                {filteredStudies.map((study) => {
                  const isStudyExpanded = expandedStudies.has(study.study_instance_uid);
                  const isStudySelected =
                    selectedStudy?.study_instance_uid === study.study_instance_uid &&
                    !selectedSeries;

                  return (
                    <div key={study.pk}>
                      <TreeNode
                        label={`${formatDate(study.study_date)} - ${study.study_description || 'No description'}`}
                        icon={<Folder className="h-4 w-4 text-blue-500" />}
                        expandedIcon={<FolderOpen className="h-4 w-4 text-blue-500" />}
                        isExpanded={isStudyExpanded}
                        isSelected={isStudySelected}
                        hasChildren={study.num_series > 0}
                        badge={study.modalities_in_study}
                        onClick={() => handleStudySelect(study)}
                        onExpand={() => toggleStudyExpanded(study.study_instance_uid)}
                      />

                      {/* Series under this study */}
                      {isStudyExpanded &&
                        selectedStudy?.study_instance_uid === study.study_instance_uid &&
                        seriesList.map((series) => {
                          const isSeriesExpanded = expandedSeries.has(
                            series.series_instance_uid
                          );
                          const isSeriesSelected =
                            selectedSeries?.series_instance_uid ===
                              series.series_instance_uid && !selectedInstance;

                          return (
                            <div key={series.pk}>
                              <TreeNode
                                label={`Series ${series.series_number ?? '-'}: ${series.series_description || series.modality}`}
                                icon={<FileImage className="h-4 w-4 text-green-500" />}
                                isExpanded={isSeriesExpanded}
                                isSelected={isSeriesSelected}
                                hasChildren={series.num_instances > 0}
                                level={1}
                                badge={`${series.num_instances} img`}
                                onClick={() => handleSeriesSelect(series)}
                                onExpand={() =>
                                  toggleSeriesExpanded(series.series_instance_uid)
                                }
                              />

                              {/* Instances under this series */}
                              {isSeriesExpanded &&
                                selectedSeries?.series_instance_uid ===
                                  series.series_instance_uid &&
                                instances.map((instance) => {
                                  const isInstanceSelected =
                                    selectedInstance?.sop_instance_uid ===
                                    instance.sop_instance_uid;

                                  return (
                                    <TreeNode
                                      key={instance.pk}
                                      label={`Instance ${instance.instance_number ?? '-'}`}
                                      icon={<File className="h-4 w-4 text-gray-500" />}
                                      isSelected={isInstanceSelected}
                                      hasChildren={false}
                                      level={2}
                                      badge={formatBytes(instance.file_size)}
                                      onClick={() => handleInstanceSelect(instance)}
                                    />
                                  );
                                })}
                            </div>
                          );
                        })}
                    </div>
                  );
                })}
              </div>
            )}
          </CardContent>
        </Card>

        {/* Metadata Panel */}
        <div className="lg:col-span-1">
          {selectedMetadata ? (
            <MetadataViewer
              title={selectedMetadata.title}
              data={selectedMetadata.data}
            />
          ) : (
            <Card className="h-full">
              <CardContent className="flex items-center justify-center h-[300px] text-muted-foreground">
                <div className="text-center">
                  <Info className="h-8 w-8 mx-auto mb-2 opacity-50" />
                  <p>Select a study, series, or instance to view details</p>
                </div>
              </CardContent>
            </Card>
          )}
        </div>
      </div>

      {/* Studies Table View */}
      <Card>
        <CardHeader className="py-3">
          <CardTitle className="text-lg flex items-center gap-2">
            <Calendar className="h-4 w-4" />
            Studies Table View
          </CardTitle>
        </CardHeader>
        <CardContent>
          <Table>
            <TableHeader>
              <TableRow>
                <TableHead>Date</TableHead>
                <TableHead>Description</TableHead>
                <TableHead>Modality</TableHead>
                <TableHead>Accession #</TableHead>
                <TableHead>Series</TableHead>
                <TableHead>Instances</TableHead>
                <TableHead></TableHead>
              </TableRow>
            </TableHeader>
            <TableBody>
              {filteredStudies.map((study) => (
                <TableRow
                  key={study.pk}
                  className={cn(
                    'cursor-pointer',
                    selectedStudy?.pk === study.pk && 'bg-muted'
                  )}
                  onClick={() => handleStudySelect(study)}
                >
                  <TableCell>{formatDate(study.study_date)}</TableCell>
                  <TableCell className="max-w-[200px] truncate">
                    {study.study_description || '-'}
                  </TableCell>
                  <TableCell>
                    <Badge variant="secondary">{study.modalities_in_study || '-'}</Badge>
                  </TableCell>
                  <TableCell>{study.accession_number || '-'}</TableCell>
                  <TableCell>{study.num_series}</TableCell>
                  <TableCell>{study.num_instances}</TableCell>
                  <TableCell>
                    <ChevronRight className="h-4 w-4 text-muted-foreground" />
                  </TableCell>
                </TableRow>
              ))}
            </TableBody>
          </Table>
        </CardContent>
      </Card>
    </div>
  );
}

export function PatientsPage() {
  const [search, setSearch] = useState('');
  const [selectedPatient, setSelectedPatient] = useState<Patient | null>(null);
  const [page, setPage] = useState(0);
  const pageSize = 20;

  const debouncedSearch = useDebounce(search, 300);

  const { data, isLoading, error } = useQuery<PaginatedResponse<Patient>>({
    queryKey: ['patients', debouncedSearch, page],
    queryFn: () =>
      apiClient.getPatients({
        patient_name: debouncedSearch || undefined,
        limit: pageSize,
        offset: page * pageSize,
      }),
  });

  const patients = data?.data || [];
  const total = data?.pagination?.total || 0;
  const totalPages = Math.ceil(total / pageSize);

  if (selectedPatient) {
    return (
      <div className="space-y-6">
        <PatientBrowser
          patient={selectedPatient}
          onClose={() => setSelectedPatient(null)}
        />
      </div>
    );
  }

  return (
    <div className="space-y-6">
      <div>
        <h2 className="text-3xl font-bold tracking-tight">Patients</h2>
        <p className="text-muted-foreground">
          Browse and search patient records, studies, series, and instances
        </p>
      </div>

      <Card>
        <CardHeader>
          <CardTitle>Patient List</CardTitle>
          <CardDescription>Total: {total} patients</CardDescription>
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
            <div className="py-8 text-center text-muted-foreground">No patients found.</div>
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
    </div>
  );
}
