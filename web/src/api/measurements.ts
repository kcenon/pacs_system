/**
 * Measurement API client functions
 *
 * @see Issue #545 - Implement Annotation & Measurement APIs
 * @see Issue #584 - Part 4: TypeScript Types & Integration Tests
 */

import axios from 'axios';
import type {
  Measurement,
  MeasurementCreateRequest,
  MeasurementCreateResponse,
  MeasurementQuery,
  MeasurementListResponse,
  MeasurementInstanceResponse,
} from '../types/measurement';

const API_BASE_URL = '/api/v1';

export async function createMeasurement(
  request: MeasurementCreateRequest
): Promise<MeasurementCreateResponse> {
  const response = await axios.post(`${API_BASE_URL}/measurements`, request);
  return response.data;
}

export async function getMeasurements(
  query?: MeasurementQuery
): Promise<MeasurementListResponse> {
  const response = await axios.get(`${API_BASE_URL}/measurements`, {
    params: query,
  });
  return response.data;
}

export async function getMeasurement(
  measurementId: string
): Promise<Measurement> {
  const response = await axios.get(
    `${API_BASE_URL}/measurements/${encodeURIComponent(measurementId)}`
  );
  return response.data;
}

export async function deleteMeasurement(measurementId: string): Promise<void> {
  await axios.delete(
    `${API_BASE_URL}/measurements/${encodeURIComponent(measurementId)}`
  );
}

export async function getInstanceMeasurements(
  sopInstanceUid: string
): Promise<MeasurementInstanceResponse> {
  const response = await axios.get(
    `${API_BASE_URL}/instances/${encodeURIComponent(sopInstanceUid)}/measurements`
  );
  return response.data;
}
