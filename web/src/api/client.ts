import axios, { type AxiosError, type AxiosInstance } from 'axios';
import type {
  ApiError,
  AssociationStatistics,
  WorklistItemInput,
  Job,
  JobListResponse,
  JobProgress,
  JobQuery,
  CreateJobRequest,
  MetadataRequest,
  MetadataResponse,
  SortedInstancesResponse,
  SortOrder,
  NavigationInfo,
  WindowLevelPresetsResponse,
  VOILUTInfo,
  FrameInfo,
  Annotation,
  AnnotationCreateRequest,
  AnnotationUpdateRequest,
  AnnotationQuery,
  AnnotationCreateResponse,
  AnnotationUpdateResponse,
  PaginatedResponse,
  Measurement,
  MeasurementCreateRequest,
  MeasurementQuery,
  MeasurementCreateResponse,
  KeyImageCreateRequest,
  KeyImageCreateResponse,
  KeyImageListResponse,
  KeyObjectSelectionDocument,
  ViewerState,
  ViewerStateCreateRequest,
  ViewerStateQuery,
  ViewerStateCreateResponse,
  ViewerStateListResponse,
  RecentStudiesResponse,
} from '../types/api';

import { clearAccessToken, getAccessToken } from '../lib/session';

const API_BASE_URL = '/api/v1';

/** A request that failed: status is undefined when no answer came back. */
export class ApiRequestError extends Error {
  readonly status?: number;
  readonly code?: string;

  constructor(message: string, status?: number, code?: string) {
    super(message);
    this.name = 'ApiRequestError';
    this.status = status;
    this.code = code;
  }
}

function asRequestError(error: AxiosError<ApiError>): ApiRequestError {
  const body = error.response?.data?.error;
  return new ApiRequestError(body?.message ?? error.message, error.response?.status, body?.code);
}

class ApiClient {
  private client: AxiosInstance;

  constructor() {
    this.client = axios.create({
      baseURL: API_BASE_URL,
      headers: {
        'Content-Type': 'application/json',
      },
    });

    this.client.interceptors.request.use((config) => {
      const token = getAccessToken();
      if (token) {
        config.headers.Authorization = `Bearer ${token}`;
      }
      return config;
    });

    this.client.interceptors.response.use(
      (response) => response,
      (error: AxiosError<ApiError>) => {
        // The token expired or was never good: back to signing in. A 403 stays: the token is
        // fine, and this page can say what it does not allow.
        if (error.response?.status === 401) {
          clearAccessToken();
        }
        throw asRequestError(error);
      }
    );
  }

  /** Asks the server about a token before keeping it, so a bad one never becomes a session. */
  async checkAccessToken(token: string) {
    try {
      await axios.get(`${API_BASE_URL}/system/status`, {
        headers: { Authorization: `Bearer ${token}` },
      });
    } catch (error) {
      throw asRequestError(error as AxiosError<ApiError>);
    }
  }

  // System endpoints
  async getSystemStatus() {
    const response = await this.client.get('/system/status');
    return response.data;
  }

  async getSystemMetrics() {
    const response = await this.client.get('/system/metrics');
    return response.data;
  }

  async getSystemConfig() {
    const response = await this.client.get('/system/config');
    return response.data;
  }

  async getSystemVersion() {
    const response = await this.client.get('/system/version');
    return response.data;
  }

  // Patient endpoints
  async getPatients(params?: Record<string, string | number | undefined>) {
    const response = await this.client.get('/patients', { params });
    return response.data;
  }

  async getPatient(patientId: string) {
    const response = await this.client.get(`/patients/${encodeURIComponent(patientId)}`);
    return response.data;
  }

  async getPatientStudies(patientId: string) {
    const response = await this.client.get(`/patients/${encodeURIComponent(patientId)}/studies`);
    return response.data;
  }

  // Study endpoints
  async getStudies(params?: Record<string, string | number>) {
    const response = await this.client.get('/studies', { params });
    return response.data;
  }

  async getStudy(studyUid: string) {
    const response = await this.client.get(`/studies/${encodeURIComponent(studyUid)}`);
    return response.data;
  }

  async getStudySeries(studyUid: string) {
    const response = await this.client.get(`/studies/${encodeURIComponent(studyUid)}/series`);
    return response.data;
  }

  // Series endpoints
  async getSeries(seriesUid: string) {
    const response = await this.client.get(`/series/${encodeURIComponent(seriesUid)}`);
    return response.data;
  }

  async getSeriesInstances(seriesUid: string) {
    const response = await this.client.get(`/series/${encodeURIComponent(seriesUid)}/instances`);
    return response.data;
  }

  // Worklist endpoints
  async getWorklist(params?: Record<string, string | number | boolean | undefined>) {
    const response = await this.client.get('/worklist', { params });
    return response.data;
  }

  async getWorklistItem(itemId: number) {
    const response = await this.client.get(`/worklist/${itemId}`);
    return response.data;
  }

  async createWorklistItem(item: WorklistItemInput) {
    const response = await this.client.post('/worklist', item);
    return response.data;
  }

