/**
 * Annotation API client functions
 *
 * @see Issue #545 - Implement Annotation & Measurement APIs
 * @see Issue #584 - Part 4: TypeScript Types & Integration Tests
 */

import axios from 'axios';
import type {
  Annotation,
  AnnotationCreateRequest,
  AnnotationCreateResponse,
  AnnotationUpdateRequest,
  AnnotationUpdateResponse,
  AnnotationQuery,
  AnnotationListResponse,
  AnnotationInstanceResponse,
} from '../types/annotation';

const API_BASE_URL = '/api/v1';

export async function createAnnotation(
  request: AnnotationCreateRequest
): Promise<AnnotationCreateResponse> {
  const response = await axios.post(`${API_BASE_URL}/annotations`, request);
  return response.data;
}

export async function getAnnotations(
  query?: AnnotationQuery
): Promise<AnnotationListResponse> {
  const response = await axios.get(`${API_BASE_URL}/annotations`, {
    params: query,
  });
  return response.data;
}

export async function getAnnotation(annotationId: string): Promise<Annotation> {
  const response = await axios.get(
    `${API_BASE_URL}/annotations/${encodeURIComponent(annotationId)}`
  );
  return response.data;
}

export async function updateAnnotation(
  annotationId: string,
  request: AnnotationUpdateRequest
): Promise<AnnotationUpdateResponse> {
  const response = await axios.put(
    `${API_BASE_URL}/annotations/${encodeURIComponent(annotationId)}`,
    request
  );
  return response.data;
}

export async function deleteAnnotation(annotationId: string): Promise<void> {
  await axios.delete(
    `${API_BASE_URL}/annotations/${encodeURIComponent(annotationId)}`
  );
}

export async function getInstanceAnnotations(
  sopInstanceUid: string
): Promise<AnnotationInstanceResponse> {
  const response = await axios.get(
    `${API_BASE_URL}/instances/${encodeURIComponent(sopInstanceUid)}/annotations`
  );
  return response.data;
}
