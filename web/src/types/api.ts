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
  tls_cert_path?: string;
  tls_key_path?: string;
  request_timeout_seconds: number;
  max_body_size: number;
}

// DICOM Server Configuration
export interface DicomServerConfig {
  ae_title: string;
  port: number;
  max_associations: number;
  max_pdu_size: number;
  idle_timeout_seconds: number;
  association_timeout_seconds: number;
  ae_whitelist: string[];
  accept_unknown_calling_ae: boolean;
  implementation_class_uid?: string;
  implementation_version_name?: string;
}

// Storage Configuration
export interface StorageConfig {
  storage_path: string;
  storage_type: 'file' | 's3' | 'azure' | 'hsm';
  max_storage_size_gb?: number;
  archive_path?: string;
  enable_compression?: boolean;
  compression_level?: number;
}

// Logging Configuration
export interface LoggingConfig {
  log_directory: string;
  log_level: 'trace' | 'debug' | 'info' | 'warn' | 'error' | 'fatal' | 'off';
  enable_console: boolean;
  enable_file: boolean;
  enable_audit_log: boolean;
  max_file_size_mb: number;
  max_files: number;
  audit_log_format: 'json' | 'syslog';
  async_mode: boolean;
}

// Combined configuration for all settings
export interface FullSystemConfig {
  rest_server: SystemConfig;
  dicom_server: DicomServerConfig;
  storage: StorageConfig;
  logging: LoggingConfig;
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

// Worklist types - aligned with backend API response
export interface WorklistItem {
  pk: number;
  step_id: string;
  step_status: 'SCHEDULED' | 'IN_PROGRESS' | 'COMPLETED' | 'DISCONTINUED';
  patient_id: string;
  patient_name: string;
  birth_date: string;
  sex: string;
  accession_no: string;
  requested_proc_id: string;
  study_uid: string;
  scheduled_datetime: string;
  station_ae: string;
  station_name: string;
  modality: string;
  procedure_desc: string;
  protocol_code: string;
  referring_phys: string;
  referring_phys_id: string;
  created_at: string;
  updated_at: string;
}

// Input type for creating/updating worklist items
export interface WorklistItemInput {
  step_id: string;
  step_status?: string;
  patient_id: string;
  patient_name?: string;
  birth_date?: string;
  sex?: string;
  accession_no?: string;
  requested_proc_id?: string;
  study_uid?: string;
  scheduled_datetime: string;
  station_ae?: string;
  station_name?: string;
  modality: string;
  procedure_desc?: string;
  protocol_code?: string;
  referring_phys?: string;
  referring_phys_id?: string;
}

export interface WorklistQuery {
  station_ae?: string;
  modality?: string;
  scheduled_date_from?: string;
  scheduled_date_to?: string;
  patient_id?: string;
  patient_name?: string;
  accession_no?: string;
  step_id?: string;
  include_all_status?: boolean;
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

// Job types - aligned with backend pacs::client::job_types
export type JobType = 'query' | 'retrieve' | 'store' | 'export' | 'import' | 'prefetch' | 'sync';
export type JobStatus = 'pending' | 'queued' | 'running' | 'completed' | 'failed' | 'cancelled' | 'paused';
export type JobPriority = 'low' | 'normal' | 'high' | 'urgent';

export interface JobProgress {
  total_items: number;
  completed_items: number;
  failed_items: number;
  skipped_items: number;
  bytes_transferred: number;
  percent_complete: number;
  current_item?: string;
  current_item_description?: string;
  elapsed_ms: number;
  estimated_remaining_ms: number;
}

export interface Job {
  job_id: string;
  type: JobType;
  status: JobStatus;
  priority: JobPriority;
  source_node_id?: string;
  destination_node_id?: string;
  patient_id?: string;
  study_uid?: string;
  series_uid?: string;
  progress: JobProgress;
  error_message?: string;
  error_details?: string;
  retry_count: number;
  max_retries: number;
  created_at?: string;
  queued_at?: string;
  started_at?: string;
  completed_at?: string;
  created_by?: string;
}

export interface JobQuery {
  status?: JobStatus;
  type?: JobType;
  limit?: number;
  offset?: number;
}

export interface CreateJobRequest {
  type: JobType;
  source_node_id?: string;
  destination_node_id?: string;
  study_uid?: string;
  series_uid?: string;
  instance_uids?: string[];
  patient_id?: string;
  patient_name?: string;
  node_id?: string;
  query_level?: string;
  priority?: JobPriority;
}

export interface JobListResponse {
  jobs: Job[];
  total: number;
}

// Metadata types (Issue #544)
export type MetadataPreset = 'image_display' | 'window_level' | 'patient_info' | 'acquisition' | 'positioning' | 'multiframe';

export interface MetadataRequest {
  tags?: string[];
  preset?: MetadataPreset;
  include_private?: boolean;
}

export interface MetadataResponse {
  tags: Record<string, string | number | number[]>;
}

export interface SortedInstance {
  sop_instance_uid: string;
  instance_number?: number;
  slice_location?: number;
  image_position_patient?: number[];
  acquisition_time?: string;
}

export type SortOrder = 'position' | 'instance_number' | 'acquisition_time';

export interface SortedInstancesResponse {
  instances: SortedInstance[];
  total: number;
}

export interface NavigationInfo {
  previous?: string;
  next?: string;
  index: number;
  total: number;
  first: string;
  last: string;
}

export interface WindowLevelPreset {
  name: string;
  center: number;
  width: number;
}

export interface WindowLevelPresetsResponse {
  presets: WindowLevelPreset[];
}

export interface VOILUTInfo {
  window_center: number[];
  window_width: number[];
  window_explanations?: string[];
  rescale_slope: number;
  rescale_intercept: number;
}

export interface FrameInfo {
  total_frames: number;
  frame_time?: number;
  frame_rate?: number;
  rows: number;
  columns: number;
}

// Annotation types (Issue #545, #584)
export type AnnotationType =
  | 'arrow'
  | 'line'
  | 'rectangle'
  | 'ellipse'
  | 'polygon'
  | 'freehand'
  | 'text'
  | 'angle'
  | 'roi';

export interface AnnotationStyle {
  color: string;
  line_width: number;
  fill_color?: string;
  fill_opacity?: number;
  font_family?: string;
  font_size?: number;
}

export interface Annotation {
  annotation_id: string;
  study_uid: string;
  series_uid?: string;
  sop_instance_uid?: string;
  frame_number?: number | null;
  user_id: string;
  annotation_type: AnnotationType;
  geometry: Record<string, unknown>;
  text?: string;
  style: AnnotationStyle;
  created_at: string;
  updated_at: string;
}

export interface AnnotationCreateRequest {
  study_uid: string;
  series_uid?: string;
  sop_instance_uid?: string;
  frame_number?: number;
  user_id: string;
  annotation_type: AnnotationType;
  geometry: Record<string, unknown>;
  text?: string;
  style?: Partial<AnnotationStyle>;
}

export interface AnnotationUpdateRequest {
  geometry?: Record<string, unknown>;
  text?: string;
  style?: Partial<AnnotationStyle>;
}

export interface AnnotationQuery {
  study_uid?: string;
  series_uid?: string;
  sop_instance_uid?: string;
  user_id?: string;
  limit?: number;
  offset?: number;
}

export interface AnnotationCreateResponse {
  annotation_id: string;
  created_at: string;
}

export interface AnnotationUpdateResponse {
  annotation_id: string;
  updated_at: string;
}

// Measurement types (Issue #545, #584)
export type MeasurementType =
  | 'length'
  | 'area'
  | 'angle'
  | 'hounsfield'
  | 'suv'
  | 'ellipse_area'
  | 'polygon_area';

export interface Measurement {
  measurement_id: string;
  sop_instance_uid: string;
  frame_number?: number | null;
  user_id: string;
  measurement_type: MeasurementType;
  geometry: Record<string, unknown>;
  value: number;
  unit: string;
  label?: string;
  created_at: string;
}

export interface MeasurementCreateRequest {
  sop_instance_uid: string;
  frame_number?: number;
  user_id: string;
  measurement_type: MeasurementType;
  geometry: Record<string, unknown>;
  value: number;
  unit: string;
  label?: string;
}

export interface MeasurementQuery {
  sop_instance_uid?: string;
  study_uid?: string;
  user_id?: string;
  measurement_type?: MeasurementType;
  limit?: number;
  offset?: number;
}

export interface MeasurementCreateResponse {
  measurement_id: string;
  value: number;
  unit: string;
}

// Key Image types (Issue #545, #584)
export interface KeyImage {
  key_image_id: string;
  study_uid: string;
  sop_instance_uid: string;
  frame_number?: number | null;
  user_id: string;
  reason?: string;
  document_title?: string;
  created_at: string;
}

export interface KeyImageCreateRequest {
  sop_instance_uid: string;
  frame_number?: number;
  user_id: string;
  reason?: string;
  document_title?: string;
}

export interface KeyImageCreateResponse {
  key_image_id: string;
  created_at: string;
}

export interface KeyImageListResponse {
  data: KeyImage[];
}

export interface KeyObjectSelectionDocument {
  document_type: string;
  study_uid: string;
  document_title: string;
  referenced_instances: Array<{
    sop_instance_uid: string;
    frame_number?: number | null;
    reason?: string;
  }>;
  created_at: string;
}

// Viewer State types (Issue #545, #584)
export interface ViewerLayout {
  rows?: number;
  columns?: number;
  type?: string;
}

export interface ViewportState {
  viewport_id?: string;
  series_uid?: string;
  sop_instance_uid?: string;
  frame_number?: number;
  window_center?: number;
  window_width?: number;
  zoom?: number;
  pan?: { x: number; y: number };
  rotation?: number;
  flip_horizontal?: boolean;
  flip_vertical?: boolean;
  invert?: boolean;
}

export interface ViewerStateContent {
  layout?: ViewerLayout;
  viewports?: ViewportState[];
  active_viewport?: string;
}

export interface ViewerState {
  state_id: string;
  study_uid: string;
  user_id: string;
  state: ViewerStateContent;
  created_at: string;
  updated_at: string;
}

export interface ViewerStateCreateRequest {
  study_uid: string;
  user_id: string;
  layout?: ViewerLayout;
  viewports?: ViewportState[];
  active_viewport?: string;
}

export interface ViewerStateQuery {
  study_uid?: string;
  user_id?: string;
  limit?: number;
}

export interface ViewerStateCreateResponse {
  state_id: string;
  created_at: string;
}

export interface ViewerStateListResponse {
  data: ViewerState[];
}

// Recent Studies types (Issue #545, #584)
export interface RecentStudy {
  user_id: string;
  study_uid: string;
  accessed_at: string;
}

export interface RecentStudiesResponse {
  data: RecentStudy[];
  total: number;
}