  async updateWorklistItem(itemId: number, item: { step_status: string }) {
    const response = await this.client.put(`/worklist/${itemId}`, item);
    return response.data;
  }

  async deleteWorklistItem(itemId: number) {
    const response = await this.client.delete(`/worklist/${itemId}`);
    return response.data;
  }

  // Audit endpoints
  async getAuditLogs(params?: Record<string, string | number | undefined>) {
    const response = await this.client.get('/audit/logs', { params });
    return response.data;
  }

  async getAuditLog(pk: number) {
    const response = await this.client.get(`/audit/logs/${pk}`);
    return response.data;
  }

  async exportAuditLogs(params?: Record<string, string | undefined>, format: 'json' | 'csv' = 'json') {
    const response = await this.client.get('/audit/export', {
      params: { ...params, format },
      responseType: format === 'csv' ? 'blob' : 'json',
    });
    return response.data;
  }

  // Association endpoints
  async getAssociations(): Promise<AssociationStatistics> {
    const response = await this.client.get('/associations/active');
    return response.data;
  }

  async getAssociation(id: string) {
    const response = await this.client.get(`/associations/${id}`);
    return response.data;
  }

  // Dashboard aggregated stats
  async getDashboardStats() {
    // Fetch multiple endpoints in parallel and aggregate
    const [associations, patients, studies] = await Promise.all([
      this.getAssociations(),
      this.getPatients({ limit: 1 }),
      this.getStudies({ limit: 1 }),
    ]);

    return {
      active_associations: associations.active_count,
      total_patients: patients.pagination?.total ?? 0,
      total_studies: studies.pagination?.total ?? 0,
    };
  }

  // Recent activity (latest audit logs)
  async getRecentActivity(limit: number = 10) {
    const response = await this.client.get('/audit/logs', {
      params: { limit, offset: 0 },
    });
    return response.data;
  }

  // Job endpoints
  async getJobs(params?: JobQuery): Promise<JobListResponse> {
    const response = await this.client.get('/jobs', { params });
    return response.data;
  }

  async getJob(jobId: string): Promise<Job> {
    const response = await this.client.get(`/jobs/${encodeURIComponent(jobId)}`);
    return response.data;
  }

  async createJob(job: CreateJobRequest): Promise<Job> {
    const response = await this.client.post('/jobs', job);
    return response.data;
  }

  async deleteJob(jobId: string): Promise<void> {
    await this.client.delete(`/jobs/${encodeURIComponent(jobId)}`);
  }

  async getJobProgress(jobId: string): Promise<JobProgress> {
    const response = await this.client.get(`/jobs/${encodeURIComponent(jobId)}/progress`);
    return response.data;
  }

  async startJob(jobId: string): Promise<Job> {
    const response = await this.client.post(`/jobs/${encodeURIComponent(jobId)}/start`);
    return response.data;
  }

  async pauseJob(jobId: string): Promise<Job> {
    const response = await this.client.post(`/jobs/${encodeURIComponent(jobId)}/pause`);
    return response.data;
  }

  async resumeJob(jobId: string): Promise<Job> {
    const response = await this.client.post(`/jobs/${encodeURIComponent(jobId)}/resume`);
    return response.data;
  }

  async cancelJob(jobId: string): Promise<Job> {
    const response = await this.client.post(`/jobs/${encodeURIComponent(jobId)}/cancel`);
    return response.data;
  }

  async retryJob(jobId: string): Promise<Job> {
    const response = await this.client.post(`/jobs/${encodeURIComponent(jobId)}/retry`);
    return response.data;
  }

  // Metadata endpoints (Issue #544)
  async getInstanceMetadata(
    sopInstanceUid: string,
    request?: MetadataRequest
  ): Promise<MetadataResponse> {
    const params: Record<string, string> = {};
    if (request?.tags?.length) {
      params.tags = request.tags.join(',');
    }
    if (request?.preset) {
      params.preset = request.preset;
    }
    if (request?.include_private) {
      params.include_private = 'true';
    }
    const response = await this.client.get(
      `/instances/${encodeURIComponent(sopInstanceUid)}/metadata`,
      { params }
    );
    return response.data;
  }

  async getSortedInstances(
    seriesUid: string,
    sortBy: SortOrder = 'position',
    direction: 'asc' | 'desc' = 'asc'
  ): Promise<SortedInstancesResponse> {
    const response = await this.client.get(
      `/series/${encodeURIComponent(seriesUid)}/instances/sorted`,
      { params: { sort_by: sortBy, direction } }
    );
    return response.data;
  }

  async getInstanceNavigation(sopInstanceUid: string): Promise<NavigationInfo> {
    const response = await this.client.get(
      `/instances/${encodeURIComponent(sopInstanceUid)}/navigation`
    );
    return response.data;
  }

  async getWindowLevelPresets(modality?: string): Promise<WindowLevelPresetsResponse> {
    const params: Record<string, string> = {};
    if (modality) {
      params.modality = modality;
    }
    const response = await this.client.get('/presets/window-level', { params });
    return response.data;
  }

