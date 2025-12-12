// System types
export interface SystemStatus {
  status: 'healthy' | 'degraded' | 'unhealthy' | 'unknown';
  message?: string;
  version: string;
  components?: Record<string, ComponentStatus>;
}

export interface ComponentStatus {
  status: 'healthy' | 'degraded' | 'unhealthy';
  message?: string;
  latency_ms?: number;
}

export interface SystemConfig {
  bind_address: string;
  port: number;
  concurrency: number;
  enable_cors: boolean;
  cors_allowed_origins: string;
  enable_tls: boolean;
  request_timeout_seconds: number;
  max_body_size: number;
}

export interface SystemMetrics {
  uptime_seconds?: number;
  requests_total?: number;
  message?: string;
}

// Dashboard statistics types
export interface DashboardStats {
  active_associations: number;
  total_patients: number;
  total_studies: number;
  storage_used_bytes: number;
  storage_total_bytes: number;
}

export interface StorageStats {
  used_bytes: number;
  total_bytes: number;
  free_bytes: number;
  used_percentage: number;
  breakdown?: StorageBreakdown[];
}

export interface StorageBreakdown {
  name: string;
  size_bytes: number;
  percentage: number;
  color?: string;
}

export interface SystemVersion {
  api_version: string;
  pacs_version: string;
  crow_version: string;
}

// Patient types
export interface Patient {
  pk: number;
  patient_id: string;
  patient_name: string;
  birth_date: string;
  sex: string;
  other_ids: string;
  ethnic_group: string;
  comments: string;
}

export interface PatientQuery {
  patient_id?: string;
  patient_name?: string;
  birth_date?: string;
  birth_date_from?: string;
  birth_date_to?: string;
  sex?: string;
  limit?: number;
  offset?: number;
}

// Study types
export interface Study {
  pk: number;
  study_instance_uid: string;
  study_id: string;
  study_date: string;
  study_time: string;
  accession_number: string;
  referring_physician: string;
  study_description: string;
  modalities_in_study: string;
  num_series: number;
  num_instances: number;
}

export interface StudyQuery {
  patient_id?: string;
  study_date?: string;
  study_date_from?: string;
  study_date_to?: string;
  modality?: string;
  accession_number?: string;
  study_description?: string;
  limit?: number;
  offset?: number;
}

// Series types
export interface Series {
  pk: number;
  study_pk?: number;
  series_instance_uid: string;
  series_number: number | null;
  modality: string;
  series_description: string;
  body_part_examined?: string;
  station_name?: string;
  num_instances: number;
}

// Instance types
export interface Instance {
  pk: number;
  series_pk: number;
  sop_instance_uid: string;
  sop_class_uid: string;
  transfer_syntax: string;
  instance_number: number | null;
  file_size: number;
}

// Worklist types
export interface WorklistItem {
  pk: number;
  scheduled_station_ae: string;
  scheduled_procedure_step_start_date: string;
  scheduled_procedure_step_start_time: string;
  modality: string;
  scheduled_performing_physician: string;
  scheduled_procedure_step_description: string;
  scheduled_procedure_step_id: string;
  requested_procedure_id: string;
  patient_id: string;
  patient_name: string;
  patient_birth_date: string;
  patient_sex: string;
  accession_number: string;
  referring_physician: string;
  study_instance_uid: string;
  status: 'SCHEDULED' | 'IN_PROGRESS' | 'COMPLETED' | 'DISCONTINUED';
}

export interface WorklistQuery {
  scheduled_station_ae?: string;
  modality?: string;
  date_from?: string;
  date_to?: string;
  status?: string;
  patient_id?: string;
  limit?: number;
  offset?: number;
}

// Audit types
export interface AuditRecord {
  pk: number;
  event_type: string;
  outcome: string;
  timestamp: string;
  user_id: string;
  source_ae: string;
  target_ae: string;
  source_ip: string;
  patient_id: string;
  study_uid: string;
  message: string;
  details: string;
}

export interface AuditQuery {
  event_type?: string;
  outcome?: string;
  user_id?: string;
  source_ae?: string;
  patient_id?: string;
  study_uid?: string;
  date_from?: string;
  date_to?: string;
  limit?: number;
  offset?: number;
}

// Association types
export interface Association {
  id: string;
  remote_ae: string;
  remote_host: string;
  remote_port: number;
  state: 'ESTABLISHED' | 'PENDING' | 'CLOSED';
  role: 'SCU' | 'SCP';
  start_time: string;
  last_activity: string;
}

// Pagination
export interface PaginatedResponse<T> {
  data: T[];
  pagination: {
    total: number;
    count: number;
  };
}

export interface ApiError {
  error: {
    code: string;
    message: string;
  };
}
