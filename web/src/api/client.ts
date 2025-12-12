import axios, { type AxiosError, type AxiosInstance } from 'axios';
import type { ApiError } from '../types/api';

const API_BASE_URL = '/api/v1';

class ApiClient {
  private client: AxiosInstance;

  constructor() {
    this.client = axios.create({
      baseURL: API_BASE_URL,
      headers: {
        'Content-Type': 'application/json',
      },
    });

    // Request interceptor for adding auth headers
    this.client.interceptors.request.use((config) => {
      // For now, use a default user ID. In production, this would come from auth
      config.headers['X-User-ID'] = 'admin';
      return config;
    });

    // Response interceptor for error handling
    this.client.interceptors.response.use(
      (response) => response,
      (error: AxiosError<ApiError>) => {
        if (error.response?.data?.error) {
          const apiError = error.response.data.error;
          throw new Error(`${apiError.code}: ${apiError.message}`);
        }
        throw error;
      }
    );
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

  async updateSystemConfig(config: Record<string, unknown>) {
    const response = await this.client.put('/system/config', config);
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
  async getWorklist(params?: Record<string, string | number | undefined>) {
    const response = await this.client.get('/worklist', { params });
    return response.data;
  }

  async getWorklistItem(itemId: number) {
    const response = await this.client.get(`/worklist/${itemId}`);
    return response.data;
  }

  async createWorklistItem(item: Record<string, unknown>) {
    const response = await this.client.post('/worklist', item);
    return response.data;
  }

  async updateWorklistItem(itemId: number, item: Record<string, unknown>) {
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
  async getAssociations() {
    const response = await this.client.get('/associations');
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
      this.getAssociations().catch(() => ({ data: [], count: 0 })),
      this.getPatients({ limit: 1 }).catch(() => ({ pagination: { total: 0 } })),
      this.getStudies({ limit: 1 }).catch(() => ({ pagination: { total: 0 } })),
    ]);

    const activeAssociations = (associations.data || []).filter(
      (a: { state: string }) => a.state === 'ESTABLISHED'
    ).length;

    return {
      active_associations: activeAssociations,
      total_patients: patients.pagination?.total ?? 0,
      total_studies: studies.pagination?.total ?? 0,
      storage_used_bytes: 0,
      storage_total_bytes: 0,
    };
  }

  // Recent activity (latest audit logs)
  async getRecentActivity(limit: number = 10) {
    const response = await this.client.get('/audit/logs', {
      params: { limit, offset: 0 },
    });
    return response.data;
  }
}

export const apiClient = new ApiClient();