  async getInstanceVOILUT(sopInstanceUid: string): Promise<VOILUTInfo> {
    const response = await this.client.get(
      `/instances/${encodeURIComponent(sopInstanceUid)}/voi-lut`
    );
    return response.data;
  }

  async getInstanceFrameInfo(sopInstanceUid: string): Promise<FrameInfo> {
    const response = await this.client.get(
      `/instances/${encodeURIComponent(sopInstanceUid)}/frame-info`
    );
    return response.data;
  }

  // Annotation endpoints (Issue #545, #584)
  async createAnnotation(
    request: AnnotationCreateRequest
  ): Promise<AnnotationCreateResponse> {
    const response = await this.client.post('/annotations', request);
    return response.data;
  }

  async getAnnotations(
    query?: AnnotationQuery
  ): Promise<PaginatedResponse<Annotation>> {
    const response = await this.client.get('/annotations', { params: query });
    return response.data;
  }

  async getAnnotation(annotationId: string): Promise<Annotation> {
    const response = await this.client.get(
      `/annotations/${encodeURIComponent(annotationId)}`
    );
    return response.data;
  }

  async updateAnnotation(
    annotationId: string,
    request: AnnotationUpdateRequest
  ): Promise<AnnotationUpdateResponse> {
    const response = await this.client.put(
      `/annotations/${encodeURIComponent(annotationId)}`,
      request
    );
    return response.data;
  }

  async deleteAnnotation(annotationId: string): Promise<void> {
    await this.client.delete(
      `/annotations/${encodeURIComponent(annotationId)}`
    );
  }

  async getInstanceAnnotations(
    sopInstanceUid: string
  ): Promise<{ data: Annotation[] }> {
    const response = await this.client.get(
      `/instances/${encodeURIComponent(sopInstanceUid)}/annotations`
    );
    return response.data;
  }

  // Measurement endpoints (Issue #545, #584)
  async createMeasurement(
    request: MeasurementCreateRequest
  ): Promise<MeasurementCreateResponse> {
    const response = await this.client.post('/measurements', request);
    return response.data;
  }

  async getMeasurements(
    query?: MeasurementQuery
  ): Promise<PaginatedResponse<Measurement>> {
    const response = await this.client.get('/measurements', { params: query });
    return response.data;
  }

  async getMeasurement(measurementId: string): Promise<Measurement> {
    const response = await this.client.get(
      `/measurements/${encodeURIComponent(measurementId)}`
    );
    return response.data;
  }

  async deleteMeasurement(measurementId: string): Promise<void> {
    await this.client.delete(
      `/measurements/${encodeURIComponent(measurementId)}`
    );
  }

  async getInstanceMeasurements(
    sopInstanceUid: string
  ): Promise<{ data: Measurement[] }> {
    const response = await this.client.get(
      `/instances/${encodeURIComponent(sopInstanceUid)}/measurements`
    );
    return response.data;
  }

  // Key Image endpoints (Issue #545, #584)
  async createKeyImage(
    studyUid: string,
    request: KeyImageCreateRequest
  ): Promise<KeyImageCreateResponse> {
    const response = await this.client.post(
      `/studies/${encodeURIComponent(studyUid)}/key-images`,
      request
    );
    return response.data;
  }

  async getStudyKeyImages(studyUid: string): Promise<KeyImageListResponse> {
    const response = await this.client.get(
      `/studies/${encodeURIComponent(studyUid)}/key-images`
    );
    return response.data;
  }

  async deleteKeyImage(keyImageId: string): Promise<void> {
    await this.client.delete(
      `/key-images/${encodeURIComponent(keyImageId)}`
    );
  }

  async exportKeyImagesAsSR(
    studyUid: string
  ): Promise<KeyObjectSelectionDocument> {
    const response = await this.client.post(
      `/studies/${encodeURIComponent(studyUid)}/key-images/export-sr`
    );
    return response.data;
  }

  // Viewer State endpoints (Issue #545, #584)
  async createViewerState(
    request: ViewerStateCreateRequest
  ): Promise<ViewerStateCreateResponse> {
    const response = await this.client.post('/viewer-states', request);
    return response.data;
  }

  async getViewerStates(
    query?: ViewerStateQuery
  ): Promise<ViewerStateListResponse> {
    const response = await this.client.get('/viewer-states', { params: query });
    return response.data;
  }

  async getViewerState(stateId: string): Promise<ViewerState> {
    const response = await this.client.get(
      `/viewer-states/${encodeURIComponent(stateId)}`
    );
    return response.data;
  }

  async deleteViewerState(stateId: string): Promise<void> {
    await this.client.delete(
      `/viewer-states/${encodeURIComponent(stateId)}`
    );
  }

  // Recent Studies endpoints (Issue #545, #584)
  async getUserRecentStudies(
    userId: string,
    limit?: number
  ): Promise<RecentStudiesResponse> {
    const params: Record<string, number> = {};
    if (limit !== undefined) {
      params.limit = limit;
    }
    const response = await this.client.get(
      `/users/${encodeURIComponent(userId)}/recent-studies`,
      { params }
    );
    return response.data;
  }
}

export const apiClient = new ApiClient();
